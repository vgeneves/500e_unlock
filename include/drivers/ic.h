/*
 * Copyright (c) 2016 Intel Corporation.
 * Copyright (c) 2020-2021 Vestas Wind Systems A/S
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Public IC Driver APIs
 */

#ifndef ZEPHYR_INCLUDE_DRIVERS_IC_H_
#define ZEPHYR_INCLUDE_DRIVERS_IC_H_

/**
 * @brief IC Interface
 * @defgroup ic_interface IC Interface
 * @ingroup io_interfaces
 * @{
 */

#include <errno.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/sys_clock.h>
#include <zephyr/sys/math_extras.h>
#include <zephyr/toolchain.h>

#include <zephyr/dt-bindings/pwm/pwm.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @name IC capture configuration flags
 * @anchor IC_CAPTURE_FLAGS
 * @{
 */

/** @cond INTERNAL_HIDDEN */
/* Bit 0 is used for IC_POLARITY_NORMAL/IC_POLARITY_INVERTED */
#define IC_CAPTURE_TYPE_SHIFT		1U
#define IC_CAPTURE_TYPE_MASK		(3U << IC_CAPTURE_TYPE_SHIFT)
#define IC_CAPTURE_MODE_SHIFT		3U
#define IC_CAPTURE_MODE_MASK		(1U << IC_CAPTURE_MODE_SHIFT)
/** @endcond */

/** IC pin capture captures period. */
#define IC_CAPTURE_TYPE_PERIOD		(1U << IC_CAPTURE_TYPE_SHIFT)

/** IC pin capture captures pulse width. */
#define IC_CAPTURE_TYPE_PULSE		(2U << IC_CAPTURE_TYPE_SHIFT)

/** IC pin capture captures both period and pulse width. */
#define IC_CAPTURE_TYPE_BOTH		(IC_CAPTURE_TYPE_PERIOD | \
					 IC_CAPTURE_TYPE_PULSE)

/** IC pin capture captures a single period/pulse width. */
#define IC_CAPTURE_MODE_SINGLE		(0U << IC_CAPTURE_MODE_SHIFT)

/** IC pin capture captures period/pulse width continuously. */
#define IC_CAPTURE_MODE_CONTINUOUS	(1U << IC_CAPTURE_MODE_SHIFT)

/** @} */

/**
 * @brief Provides a type to hold IC configuration flags.
 *
 * The lower 8 bits are used for standard flags.
 * The upper 8 bits are reserved for SoC specific flags.
 *
 * @see @ref IC_CAPTURE_FLAGS.
 */

typedef uint16_t ic_flags_t;

/**
 * @brief Container for IC information specified in devicetree.
 *
 * This type contains a pointer to a IC device, channel number (controlled by
 * the IC device), the IC signal period in nanoseconds and the flags
 * applicable to the channel. Note that not all IC drivers support flags. In
 * such case, flags will be set to 0.
 *
 * @see IC_DT_SPEC_GET_BY_NAME
 * @see IC_DT_SPEC_GET_BY_NAME_OR
 * @see IC_DT_SPEC_GET_BY_IDX
 * @see IC_DT_SPEC_GET_BY_IDX_OR
 * @see IC_DT_SPEC_GET
 * @see IC_DT_SPEC_GET_OR
 */
struct ic_dt_spec {
	/** IC device instance. */
	const struct device *dev;
	/** Channel number. */
	uint32_t channel;
	/** Period in nanoseconds. */
	uint32_t period;
	/** Flags. */
	ic_flags_t flags;
};

/**
 * @brief Static initializer for a struct ic_dt_spec
 *
 * This returns a static initializer for a struct ic_dt_spec given a devicetree
 * node identifier and an index.
 *
 * Example devicetree fragment:
 *
 * @code{.dts}
 *    n: node {
 *        ics = <&ic1 1 1000 IC_POLARITY_NORMAL>,
 *               <&ic2 3 2000 IC_POLARITY_INVERTED>;
 *        ic-names = "alpha", "beta";
 *    };
 * @endcode
 *
 * Example usage:
 *
 * @code{.c}
 *    const struct ic_dt_spec spec =
 *        IC_DT_SPEC_GET_BY_NAME(DT_NODELABEL(n), alpha);
 *
 *    // Initializes 'spec' to:
 *    // {
 *    //         .dev = DEVICE_DT_GET(DT_NODELABEL(ic1)),
 *    //         .channel = 1,
 *    //         .period = 1000,
 *    //         .flags = IC_POLARITY_NORMAL,
 *    // }
 * @endcode
 *
 * The device (dev) must still be checked for readiness, e.g. using
 * device_is_ready(). It is an error to use this macro unless the node exists,
 * has the 'ics' property, and that 'ics' property specifies a IC controller,
 * a channel, a period in nanoseconds and optionally flags.
 *
 * @param node_id Devicetree node identifier.
 * @param name Lowercase-and-underscores name of a ics element as defined by
 *             the node's ic-names property.
 *
 * @return Static initializer for a struct ic_dt_spec for the property.
 *
 * @see IC_DT_SPEC_INST_GET_BY_NAME
 */
#define IC_DT_SPEC_GET_BY_NAME(node_id, name)				       \
	{								       \
		.dev = DEVICE_DT_GET(DT_ICS_CTLR_BY_NAME(node_id, name)),     \
		.channel = DT_ICS_CHANNEL_BY_NAME(node_id, name),	       \
		.period = DT_ICS_PERIOD_BY_NAME(node_id, name),	       \
		.flags = DT_ICS_FLAGS_BY_NAME(node_id, name),		       \
	}

/**
 * @brief Static initializer for a struct ic_dt_spec from a DT_DRV_COMPAT
 *        instance.
 *
 * @param inst DT_DRV_COMPAT instance number
 * @param name Lowercase-and-underscores name of a ics element as defined by
 *             the node's ic-names property.
 *
 * @return Static initializer for a struct ic_dt_spec for the property.
 *
 * @see IC_DT_SPEC_GET_BY_NAME
 */
#define IC_DT_SPEC_INST_GET_BY_NAME(inst, name)			       \
	IC_DT_SPEC_GET_BY_NAME(DT_DRV_INST(inst), name)

/**
 * @brief Like IC_DT_SPEC_GET_BY_NAME(), with a fallback to a default value.
 *
 * If the devicetree node identifier 'node_id' refers to a node with a property
 * 'ics', this expands to <tt>IC_DT_SPEC_GET_BY_NAME(node_id, name)</tt>. The
 * @p default_value parameter is not expanded in this case. Otherwise, this
 * expands to @p default_value.
 *
 * @param node_id Devicetree node identifier.
 * @param name Lowercase-and-underscores name of a ics element as defined by
 *             the node's ic-names property
 * @param default_value Fallback value to expand to.
 *
 * @return Static initializer for a struct ic_dt_spec for the property,
 *         or @p default_value if the node or property do not exist.
 *
 * @see IC_DT_SPEC_INST_GET_BY_NAME_OR
 */
#define IC_DT_SPEC_GET_BY_NAME_OR(node_id, name, default_value)	       \
	COND_CODE_1(DT_NODE_HAS_PROP(node_id, ics),			       \
		    (IC_DT_SPEC_GET_BY_NAME(node_id, name)),		       \
		    (default_value))

/**
 * @brief Like IC_DT_SPEC_INST_GET_BY_NAME(), with a fallback to a default
 *        value.
 *
 * @param inst DT_DRV_COMPAT instance number
 * @param name Lowercase-and-underscores name of a ics element as defined by
 *             the node's ic-names property.
 * @param default_value Fallback value to expand to.
 *
 * @return Static initializer for a struct ic_dt_spec for the property,
 *         or @p default_value if the node or property do not exist.
 *
 * @see IC_DT_SPEC_GET_BY_NAME_OR
 */
#define IC_DT_SPEC_INST_GET_BY_NAME_OR(inst, name, default_value)	       \
	IC_DT_SPEC_GET_BY_NAME_OR(DT_DRV_INST(inst), name, default_value)

/**
 * @brief Static initializer for a struct ic_dt_spec
 *
 * This returns a static initializer for a struct ic_dt_spec given a devicetree
 * node identifier and an index.
 *
 * Example devicetree fragment:
 *
 * @code{.dts}
 *    n: node {
 *        ics = <&ic1 1 1000 IC_POLARITY_NORMAL>,
 *               <&ic2 3 2000 IC_POLARITY_INVERTED>;
 *    };
 * @endcode
 *
 * Example usage:
 *
 * @code{.c}
 *    const struct ic_dt_spec spec =
 *        IC_DT_SPEC_GET_BY_IDX(DT_NODELABEL(n), 1);
 *
 *    // Initializes 'spec' to:
 *    // {
 *    //         .dev = DEVICE_DT_GET(DT_NODELABEL(ic2)),
 *    //         .channel = 3,
 *    //         .period = 2000,
 *    //         .flags = IC_POLARITY_INVERTED,
 *    // }
 * @endcode
 *
 * The device (dev) must still be checked for readiness, e.g. using
 * device_is_ready(). It is an error to use this macro unless the node exists,
 * has the 'ics' property, and that 'ics' property specifies a IC controller,
 * a channel, a period in nanoseconds and optionally flags.
 *
 * @param node_id Devicetree node identifier.
 * @param idx Logical index into 'ics' property.
 *
 * @return Static initializer for a struct ic_dt_spec for the property.
 *
 * @see IC_DT_SPEC_INST_GET_BY_IDX
 */
#define IC_DT_SPEC_GET_BY_IDX(node_id, idx)				       \
	{								       \
		.dev = DEVICE_DT_GET(DT_ICS_CTLR_BY_IDX(node_id, idx)),       \
		.channel = DT_ICS_CHANNEL_BY_IDX(node_id, idx),	       \
		.period = DT_ICS_PERIOD_BY_IDX(node_id, idx),		       \
		.flags = DT_ICS_FLAGS_BY_IDX(node_id, idx),		       \
	}

/**
 * @brief Static initializer for a struct ic_dt_spec from a DT_DRV_COMPAT
 *        instance.
 *
 * @param inst DT_DRV_COMPAT instance number
 * @param idx Logical index into 'ics' property.
 *
 * @return Static initializer for a struct ic_dt_spec for the property.
 *
 * @see IC_DT_SPEC_GET_BY_IDX
 */
#define IC_DT_SPEC_INST_GET_BY_IDX(inst, idx)				       \
	IC_DT_SPEC_GET_BY_IDX(DT_DRV_INST(inst), idx)

/**
 * @brief Like IC_DT_SPEC_GET_BY_IDX(), with a fallback to a default value.
 *
 * If the devicetree node identifier 'node_id' refers to a node with a property
 * 'ics', this expands to <tt>IC_DT_SPEC_GET_BY_IDX(node_id, idx)</tt>. The
 * @p default_value parameter is not expanded in this case. Otherwise, this
 * expands to @p default_value.
 *
 * @param node_id Devicetree node identifier.
 * @param idx Logical index into 'ics' property.
 * @param default_value Fallback value to expand to.
 *
 * @return Static initializer for a struct ic_dt_spec for the property,
 *         or @p default_value if the node or property do not exist.
 *
 * @see IC_DT_SPEC_INST_GET_BY_IDX_OR
 */
#define IC_DT_SPEC_GET_BY_IDX_OR(node_id, idx, default_value)		       \
	COND_CODE_1(DT_NODE_HAS_PROP(node_id, ics),			       \
		    (IC_DT_SPEC_GET_BY_IDX(node_id, idx)),		       \
		    (default_value))

/**
 * @brief Like IC_DT_SPEC_INST_GET_BY_IDX(), with a fallback to a default
 *        value.
 *
 * @param inst DT_DRV_COMPAT instance number
 * @param idx Logical index into 'ics' property.
 * @param default_value Fallback value to expand to.
 *
 * @return Static initializer for a struct ic_dt_spec for the property,
 *         or @p default_value if the node or property do not exist.
 *
 * @see IC_DT_SPEC_GET_BY_IDX_OR
 */
#define IC_DT_SPEC_INST_GET_BY_IDX_OR(inst, idx, default_value)	       \
	IC_DT_SPEC_GET_BY_IDX_OR(DT_DRV_INST(inst), idx, default_value)

/**
 * @brief Equivalent to <tt>IC_DT_SPEC_GET_BY_IDX(node_id, 0)</tt>.
 *
 * @param node_id Devicetree node identifier.
 *
 * @return Static initializer for a struct ic_dt_spec for the property.
 *
 * @see IC_DT_SPEC_GET_BY_IDX
 * @see IC_DT_SPEC_INST_GET
 */
#define IC_DT_SPEC_GET(node_id) IC_DT_SPEC_GET_BY_IDX(node_id, 0)

/**
 * @brief Equivalent to <tt>IC_DT_SPEC_INST_GET_BY_IDX(inst, 0)</tt>.
 *
 * @param inst DT_DRV_COMPAT instance number
 *
 * @return Static initializer for a struct ic_dt_spec for the property.
 *
 * @see IC_DT_SPEC_INST_GET_BY_IDX
 * @see IC_DT_SPEC_GET
 */
#define IC_DT_SPEC_INST_GET(inst) IC_DT_SPEC_GET(DT_DRV_INST(inst))

/**
 * @brief Equivalent to
 *        <tt>IC_DT_SPEC_GET_BY_IDX_OR(node_id, 0, default_value)</tt>.
 *
 * @param node_id Devicetree node identifier.
 * @param default_value Fallback value to expand to.
 *
 * @return Static initializer for a struct ic_dt_spec for the property.
 *
 * @see IC_DT_SPEC_GET_BY_IDX_OR
 * @see IC_DT_SPEC_INST_GET_OR
 */
#define IC_DT_SPEC_GET_OR(node_id, default_value)			       \
	IC_DT_SPEC_GET_BY_IDX_OR(node_id, 0, default_value)

/**
 * @brief Equivalent to
 *        <tt>IC_DT_SPEC_INST_GET_BY_IDX_OR(inst, 0, default_value)</tt>.
 *
 * @param inst DT_DRV_COMPAT instance number
 * @param default_value Fallback value to expand to.
 *
 * @return Static initializer for a struct ic_dt_spec for the property.
 *
 * @see IC_DT_SPEC_INST_GET_BY_IDX_OR
 * @see IC_DT_SPEC_GET_OR
 */
#define IC_DT_SPEC_INST_GET_OR(inst, default_value)			       \
	IC_DT_SPEC_GET_OR(DT_DRV_INST(inst), default_value)

/**
 * @brief IC capture callback handler function signature
 *
 * @note The callback handler will be called in interrupt context.
 *
 *
 * @param[in] dev IC device instance.
 * @param channel IC channel.

 * @param period_cycles Captured IC period width (in clock cycles). HW
 *                      specific.
 * @param pulse_cycles Captured IC pulse width (in clock cycles). HW specific.
 * @param status Status for the IC capture (0 if no error, negative errno
 *               otherwise. See ic_capture_cycles() return value
 *               descriptions for details).
 * @param user_data User data passed to ic_configure_capture()
 */
typedef void (*ic_capture_callback_handler_t)(const struct device *dev,
					       uint32_t channel,
					       uint32_t period_cycles,
					       uint32_t pulse_cycles,
					       int status, void *user_data);

/** @cond INTERNAL_HIDDEN */
/**
 * @brief IC driver API call to configure IC pin period and pulse width.
 * @see ic_set_cycles() for argument description.
 */
typedef int (*ic_set_cycles_t)(const struct device *dev, uint32_t channel,
				uint32_t period_cycles, uint32_t pulse_cycles,
				ic_flags_t flags);

/**
 * @brief IC driver API call to obtain the IC cycles per second (frequency).
 * @see ic_get_cycles_per_sec() for argument description
 */
typedef int (*ic_get_cycles_per_sec_t)(const struct device *dev,
					uint32_t channel, uint64_t *cycles);

/**
 * @brief IC driver API call to configure IC capture.
 * @see ic_configure_capture() for argument description.
 */
typedef int (*ic_configure_capture_t)(const struct device *dev,
				       uint32_t channel, ic_flags_t flags,
				       ic_capture_callback_handler_t cb,
				       void *user_data);

/**
 * @brief IC driver API call to enable IC capture.
 * @see ic_enable_capture() for argument description.
 */
typedef int (*ic_enable_capture_t)(const struct device *dev, uint32_t channel);

/**
 * @brief IC driver API call to disable IC capture.
 * @see ic_disable_capture() for argument description
 */
typedef int (*ic_disable_capture_t)(const struct device *dev,
				     uint32_t channel);

/** @brief IC driver API definition. */
__subsystem struct ic_driver_api {
	ic_get_cycles_per_sec_t get_cycles_per_sec;

	ic_configure_capture_t configure_capture;
	ic_enable_capture_t enable_capture;
	ic_disable_capture_t disable_capture;
};
/** @endcond */

/**
 * @brief Get the clock rate (cycles per second) for a single IC output.
 *
 * @param[in] dev IC device instance.
 * @param channel IC channel.
 * @param[out] cycles Pointer to the memory to store clock rate (cycles per
 *                    sec). HW specific.
 *
 * @retval 0 If successful.
 * @retval -errno Negative errno code on failure.
 */
__syscall int ic_get_cycles_per_sec(const struct device *dev, uint32_t channel,
				     uint64_t *cycles);

static inline int z_impl_ic_get_cycles_per_sec(const struct device *dev,
						uint32_t channel,
						uint64_t *cycles)
{
	const struct ic_driver_api *api =
		(const struct ic_driver_api *)dev->api;

	return api->get_cycles_per_sec(dev, channel, cycles);
}

/**
 * @brief Convert from IC cycles to microseconds.
 *
 * @param[in] dev IC device instance.
 * @param channel IC channel.
 * @param cycles Cycles to be converted.
 * @param[out] usec Pointer to the memory to store calculated usec.
 *
 * @retval 0 If successful.
 * @retval -ERANGE If result is too large.
 * @retval -errno Other negative errno code on failure.
 */
static inline int ic_cycles_to_usec(const struct device *dev, uint32_t channel,
				     uint32_t cycles, uint64_t *usec)
{
	int err;
	uint64_t temp;
	uint64_t cycles_per_sec;

	err = ic_get_cycles_per_sec(dev, channel, &cycles_per_sec);
	if (err < 0) {
		return err;
	}

	if (u64_mul_overflow(cycles, (uint64_t)USEC_PER_SEC, &temp)) {
		return -ERANGE;
	}

	*usec = temp / cycles_per_sec;

	return 0;
}

/**
 * @brief Convert from IC cycles to nanoseconds.
 *
 * @param[in] dev IC device instance.
 * @param channel IC channel.
 * @param cycles Cycles to be converted.
 * @param[out] nsec Pointer to the memory to store the calculated nsec.
 *
 * @retval 0 If successful.
 * @retval -ERANGE If result is too large.
 * @retval -errno Other negative errno code on failure.
 */
static inline int ic_cycles_to_nsec(const struct device *dev, uint32_t channel,
				     uint32_t cycles, uint64_t *nsec)
{
	int err;
	uint64_t temp;
	uint64_t cycles_per_sec;

	err = ic_get_cycles_per_sec(dev, channel, &cycles_per_sec);
	if (err < 0) {
		return err;
	}

	if (u64_mul_overflow(cycles, (uint64_t)NSEC_PER_SEC, &temp)) {
		return -ERANGE;
	}

	*nsec = temp / cycles_per_sec;

	return 0;
}

/**
 * @brief Configure IC period/pulse width capture for a single IC input.
 *
 * After configuring IC capture using this function, the capture can be
 * enabled/disabled using ic_enable_capture() and
 * ic_disable_capture().
 *
 * @note This API function cannot be invoked from user space due to the use of a
 * function callback. In user space, one of the simpler API functions
 * (ic_capture_cycles(), ic_capture_usec(), or
 * ic_capture_nsec()) can be used instead.
 *
 *
 * @param[in] dev IC device instance.
 * @param channel IC channel.
 * @param flags IC capture flags
 * @param[in] cb Application callback handler function to be called upon capture
 * @param[in] user_data User data to pass to the application callback handler
 *                      function
 *
 * @retval -EINVAL if invalid function parameters were given
 * @retval -ENOSYS if IC capture is not supported or the given flags are not
 *                  supported
 * @retval -EIO if IO error occurred while configuring
 * @retval -EBUSY if IC capture is already in progress
 */
static inline int ic_configure_capture(const struct device *dev,
					uint32_t channel, ic_flags_t flags,
					ic_capture_callback_handler_t cb,
					void *user_data)
{
	const struct ic_driver_api *api =
		(const struct ic_driver_api *)dev->api;

	if (api->configure_capture == NULL) {
		return -ENOSYS;
	}

	return api->configure_capture(dev, channel, flags, cb,
					      user_data);
}

/**
 * @brief Enable IC period/pulse width capture for a single IC input.
 *
 * The IC pin must be configured using ic_configure_capture() prior to
 * calling this function.
 *
 *
 * @param[in] dev IC device instance.
 * @param channel IC channel.
 *
 * @retval 0 If successful.
 * @retval -EINVAL if invalid function parameters were given
 * @retval -ENOSYS if IC capture is not supported
 * @retval -EIO if IO error occurred while enabling IC capture
 * @retval -EBUSY if IC capture is already in progress
 */
__syscall int ic_enable_capture(const struct device *dev, uint32_t channel);

static inline int z_impl_ic_enable_capture(const struct device *dev,
					    uint32_t channel)
{
	const struct ic_driver_api *api =
		(const struct ic_driver_api *)dev->api;

	if (api->enable_capture == NULL) {
		return -ENOSYS;
	}

	return api->enable_capture(dev, channel);
}

/**
 * @brief Disable IC period/pulse width capture for a single IC input.
 *
 * @param[in] dev IC device instance.
 * @param channel IC channel.
 *
 * @retval 0 If successful.
 * @retval -EINVAL if invalid function parameters were given
 * @retval -ENOSYS if IC capture is not supported
 * @retval -EIO if IO error occurred while disabling IC capture
 */
__syscall int ic_disable_capture(const struct device *dev, uint32_t channel);

static inline int z_impl_ic_disable_capture(const struct device *dev,
					     uint32_t channel)
{
	const struct ic_driver_api *api =
		(const struct ic_driver_api *)dev->api;

	if (api->disable_capture == NULL) {
		return -ENOSYS;
	}

	return api->disable_capture(dev, channel);
}

/**
 * @brief Capture a single IC period/pulse width in clock cycles for a single
 *        IC input.
 *
 * This API function wraps calls to ic_configure_capture(),
 * ic_enable_capture(), and ic_disable_capture() and passes
 * the capture result to the caller. The function is blocking until either the
 * IC capture is completed or a timeout occurs.
 *
 * @param[in] dev IC device instance.
 * @param channel IC channel.
 * @param flags IC capture flags.
 * @param[out] period Pointer to the memory to store the captured IC period
 *                    width (in clock cycles). HW specific.
 * @param[out] pulse Pointer to the memory to store the captured IC pulse width
 *                   (in clock cycles). HW specific.
 * @param timeout Waiting period for the capture to complete.
 *
 * @retval 0 If successful.
 * @retval -EBUSY IC capture already in progress.
 * @retval -EAGAIN Waiting period timed out.
 * @retval -EIO IO error while capturing.
 * @retval -ERANGE If result is too large.
 */
__syscall int ic_capture_cycles(const struct device *dev, uint32_t channel,
				 ic_flags_t flags, uint32_t *period,
				 uint32_t *pulse, k_timeout_t timeout);

/**
 * @brief Capture a single IC period/pulse width in microseconds for a single
 *        IC input.
 *
 * This API function wraps calls to ic_capture_cycles() and
 * ic_cycles_to_usec() and passes the capture result to the caller. The
 * function is blocking until either the IC capture is completed or a timeout
 * occurs.
 *
 * @param[in] dev IC device instance.
 * @param channel IC channel.
 * @param flags IC capture flags.
 * @param[out] period Pointer to the memory to store the captured IC period
 *                    width (in usec).
 * @param[out] pulse Pointer to the memory to store the captured IC pulse width
 *                   (in usec).
 * @param timeout Waiting period for the capture to complete.
 *
 * @retval 0 If successful.
 * @retval -EBUSY IC capture already in progress.
 * @retval -EAGAIN Waiting period timed out.
 * @retval -EIO IO error while capturing.
 * @retval -ERANGE If result is too large.
 * @retval -errno Other negative errno code on failure.
 */
static inline int ic_capture_usec(const struct device *dev, uint32_t channel,
				   ic_flags_t flags, uint64_t *period,
				   uint64_t *pulse, k_timeout_t timeout)
{
	int err;
	uint32_t pulse_cycles;
	uint32_t period_cycles;

	err = ic_capture_cycles(dev, channel, flags, &period_cycles,
				 &pulse_cycles, timeout);
	if (err < 0) {
		return err;
	}

	err = ic_cycles_to_usec(dev, channel, period_cycles, period);
	if (err < 0) {
		return err;
	}

	err = ic_cycles_to_usec(dev, channel, pulse_cycles, pulse);
	if (err < 0) {
		return err;
	}

	return 0;
}

/**
 * @brief Capture a single IC period/pulse width in nanoseconds for a single
 *        IC input.
 *
 * This API function wraps calls to ic_capture_cycles() and
 * ic_cycles_to_nsec() and passes the capture result to the caller. The
 * function is blocking until either the IC capture is completed or a timeout
 * occurs.
 *
 * @param[in] dev IC device instance.
 * @param channel IC channel.
 * @param flags IC capture flags.
 * @param[out] period Pointer to the memory to store the captured IC period
 *                    width (in nsec).
 * @param[out] pulse Pointer to the memory to store the captured IC pulse width
 *                   (in nsec).
 * @param timeout Waiting period for the capture to complete.
 *
 * @retval 0 If successful.
 * @retval -EBUSY IC capture already in progress.
 * @retval -EAGAIN Waiting period timed out.
 * @retval -EIO IO error while capturing.
 * @retval -ERANGE If result is too large.
 * @retval -errno Other negative errno code on failure.
 */
static inline int ic_capture_nsec(const struct device *dev, uint32_t channel,
				   ic_flags_t flags, uint64_t *period,
				   uint64_t *pulse, k_timeout_t timeout)
{
	int err;
	uint32_t pulse_cycles;
	uint32_t period_cycles;

	err = ic_capture_cycles(dev, channel, flags, &period_cycles,
				 &pulse_cycles, timeout);
	if (err < 0) {
		return err;
	}

	err = ic_cycles_to_nsec(dev, channel, period_cycles, period);
	if (err < 0) {
		return err;
	}

	err = ic_cycles_to_nsec(dev, channel, pulse_cycles, pulse);
	if (err < 0) {
		return err;
	}

	return 0;
}

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#include <syscalls/ic.h>

#endif /* ZEPHYR_INCLUDE_DRIVERS_IC_H_ */

