/* adc.h - driver de usuario del mcp3008.
 * bitacora clase 21: no hay driver del chip en el kernel. spi-bcm2835
 * expone /dev/spidev0.0 y aqui se arma la trama de 3 bytes a 1 mhz.
 * una syscall (ioctl) por muestra, no mmap de registros. el reloj de 1 mhz
 * no lo puede generar este proceso a tiempo si lo hiciera a mano. */
#ifndef adc_h
#define adc_h
#include <stdint.h>

typedef struct { int fd; } adc_t;

int adc_abrir(adc_t *a, const char *ruta);
/* toma el mutex del bus, lee un canal y lo suelta. canal fuera de 0..7 -> einval, sin ioctl. */
int adc_leer(adc_t *a, unsigned canal, uint16_t *valor);
void adc_cerrar(adc_t *a);
#endif
