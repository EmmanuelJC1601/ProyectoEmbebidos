
#ifndef spi_h
#define spi_h
#include <stddef.h>
#include <stdint.h>

int spi_iniciar(void);
void spi_cerrar(void);
int spi_preparar(int fd, uint32_t hz);
int spi_transferir(int fd, const uint8_t *tx, uint8_t *rx, size_t n, uint32_t hz);
int spi_tomar(void);
void spi_soltar(void);
#endif
