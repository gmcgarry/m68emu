#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ctype.h>	/* isspace() */
#include <time.h>	/* nanosleep() */
#include <getopt.h>	/* getopt() */
#include <signal.h>	/* signal() */
#include <sys/ioctl.h>
#include <termios.h>

#include "uart.h"
#include "m68emu.h"

uint8_t *memspace;

M68_CTX ctx;
unsigned int memsize = 0x2000;
uint64_t clockcount = 0;
long ns_per_clock = 1000000000LL / 3500000;



int verbose = 0;
int trace = 0;
int running = 0;
int done = 0;
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

void enable_raw_mode()
{
	struct termios term;

	tcgetattr(0, &term);
	term.c_lflag &= ~(ICANON | ECHO); // Disable echo as well
	tcsetattr(0, TCSANOW, &term);
}

void disable_raw_mode()
{
	struct termios term;

	tcgetattr(0, &term);
	term.c_lflag |= ICANON | ECHO;
	tcsetattr(0, TCSANOW, &term);
}

bool kbhit()
{
	int byteswaiting;
	ioctl(0, FIONREAD, &byteswaiting);
	return byteswaiting > 0;
}


void
handler(int sig)
{
	running = 0;
	skipbpt = 0;
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
		sscanf(line + 2, "%2X", &line_len); line_len -= 3;
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
				fprintf(stderr, "ERROR: address too large for memory space (memsize 0x%x, addr 0x%x, offset 0x%x)\n", memsize, line_address, line_len);
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

uint8_t
readfunc(struct M68_CTX *ctx, const uint16_t addr)
{
	if (addr == 0x15c7) {
		ctx->trace = true;
	}
	if (verbose && ctx->trace) {
		printf("	MEM RD %04X = %02X\n", addr, memspace[addr]);
	}

	if (addr == 0) {
		return 1;	// I/O pad always high
	}

	if (uart_active(addr))
		return uart_read(addr);

	return memspace[addr];
}

void
writefunc(struct M68_CTX *ctx, const uint16_t addr, const uint8_t data)
{
	if (verbose && ctx->trace) {
		printf("	MEM WR %04X = %02X\n", addr, data);
	}
	memspace[addr] = data;

	if (addr == 0) {
		printf("#%lu\n", clockcount);
		printf("%d#", data & 1);
	}

	if (uart_active(addr))
		uart_write(addr, data);
}

void
uart_tx(uint8_t data)
{
	putchar(data);
	fflush(stdout);
}



/* --- commands --- */

void
quit(const char *arg)
{
	done = 1;
}

void
step(const char *arg)
{
	int i;

	int count = 1;
	if (*arg)
		count = strtoull(arg, NULL, 10);
	if (count > 0) {
		for (i = 0; i < count-1; i++)
			m68_exec_cycle(&ctx);
		ctx.trace = 1;
		m68_exec_cycle(&ctx);
		ctx.trace = 0;
	}
}

void
cont(const char *arg)
{
	running = 1;
	enable_raw_mode();
	while (running) {
		if (ctx.pc_next == breakpoint && !skipbpt) {
			skipbpt = 1;
			printf("breakpoint %04x\n", breakpoint);
			break;
		}
		skipbpt = 0;
		int cycles = m68_exec_cycle(&ctx);
		delay(cycles);
		if (kbhit())
			uart_rx(getchar());

	}
	if (!skipbpt)
		step(arg);
	disable_raw_mode();
}

void
run(const char *arg)
{
	m68_reset(&ctx);
	cont(arg);
}

void
breakpt(const char *arg)
{
	breakpoint = strtoul(arg, NULL, 16);
	skipbpt = 0;
}

void
show(const char *arg)
{
	printf("A: %02x X: %02x SP: %04x PC: %04x CCR: %02x\n",
		ctx.reg_acc, ctx.reg_x, ctx.reg_sp, ctx.reg_pc, ctx.reg_ccr);
}

void
dump(const char* arg)
{
	uint16_t addr = strtoul(arg, NULL, 16);
	printf("%04X: %02x\n", addr, memspace[addr]);
}

void
jump(const char* arg)
{
	ctx.reg_pc = strtoul(arg, NULL, 16);
}

void help(const char* arg);

struct command {
	char *name;
	void (*func)(const char* word);
	char *doc;
} commands[] = {
	{ "break", breakpt, "set breakpoint" },
	{ "continue", cont, "continue execution" },
	{ "examine", dump, "examine memory location" },
	{ "goto", jump, "set PC to address" },
	{ "help", help, "command help" },
	{ "quit", quit, "exit emulator" },
	{ "run", run, "reset and start execution" },
	{ "show", show, "show registers" },
	{ "step", step, "single step over instruction" },
};
#define NCOMMANDS (int)(sizeof(commands)/sizeof(commands[0]))

void
help(const char* arg)
{
	int printed = 0;
	int i;

	for (i = 0; i < NCOMMANDS; i++) {
		if (!*arg || (strcmp(arg, commands[i].name) == 0)) {
			printf("%s\t\t%s.\n", commands[i].name, commands[i].doc);
			printed++;
		}
	}

	if (!printed) {
		if (*arg)
			printf ("No commands match `%s'.  Possibilties are:\n", arg);
		for (i = 0; i < NCOMMANDS; i++)
			printf("%s\t%s\n", commands[i].name, commands[i].doc);
	}
}

void
execute(char *line)
{
	int i, j;
	struct command *command;
	char *word;

	i = 0;
	while (isspace(line[i]))
		i++;
	word = line + i;

	while (line[i] && !isspace(line[i]))
		i++;

	if (line[i])
		line[i++] = '\0';

	command = NULL;
	for (j = 0; j < NCOMMANDS; j++) {
		if (strcmp(word, commands[j].name) == 0) {
			command = &commands[j];
			break;
		}
	}

	if (!command) {
		printf("%s: unrecognised command\n", word);
		return;
	}

	while (isspace(line[i]))
		i++;
	word = line + i;

	(*(command->func))(word);
}

void
usage()
{
	printf("Usage: m68em [-v level] [-t] <srec-file>\n");
}

int
main(int argc, char *argv[])
{
	int opt;
	int rc;

	while ((opt = getopt(argc, argv, "hc:m:v:t")) != -1) {
		switch (opt) {
		case 'c':
			ns_per_clock = 1000000000LL / atol(optarg);
			break;
		case 'm':
			memsize = strtoul(optarg, NULL, 16);
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
			fprintf(stderr, "ERROR: unrecognised argument = %s\n", optarg);
			return 1;
		}
	}

	if (optind >= argc) {
		usage();
		return 1;
	}

	memspace = calloc(memsize, 1);
	if (memspace == NULL) {
		fprintf(stderr, "ERROR: cannot allocate %u bytes for memory\n", memsize);
		return 1;
	}

	rc = parse_srec(argv[optind], memspace, memsize);
	if (rc < 0) {
		fprintf(stderr, "ERROR: cannot parse srec file\n");
		return rc;
	}

	ctx.read_mem	= &readfunc;
	ctx.write_mem = &writefunc;
	ctx.opdecode	= NULL;
	m68_init(&ctx, M68_CPU_HC05C4);
	ctx.trace = trace;

	uart_attach(0x0d, uart_tx);

	signal(SIGINT, handler);

	char line[1024];
	char *linep = line;

	while (!done) {
		size_t len = sizeof(line);

		printf("> ");
		int n = getline(&linep, &len, stdin);
		n -= 1;
		if (n <= 0)
			continue;
		// printf("command: \"%.*s\" (%d)\n", n, linep, n);
		linep[n] = '\0';
		execute(linep);
	}

	return 0;
}

