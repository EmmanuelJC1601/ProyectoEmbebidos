/* trama.h - bytes del mcp3008, sin abrir el spi. */
#ifndef trama_h
#define trama_h
#include <stdint.h>
#include "constantes.h"

/* canal 0..7. la trama de un canal simple es inicio, (0x08+canal)<<4, 0x00. */
void armar_trama(unsigned canal, uint8_t tx[n_trama]);
/* 10 bits. los bits de arriba de rx[1] no forman parte de la muestra. */
uint16_t decodificar_muestra(const uint8_t rx[n_trama]);
#endif
