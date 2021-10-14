#include <stdio.h>

#include "uart.h"
#include "m68emu.h"

uint8_t memspace[0x2000];

uint64_t clockcount = 0;
double ns_per_clock = 1.e9 / 3.5e6;

int verbose = 9;

int 
parse_srec(char *filename, uint8_t *mem, int memsize)
{
	char *line = NULL;
	uint8_t	line_content[128];
	size_t len = 0;
	int i, temp;
	int read;
	int line_len, line_type, line_address;
	FILE *sf;

	if (verbose > 2)
		printf("Opening filename %s \n", filename);

	sf = fopen(filename, "r");
	if (sf == 0)
		return -1;

	if (verbose > 2)
		printf("File open\n");

	while ((read = getline(&line, &len, sf)) != -1) {
		if (verbose > 2)
			printf("\nRead %d chars: %s", read, line);
		if (line[0] != 'S') {
			if (verbose > 1)
				printf("--- : invalid\n");
			return -1;
		}
		sscanf(line + 1, "%1X", &line_type);
		if (line_type == 0)
			continue;
		sscanf(line + 2, "%2X", &line_len); line_len -= 4;
		sscanf(line + 4, "%4X", &line_address);
		if (verbose > 2)
			printf("Line len %d B, type %d, address 0x%4.4x\n", line_len, line_type, line_address);
		if (line_type == 1 || line_type == 9) {
			for (i = 0; i < line_len; i++) {
				sscanf(line + 8 + i * 2, "%2X", &temp);
				line_content[i] = temp;
			}
			if (verbose > 2)
				printf("PM ");
			if (line_address + line_len > memsize) {
				printf("line_address = 0x%x, line_len = %x\n", line_address, line_len);
				return -1;
			}
			for (i = 0; i < line_len; i++)
				mem[line_address + i] = line_content[i];
		}
		if (verbose > 2)
			for (i = 0; i < line_len; i++)
				printf("%2.2X", line_content[i]);
		if (verbose > 2)
			printf("\n");
	}
	fclose(sf);
	return 0;
}

uint8_t readfunc(struct M68_CTX *ctx, const uint16_t addr)
{
	if (addr == 0x15c7) {
		ctx->trace = true;
	}
	if (ctx->trace) {
		printf("  MEM RD %04X = %02X\n", addr, memspace[addr]);
	}

	if (addr == 0) {
		return 1;	// I/O pad always high
	}

	if (uart_active(addr))
		return uart_read(addr);

	return memspace[addr];
}

void writefunc(struct M68_CTX *ctx, const uint16_t addr, const uint8_t data)
{
	if (ctx->trace) {
		printf("  MEM WR %04X = %02X\n", addr, data);
	}
	memspace[addr] = data;

	if (addr == 0) {
		printf("#%lu\n", clockcount);
		printf("%d#", data & 1);
	}

	if (uart_active(addr))
		uart_write(addr, data);
}

int main(int argc, char *argv[])
{
	M68_CTX ctx;
	int rc;

	if (argc != 2) {
		printf("m68emu <ihex-file>\n");
		return 1;
	}

	rc = parse_srec(argv[1], memspace, sizeof(memspace));
	if (rc < 0) {
		printf("error parsing srec file\n");
		return rc;
	}

	ctx.read_mem  = &readfunc;
	ctx.write_mem = &writefunc;
	ctx.opdecode  = NULL;
	m68_init(&ctx, M68_CPU_HC05C4);
	ctx.trace = true;

	uart_attach(0x0d);

	printf("$timescale 1ns $end\n");
	printf("$var wire 1 # dq $end\n");

	do {
		clockcount += m68_exec_cycle(&ctx);
	} while (clockcount < 10000);
	printf("PC: %04x\n", ctx.reg_pc);
}

