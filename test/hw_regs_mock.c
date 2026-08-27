/* Storage for the mocked registers declared in src/hw_regs.h.
 * Host build only. */

#ifdef UNIT_TEST

#include <stdint.h>
#include <string.h>

volatile uint8_t CTRL_REG;
volatile uint8_t STATUS_REG;
volatile uint8_t RF_CHAN_REG;
volatile uint8_t RF_FIFO_REG;
volatile uint8_t IRQ_FLAGS_REG;
volatile uint8_t LED_REG;
volatile uint8_t ADC_LO_REG;
volatile uint8_t ADC_HI_REG;

/* Convenience for setUp(): put every register back to a known zero state so
 * one test cannot leak hardware state into the next. */
void hw_regs_reset(void)
{
    CTRL_REG      = 0x00;
    STATUS_REG    = 0x00;
    RF_CHAN_REG   = 0x00;
    RF_FIFO_REG   = 0x00;
    IRQ_FLAGS_REG = 0x00;
    LED_REG       = 0x00;
    ADC_LO_REG    = 0x00;
    ADC_HI_REG    = 0x00;
}

#endif
