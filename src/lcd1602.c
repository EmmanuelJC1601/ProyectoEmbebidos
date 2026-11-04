#include "lcd1602.h"
#include "util.h"
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/i2c-dev.h>

/* Mapa de bits del PCF8574 en la mochila estandar */
#define PCF_RS   0x01u
#define PCF_EN   0x04u
#define PCF_BL   0x08u        /* P1 (RW) queda en 0: siempre escritura */

/* Comandos HD44780 */
#define CMD_CLEAR        0x01u
#define CMD_ENTRY_MODE   0x06u  /* cursor a la derecha, sin desplazar */
#define CMD_DISPLAY_ON   0x0Cu  /* pantalla on, cursor off */
#define CMD_DISPLAY_OFF  0x08u
#define CMD_FUNC_4BIT_2L 0x28u  /* 4 bits, 2 lineas, 5x8 */
#define CMD_SET_DDRAM    0x80u
#define CMD_SET_CGRAM    0x40u
#define ROW1_OFFSET      0x40u
#define INIT_NIBBLE      0x30u
#define INIT_NIBBLE_4BIT 0x20u

/* Tiempos del datasheet (con margen) */
#define EN_PULSE_US      2u
#define CMD_US           60u
#define CLEAR_US         2500u
#define POWER_ON_MS      50u
#define INIT_STEP1_US    4500u
#define INIT_STEP2_US    150u

static int pcf_write(lcd_t *l, uint8_t v)
{
    if (write(l->fd, &v, 1) != 1) {
        LOG_ERRNO("write I2C LCD");
        return -1;
    }
    return 0;
}

static int pulse(lcd_t *l, uint8_t data)
{
    if (pcf_write(l, (uint8_t)(data | PCF_EN)) < 0) return -1;
    usleep(EN_PULSE_US);
    if (pcf_write(l, (uint8_t)(data & ~PCF_EN)) < 0) return -1;
    usleep(CMD_US);
    return 0;
}

static int send_nibble(lcd_t *l, uint8_t nibble_hi, uint8_t rs)
{
    return pulse(l, (uint8_t)((nibble_hi & 0xF0u) | rs | l->backlight));
}

static int send_byte(lcd_t *l, uint8_t v, uint8_t rs)
{
    if (send_nibble(l, v, rs) < 0) return -1;
    return send_nibble(l, (uint8_t)(v << 4), rs);
}

static int command(lcd_t *l, uint8_t c) { return send_byte(l, c, 0); }

int lcd_open(lcd_t *l, const char *bus_path, unsigned i2c_addr)
{
    memset(l, 0, sizeof(*l));
    l->backlight = PCF_BL;

    l->fd = open(bus_path, O_WRONLY);
    if (l->fd < 0) { LOG_ERRNO("open i2c"); return -1; }
    if (ioctl(l->fd, I2C_SLAVE, i2c_addr) < 0) {
        LOG_ERRNO("ioctl I2C_SLAVE (LCD)");
        goto fail;
    }

    /* Secuencia de inicializacion a 4 bits del datasheet HD44780 */
    usleep(POWER_ON_MS * 1000u);
    if (send_nibble(l, INIT_NIBBLE, 0) < 0) goto fail;
    usleep(INIT_STEP1_US);
    if (send_nibble(l, INIT_NIBBLE, 0) < 0) goto fail;
    usleep(INIT_STEP2_US);
    if (send_nibble(l, INIT_NIBBLE, 0) < 0) goto fail;
    if (send_nibble(l, INIT_NIBBLE_4BIT, 0) < 0) goto fail;
    if (command(l, CMD_FUNC_4BIT_2L) < 0 || command(l, CMD_DISPLAY_OFF) < 0) goto fail;
    if (lcd_clear(l) < 0) goto fail;
    if (command(l, CMD_ENTRY_MODE) < 0 || command(l, CMD_DISPLAY_ON) < 0) goto fail;
    return 0;
fail:
    close(l->fd);
    l->fd = -1;
    return -1;
}

void lcd_close(lcd_t *l)
{
    if (l->fd >= 0) {
        lcd_clear(l);
        lcd_set_backlight(l, false);
        close(l->fd);
    }
    l->fd = -1;
}

int lcd_clear(lcd_t *l)
{
    if (command(l, CMD_CLEAR) < 0) return -1;
    usleep(CLEAR_US);
    for (unsigned r = 0; r < LCD_ROWS; r++)
        l->cache_valid[r] = false;
    return 0;
}

int lcd_set_backlight(lcd_t *l, bool on)
{
    l->backlight = on ? PCF_BL : 0u;
    return pcf_write(l, l->backlight);
}

int lcd_print_row(lcd_t *l, unsigned row, const char *text)
{
    if (row >= LCD_ROWS) { errno = EINVAL; return -1; }

    char line[LCD_COLS];
    size_t n = strlen(text);
    if (n > LCD_COLS) n = LCD_COLS;
    memset(line, ' ', sizeof(line));
    memcpy(line, text, n);

    if (l->cache_valid[row] && memcmp(l->cache[row], line, LCD_COLS) == 0)
        return 0; /* nada cambio: cero trafico I2C */

    uint8_t addr = (uint8_t)(CMD_SET_DDRAM | (row == 0 ? 0u : ROW1_OFFSET));
    if (command(l, addr) < 0) return -1;
    for (unsigned i = 0; i < LCD_COLS; i++)
        if (send_byte(l, (uint8_t)line[i], PCF_RS) < 0) {
            l->cache_valid[row] = false;
            return -1;
        }
    memcpy(l->cache[row], line, LCD_COLS);
    l->cache_valid[row] = true;
    return 0;
}

int lcd_create_char(lcd_t *l, unsigned slot, const uint8_t bitmap[8])
{
    if (slot > 7u) { errno = EINVAL; return -1; }
    if (command(l, (uint8_t)(CMD_SET_CGRAM | (slot << 3))) < 0) return -1;
    for (unsigned i = 0; i < 8u; i++)
        if (send_byte(l, bitmap[i], PCF_RS) < 0) return -1;
    return command(l, CMD_SET_DDRAM);
}
