#include <stdio.h>
#include <string.h>

#include "acia.h"

#define CTRL				0
#define		BAUD_MASK		(3 << 0)
#define 		BAUD_RESET	(3 << 0)
#define			BAUD_CLK	(2 << 0)
#define			BAUD_CLK16	(1 << 0)
#define			BAUD_CLK64	(0 << 0)
#define		MODE_MASK		(7 << 2)
#define			MODE_7E2	(0 << 2)
#define			MODE_7O2	(1 << 2)
#define			MODE_7E1	(2 << 2)
#define			MODE_7O1	(3 << 2)
#define			MODE_8N2	(4 << 2)
#define			MODE_8N1	(5 << 2)
#define			MODE_8E1	(6 << 2)
#define			MODE_8O1	(7 << 2)
#define		RTS_MASK		(3 << 5)
#define			RTS_LOW		(0 << 5)
#define			RTS_LOW_TXIE	(1 << 5)
#define			RTS_HIGH	(2 << 5)
#define			RTS_LOW_BRK	(3 << 5)
#define		RXIE			(1 << 7)
#define TXDATA				1
#define STATUS				2
#define		RXAVAIL			(1 << 0)
#define		TXEMPTY			(1 << 1)
#define		DCD			(1 << 2)
#define		CTS			(1 << 3)
#define		RXERROR			(1 << 4)
#define		TXERROR			(1 << 5)
#define		PARITYERROR		(1 << 6)
#define		INTF			(1 << 7)
#define RXDATA				3


static unsigned int baseaddr;
static uint8_t regs[4];

static void (*on_write)(uint8_t data);


static void dump()
{
	printf("CTRL %02x, TXDATA %02x, STATUS %02x, RXDATA %02x\n", regs[CTRL], regs[TXDATA], regs[STATUS], regs[RXDATA]);
}

void
acia_attach(uint16_t addr, void (*on_tx)(uint8_t))
{
	baseaddr = addr;
	on_write = on_tx;

	regs[STATUS] = TXEMPTY & ~RXAVAIL;
}

int 
acia_active(uint16_t addr)
{
	return (addr >= baseaddr && addr<baseaddr+2);
}

uint8_t
acia_read(uint16_t addr)
{
	int idx = 0x2 | (addr - baseaddr);
	uint8_t ch = regs[idx];

	if (idx == RXDATA) 
		regs[STATUS] &= ~RXAVAIL;

//	printf("ACIA: reading reg %d: 0x%02x\n", idx, ch);
//	dump();

	return ch;
}

void
acia_write(uint16_t addr, uint8_t data)
{
	int idx = addr - baseaddr;

//	printf("ACIA: writing reg %d: 0x%02x -> 0x%02x\n", idx, regs[idx], data);

	regs[idx] = data;
	if (idx == TXDATA) {
		regs[STATUS] &= ~TXEMPTY;
		if (on_write)
			on_write(data);
		regs[STATUS] |= TXEMPTY;
	}
	if ((regs[CTRL] & 3) == BAUD_RESET) {
		regs[STATUS] = TXEMPTY & ~RXAVAIL;
		regs[CTRL] &= ~3;
	}

//	dump();
}

void
acia_rx(uint8_t data)
{
	regs[RXDATA] = data;
	regs[STATUS] |= RXAVAIL;
	if ((regs[CTRL] & RXIE) == RXIE)
		printf("do ACIA interrupt\n");

//	printf("character from keyboard\n");
//	dump();
}
