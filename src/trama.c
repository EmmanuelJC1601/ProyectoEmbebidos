#include "trama.h"

void armar_trama(unsigned canal, uint8_t tx[n_trama])
{
  tx[0] = (uint8_t)bit_inicio;
  tx[1] = (uint8_t)((bit_simple + canal) << desplaza_canal);
  tx[2] = 0;
}

uint16_t decodificar_muestra(const uint8_t rx[n_trama])
{
  return (uint16_t)(((unsigned)(rx[1] & mascara_alta) << 8) | rx[2]);
}
