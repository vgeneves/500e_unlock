/*
 * Copyright (c) 2016 Linaro Limited.
 * Copyright (c) 2020 Teslabs Engineering S.L.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT st_stm32_ic

#include <errno.h>

#include <soc.h>
#include <stm32_ll_rcc.h>
#include <stm32_ll_tim.h>
#include <drivers/ic.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/init.h>

#include <zephyr/drivers/clock_control/stm32_clock_control.h>
#include <zephyr/dt-bindings/pwm/stm32_pwm.h>

#include <zephyr/logging/log.h>
#include <zephyr/irq.h>

LOG_MODULE_REGISTER(ic_stm32, CONFIG_PWM_LOG_LEVEL);

/* L0 series MCUs only have 16-bit timers and don't have below macro defined */
#ifndef IS_TIM_32B_COUNTER_INSTANCE
#define IS_TIM_32B_COUNTER_INSTANCE(INSTANCE) (0)
#endif

struct ic_stm32_capture_data {
	ic_capture_callback_handler_t callback;
	void *user_data;
	uint32_t period;
	uint32_t overflows;
	uint8_t skip_irq;
	bool continuous;
};

/* first capture is always nonsense, second is nonsense when polarity changed */
#define SKIPPED_IC_CAPTURES 0u

/** PWM data. */
struct ic_stm32_data {
	/** Timer clock (Hz). */
	uint32_t tim_clk;
	struct ic_stm32_capture_data capture;
};

/** PWM configuration. */
struct ic_stm32_config {
	TIM_TypeDef *timer;
	uint32_t prescaler;
	uint32_t countermode;
	struct stm32_pclken pclken;
	const struct pinctrl_dev_config *pcfg;

	void (*irq_config_func)(const struct device *dev);
};

/** Maximum number of timer channels : some stm32 soc have 6 else only 4 */
#if defined(LL_TIM_CHANNEL_CH6)
#define TIMER_HAS_6CH 1
#define TIMER_MAX_CH 6u
#else
#define TIMER_HAS_6CH 0
#define TIMER_MAX_CH 4u
#endif

/**
 * Obtain timer clock speed.
 *
 * @param pclken  Timer clock control subsystem.
 * @param tim_clk Where computed timer clock will be stored.
 *
 * @return 0 on success, error code otherwise.
 */
static int get_tim_clk(const struct stm32_pclken *pclken, uint32_t *tim_clk)
{
	int r;
	const struct device *clk;
	uint32_t bus_clk, apb_psc;

	clk = DEVICE_DT_GET(STM32_CLOCK_CONTROL_NODE);

	r = clock_control_get_rate(clk, (clock_control_subsys_t)pclken,
				   &bus_clk);
	if (r < 0) {
		return r;
	}

#if defined(CONFIG_SOC_SERIES_STM32H7X)
	if (pclken->bus == STM32_CLOCK_BUS_APB1) {
		apb_psc = STM32_D2PPRE1;
	} else {
		apb_psc = STM32_D2PPRE2;
	}
#else
	if (pclken->bus == STM32_CLOCK_BUS_APB1) {
		apb_psc = STM32_APB1_PRESCALER;
	}
#if !defined(CONFIG_SOC_SERIES_STM32C0X) && !defined(CONFIG_SOC_SERIES_STM32F0X) &&                \
	!defined(CONFIG_SOC_SERIES_STM32G0X)
	else {
		apb_psc = STM32_APB2_PRESCALER;
	}
#endif
#endif

#if defined(RCC_DCKCFGR_TIMPRE) || defined(RCC_DCKCFGR1_TIMPRE) || \
	defined(RCC_CFGR_TIMPRE)
	/*
	 * There are certain series (some F4, F7 and H7) that have the TIMPRE
	 * bit to control the clock frequency of all the timers connected to
	 * APB1 and APB2 domains.
	 *
	 * Up to a certain threshold value of APB{1,2} prescaler, timer clock
	 * equals to HCLK. This threshold value depends on TIMPRE setting
	 * (2 if TIMPRE=0, 4 if TIMPRE=1). Above threshold, timer clock is set
	 * to a multiple of the APB domain clock PCLK{1,2} (2 if TIMPRE=0, 4 if
	 * TIMPRE=1).
	 */

	if (LL_RCC_GetTIMPrescaler() == LL_RCC_TIM_PRESCALER_TWICE) {
		/* TIMPRE = 0 */
		if (apb_psc <= 2u) {
			LL_RCC_ClocksTypeDef clocks;

			LL_RCC_GetSystemClocksFreq(&clocks);
			*tim_clk = clocks.HCLK_Frequency;
		} else {
			*tim_clk = bus_clk * 2u;
		}
	} else {
		/* TIMPRE = 1 */
		if (apb_psc <= 4u) {
			LL_RCC_ClocksTypeDef clocks;

			LL_RCC_GetSystemClocksFreq(&clocks);
			*tim_clk = clocks.HCLK_Frequency;
		} else {
			*tim_clk = bus_clk * 4u;
		}
	}
#else
	/*
	 * If the APB prescaler equals 1, the timer clock frequencies
	 * are set to the same frequency as that of the APB domain.
	 * Otherwise, they are set to twice (×2) the frequency of the
	 * APB domain.
	 */
	if (apb_psc == 1u) {
		*tim_clk = bus_clk;
	} else {
		*tim_clk = bus_clk * 2u;
	}
#endif

	return 0;
}

static int init_capture_channel(const struct device *dev, uint32_t channel,
				ic_flags_t flags, uint32_t ll_channel)
{
	const struct ic_stm32_config *cfg = dev->config;
	bool is_inverted = (flags & PWM_POLARITY_MASK) == PWM_POLARITY_INVERTED;
	LL_TIM_IC_InitTypeDef ic;

	LL_TIM_IC_StructInit(&ic);
	ic.ICPrescaler = TIM_ICPSC_DIV1;
	ic.ICFilter = LL_TIM_IC_FILTER_FDIV1;

	if (ll_channel == LL_TIM_CHANNEL_CH1) {
		if (channel == 1u) {
			ic.ICActiveInput = LL_TIM_ACTIVEINPUT_DIRECTTI;
			ic.ICPolarity = is_inverted ? LL_TIM_IC_POLARITY_FALLING
					: LL_TIM_IC_POLARITY_RISING;
		} else {
			ic.ICActiveInput = LL_TIM_ACTIVEINPUT_INDIRECTTI;
			ic.ICPolarity = is_inverted ? LL_TIM_IC_POLARITY_RISING
					: LL_TIM_IC_POLARITY_FALLING;
		}
	} else {
		if (channel == 1u) {
			ic.ICActiveInput = LL_TIM_ACTIVEINPUT_INDIRECTTI;
			ic.ICPolarity = is_inverted ? LL_TIM_IC_POLARITY_RISING
					: LL_TIM_IC_POLARITY_FALLING;
		} else {
			ic.ICActiveInput = LL_TIM_ACTIVEINPUT_DIRECTTI;
			ic.ICPolarity = is_inverted ? LL_TIM_IC_POLARITY_FALLING
					: LL_TIM_IC_POLARITY_RISING;
		}
	}

	if (LL_TIM_IC_Init(cfg->timer, ll_channel, &ic) != SUCCESS) {
		LOG_ERR("Could not initialize channel for PWM capture");
		return -EIO;
	}

	return 0;
}

static int ic_stm32_configure_capture(const struct device *dev,
				       uint32_t channel, ic_flags_t flags,
				       ic_capture_callback_handler_t cb,
				       void *user_data)
{

	/*
	 * Capture is implemented using the slave mode controller.
	 * This allows for high accuracy, but only CH1 and CH2 are supported.
	 * Alternatively all channels could be supported with ISR based resets.
	 * This is currently not implemented!
	 */

	const struct ic_stm32_config *cfg = dev->config;
	struct ic_stm32_data *data = dev->data;
	struct ic_stm32_capture_data *cpt = &data->capture;
	int ret;

	if ((channel != 1u)) {
		LOG_ERR("PWM capture only supported on first channel");
		return -ENOTSUP;
	}

	if (LL_TIM_IsEnabledIT_CC1(cfg->timer)) {
		LOG_ERR("PWM Capture already in progress");
		return -EBUSY;
	}

	if (!(flags & IC_CAPTURE_TYPE_PERIOD)) {
		LOG_ERR("Only Period PWM capture is supported");
		return -EINVAL;
	}

	cpt->callback = cb; /* even if the cb is reset, this is not an error */
	cpt->user_data = user_data;
	cpt->continuous = (flags & IC_CAPTURE_MODE_CONTINUOUS) ? true : false;

	ret = init_capture_channel(dev, channel, flags, LL_TIM_CHANNEL_CH1);
	if (ret < 0) {
		return ret;
	}

	//LL_TIM_SetTriggerInput(cfg->timer, LL_TIM_TS_TI1FP1);
	//LL_TIM_SetTriggerInput(cfg->timer, LL_TIM_TS_ITR0);

	LL_TIM_EnableARRPreload(cfg->timer);
	if (!IS_TIM_32B_COUNTER_INSTANCE(cfg->timer)) {
		LL_TIM_SetAutoReload(cfg->timer, 0xffffu);
	} else {
		LL_TIM_SetAutoReload(cfg->timer, 0xffffffffu);
	}
	LL_TIM_EnableUpdateEvent(cfg->timer);

	return 0;
}

static int ic_stm32_enable_capture(const struct device *dev, uint32_t channel)
{
	const struct ic_stm32_config *cfg = dev->config;
	struct ic_stm32_data *data = dev->data;

	if ((channel != 1u) && (channel != 2u)) {
		LOG_ERR("PWM capture only supported on first two channels");
		return -EINVAL;
	}

	if (LL_TIM_IsEnabledIT_CC1(cfg->timer)) {
		LOG_ERR("PWM capture already active");
		return -EBUSY;
	}

	if (!data->capture.callback) {
		LOG_ERR("PWM capture not configured");
		return -EINVAL;
	}

	data->capture.skip_irq = SKIPPED_IC_CAPTURES;
	data->capture.overflows = 0u;
	LL_TIM_ClearFlag_CC1(cfg->timer);
	LL_TIM_ClearFlag_UPDATE(cfg->timer);

	//LL_TIM_SetUpdateSource(cfg->timer, LL_TIM_UPDATESOURCE_COUNTER);
	LL_TIM_EnableIT_CC1(cfg->timer);
	
	LL_TIM_EnableIT_UPDATE(cfg->timer);
	LL_TIM_CC_EnableChannel(cfg->timer, LL_TIM_CHANNEL_CH1);
	LL_TIM_GenerateEvent_UPDATE(cfg->timer);
	
	return 0;
}

static int ic_stm32_disable_capture(const struct device *dev, uint32_t channel)
{
	const struct ic_stm32_config *cfg = dev->config;

	if ((channel != 1u) && (channel != 2u)) {
		LOG_ERR("PWM capture only supported on first two channels");
		return -EINVAL;
	}

	LL_TIM_SetUpdateSource(cfg->timer, LL_TIM_UPDATESOURCE_REGULAR);
	LL_TIM_DisableIT_CC1(cfg->timer);
	LL_TIM_DisableIT_UPDATE(cfg->timer);
	LL_TIM_CC_DisableChannel(cfg->timer, LL_TIM_CHANNEL_CH1);

	return 0;
}

static void get_pwm_capture(const struct device *dev, uint32_t channel)
{
	const struct ic_stm32_config *cfg = dev->config;
	struct ic_stm32_data *data = dev->data;
	struct ic_stm32_capture_data *cpt = &data->capture;

	cpt->period = LL_TIM_IC_GetCaptureCH1(cfg->timer);
}

static void ic_stm32_isr(const struct device *dev)
{
	const struct ic_stm32_config *cfg = dev->config;
	struct ic_stm32_data *data = dev->data;
	struct ic_stm32_capture_data *cpt = &data->capture;
	int status = 0;
	uint32_t in_ch = LL_TIM_IsEnabledIT_CC1(cfg->timer) ? 1u : 2u;
	
	if (cpt->skip_irq == 0u) {
		if (LL_TIM_IsActiveFlag_UPDATE(cfg->timer)) {
			LL_TIM_ClearFlag_UPDATE(cfg->timer);
			cpt->overflows++;
			LOG_ERR("counter overflow during PWM capture");
			status = -ERANGE;
			if (cpt->callback != NULL) {
				cpt->callback(dev, in_ch, 0xFFFF,
					0u,
					status, cpt->user_data);
			}
		}

		if (LL_TIM_IsActiveFlag_CC1(cfg->timer)) {
			LL_TIM_ClearFlag_CC1(cfg->timer);

			get_pwm_capture(dev, in_ch);

			
				

			if (!cpt->continuous) {
				ic_stm32_disable_capture(dev, in_ch);
			} else {
				cpt->overflows = 0u;
			}

			LL_TIM_SetCounter(cfg->timer, 0);

			if (cpt->callback != NULL) {
				cpt->callback(dev, in_ch, cpt->period,
					0u,
					status, cpt->user_data);
			}
		}
	} else {
		if (LL_TIM_IsActiveFlag_UPDATE(cfg->timer)) {
			LL_TIM_ClearFlag_UPDATE(cfg->timer);
		}

		if (LL_TIM_IsActiveFlag_CC1(cfg->timer)) {
			LL_TIM_ClearFlag_CC1(cfg->timer);
			cpt->skip_irq--;
		}
	}
}

static int ic_stm32_get_cycles_per_sec(const struct device *dev,
					uint32_t channel, uint64_t *cycles)
{
	struct ic_stm32_data *data = dev->data;
	const struct ic_stm32_config *cfg = dev->config;

	*cycles = (uint64_t)(data->tim_clk / (cfg->prescaler + 1));

	return 0;
}

static const struct ic_driver_api ic_stm32_driver_api = {
	.get_cycles_per_sec = ic_stm32_get_cycles_per_sec,

	.configure_capture = ic_stm32_configure_capture,
	.enable_capture = ic_stm32_enable_capture,
	.disable_capture = ic_stm32_disable_capture,
};

static int ic_stm32_init(const struct device *dev)
{
	struct ic_stm32_data *data = dev->data;
	const struct ic_stm32_config *cfg = dev->config;

	int r;
	const struct device *clk;
	LL_TIM_InitTypeDef init;

	/* enable clock and store its speed */
	clk = DEVICE_DT_GET(STM32_CLOCK_CONTROL_NODE);

	if (!device_is_ready(clk)) {
		LOG_ERR("clock control device not ready");
		return -ENODEV;
	}

	r = clock_control_on(clk, (clock_control_subsys_t)&cfg->pclken);
	if (r < 0) {
		LOG_ERR("Could not initialize clock (%d)", r);
		return r;
	}

	r = get_tim_clk(&cfg->pclken, &data->tim_clk);
	if (r < 0) {
		LOG_ERR("Could not obtain timer clock (%d)", r);
		return r;
	}

	/* configure pinmux */
	r = pinctrl_apply_state(cfg->pcfg, PINCTRL_STATE_DEFAULT);
	if (r < 0) {
		LOG_ERR("PWM pinctrl setup failed (%d)", r);
		return r;
	}

	/* initialize timer */
	LL_TIM_StructInit(&init);

	init.Prescaler = cfg->prescaler;
	init.CounterMode = cfg->countermode;
	init.Autoreload = 0u;
	init.ClockDivision = LL_TIM_CLOCKDIVISION_DIV1;

	if (LL_TIM_Init(cfg->timer, &init) != SUCCESS) {
		LOG_ERR("Could not initialize timer");
		return -EIO;
	}

#if !defined(CONFIG_SOC_SERIES_STM32L0X) && !defined(CONFIG_SOC_SERIES_STM32L1X)
	/* enable outputs and counter */
	if (IS_TIM_BREAK_INSTANCE(cfg->timer)) {
		LL_TIM_EnableAllOutputs(cfg->timer);
	}
#endif

	LL_TIM_EnableCounter(cfg->timer);

	cfg->irq_config_func(dev);

	return 0;
}

#define IRQ_CONFIG_FUNC(index)                                                 \
static void ic_stm32_irq_config_func_##index(const struct device *dev)        \
{                                                                              \
	IRQ_CONNECT(DT_IRQN(DT_INST_PARENT(index)),                            \
			DT_IRQ(DT_INST_PARENT(index), priority),               \
			ic_stm32_isr, DEVICE_DT_INST_GET(index), 0);          \
	irq_enable(DT_IRQN(DT_INST_PARENT(index)));                            \
}
#define CAPTURE_INIT(index)                                                    \
	.irq_config_func = ic_stm32_irq_config_func_##index

#define DT_INST_CLK(index, inst)                                               \
	{                                                                      \
		.bus = DT_CLOCKS_CELL(DT_INST_PARENT(index), bus),             \
		.enr = DT_CLOCKS_CELL(DT_INST_PARENT(index), bits)             \
	}

#define IC_DEVICE_INIT(index)                                                 \
	static struct ic_stm32_data ic_stm32_data_##index;                   \
	IRQ_CONFIG_FUNC(index)						       \
									       \
	PINCTRL_DT_INST_DEFINE(index);					       \
									       \
	static const struct ic_stm32_config ic_stm32_config_##index = {      \
		.timer = (TIM_TypeDef *)DT_REG_ADDR(DT_INST_PARENT(index)),    \
		.prescaler = DT_PROP(DT_INST_PARENT(index), st_prescaler),     \
		.countermode = DT_PROP(DT_INST_PARENT(index), st_countermode), \
		.pclken = DT_INST_CLK(index, timer),                           \
		.pcfg = PINCTRL_DT_INST_DEV_CONFIG_GET(index),		       \
		CAPTURE_INIT(index)					       \
	};                                                                     \
									       \
	DEVICE_DT_INST_DEFINE(index, &ic_stm32_init, NULL,                    \
			    &ic_stm32_data_##index,                           \
			    &ic_stm32_config_##index, POST_KERNEL,            \
			    CONFIG_PWM_INIT_PRIORITY,                          \
			    &ic_stm32_driver_api);

DT_INST_FOREACH_STATUS_OKAY(IC_DEVICE_INIT)
