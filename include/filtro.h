/* filtro.h - logica pura, sin hardware, para probarla en la pc. */
#ifndef filtro_h
#define filtro_h
#include <stdbool.h>
#include <stdint.h>
#include "constantes.h"

typedef struct {
  uint16_t valor;
  uint16_t hist[fondo_filtro];
  uint8_t n;
  uint8_t pos;
  bool listo;
} filtro_t;

/* mediana de fondo_filtro y banda muerta. un pico de una muestra no sale.
 * si el cambio se queda, acerca un cuarto por muestra.
 * parametros: f estado del canal, muestra cruda 0..1023 (si viene mayor, se recorta). */
uint16_t filtrar_pot(filtro_t *f, uint16_t muestra);

/* escala 0..adc_max hacia 0..tope. sirve para la paleta y las barras. */
uint32_t escalar_adc(uint16_t crudo, uint32_t tope);

typedef struct {
  int64_t desde_ms;
  bool activo;
  bool disparo;
} combo_t;

/* true una sola vez cuando condicion lleva umbral_ms en alto.
 * soltar y volver a pulsar reinicia la cuenta. es el apagado de 3 s. */
bool combo_mantener(combo_t *c, bool condicion, int64_t ahora, int64_t umbral_ms);
#endif
