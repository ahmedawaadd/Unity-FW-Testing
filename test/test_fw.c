/* Tests for start_DCO().
 *
 * msp430_mock.h replaces <msp430.h> on the host: the Timer B and clock
 * registers become plain variables this file can read back after the
 * call, and _DINT/_EINT become counters. */

#include "unity.h"
#include "msp430_mock.h"
#include "dco.h"

void setUp(void)
{
    /* Every register back to zero so one test cannot leak into the next. */
    msp430_mock_reset();
}

void tearDown(void) { }

/* The three tick values reach the three capture/compare registers. */
static void test_start_DCO_writes_the_tick_values(void)
{
    start_DCO(1000u, 250u, 750u);

    TEST_ASSERT_EQUAL_HEX16(1000u, TBCCR0);
    TEST_ASSERT_EQUAL_HEX16(250u,  TBCCR1);
    TEST_ASSERT_EQUAL_HEX16(750u,  TBCCR2);
}

/* Timer B ends up on SMCLK with the mode-control bit set, i.e. running. */
static void test_start_DCO_selects_smclk_and_starts_timer(void)
{
    start_DCO(100u, 10u, 20u);

    TEST_ASSERT_EQUAL_HEX16(TBSSEL_2 | MC0, TBCTL);
}

static void test_start_DCO_sets_pwm_output_modes(void)
{
    start_DCO(100u, 10u, 20u);

    TEST_ASSERT_EQUAL_HEX16(OUTMOD_3, TBCCTL1);
    TEST_ASSERT_EQUAL_HEX16(OUTMOD_7, TBCCTL2);
}

/* Start from all-ones so the clear-masks have something to clear. This is
 * the test that can catch a real bug: if the mask is wrong it will clear
 * bits belonging to some other part of the clock config, and the exact
 * expected values below will not match. */
static void test_start_DCO_clears_only_its_own_clock_bits(void)
{
    BCSCTL1 = 0xFFu;
    BCSCTL3 = 0xFFu;

    start_DCO(100u, 10u, 20u);

    TEST_ASSERT_EQUAL_HEX8(0x00u, BCSCTL1 & (XTS | DIVA_3));
    TEST_ASSERT_EQUAL_HEX8(0x8Fu, BCSCTL1);   /* the other bits survived */

    TEST_ASSERT_EQUAL_HEX8(0x00u, BCSCTL3 & LFXT1S_3);
    TEST_ASSERT_EQUAL_HEX8(0xCFu, BCSCTL3);
}

/* Interrupts are disabled once and re-enabled once -- an early return or a
 * missing _EINT() would leave them off for good. */
static void test_start_DCO_leaves_interrupts_enabled(void)
{
    start_DCO(100u, 10u, 20u);

    TEST_ASSERT_EQUAL_INT(1, dint_calls);
    TEST_ASSERT_EQUAL_INT(1, eint_calls);
}

/* Zero ticks is a legal write; the timer just never reaches its period. */
static void test_start_DCO_accepts_zero_ticks(void)
{
    start_DCO(0u, 0u, 0u);

    TEST_ASSERT_EQUAL_HEX16(0u, TBCCR0);
    TEST_ASSERT_EQUAL_HEX16(0u, TBCCR1);
    TEST_ASSERT_EQUAL_HEX16(0u, TBCCR2);
    TEST_ASSERT_EQUAL_HEX16(TBSSEL_2 | MC0, TBCTL);
}

/* Full-scale values must survive as 16-bit, not get truncated somewhere. */
static void test_start_DCO_accepts_max_ticks(void)
{
    start_DCO(0xFFFFu, 0xFFFFu, 0xFFFFu);

    TEST_ASSERT_EQUAL_HEX16(0xFFFFu, TBCCR0);
    TEST_ASSERT_EQUAL_HEX16(0xFFFFu, TBCCR1);
    TEST_ASSERT_EQUAL_HEX16(0xFFFFu, TBCCR2);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_start_DCO_writes_the_tick_values);
    RUN_TEST(test_start_DCO_selects_smclk_and_starts_timer);
    RUN_TEST(test_start_DCO_sets_pwm_output_modes);
    RUN_TEST(test_start_DCO_clears_only_its_own_clock_bits);
    RUN_TEST(test_start_DCO_leaves_interrupts_enabled);
    RUN_TEST(test_start_DCO_accepts_zero_ticks);
    RUN_TEST(test_start_DCO_accepts_max_ticks);

    return UNITY_END();
}
