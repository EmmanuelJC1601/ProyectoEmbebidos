#include "juego.h"
#include "filtro.h"
#include "dibujo.h"
#include <stdio.h>
#include <string.h>

static int base;
static int ambos_listo;
static int ambos_abajo;
static int pot_v1 = -1;
static int pot_v2 = -1;
static char visto_txt[7][40];

/* muestra perillas, botones, latencia y fps.
 * b1 y b2 se quedan en pantalla: si b2 volviera al menu no se podria ver.
 * volver es un toque de los dos a la vez. los 3 s siguen apagando.
 * solo repinta la linea que cambio: si no, la propia pantalla tira el fps. */
static void prueba_iniciar(estado_t *e)
{
  (void)e;
  ambos_listo = 0;
  ambos_abajo = 0;
  base = 0;
  pot_v1 = -1;
  pot_v2 = -1;
  memset(visto_txt, 0, sizeof visto_txt);
  dibujo_limpiar(color_menu);
}

static void prueba_actualizar(estado_t *e)
{
  int ambos = atomic_load(&e->nivel[idx_b1]) && atomic_load(&e->nivel[idx_b2]);
  /* el primer vistazo no cuenta: pueden seguir presionados del menu. */
  if (!ambos_listo) {
    ambos_listo = 1;
    ambos_abajo = ambos;
    return;
  }
  if (ambos) {
    ambos_abajo = 1;
    return;
  }
  if (ambos_abajo) {
    ambos_abajo = 0;
    juego_cambiar(e, juego_menu());
  }
}

static void barra(int y, unsigned crudo)
{
  int w = (int)escalar_adc((uint16_t)crudo, 360u);
  dibujo_rect(20, y, 360, 12, color_dim);
  if (w > 0)
    dibujo_rect(20, y, w, 12, color_oro);
}

static void linea(int i, int y, const char *texto)
{
  if (strcmp(visto_txt[i], texto) == 0)
    return;
  snprintf(visto_txt[i], sizeof visto_txt[i], "%s", texto);
  dibujo_rect(16, y, 448, 18, color_menu);
  dibujo_texto(16, y, texto, color_blanco, 2);
}

static void prueba_dibujar(estado_t *e)
{
  char buf[48];
  unsigned p1 = atomic_load(&e->pot[idx_p1]);
  unsigned p2 = atomic_load(&e->pot[idx_p2]);
  if (!base) {
    dibujo_texto(16, 12, "PRUEBA", color_oro, 3);
    dibujo_texto(16, alto_px - 28, "AMBOS 1 TOQUE MENU  3S APAGA", color_dim, 2);
    base = 1;
  }
  snprintf(buf, sizeof buf, "P1 %4u", p1 % 10000u);
  linea(0, 52, buf);
  if (pot_v1 != (int)p1) {
    barra(74, p1);
    pot_v1 = (int)p1;
  }
  snprintf(buf, sizeof buf, "P2 %4u", p2 % 10000u);
  linea(1, 96, buf);
  if (pot_v2 != (int)p2) {
    barra(118, p2);
    pot_v2 = (int)p2;
  }
  snprintf(buf, sizeof buf, "B1 %c %3u   B2 %c %3u",
           atomic_load(&e->nivel[idx_b1]) ? '#' : '-',
           atomic_load(&e->pulsos[idx_b1]) % 1000u,
           atomic_load(&e->nivel[idx_b2]) ? '#' : '-',
           atomic_load(&e->pulsos[idx_b2]) % 1000u);
  linea(2, 142, buf);
  snprintf(buf, sizeof buf, "LAT %5u US  MAX %5u",
           atomic_load(&e->latencia_us), atomic_load(&e->latencia_max));
  linea(3, 168, buf);
  snprintf(buf, sizeof buf, "FPS %2u  CUADRO %5u US",
           atomic_load(&e->fps), atomic_load(&e->cuadro_us));
  linea(4, 194, buf);
  snprintf(buf, sizeof buf, "ADC %u TFT %u BOOT %u",
           atomic_load(&e->errores_adc), atomic_load(&e->errores_tft),
           atomic_load(&e->arranques));
  linea(5, 220, buf);
  snprintf(buf, sizeof buf, "RECORD %05u", atomic_load(&e->record) % 100000u);
  linea(6, 246, buf);
}

static const juego_t prueba_ops = { "prueba", prueba_iniciar, prueba_actualizar, prueba_dibujar };

const juego_t *juego_prueba(void)
{
  return &prueba_ops;
}
