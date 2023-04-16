#include <zephyr/kernel.h>

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

	while (1) {
		k_sleep(K_MSEC(1000));
	}
}

