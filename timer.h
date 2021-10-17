#include <stdint.h>

int timer_active(uint16_t addr);
void timer_attach(uint16_t addr);
uint8_t timer_read(uint16_t addr);
void timer_write(uint16_t addr, uint8_t data);

void timer_add(int ch);
