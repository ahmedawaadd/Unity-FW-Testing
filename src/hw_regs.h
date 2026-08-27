#ifndef HW_REGS_H
#define HW_REGS_H

#include <stdint.h>

#ifdef UNIT_TEST
/* Host build: registers are plain variables the tests can read and write.
 * Definitions live in test/hw_regs_mock.c. */
extern volatile uint8_t CTRL_REG;
extern volatile uint8_t STATUS_REG;
#else
/* Target build: real memory-mapped registers. Fix these addresses to match
 * the receiver-tag datasheet before the first target build. */
#define CTRL_REG   (*(volatile uint8_t *)0x4000)
#define STATUS_REG (*(volatile uint8_t *)0x4001)
#endif

#endif /* HW_REGS_H */
