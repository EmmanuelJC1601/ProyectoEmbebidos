#include "juego.h"

int pulso_nuevo(unsigned actual, unsigned *visto, int *listo)
{
  if (!*listo) {
    *visto = actual;
    *listo = 1;
    return 0;
  }
  if (actual != *visto) {
    *visto = actual;
    return 1;
  }
  return 0;
}

static const juego_t *activo;

void juegos_iniciar(estado_t *e)
{
  activo = juego_menu();
  activo->iniciar(e);
}

const juego_t *juego_activo(void)
{
  return activo;
}

void juego_cambiar(estado_t *e, const juego_t *j)
{
  activo = j;
  j->iniciar(e);
}
