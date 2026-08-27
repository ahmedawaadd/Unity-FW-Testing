/* Storage for the mocked registers declared in src/hw_regs.h.
 * Host build only. */

#ifdef UNIT_TEST

#include <stdint.h>

volatile uint8_t CTRL_REG;
volatile uint8_t STATUS_REG;

#endif
