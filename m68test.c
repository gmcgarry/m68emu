#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <getopt.h>

#include <signal.h>
#include <sys/ioctl.h>
#include <termios.h>

#include "uart.h"
#include "m68emu.h"

uint8_t memspace[0x2000];

M68_CTX ctx;
uint64_t clockcount = 0;
long ns_per_clock = 1000000000LL / 3500000;
int running = 0;

int verbose = 0;
int trace = 0;
uint16_t breakpoint = 0;
int skipbpt = 0;

void delay(int cycles)
{
	struct timespec ts;
	ts.tv_sec = 0;
	ts.tv_nsec = cycles * ns_per_clock;

	while (true) {
		int rc = nanosleep(&ts, &ts);
		if (rc == 0)
			break;
	}
}

void
handler(int sig)
{
	running = 0;
}

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
	if (verbose && ctx->trace) {
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
	if (verbose && ctx->trace) {
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

void step()
{
	ctx.trace = 1;
	m68_exec_cycle(&ctx);
	ctx.trace = 0;
}

void run()
{
	running = 1;
	while (running) {
		if (ctx.pc_next == breakpoint && !skipbpt) {
			skipbpt = 1;
			printf("breakpoint %04x\n", breakpoint);
			break;
		}
		skipbpt = 0;
		int cycles = m68_exec_cycle(&ctx);
		delay(cycles);
	}
	if (!skipbpt)
		step();
}

void set_breakpoint()
{
	skipbpt = 0;
	breakpoint = 0;
}

void
show_registers()
{
	printf("A: %02x X: %02x SP: %04x PC: %04x CCR: %02x\n",
		ctx.reg_acc, ctx.reg_x, ctx.reg_sp, ctx.reg_pc, ctx.reg_ccr);
}

void
usage()
{
	fprintf(stderr, "Usage: m68em [-v level] [-t] <srec-file>\n");
}

int main(int argc, char *argv[])
{
	int opt;
	int rc;

	while ((opt = getopt(argc, argv, "v:c:t")) != -1) {
		switch (opt) {
		case 'c':
			ns_per_clock = 1000000000LL / atol(optarg);
			break;
		case 'v':
			verbose = atoi(optarg);
			break;
		case 't':
			trace = 1;
			break;
		case 'h':
			usage();
			return 0;
		default:
			printf("unrecognised argument = %s\n", optarg);
			return 1;
		}
	}

	if (optind >= argc) {
		usage();
		return -1;
	}

	rc = parse_srec(argv[optind], memspace, sizeof(memspace));
	if (rc < 0) {
		printf("error parsing srec file\n");
		return rc;
	}

	ctx.read_mem  = &readfunc;
	ctx.write_mem = &writefunc;
	ctx.opdecode  = NULL;
	m68_init(&ctx, M68_CPU_HC05C4);
	ctx.trace = trace;

	uart_attach(0x0d);

	signal(SIGINT, handler);

	while (1) {
		static char line[1024];
		char *linep = line;
		size_t len = sizeof(line);

		printf("> ");
		int n = getline(&linep, &len, stdin);
		n -= 1;
		if (n <= 0)
			continue;
		// printf("command: \"%.*s\" (%d)\n", n, linep, n);
		if (strncmp(line, "quit", n) == 0) {
			break;
		} else if (strncmp(line, "run", n) == 0) {
			m68_reset(&ctx);
			run();
		} else if (strncmp(line, "continue", n) == 0) {
			run();
		} else if (strncmp(line, "step", n) == 0) {
			step();
		} else if (strncmp(line, "registers", n) == 0) {
			show_registers();
		} else if (strncmp(line, "break", n) == 0) {
			set_breakpoint();
		}
	}

}

