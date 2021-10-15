#include <stdint.h>

int uart_active(uint16_t addr);
void uart_attach(uint16_t addr, void (*on_tx)(uint8_t));
uint8_t uart_read(uint16_t addr);
void uart_write(uint16_t addr, uint8_t data);

void uart_rx(uint8_t ch);
