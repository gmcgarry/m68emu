#include <stdint.h>

int acia_active(uint16_t addr);
void acia_attach(uint16_t addr, void (*on_tx)(uint8_t));
uint8_t acia_read(uint16_t addr);
void acia_write(uint16_t addr, uint8_t data);

void acia_rx(uint8_t ch);
