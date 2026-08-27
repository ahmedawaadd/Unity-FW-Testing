/* Storage for the mocked MSP430 registers. Host build only. */

#ifdef UNIT_TEST

#include "msp430_mock.h"

volatile uint8_t  BCSCTL1;
volatile uint8_t  BCSCTL3;
volatile uint16_t TBCTL;
volatile uint16_t TBCCR0;
volatile uint16_t TBCCR1;
volatile uint16_t TBCCR2;
volatile uint16_t TBCCTL1;
volatile uint16_t TBCCTL2;

int dint_calls;
int eint_calls;

void msp430_mock_reset(void)
{
    BCSCTL1 = 0x00u;
    BCSCTL3 = 0x00u;
    TBCTL   = 0x0000u;
    TBCCR0  = 0x0000u;
    TBCCR1  = 0x0000u;
    TBCCR2  = 0x0000u;
    TBCCTL1 = 0x0000u;
    TBCCTL2 = 0x0000u;

    dint_calls = 0;
    eint_calls = 0;
}

#endif
