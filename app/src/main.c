#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main, CONFIG_APP_LOG_LEVEL);

void main(void)
{
	int ret;
	const struct device *sensor;

	LOG_INF("500e speed unlock");

	while (1) {
		k_sleep(K_MSEC(1000));
	}
}

