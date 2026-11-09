#include "estado.h"

void estado_iniciar(estado_t *e)
{
  unsigned i;
  for (i = 0; i < n_botones; i++) {
    atomic_init(&e->nivel[i], 0);
    atomic_init(&e->pulsos[i], 0u);
  }
  for (i = 0; i < n_potes; i++)
    atomic_init(&e->pot[i], 0u);
  atomic_init(&e->latencia_us, 0u);
  atomic_init(&e->latencia_max, 0u);
  atomic_init(&e->errores_adc, 0u);
  atomic_init(&e->errores_tft, 0u);
  atomic_init(&e->fps, 0u);
  atomic_init(&e->cuadro_us, 0u);
  atomic_init(&e->record, 0u);
  atomic_init(&e->arranques, 0u);
  atomic_init(&e->adc_listo, 0);
  atomic_init(&e->salir, 0);
  atomic_init(&e->apagar, 0);
  atomic_init(&e->fallo, 0);
}
