/* spi.h - bus compartido por el mcp3008 (ce0) y la tft (ce1).
 * bitacora clase 9: este es el recurso compartido entre dos hilos.
 * se protege con pthread_mutex y herencia de prioridad, no con atomicos,
 * porque una transferencia no es una palabra.
 * el kernel tambien serializa el controlador, pero el mutex deja la seccion
 * critica escrita donde la bitacora pide verla, y suelta el bus entre bloques
 * de 4 kib para que el adc no espere un cuadro entero. */
#ifndef spi_h
#define spi_h
#include <stddef.h>
#include <stdint.h>

int spi_iniciar(void);
void spi_cerrar(void);
/* 0 bien, -1 error. el llamador ya tomo el bus, o es el unico hilo. */
int spi_preparar(int fd, uint32_t hz);
int spi_transferir(int fd, const uint8_t *tx, uint8_t *rx, size_t n, uint32_t hz);
int spi_tomar(void);
void spi_soltar(void);
#endif
