/**
 * @file    timer.h
 * @brief   Lightweight hardware timer driver – tick & busy-wait delay API.
 *
 * @details
 * Configures one 16-bit general-purpose timer on STM32F103xB as a free-running
 * 1 µs counter operated in polling mode (no interrupts required).
 *
 * Overflow events are detected *lazily* inside timer_get_tick_us() by inspecting
 * the hardware UIF flag, so the 64-bit tick counter keeps advancing correctly
 * without any ISR overhead.
 *
 * Typical usage
 * -------------
 * @code
 *   // 1. Initialise once at startup
 *   if (timer_init(TIM2) != TIMER_OK) { Error_Handler(); }
 *
 *   // 2. Measure elapsed time
 *   uint64_t t0 = timer_get_tick_us();
 *   do_work();
 *   uint64_t elapsed_us = timer_get_tick_us() - t0;
 *
 *   // 3. Blocking delays
 *   timer_delay_us(500);      // 500 µs
 *   timer_delay_ms(100);      // 100 ms
 *
 *   // 4. Teardown (optional)
 *   timer_deinit();
 * @endcode
 *
 * Supported timers : TIM1, TIM2, TIM3, TIM4 (STM32F103xB)
 * Tick resolution  : 1 µs (timer clock must be ≥ 1 MHz)
 * 64-bit range     : ~584 942 years before overflow
 * Reentrancy       : NOT safe for concurrent access from ISR + main context
 *
 * @note  Only one timer instance may be active at a time.
 * @note  Do NOT mix with HAL_Delay(); both rely on timing resources.
 *
 * @author  ltc1342
 * @date    2026-06-24
 */

#ifndef TIMER_H_
#define TIMER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "stm32f1xx_hal.h"

/* ─────────────────────────────────────────────────────────────────────────── */
/*  Return codes                                                               */
/* ─────────────────────────────────────────────────────────────────────────── */

/**
 * @defgroup TIMER_RC Timer return codes
 * @{
 */
/** @brief Driver operation status codes. */
typedef enum {
    TIMER_OK                =  0, /**< Operation successful. */
    TIMER_ERR_INVALID_PARAM = -1, /**< Invalid pointer or timer instance. */
	TIMER_ERR_NOT_INIT 	 	= -2, /**< Driver not initialised. */
	TIMER_ERR_LOW_FREQ 		= -3  /**< Timer clock < 1 MHz. */
} TimerStatus_t;
/** @} */

/* ─────────────────────────────────────────────────────────────────────────── */
/*  Public API – six functions                                                 */
/* ─────────────────────────────────────────────────────────────────────────── */

/**
 * @brief  Initialise the timer peripheral for 1 µs tick generation.
 *
 * @details
 * Steps performed internally:
 *  -# Validate @p TIMx against the supported list.
 *  -# Verify the peripheral input clock is ≥ 1 MHz.
 *  -# Enable the RCC clock gate for the peripheral.
 *  -# Stop the counter, configure PSC for 1 tick = 1 µs.
 *  -# Set ARR = 0xFFFF (full 16-bit free-running range).
 *  -# Force a register-shadow update (UG event); clear the UIF flag.
 *  -# Disable all DMA/interrupt requests (DIER = 0).
 *  -# Reset the internal overflow counter.
 *  -# Start the counter (CEN = 1).
 *
 * A second call replaces the active instance (re-initialises with new @p TIMx).
 *
 * @param[in] TIMx  Timer instance: TIM1, TIM2, TIM3, or TIM4.
 *
 * @retval TIMER_OK                 Success – driver is ready.
 * @retval TIMER_ERR_INVALID_PARAM  @p TIMx is NULL or not supported.
 * @retval TIMER_ERR_LOW_FREQ       Peripheral clock < 1 MHz; prescaler
 *                                  would produce incorrect delays.
 */
__attribute__((warn_unused_result))
TimerStatus_t timer_init(TIM_TypeDef *TIMx);

/**
 * @brief  Stop the timer and release driver state.
 *
 * @details
 * Clears CEN, disables DIER, clears SR, then nulls all internal state.
 * After this call, timer_get_tick_us(), timer_get_tick_ms(),
 * timer_delay_us(), and timer_delay_ms() all return immediately (0 or no-op).
 *
 * @retval TIMER_OK            Success.
 * @retval TIMER_ERR_NOT_INIT  timer_init() has not been called.
 */
__attribute__((warn_unused_result))
TimerStatus_t timer_deinit(void);

/**
 * @brief  Return a monotonic 64-bit microsecond timestamp.
 *
 * @details
 * Reads the 16-bit CNT register and folds accumulated overflow events into a
 * 64-bit result.  Overflow detection is *lazy*: the UIF flag in SR is polled
 * each time this function is called; no ISR is needed.
 *
 * The result is computed as:
 * @code
 *   tick_us = (overflow_count << 16) | CNT
 * @endcode
 *
 * A short critical section (IRQ disabled for ~4 instructions) ensures that an
 * overflow occurring between reading the overflow counter and reading CNT is
 * not lost.
 *
 * @return  Microseconds elapsed since timer_init(), or @c 0 if not initialised.
 *
 * @warning Not safe to call concurrently from both an ISR and the main context.
 */
uint64_t timer_get_tick_us(void);

/**
 * @brief  Return a monotonic 64-bit millisecond timestamp.
 *
 * @details
 * Derived from timer_get_tick_us() divided by 1000.
 * Resolution is 1 ms; jitter is < 1 ms.
 *
 * @return  Milliseconds elapsed since timer_init(), or @c 0 if not initialised.
 */
uint64_t timer_get_tick_ms(void);

/**
 * @brief  Busy-wait for @p us microseconds.
 *
 * @details
 * Uses wrap-safe 16-bit subtraction on the CNT register so the wait is always
 * exact regardless of where the counter starts.  Delays longer than 65 535 µs
 * are chunked automatically into 65 535 µs segments.
 *
 * @param[in] us  Delay in microseconds.  Passing 0 returns immediately.
 *
 * @note  This is a blocking, CPU-spinning delay.  Do not use inside an ISR or
 *        an RTOS task that must not block the scheduler.
 */
void timer_delay_us(uint32_t us);

/**
 * @brief  Busy-wait for @p ms milliseconds.
 *
 * @details
 * Implemented as repeated 65 ms chunks plus a final remainder, all delegated
 * to timer_delay_us().  Total delay accuracy is the same as timer_delay_us().
 *
 * @param[in] ms  Delay in milliseconds.  Passing 0 returns immediately.
 *
 * @note  For delays > 1 s in an RTOS environment, consider vTaskDelay() or
 *        osDelay() instead to yield CPU time.
 */
void timer_delay_ms(uint32_t ms);

#ifdef __cplusplus
}
#endif

#endif /* TIMER_H_ */
