#include <stdio.h>
#include <string.h>

#include "acia.h"

#define CTRL		0
#define 	BAUDRATE_RESET	(3 << 0)
#define		BAUDRATE_CLK	(2 << 0)
#define		BAUDRATE_CLK16	(1 << 0)
#define		BAUDRATE_CLK64	(0 << 0)
#define		MODE_7E2	(0 << 2)
#define		MODE_7O2	(1 << 2)
#define		MODE_7E1	(2 << 2)
#define		MODE_7O1	(3 << 2)
#define		MODE_8N2	(4 << 2)
#define		MODE_8N1	(5 << 2)
#define		MODE_8E1	(6 << 2)
#define		MODE_8O1	(7 << 2)
#define		RTS_LOW		(0 << 5)
#define		RTS_LOW_TXIE	(1 << 5)
#define		RTS_HIGH	(2 << 5)
#define		RTS_LOW_BRK	(3 << 5)
#define		RXIE		(1 << 7)
#define TXDATA		1
#define STATUS		2
#define		RXAVAIL		(1 << 0)
#define		TXEMPTY		(1 << 1)
#define		DCD		(1 << 2)
#define		CTS		(1 << 3)
#define		RXERROR		(1 << 4)
#define		TXERROR		(1 << 5)
#define		PARITYERROR	(1 << 6)
#define		INTF		(1 << 7)
#define RXDATA		3


static unsigned int baseaddr;
static uint8_t regs[4];

static void (*on_write)(uint8_t data);

void
acia_attach(uint16_t addr, void (*on_tx)(uint8_t))
{
	baseaddr = addr;
	on_write = on_tx;

	regs[STATUS] |= TXEMPTY & ~RXAVAIL;
}

int 
acia_active(uint16_t addr)
{
	return (addr >= baseaddr && addr<baseaddr+1);
}

uint8_t
acia_read(uint16_t addr)
{
	int idx = addr - baseaddr;
	uint8_t ch = regs[2+idx];

	ch = regs[idx];
	if (idx == RXDATA) 
		regs[STATUS] &= ~RXAVAIL;

	printf("UART: reading reg %d: 0x%02x\n", idx, ch);

	return ch;
}

void
acia_write(uint16_t addr, uint8_t data)
{
	int idx = addr - baseaddr;

	printf("UART: writing reg %d: 0x%02x\n", idx, data);

	regs[idx] = data;
	regs[STATUS] &= ~TXEMPTY;
	if (idx == TXDATA)
		if (on_write) on_write(data);
}

void
acia_rx(uint8_t data)
{
	regs[RXDATA] = data;
	regs[STATUS] |= RXAVAIL;
}
