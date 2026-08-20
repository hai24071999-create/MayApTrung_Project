/**
 * @file    timer.c
 * @brief   Implementation of the lightweight hardware timer driver (see timer.h).
 *
 * @details
 * Design decisions
 * ----------------
 * 1. **No interrupts.**  Overflow is tracked lazily via the hardware UIF flag
 *    polled inside timer_get_tick_us().  This keeps the driver self-contained:
 *    the caller never needs to implement a timer ISR.
 *
 * 2. **Atomic overflow capture.**  A minimal critical section (~4 instructions)
 *    is used to read `s_overflow_cnt` and `CNT` atomically.  This prevents a
 *    race where the counter wraps between the two reads.
 *
 * 3. **Runtime PSC computation.**  The prescaler is derived from the actual APB
 *    clock frequency so the driver is portable across different system clocks
 *    (e.g. 8, 36, 48, 72 MHz) without recompilation.
 *
 * 4. **64-bit tick range.**  At 1 MHz, 2^64 µs ≈ 584 942 years, so the 64-bit
 *    counter never rolls over in practice.
 *
 * @note  All public-API contracts are documented in timer.h.
 *
 * @author  <your name>
 * @date    2025
 */

#include "timer.h"

#include <stdbool.h>
#include <stddef.h>

/* ─────────────────────────────────────────────────────────────────────────── */
/*  Private constants                                                          */
/* ─────────────────────────────────────────────────────────────────────────── */

/** Minimum acceptable timer input clock (Hz).  Below this the prescaler
 *  cannot achieve a 1 µs tick and delays would be wrong. */
#define TIMER_MIN_CLK_HZ    (1000000UL)

/** Target tick frequency: 1 tick = 1 µs. */
#define TIMER_TICK_HZ       (1000000UL)

/** Maximum value loaded into ARR – puts the counter in 16-bit free-run mode. */
#define TIMER_ARR_MAX       (0xFFFFU)

/** Largest single delay chunk passed to the 16-bit busy-wait loop.
 *  Must be ≤ 65 535 to guarantee wrap-safe subtraction never spans more than
 *  one full counter period. */
#define TIMER_CHUNK_MAX_US  (65535UL)

/** Number of milliseconds consumed per 65 ms chunk in timer_delay_ms(). */
#define TIMER_CHUNK_MAX_MS  (65UL)

/** Microseconds per millisecond. */
#define US_PER_MS           (1000UL)

/* ─────────────────────────────────────────────────────────────────────────── */
/*  Private state                                                              */
/* ─────────────────────────────────────────────────────────────────────────── */

/** Pointer to the active timer peripheral.  NULL = driver not initialised. */
static TIM_TypeDef *s_tim = NULL;

/**
 * @brief  Number of times the 16-bit counter has wrapped to 0.
 *
 * Incremented inside the critical section in timer_get_tick_us() whenever the
 * UIF flag is found set.  Declared @c volatile to prevent the compiler from
 * caching the value across the critical-section boundary.
 */
static volatile uint64_t s_overflow_cnt = 0U;

/* ─────────────────────────────────────────────────────────────────────────── */
/*  Private helpers (static – not visible outside this translation unit)       */
/* ─────────────────────────────────────────────────────────────────────────── */

/**
 * @brief  Check whether @p TIMx is a supported peripheral on STM32F103xB.
 *
 * @param[in] TIMx  Timer instance to test.
 * @return  @c true if TIM1, TIM2, TIM3, or TIM4; @c false otherwise.
 */
static bool timer_is_valid(const TIM_TypeDef *TIMx)
{
    return (TIMx == TIM1) ||
           (TIMx == TIM2) ||
           (TIMx == TIM3) ||
           (TIMx == TIM4);
}

/**
 * @brief  Enable the RCC peripheral clock gate for @p TIMx.
 *
 * @param[in] TIMx  Timer instance (caller has already validated it).
 */
static void timer_clk_enable(TIM_TypeDef *TIMx)
{
    if      (TIMx == TIM1) { __HAL_RCC_TIM1_CLK_ENABLE(); }
    else if (TIMx == TIM2) { __HAL_RCC_TIM2_CLK_ENABLE(); }
    else if (TIMx == TIM3) { __HAL_RCC_TIM3_CLK_ENABLE(); }
    else                   { __HAL_RCC_TIM4_CLK_ENABLE(); }
}

/**
 * @brief  Compute the timer input clock frequency (Hz) from RCC registers.
 *
 * @details
 * STM32F1 multiplier rule: if the APB prescaler divides by more than 1, the
 * timer input clock is PCLK × 2; otherwise it equals PCLK.
 *
 * TIM1 hangs on APB2; TIM2/3/4 hang on APB1.
 *
 * @param[in] TIMx  Timer instance (caller has already validated it).
 * @return  Timer input clock in Hz.
 */
static uint32_t timer_get_clk_hz(const TIM_TypeDef *TIMx)
{
    uint32_t pclk_hz;
    uint32_t apb_presc_bits;

    if (TIMx == TIM1) {
        pclk_hz        = HAL_RCC_GetPCLK2Freq();
        apb_presc_bits = (RCC->CFGR & RCC_CFGR_PPRE2) >> RCC_CFGR_PPRE2_Pos;
    } else {
        pclk_hz        = HAL_RCC_GetPCLK1Freq();
        apb_presc_bits = (RCC->CFGR & RCC_CFGR_PPRE1) >> RCC_CFGR_PPRE1_Pos;
    }

    /*
     * PPRE field encoding (RM0008 §7.3.2):
     *   0b0xx (0–3) → prescaler = 1   → TIMCLK = PCLK
     *   0b1xx (4–7) → prescaler = 2–16 → TIMCLK = PCLK × 2
     */
    return (apb_presc_bits >= 4U) ? (pclk_hz * 2U) : pclk_hz;
}

/* ─────────────────────────────────────────────────────────────────────────── */
/*  Public API implementation                                                  */
/* ─────────────────────────────────────────────────────────────────────────── */

/* -------------------------------------------------------------------------- */
TimerStatus_t timer_init(TIM_TypeDef *TIMx)
{
    /* ── 1. Validate argument ──────────────────────────────────────────────── */
    if (!timer_is_valid(TIMx)) {
        return TIMER_ERR_INVALID_PARAM ;
    }

    /* ── 2. Verify clock is fast enough for 1 µs resolution ──────────────── */
    uint32_t clk_hz = timer_get_clk_hz(TIMx);
    if (clk_hz < TIMER_MIN_CLK_HZ) {
        return TIMER_ERR_LOW_FREQ;
    }

    /* ── 3. Enable peripheral RCC clock ───────────────────────────────────── */
    timer_clk_enable(TIMx);

    /* ── 4. Stop counter while configuring ────────────────────────────────── */
    TIMx->CR1 &= ~TIM_CR1_CEN;

    /* ── 5. Prescaler → 1 tick = 1 µs ────────────────────────────────────── */
    /*       PSC = (clk_hz / 1 000 000) - 1  (e.g. 72 MHz → PSC = 71)       */
    TIMx->PSC = (uint16_t)((clk_hz / TIMER_TICK_HZ) - 1U);

    /* ── 6. Auto-reload = 0xFFFF → free-running 16-bit counter ───────────── */
    TIMx->ARR = TIMER_ARR_MAX;
    TIMx->CNT = 0U;

    /* ── 7. Force shadow-register update; clear the spurious UIF it causes ── */
    TIMx->EGR |=  TIM_EGR_UG;
    TIMx->SR  &= ~TIM_SR_UIF;

    /* ── 8. Disable all DMA and interrupt requests (pure polling mode) ─────── */
    TIMx->DIER = 0x0000U;

    /* ── 9. Reset driver state BEFORE starting the counter ─────────────────── */
    s_overflow_cnt = 0U;
    s_tim          = TIMx;

    /* ── 10. Start the counter ─────────────────────────────────────────────── */
    TIMx->CR1 |= TIM_CR1_CEN;

    return TIMER_OK;
}

/* -------------------------------------------------------------------------- */
TimerStatus_t timer_deinit(void)
{
    if (s_tim == NULL) {
        return TIMER_ERR_NOT_INIT;
    }
    
    s_tim->CR1 &= ~TIM_CR1_CEN;   /* Stop counter                   */
    s_tim->DIER = 0x0000U;         /* Disable all DMA/IRQ requests   */
    s_tim->SR   = 0x0000U;         /* Clear all pending status flags  */

    /* Null the pointer LAST so any concurrent reader sees a consistent state */
    s_overflow_cnt = 0U;
    s_tim          = NULL;

    return TIMER_OK;
}

/* -------------------------------------------------------------------------- */
uint64_t timer_get_tick_us(void)
{
    if (s_tim == NULL) {
        return 0U;
    }

    uint16_t cnt;
    uint64_t ovf_snap;

    /*
     * Critical section (~4 Cortex-M instructions).
     *
     * Problem: if an overflow occurs between reading s_overflow_cnt and reading
     * CNT, we would return a value that is 65 536 µs too small.
     *
     * Solution:
     *   a) Snapshot s_overflow_cnt.
     *   b) Read CNT.
     *   c) Check UIF – if set, overflow happened; increment both the persistent
     *      counter and the local snapshot, then re-read CNT (it may have
     *      advanced a few ticks since the wrap).
     *
     * The IRQ disable window is intentionally as short as possible.
     */
    __disable_irq();

    ovf_snap = s_overflow_cnt;
    cnt      = (uint16_t)(s_tim->CNT);

    if ((s_tim->SR & TIM_SR_UIF) != 0U) {
        s_tim->SR  &= ~TIM_SR_UIF; /* Acknowledge – must be cleared in SW  */
        s_overflow_cnt++;           /* Update persistent counter             */
        ovf_snap++;                 /* Reflect wrap in this return value     */
        cnt = (uint16_t)(s_tim->CNT); /* Re-read CNT after the overflow      */
    }

    __enable_irq();

    /*
     * Result: upper 48 bits = overflows × 65 536 µs; lower 16 bits = CNT (µs).
     * Equivalent to: ovf_snap × 65536 + cnt.
     */
    return (ovf_snap << 16U) | (uint64_t)cnt;
}

/* -------------------------------------------------------------------------- */
uint64_t timer_get_tick_ms(void)
{
    /* Resolution: 1 ms.  Jitter: < 1 ms (floor division). */
    return timer_get_tick_us() / US_PER_MS;
}

/* -------------------------------------------------------------------------- */
void timer_delay_us(uint32_t us)
{
    if ((s_tim == NULL) || (us == 0U)) {
        return;
    }

    while (us > 0U) {
        /*
         * Cap the chunk to TIMER_CHUNK_MAX_US so the busy-wait never needs to
         * span more than one full 16-bit counter period.
         *
         * Wrap-safe arithmetic: (uint16_t)(cnt_now - start) correctly handles
         * the case where CNT wraps from 0xFFFF → 0x0000 mid-wait.
         * Example: start = 0xFFF0, chunk = 100
         *   After wrap: cnt_now = 0x0033
         *   (uint16_t)(0x0033 - 0xFFF0) = 0x0043 = 67 … wait continues until 100.
         */
        uint16_t chunk = (us > TIMER_CHUNK_MAX_US) ?
                         (uint16_t)TIMER_CHUNK_MAX_US :
                         (uint16_t)us;

        uint16_t start = (uint16_t)(s_tim->CNT);

        while ((uint16_t)((uint16_t)(s_tim->CNT) - start) < chunk) {
            __NOP(); /* Prevent the optimiser from collapsing the loop */
        }

        us -= (uint32_t)chunk;
    }
}

/* -------------------------------------------------------------------------- */
void timer_delay_ms(uint32_t ms)
{
    if ((s_tim == NULL) || (ms == 0U)) {
        return;
    }

    /*
     * Break large delays into 65 ms chunks so that each call to timer_delay_us()
     * never exceeds TIMER_CHUNK_MAX_US (65 535 µs).
     *
     * 65 ms × 1000 µs/ms = 65 000 µs  <  65 535 → safely within 16-bit range.
     */
    while (ms > TIMER_CHUNK_MAX_MS) {
        timer_delay_us(TIMER_CHUNK_MAX_MS * US_PER_MS); /* 65 000 µs chunk */
        ms -= TIMER_CHUNK_MAX_MS;
    }

    /* Remaining portion: 0 < ms ≤ 65 → ms × 1000 ≤ 65 000 µs */
    if (ms > 0U) {
        timer_delay_us(ms * US_PER_MS);
    }
}
