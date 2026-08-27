/* Timer B / DCO setup.
 *
 * The only change from the target source is the include swap below --
 * the function body is untouched. */

#ifdef UNIT_TEST
#include "msp430_mock.h"
#else
#include <msp430.h>
#endif

#include "dco.h"

void start_DCO(INT16U TB0_ticks, INT16U TB1_ticks, INT16U TB2_ticks)
{
    // configure TIMBERB to use the DCO
    BCSCTL1 &= ~(XTS+DIVA_3);
    BCSCTL3 &= ~(LFXT1S_3);
    _DINT();
    TBCTL = TBSSEL_2;             // SM CLK
    TBCCR0 = TB0_ticks;
    TBCCR1 = TB1_ticks;
    TBCCR2 = TB2_ticks;
    TBCCTL1 = OUTMOD_3;           // PWM set/reset
    TBCCTL2 = OUTMOD_7;           // PWM reset/set
    TBCTL |= MC0;
    _EINT();
}
