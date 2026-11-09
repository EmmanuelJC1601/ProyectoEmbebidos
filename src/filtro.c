#include "filtro.h"
#include "constantes.h"

/* n es chico (fondo_filtro). ordenar copia, no el historial. */
static int mediana(const uint16_t *v, int n)
{
  uint16_t tmp[fondo_filtro];
  int i;
  int j;
  for (i = 0; i < n; i++)
    tmp[i] = v[i];
  for (i = 1; i < n; i++) {
    uint16_t x = tmp[i];
    j = i;
    while (j > 0 && tmp[j - 1] > x) {
      tmp[j] = tmp[j - 1];
      j--;
    }
    tmp[j] = x;
  }
  return (int)tmp[n / 2];
}

uint16_t filtrar_pot(filtro_t *f, uint16_t muestra)
{
  int promedio;
  int diff;
  int mag;
  int paso;
  if (muestra > adc_max)
    muestra = adc_max;
  f->hist[f->pos] = muestra;
  f->pos = (uint8_t)((f->pos + 1u) % (unsigned)fondo_filtro);
  if (f->n < (uint8_t)fondo_filtro)
    f->n++;
  promedio = mediana(f->hist, (int)f->n);
  if (!f->listo) {
    f->valor = (uint16_t)promedio;
    f->listo = true;
    return f->valor;
  }
  diff = promedio - (int)f->valor;
  mag = diff < 0 ? -diff : diff;
  if ((unsigned)mag <= (unsigned)banda_muerta)
    return f->valor;
  /* division, no desplazamiento: el desplazamiento de un negativo no esta definido igual en todos lados. */
  paso = diff / (1 << paso_filtro);
  if (paso == 0)
    paso = diff > 0 ? 1 : -1;
  f->valor = (uint16_t)((int)f->valor + paso);
  return f->valor;
}

uint32_t escalar_adc(uint16_t crudo, uint32_t tope)
{
  if (crudo > adc_max)
    crudo = adc_max;
  return ((uint32_t)crudo * tope) / (uint32_t)adc_max;
}

bool combo_mantener(combo_t *c, bool condicion, int64_t ahora, int64_t umbral_ms)
{
  if (!condicion) {
    c->activo = false;
    c->disparo = false;
    return false;
  }
  if (!c->activo) {
    c->activo = true;
    c->desde_ms = ahora;
    return false;
  }
  if (!c->disparo && (ahora - c->desde_ms) >= umbral_ms) {
    c->disparo = true;
    return true;
  }
  return false;
}
