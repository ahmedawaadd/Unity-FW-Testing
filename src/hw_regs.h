#ifndef HW_REGS_H
#define HW_REGS_H

#include <stdint.h>

/* ------------------------------------------------------------------------
 * Register map for the receiver-tag board.
 *
 * Host build (-DUNIT_TEST): every register is a plain variable that tests
 * can read and write. Storage lives in test/hw_regs_mock.c.
 *
 * Target build: real memory-mapped addresses. These are placeholders --
 * fix them against the datasheet before the first target build.
 * ---------------------------------------------------------------------- */

#ifdef UNIT_TEST

extern volatile uint8_t CTRL_REG;
extern volatile uint8_t STATUS_REG;
extern volatile uint8_t RF_CHAN_REG;
extern volatile uint8_t RF_FIFO_REG;
extern volatile uint8_t IRQ_FLAGS_REG;
extern volatile uint8_t LED_REG;
extern volatile uint8_t ADC_LO_REG;
extern volatile uint8_t ADC_HI_REG;

#else

#define CTRL_REG      (*(volatile uint8_t *)0x4000)
#define STATUS_REG    (*(volatile uint8_t *)0x4001)
#define RF_CHAN_REG   (*(volatile uint8_t *)0x4002)
#define RF_FIFO_REG   (*(volatile uint8_t *)0x4003)
#define IRQ_FLAGS_REG (*(volatile uint8_t *)0x4004)
#define LED_REG       (*(volatile uint8_t *)0x4005)
#define ADC_LO_REG    (*(volatile uint8_t *)0x4006)
#define ADC_HI_REG    (*(volatile uint8_t *)0x4007)

#endif

/* CTRL_REG bits */
#define CTRL_RF_ENABLE   0u
#define CTRL_RX_MODE     1u
#define CTRL_CRC_EN      2u
#define CTRL_AUTO_ACK    3u
#define CTRL_LNA_BOOST   4u
#define CTRL_SOFT_RESET  7u

/* STATUS_REG bits (read-only on target) */
#define STATUS_FIFO_READY 0u
#define STATUS_FIFO_EMPTY 1u
#define STATUS_PLL_LOCK   2u
#define STATUS_OVERFLOW   3u
#define STATUS_FAULT      7u

/* IRQ_FLAGS_REG bits -- write 1 to clear */
#define IRQ_RX_DONE   0u
#define IRQ_CRC_ERR   1u
#define IRQ_TIMEOUT   2u
#define IRQ_OVERFLOW  3u

/* LED_REG bits */
#define LED_POWER   0u
#define LED_LINK    1u
#define LED_FAULT   2u

#define RT_BIT(n) ((uint8_t)(1u << (n)))

#endif /* HW_REGS_H */
