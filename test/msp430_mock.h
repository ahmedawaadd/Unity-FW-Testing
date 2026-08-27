#ifndef MSP430_MOCK_H
#define MSP430_MOCK_H

/* ------------------------------------------------------------------------
 * Host-side stand-in for <msp430.h>, covering only what start_DCO() uses.
 *
 * Grow this on demand: when the linker reports an undefined symbol, add
 * that one register. Do not try to mirror the whole device header.
 *
 * The bit constants below are copied from the TI device header. Verify
 * them against the header for YOUR specific part before trusting a test
 * that uses them -- a wrong constant here gives you a test that passes
 * for the wrong reason.
 * ---------------------------------------------------------------------- */

#include <stdint.h>

/* --- Registers: variables on host, real addresses on target ----------- */

extern volatile uint8_t  BCSCTL1;
extern volatile uint8_t  BCSCTL3;
extern volatile uint16_t TBCTL;
extern volatile uint16_t TBCCR0;
extern volatile uint16_t TBCCR1;
extern volatile uint16_t TBCCR2;
extern volatile uint16_t TBCCTL1;
extern volatile uint16_t TBCCTL2;

/* --- Intrinsics ------------------------------------------------------- */
/* Counted rather than ignored, so a test can prove the disable/enable
 * pair stays balanced. */

extern int dint_calls;
extern int eint_calls;

#define _DINT()  (dint_calls++)
#define _EINT()  (eint_calls++)

/* --- Bit constants (copied from the TI device header) ----------------- */

/* BCSCTL1 */
#define XTS        0x40u
#define DIVA_3     0x30u

/* BCSCTL3 */
#define LFXT1S_3   0x30u

/* TBCTL */
#define TBSSEL_2   0x0200u   /* SMCLK */
#define MC0        0x0010u

/* TBCCTLx */
#define OUTMOD_3   0x0060u   /* PWM set/reset */
#define OUTMOD_7   0x00E0u   /* PWM reset/set */

/* Put every mocked register and counter back to a known state. */
void msp430_mock_reset(void);

#endif /* MSP430_MOCK_H */
