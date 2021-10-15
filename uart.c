#include <stdio.h>
#include <string.h>

#include "uart.h"

#define BRATE	0		/* sci baud rate reg. (-,-,SCP1,SCP0,-,SCR2,SCR1,SCR0) */
#define 	SCP1	0x20
#define 	SCP0	0x10
#define		SCR2	0x04
#define		SCR1	0x02
#define		SCR0	0x01
#define SCCR1	1		/* sci control reg. 1 (R8,T8,-,M,WAKE,-,-,-) */
#define		R8	0x80
#define		T8	0x40
#define		M	0x10
#define		WAKE	0x08
#define SCCR2	2		/* sci control reg. 2 (TIE,TCIE,RIE,ILIE,TE,RE,RWU,SBK) */
#define		TIE	0x80
#define		TCIE	0x40
#define		RIE	0x20
#define		ILIE	0x10
#define		TE	0x08
#define		RE	0x04
#define		RWU	0x02
#define		SBK	0x01
#define SCSR	3		/* sci status reg. (TDRE,TC,RDRF,IDLE,OR,NF,FE,-) */
#define		TDRE	0x80
#define		TC	0x40
#define		RDRF	0x20
#define		IDLE	0x10
#define		OR	0x08
#define		NF	0x04
#define		FE	0x02
#define SCDAT	4		/* sci data register (read: RDR, write: TDR) */

static unsigned int baseaddr;
static uint8_t regs[4];
static uint8_t txreg;
static uint8_t rxreg;

static void (*on_write)(uint8_t data);

void
uart_attach(uint16_t addr, void (*on_tx)(uint8_t))
{
	baseaddr = addr;
	on_write = on_tx;

	regs[SCSR] |= TDRE;
}

int 
uart_active(uint16_t addr)
{
	return (addr >= baseaddr && addr<baseaddr+5);
}

uint8_t
uart_read(uint16_t addr)
{
	int idx = addr - baseaddr;
	uint8_t ch = regs[idx];

	if (idx == SCDAT) {
		ch = rxreg;
		regs[SCSR] &= ~RDRF;
	} else {
		ch = regs[idx];
	}

//	printf("UART: reading reg %d: 0x%02x\n", idx, ch);

	return ch;
}

void
uart_write(uint16_t addr, uint8_t data)
{
	int idx = addr - baseaddr;

//	printf("UART: writing reg %d: 0x%02x\n", idx, data);

	if (idx == SCDAT) {
		txreg = data;
		if (on_write) on_write(data);
		regs[SCSR] |= TDRE;
	} else {
		regs[idx] = data;
	}
}

void
uart_rx(uint8_t data)
{
	rxreg = data;
	regs[SCSR] |= RDRF;
}
