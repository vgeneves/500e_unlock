#include "zephyr/dt-bindings/pwm/pwm.h"
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/pwm.h>
#include <drivers/ic.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main, CONFIG_APP_LOG_LEVEL);


#define IC_IN_IDX 0
#define PWM_OUT_IDX 1
#define PWM_TEST_IDX 2

#define PWM_NODE DT_INST(0, app_pwm_ios)

#define IC_IN_CTLR \
	DT_PWMS_CTLR_BY_IDX(PWM_NODE, IC_IN_IDX)
#define IC_IN_CHANNEL \
	DT_PWMS_CHANNEL_BY_IDX(PWM_NODE, IC_IN_IDX)
#define IC_IN_FLAGS \
	DT_PWMS_FLAGS_BY_IDX(PWM_NODE, IC_IN_IDX)

#define PWM_OUT_CTLR \
	DT_PWMS_CTLR_BY_IDX(PWM_NODE, PWM_OUT_IDX)
#define PWM_OUT_CHANNEL \
	DT_PWMS_CHANNEL_BY_IDX(PWM_NODE, PWM_OUT_IDX)
#define PWM_OUT_FLAGS \
	DT_PWMS_FLAGS_BY_IDX(PWM_NODE, PWM_OUT_IDX)

#define PWM_TEST_CTLR \
	DT_PWMS_CTLR_BY_IDX(PWM_NODE, PWM_TEST_IDX)
#define PWM_TEST_CHANNEL \
	DT_PWMS_CHANNEL_BY_IDX(PWM_NODE, PWM_TEST_IDX)
#define PWM_TEST_FLAGS \
	DT_PWMS_FLAGS_BY_IDX(PWM_NODE, PWM_TEST_IDX)

struct test_pwm {
	const struct device *dev;
	uint32_t pwm;
	pwm_flags_t flags;
};

static void continuous_capture_callback(const struct device *dev,
					uint32_t pwm,
					uint32_t period_cycles,
					uint32_t pulse_cycles,
					int status,
					void *user_data)
{
	uint64_t period;
	struct test_pwm out;

	out.dev = DEVICE_DT_GET(PWM_OUT_CTLR);
	out.pwm = PWM_OUT_CHANNEL;
	out.flags = PWM_OUT_FLAGS;

	ic_cycles_to_usec(dev, pwm, period_cycles, &period);

	if (status == 0) {
		printk("%d/%d \n",period_cycles, (uint32_t)period / 1000);
		pwm_set(out.dev, out.pwm, PWM_MSEC(period/1000), PWM_MSEC((3 * period) / 4000), 0);
	} else {
		printk("Overflow (%d) \n", status);
		pwm_set(out.dev, out.pwm, PWM_MSEC(0), PWM_MSEC(0), 0);
	}
	
}

void main(void)
{
	struct test_pwm in, out, test;

	LOG_INF("500e speed unlock");

	in.dev = DEVICE_DT_GET(IC_IN_CTLR);
	in.pwm = IC_IN_CHANNEL;
	in.flags = IC_IN_FLAGS;
	if (!device_is_ready(in.dev)) {
		printk("pwm loopback intput device is not ready\n");
		return;
	}

	out.dev = DEVICE_DT_GET(PWM_OUT_CTLR);
	out.pwm = PWM_OUT_CHANNEL;
	out.flags = PWM_OUT_FLAGS;
	if (!device_is_ready(out.dev)) {
		printk("pwm loopback output device is not ready\n");
		return;
	}

	test.dev = DEVICE_DT_GET(PWM_TEST_CTLR);
	test.pwm = PWM_TEST_CHANNEL;
	test.flags = PWM_TEST_FLAGS;
	if (!device_is_ready(test.dev)) {
		printk("pwm loopback test device is not ready\n");
		return;
	}

	if (pwm_set(test.dev, test.pwm, PWM_MSEC(1000), PWM_MSEC(250), 0)) {
			printk("Fail to set the period and pulse width\n");
			return;
	}

	if(ic_configure_capture(in.dev, in.pwm, IC_CAPTURE_MODE_CONTINUOUS |
					    IC_CAPTURE_TYPE_PERIOD | PWM_POLARITY_NORMAL,
					    continuous_capture_callback, NULL))
		printk("Failed to configure capture");

	printk("PWM DONE\n");
	ic_enable_capture(in.dev, in.pwm);
	while (1) {
		static int i = 0;

		i++;
		if (i > 300)
			i = 0;

		pwm_set(test.dev, test.pwm, PWM_MSEC(4 * i), PWM_MSEC(3 * i), 0);

		printk("Set %d msec\n", 4*i);
		k_sleep(K_MSEC(1000));
	}
}

