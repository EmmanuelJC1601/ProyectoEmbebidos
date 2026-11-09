#include "adc.h"
#include "constantes.h"
#include "spi.h"
#include "trama.h"
#include "util.h"
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

int adc_abrir(adc_t *a, const char *ruta)
{
  a->fd = open(ruta, O_RDWR);
  if (a->fd < 0) {
    log_errno("open adc");
    return -1;
  }
  if (spi_preparar(a->fd, (uint32_t)hz_adc) != 0) {
    log_errno("ioctl adc");
    close(a->fd);
    a->fd = -1;
    return -1;
  }
  return 0;
}

int adc_leer(adc_t *a, unsigned canal, uint16_t *valor)
{
  uint8_t tx[n_trama];
  uint8_t rx[n_trama];
  int paso;
  if (canal >= (unsigned)n_canales) {
    errno = EINVAL;
    return -1;
  }
  if (a->fd < 0) {
    errno = ENODEV;
    return -1;
  }
  armar_trama(canal, tx);
  /* bitacora clase 9: la muestra y el cuadro de la tft comparten el spi.
   * se baja el reloj a 1 mhz en este fd: la tft lo dejo en 32 mhz.
   * la primera conversion despues de ese cambio suele salir en cero. */
  if (spi_tomar() != 0)
    return -1;
  if (spi_preparar(a->fd, (uint32_t)hz_adc) != 0) {
    spi_soltar();
    return -1;
  }
  *valor = 0;
  for (paso = 0; paso < 2; paso++) {
    memset(rx, 0, sizeof rx);
    if (spi_transferir(a->fd, tx, rx, n_trama, (uint32_t)hz_adc) != 0) {
      spi_soltar();
      return -1;
    }
    *valor = decodificar_muestra(rx);
  }
  spi_soltar();
  return 0;
}

void adc_cerrar(adc_t *a)
{
  if (a->fd >= 0 && close(a->fd) != 0)
    log_errno("close adc");
  a->fd = -1;
}
