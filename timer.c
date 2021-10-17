#include <stdio.h>
#include <string.h>

#include "timer.h"

#define PRESCALER_MASK	0x011	/* /8 */

/*
 *  - 8-bit software programmable timer
 *  - 7-bit pre-scaler
 *  - timer decrements, when timer reaches zero, TCR7 is set
 *    if TCR6 is clear (and I-bit in CCR is clear), then and
 *    interrupt occurs.
 *  - the timer interrupt bit (TCR7) remains set until it is
 *    cleaed 
 *  - bit 2/1/0 specify the prescaler to extend the length
 *    of the timer (read-only)
 */

#define DATA			0	/* timer data register (TDR) */
#define CTRL			1	/* timer control register (TCR) */
#define		PRESCALER_RESET	(1<<3)	/* always read as zero */
#define		MODE		(3<<4)
#define			MODE1	(0<<4)	/* disabled */
#define			MODE2	(1<<4)	/* internal clock AND timer pin */
#define			MODE3	(2<<4)	/* all inputs disabled */
#define			MODE4	(3<<4)	/* timer pin */
#define		EXTCLOCKPIN	(1<<4)
#define		EXTCLOCK	(1<<5)
#define 	INTDISABLE	(1<<6)
#define		INTF		(1<<7)


static unsigned int baseaddr;
static uint8_t regs[2];

static int prescaler = (1 << PRESCALER_MASK);


static void dump()
{
	printf("CTRL %02x, DATA %02x\n", regs[CTRL], regs[DATA]);
}

void
timer_attach(uint16_t addr)
{
	baseaddr = addr;

	regs[DATA] = 0;
	regs[CTRL] = INTF | INTDISABLE | PRESCALER_MASK;
}

int 
timer_active(uint16_t addr)
{
	return (addr >= baseaddr && addr<baseaddr+2);
}

uint8_t
timer_read(uint16_t addr)
{
	int idx = (addr - baseaddr);
	uint8_t ch = regs[idx];

	if (idx == CTRL)
		ch &= ~PRESCALER_RESET;

	printf("TIMER: reading reg %d: 0x%02x\n", idx, ch);
	dump();

	return ch;
}

void
timer_write(uint16_t addr, uint8_t data)
{
	int idx = addr - baseaddr;

	printf("TIMER: writing reg %d: 0x%02x -> 0x%02x\n", idx, regs[idx], data);

	if (idx == CTRL) {
		if (data & PRESCALER_RESET)
			prescaler = (1 << PRESCALER_MASK);
		data &= 0xf0;
		data |= PRESCALER_MASK;
	}
	regs[idx] = data;

	dump();
}

void
timer_add(int count)
{
	while (count-- > 0) {
		if (prescaler-- > 0)
			continue;
		prescaler = (1 << PRESCALER_MASK);
		if (regs[DATA]-- > 0)
			continue;
		regs[DATA] = 0xff;
		regs[CTRL] |= INTF;
		if ((regs[CTRL] & INTDISABLE) == 0) {
			printf("TIMER INTERRUPT");
		}
	}

//	dump();
}
