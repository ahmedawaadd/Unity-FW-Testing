#ifndef DCO_H
#define DCO_H

/* Project typedef. Replace this block with an include of your real
 * types header once the actual firmware is in the tree. */
#ifndef INT16U
#include <stdint.h>
typedef uint16_t INT16U;
#endif

void start_DCO(INT16U TB0_ticks, INT16U TB1_ticks, INT16U TB2_ticks);

#endif /* DCO_H */
