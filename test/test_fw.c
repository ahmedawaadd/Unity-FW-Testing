/* Sanity check that the Unity host build works.
 * Replace these with real firmware tests once src/ has the receiver-tag file. */

#include "unity.h"

void setUp(void) { }
void tearDown(void) { }

static void test_unity_is_wired_up(void)
{
    TEST_ASSERT_EQUAL_HEX8(0x5A, 0x5A);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_unity_is_wired_up);
    return UNITY_END();
}
