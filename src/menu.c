#include "juego.h"
#include "dibujo.h"
#include <stdio.h>

static unsigned visto_b1, visto_b2;
static int listo1, listo2;
static unsigned idx;
static int repintar;

/* b1 recorre, b2 entra. si los dos estan presionados no se navega:
 * ese gesto es el apagado de 3 s y no debe cambiar de juego a medias. */
static void menu_iniciar(estado_t *e)
{
  (void)e;
  listo1 = 0;
  listo2 = 0;
  idx = 0;
  repintar = 1;
  dibujo_limpiar(color_menu);
}

static void menu_actualizar(estado_t *e)
{
  int ambos = atomic_load(&e->nivel[idx_b1]) && atomic_load(&e->nivel[idx_b2]);
  unsigned b1 = atomic_load(&e->pulsos[idx_b1]);
  unsigned b2 = atomic_load(&e->pulsos[idx_b2]);
  if (ambos) {
    visto_b1 = b1;
    visto_b2 = b2;
    listo1 = 1;
    listo2 = 1;
    return;
  }
  if (pulso_nuevo(b1, &visto_b1, &listo1)) {
    idx = (idx + 1u) % 3u;
    repintar = 1;
  }
  if (pulso_nuevo(b2, &visto_b2, &listo2)) {
    const juego_t *siguiente = juego_dino();
    if (idx == 1u)
      siguiente = juego_pong();
    else if (idx == 2u)
      siguiente = juego_prueba();
    juego_cambiar(e, siguiente);
  }
}

static void menu_dibujar(estado_t *e)
{
  static const char *nombres[3] = { "DINO", "PONG", "PRUEBA" };
  char linea[32];
  unsigned rec = atomic_load(&e->record);
  int i;
  int y = 150;
  if (!repintar)
    return;
  repintar = 0;
  dibujo_limpiar(color_menu);
  dibujo_texto((ancho_px - dibujo_ancho("ARCADE", 4)) / 2, 36, "ARCADE", color_oro, 4);
  dibujo_texto((ancho_px - dibujo_ancho("ELIGE", 2)) / 2, 92, "ELIGE", color_dim, 2);
  for (i = 0; i < 3; i++) {
    uint16_t tinta = (unsigned)i == idx ? color_negro : color_blanco;
    int ancho = dibujo_ancho(nombres[i], 3);
    int x = (ancho_px - ancho) / 2;
    if ((unsigned)i == idx)
      dibujo_rect(x - 16, y - 6, ancho + 32, 32, color_sel);
    dibujo_texto(x, y, nombres[i], tinta, 3);
    y += 46;
  }
  snprintf(linea, sizeof linea, "RECORD %05u", rec % 100000u);
  dibujo_texto(16, alto_px - 44, linea, color_dim, 2);
  dibujo_texto(16, alto_px - 24, "B1 SIGUE  B2 ENTRA", color_blanco, 2);
}

static const juego_t menu_ops = { "menu", menu_iniciar, menu_actualizar, menu_dibujar };

const juego_t *juego_menu(void)
{
  return &menu_ops;
}