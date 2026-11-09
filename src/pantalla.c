#include "pantalla.h"
#include "constantes.h"
#include "dibujo.h"
#include "spi.h"
#include "util.h"
#include <errno.h>
#include <fcntl.h>
#include <gpiod.h>
#include <string.h>
#include <unistd.h>

enum {
  cmd_reset = 0x01,
  cmd_sleep_out = 0x11,
  cmd_normal = 0x13,
  cmd_inverso_off = 0x20,
  cmd_inverso_on = 0x21,
  cmd_disp_on = 0x29,
  cmd_col = 0x2a,
  cmd_fila = 0x2b,
  cmd_mem = 0x2c,
  cmd_madctl = 0x36,
  cmd_formato = 0x3a,
  cmd_acceso = 0xf0,
  cmd_inv_col = 0xb4,
  cmd_funcion = 0xb6,
  cmd_pot1 = 0xc1,
  cmd_pot2 = 0xc2,
  cmd_vcom = 0xc5,
  cmd_gamma_p = 0xe0,
  cmd_gamma_n = 0xe1,
  cmd_ajuste = 0xe8
};

static struct gpiod_chip *chip;
static struct gpiod_line *dc;
static struct gpiod_line *rst;
static struct gpiod_line *luz;
static int fd = -1;
static int viva;
static int nivel_luz = -1;
static int dc_ahora = -1;
static uint8_t bloque[tam_bloque];

static const uint8_t gamma_positiva[] = {
  0xf0, 0x09, 0x0b, 0x06, 0x04, 0x15, 0x2f,
  0x54, 0x42, 0x3c, 0x17, 0x14, 0x18, 0x1b
};
static const uint8_t gamma_negativa[] = {
  0xf0, 0x09, 0x0b, 0x06, 0x04, 0x03, 0x2d,
  0x43, 0x42, 0x3b, 0x16, 0x14, 0x17, 0x1b
};
static const uint8_t ajuste[] = { 0x40, 0x8a, 0x00, 0x00, 0x29, 0x19, 0xa5, 0x33 };
static const uint8_t funcion[] = { 0x80, 0x02, 0x3b };

static int nivel(struct gpiod_line *linea, int valor)
{
  if (!linea) {
    errno = ENODEV;
    return -1;
  }
  if (gpiod_line_set_value(linea, valor) < 0) {
    log_errno("gpiod_line_set_value");
    return -1;
  }
  return 0;
}

static int pedir_salida(struct gpiod_line **out, unsigned pin, int valor)
{
  struct gpiod_line *linea = gpiod_chip_get_line(chip, pin);
  if (!linea) {
    log_errno("gpiod_chip_get_line tft");
    return -1;
  }
  if (gpiod_line_request_output(linea, nombre_gpio, valor) < 0) {
    log_errno("gpiod_line_request_output tft");
    return -1;
  }
  *out = linea;
  return 0;
}

static int poner_dc(int valor)
{
  if (dc_ahora == valor)
    return 0;
  if (nivel(dc, valor) != 0)
    return -1;
  dc_ahora = valor;
  return 0;
}

static int comando(uint8_t cmd)
{
  if (poner_dc(0) != 0)
    return -1;
  return spi_transferir(fd, &cmd, NULL, 1, (uint32_t)hz_tft);
}

static int datos(const uint8_t *p, size_t n)
{
  if (n == 0)
    return 0;
  if (poner_dc(1) != 0)
    return -1;
  return spi_transferir(fd, p, NULL, n, (uint32_t)hz_tft);
}

static int dato_u8(uint8_t v)
{
  return datos(&v, 1);
}

static int ventana(int x, int y, int w, int h)
{
  uint8_t b[4];
  int x0 = x + x_origen;
  int x1 = x + w - 1 + x_origen;
  int y0 = y + y_origen;
  int y1 = y + h - 1 + y_origen;
  if (comando(cmd_col) != 0)
    return -1;
  b[0] = (uint8_t)((unsigned)x0 >> 8);
  b[1] = (uint8_t)x0;
  b[2] = (uint8_t)((unsigned)x1 >> 8);
  b[3] = (uint8_t)x1;
  if (datos(b, 4) != 0)
    return -1;
  if (comando(cmd_fila) != 0)
    return -1;
  b[0] = (uint8_t)((unsigned)y0 >> 8);
  b[1] = (uint8_t)y0;
  b[2] = (uint8_t)((unsigned)y1 >> 8);
  b[3] = (uint8_t)y1;
  if (datos(b, 4) != 0)
    return -1;
  return comando(cmd_mem);
}

/* el llamador ya tiene el mutex del spi. no se suelta entre trozos:
 * si el adc mete un spi_setup a 1 mhz en medio, el cuadro de la tft
 * hereda ese reloj y el fps cae. */
static int mandar_rect(int x, int y, int w, int h)
{
  const uint16_t *fb = dibujo_buffer();
  int bytes_fila = w * 2;
  int max_filas;
  int y0 = y;
  if (w <= 0 || h <= 0)
    return 0;
  max_filas = (int)tam_bloque / bytes_fila;
  if (max_filas < 1)
    max_filas = 1;
  while (y0 < y + h) {
    int filas = y + h - y0;
    int fila;
    int pos = 0;
    if (filas > max_filas)
      filas = max_filas;
    if (ventana(x, y0, w, filas) != 0)
      return -1;
    for (fila = 0; fila < filas; fila++) {
      int col;
      const uint16_t *src = fb + ((size_t)(y0 + fila) * (size_t)ancho_px + (size_t)x);
      for (col = 0; col < w; col++) {
        uint16_t c = src[col];
        bloque[pos++] = (uint8_t)(c >> 8);
        bloque[pos++] = (uint8_t)c;
      }
    }
    if (datos(bloque, (size_t)pos) != 0)
      return -1;
    y0 += filas;
  }
  return 0;
}

static int negro(void)
{
  int y;
  int filas = (int)tam_bloque / (ancho_px * 2);
  memset(bloque, 0, sizeof bloque);
  if (filas < 1)
    filas = 1;
  for (y = 0; y < alto_px; y += filas) {
    int h = filas;
    if (y + h > alto_px)
      h = alto_px - y;
    if (spi_tomar() != 0)
      return -1;
    if (ventana(0, y, ancho_px, h) != 0 || datos(bloque, (size_t)ancho_px * (size_t)h * 2u) != 0) {
      spi_soltar();
      return -1;
    }
    spi_soltar();
  }
  return 0;
}

static int secuencia(void)
{
  if (spi_tomar() != 0)
    return -1;
  if (comando(cmd_reset) != 0)
    goto falla;
  spi_soltar();
  dormir_ms((unsigned)ms_panel);
  if (spi_tomar() != 0)
    return -1;
  if (comando(cmd_sleep_out) != 0)
    goto falla;
  spi_soltar();
  dormir_ms((unsigned)ms_panel);
  if (spi_tomar() != 0)
    return -1;
  if (comando(cmd_acceso) != 0 || dato_u8(0xc3) != 0)
    goto falla;
  if (comando(cmd_acceso) != 0 || dato_u8(0x96) != 0)
    goto falla;
  if (comando(cmd_madctl) != 0 || dato_u8((uint8_t)madctl_paisaje) != 0)
    goto falla;
  if (comando(cmd_formato) != 0 || dato_u8(0x55) != 0)
    goto falla;
  if (comando(cmd_inv_col) != 0 || dato_u8(0x01) != 0)
    goto falla;
  if (comando(cmd_funcion) != 0 || datos(funcion, sizeof funcion) != 0)
    goto falla;
  if (comando(cmd_ajuste) != 0 || datos(ajuste, sizeof ajuste) != 0)
    goto falla;
  if (comando(cmd_pot1) != 0 || dato_u8(0x06) != 0)
    goto falla;
  if (comando(cmd_pot2) != 0 || dato_u8(0xa7) != 0)
    goto falla;
  if (comando(cmd_vcom) != 0 || dato_u8(0x18) != 0)
    goto falla;
  if (comando(cmd_gamma_p) != 0 || datos(gamma_positiva, sizeof gamma_positiva) != 0)
    goto falla;
  if (comando(cmd_gamma_n) != 0 || datos(gamma_negativa, sizeof gamma_negativa) != 0)
    goto falla;
  if (comando(cmd_acceso) != 0 || dato_u8(0x3c) != 0)
    goto falla;
  if (comando(cmd_acceso) != 0 || dato_u8(0x69) != 0)
    goto falla;
  if (comando(invertir_color ? cmd_inverso_on : cmd_inverso_off) != 0)
    goto falla;
  if (comando(cmd_normal) != 0)
    goto falla;
  spi_soltar();
  dormir_ms((unsigned)ms_reset);
  if (spi_tomar() != 0)
    return -1;
  if (comando(cmd_disp_on) != 0)
    goto falla;
  spi_soltar();
  dormir_ms((unsigned)ms_panel);
  return negro();
falla:
  spi_soltar();
  return -1;
}

static void soltar_gpio(void)
{
  if (dc) {
    gpiod_line_release(dc);
    dc = NULL;
  }
  if (rst) {
    gpiod_line_release(rst);
    rst = NULL;
  }
  if (luz) {
    gpiod_line_release(luz);
    luz = NULL;
  }
  dc_ahora = -1;
  if (chip) {
    gpiod_chip_close(chip);
    chip = NULL;
  }
  nivel_luz = -1;
}

int pantalla_abrir(void)
{
  if (viva)
    return 0;
  if (fd < 0) {
    chip = gpiod_chip_open(ruta_gpio);
    if (!chip) {
      log_errno("gpiod tft");
      return -1;
    }
    if (pedir_salida(&dc, (unsigned)pin_dc, 0) != 0 ||
        pedir_salida(&rst, (unsigned)pin_rst, 1) != 0 ||
        pedir_salida(&luz, (unsigned)pin_luz, luz_encendida) != 0) {
      soltar_gpio();
      return -1;
    }
    nivel_luz = luz_encendida;
    fd = open(ruta_spi_tft, O_RDWR);
    if (fd < 0) {
      log_errno("open tft");
      soltar_gpio();
      return -1;
    }
    if (spi_preparar(fd, (uint32_t)hz_tft) != 0) {
      log_errno("ioctl tft");
      pantalla_cerrar();
      return -1;
    }
  }
  if (nivel(rst, 0) != 0)
    return -1;
  dormir_ms((unsigned)ms_reset);
  if (nivel(rst, 1) != 0)
    return -1;
  dormir_ms((unsigned)ms_panel);
  if (secuencia() != 0) {
    log_aviso("la tft no acepto la secuencia: se reintenta");
    viva = 0;
    return -1;
  }
  viva = 1;
  log_info("tft st7796s %dx%d en %s, dc gpio %u, rst gpio %u, luz gpio %u",
           ancho_px, alto_px, ruta_spi_tft, (unsigned)pin_dc, (unsigned)pin_rst, (unsigned)pin_luz);
  return 0;
}

int pantalla_volcar(void)
{
  zona_t zonas[zonas_max];
  int n;
  int i;
  if (!viva)
    return -1;
  n = dibujo_zonas(zonas, zonas_max);
  if (n <= 0)
    return 0;
  /* un solo tramo con el reloj de la tft. el adc espera a que termine
   * y corre en el hueco de 33 ms, no entre trozos de 4 kib. */
  if (spi_tomar() != 0)
    return -1;
  dc_ahora = -1;
  if (spi_preparar(fd, (uint32_t)hz_tft) != 0) {
    spi_soltar();
    viva = 0;
    return -1;
  }
  for (i = 0; i < n; i++) {
    if (mandar_rect(zonas[i].x, zonas[i].y, zonas[i].w, zonas[i].h) != 0) {
      spi_soltar();
      viva = 0;
      return -1;
    }
  }
  spi_soltar();
  return 0;
}

void pantalla_luz(int encendida)
{
  int valor = encendida ? luz_encendida : 1 - luz_encendida;
  if (!luz || valor == nivel_luz)
    return;
  if (nivel(luz, valor) == 0)
    nivel_luz = valor;
}

int pantalla_viva(void)
{
  return viva;
}

void pantalla_cerrar(void)
{
  viva = 0;
  if (fd >= 0 && close(fd) != 0)
    log_errno("close tft");
  fd = -1;
  soltar_gpio();
}
