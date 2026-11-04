/* lcd1602.h - LCD 16x2 HD44780 con mochila I2C PCF8574, modo de 4 bits. */
#ifndef LCD1602_H
#define LCD1602_H
#include <stdbool.h>
#include <stdint.h>
#include "config.h"

typedef struct {
    int     fd;
    uint8_t backlight;                       /* bit de luz de fondo ya posicionado */
    char    cache[LCD_ROWS][LCD_COLS];       /* lo que esta en pantalla: evita reescribir */
    bool    cache_valid[LCD_ROWS];
} lcd_t;

int  lcd_open(lcd_t *l, const char *bus_path, unsigned i2c_addr);
void lcd_close(lcd_t *l);
int  lcd_clear(lcd_t *l);
/* Escribe una fila completa (rellena con espacios). Solo toca el bus si cambio algo. */
int  lcd_print_row(lcd_t *l, unsigned row, const char *text);
int  lcd_set_backlight(lcd_t *l, bool on);
/* Caracteres personalizados 5x8 (slots 0-7): reserva para sprites de los juegos. */
int  lcd_create_char(lcd_t *l, unsigned slot, const uint8_t bitmap[8]);
#endif
