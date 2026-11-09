#include "dino.h"
#include "juego.h"
#include "archivo.h"
#include "dibujo.h"
#include "util.h"
#include <stdio.h>

enum {
  dx = 36,
  dw = 40,
  dh = 46,
  suelo = 262,
  salto_v = -11,
  grav = 1,
  vel_ini = 7,
  vel_max = 13,
  cada = 150,
  n_obs = 8,
  n_nubes = 2,
  vel_nube = 2,
  pata_cada = 4,
  cactus_w = 18
};

typedef struct {
  int x;
  int h;
  int activo;
  int ox, oy, ow, oh;
  int viejo;
} obs_t;

static unsigned visto1, visto2;
static int listo1, listo2;
static int vivo;
static int y;
static int vy;
static int vel;
static unsigned puntos;
static unsigned azar = 1u;
static int marco;
static int scroll;
static int guardado;
static int panel;
static int base;
static obs_t obs[n_obs];
static int nube_x[n_nubes];
static int nube_y[n_nubes];
static int nube_ox[n_nubes];
static int nube_vista[n_nubes];
static int dino_ox, dino_oy, dino_viejo;

static unsigned azar_paso(void)
{
  azar = azar * 1664525u + 1013904223u;
  return azar >> 16;
}

int dino_vivo(void) { return vivo; }
int dino_techo(void) { return y; }
unsigned dino_puntos(void) { return puntos; }

void forzar_cactus(int x, int alto)
{
  obs[0].x = x;
  obs[0].h = alto;
  obs[0].activo = 1;
  obs[0].viejo = 0;
}

static void limpiar_obs(void)
{
  int i;
  for (i = 0; i < n_obs; i++) {
    obs[i].activo = 0;
    obs[i].viejo = 0;
  }
}

static int extremo(void)
{
  int m = -1;
  int i;
  for (i = 0; i < n_obs; i++) {
    if (obs[i].activo && obs[i].x > m)
      m = obs[i].x;
  }
  return m;
}

static void soltar_cactus(void)
{
  int i;
  int libre = -1;
  int der = extremo();
  if (der >= 0 && ancho_px - der < 200)
    return;
  for (i = 0; i < n_obs; i++) {
    if (!obs[i].activo) {
      libre = i;
      break;
    }
  }
  if (libre < 0)
    return;
  obs[libre].x = ancho_px + 8;
  obs[libre].h = (azar_paso() & 1u) ? 36 : 52;
  obs[libre].activo = 1;
  obs[libre].viejo = 0;
}

static int choca(const obs_t *o)
{
  int ax0 = dx + 8;
  int ay0 = y + 8;
  int ax1 = dx + dw - 6;
  int ay1 = y + dh - 2;
  int bx0 = o->x + 2;
  int by0 = suelo - o->h;
  int bx1 = o->x + cactus_w - 2;
  int by1 = suelo - 2;
  if (ax1 <= bx0 || bx1 <= ax0 || ay1 <= by0 || by1 <= ay0)
    return 0;
  return 1;
}

static void dino_iniciar(estado_t *e)
{
  (void)e;
  listo1 = 0;
  listo2 = 0;
  vivo = 1;
  y = suelo - dh;
  vy = 0;
  vel = vel_ini;
  puntos = 0;
  marco = 0;
  scroll = 0;
  guardado = 0;
  panel = 0;
  base = 0;
  dino_viejo = 0;
  nube_x[0] = 90;
  nube_y[0] = 36;
  nube_x[1] = 280;
  nube_y[1] = 78;
  nube_vista[0] = 0;
  nube_vista[1] = 0;
  limpiar_obs();
  soltar_cactus();
  dibujo_limpiar(color_cielo);
}

static void dino_actualizar(estado_t *e)
{
  unsigned b1 = atomic_load(&e->pulsos[idx_b1]);
  unsigned b2 = atomic_load(&e->pulsos[idx_b2]);
  int ambos = atomic_load(&e->nivel[idx_b1]) && atomic_load(&e->nivel[idx_b2]);
  int i;
  if (ambos) {
    visto1 = b1;
    visto2 = b2;
    listo1 = 1;
    listo2 = 1;
    return;
  }
  if (pulso_nuevo(b2, &visto2, &listo2)) {
    juego_cambiar(e, juego_menu());
    return;
  }
  if (!vivo) {
    if (pulso_nuevo(b1, &visto1, &listo1))
      dino_iniciar(e);
    return;
  }
  if (pulso_nuevo(b1, &visto1, &listo1) && y >= suelo - dh)
    vy = salto_v;
  vy += grav;
  if (vy > 12)
    vy = 12;
  y += vy;
  if (y > suelo - dh) {
    y = suelo - dh;
    vy = 0;
  }
  scroll += vel;
  marco++;
  for (i = 0; i < n_obs; i++) {
    if (!obs[i].activo)
      continue;
    obs[i].x -= vel;
    if (obs[i].x + cactus_w < 0)
      obs[i].activo = 0;
  }
  soltar_cactus();
  for (i = 0; i < n_nubes; i++) {
    nube_x[i] -= vel_nube;
    if (nube_x[i] < -50)
      nube_x[i] = ancho_px + (int)(azar_paso() % 80u);
  }
  for (i = 0; i < n_obs; i++) {
    if (obs[i].activo && choca(&obs[i]))
      vivo = 0;
  }
  if (!vivo) {
    if (!guardado) {
      unsigned rec = atomic_load(&e->record);
      guardado = 1;
      if (puntos > rec) {
        atomic_store(&e->record, puntos);
        if (guardar_record(atomic_load(&e->arranques), puntos) != 0)
          log_aviso("no se guardo el record");
      }
    }
    return;
  }
  puntos++;
  if (puntos > 0 && (puntos % (unsigned)cada) == 0 && vel < vel_max)
    vel++;
}

static void dibujar_suelo(void)
{
  int x;
  int y = suelo + 8;
  /* la linea y la tierra van una sola vez. cada cuadro solo sale
   * la fila de los guiones: mandar 480x14 no cabe en 33 ms. */
  dibujo_rect(0, y, ancho_px, 3, color_cielo);
  for (x = 0; x < ancho_px; x += 16) {
    int px = x - (scroll % 16);
    if (px < 0)
      continue;
    dibujo_pixel(px, y, color_suelo);
    dibujo_pixel(px + 1, y, color_suelo);
    dibujo_pixel(px + 2, y, color_suelo);
  }
}

static void borrar(int x, int y0, int w, int h)
{
  dibujo_rect(x, y0, w, h, color_cielo);
}

static void dibujar_dino(void)
{
  int pata = (marco / pata_cada) & 1;
  int aire = y < suelo - dh;
  dibujo_rect(dx + 4, y + 20, 22, 16, color_dino);
  dibujo_rect(dx + 22, y + 4, 16, 16, color_dino);
  dibujo_rect(dx, y + 24, 10, 6, color_dino);
  dibujo_rect(dx + 30, y + 8, 3, 3, color_ojo);
  if (aire) {
    dibujo_rect(dx + 10, y + 34, 6, 10, color_dino);
    dibujo_rect(dx + 20, y + 34, 6, 8, color_dino);
  } else if (pata) {
    dibujo_rect(dx + 8, y + 34, 6, 12, color_dino);
    dibujo_rect(dx + 22, y + 32, 6, 8, color_dino);
  } else {
    dibujo_rect(dx + 8, y + 32, 6, 8, color_dino);
    dibujo_rect(dx + 22, y + 34, 6, 12, color_dino);
  }
  dino_ox = dx;
  dino_oy = y;
  dino_viejo = 1;
}

static void dibujar_cactus(obs_t *o)
{
  int cy = suelo - o->h;
  dibujo_rect(o->x + 6, cy, 6, o->h, color_cactus);
  if (o->h > 40) {
    dibujo_rect(o->x, cy + 10, 8, 4, color_cactus);
    dibujo_rect(o->x, cy + 10, 4, 12, color_cactus);
    dibujo_rect(o->x + 10, cy + 16, 8, 4, color_cactus);
    dibujo_rect(o->x + 14, cy + 16, 4, 12, color_cactus);
  }
  o->ox = o->x;
  o->oy = cy;
  o->ow = cactus_w;
  o->oh = o->h;
  o->viejo = 1;
}

static void dibujar_nube(int i)
{
  dibujo_rect(nube_x[i], nube_y[i] + 4, 36, 8, color_nube);
  dibujo_rect(nube_x[i] + 8, nube_y[i], 16, 8, color_nube);
  nube_ox[i] = nube_x[i];
  nube_vista[i] = 1;
}

static void hud(estado_t *e)
{
  char buf[32];
  unsigned rec = atomic_load(&e->record);
  dibujo_rect(220, 6, 250, 20, color_cielo);
  snprintf(buf, sizeof buf, "HI %05u %05u", rec % 100000u, puntos % 100000u);
  dibujo_texto(226, 8, buf, color_texto, 2);
}

static void dino_dibujar(estado_t *e)
{
  int i;
  if (!vivo) {
    if (!panel) {
      dibujo_rect(90, 110, 300, 70, color_panel);
      dibujo_texto(150, 124, "CHOQUE", color_oro, 3);
      dibujo_texto(110, 156, "B1 OTRA   B2 MENU", color_blanco, 2);
      panel = 1;
    }
    return;
  }
  if (dino_viejo)
    borrar(dino_ox, dino_oy, dw, dh);
  for (i = 0; i < n_nubes; i++) {
    if (nube_vista[i])
      borrar(nube_ox[i], nube_y[i], 48, 16);
  }
  for (i = 0; i < n_obs; i++) {
    if (obs[i].viejo)
      borrar(obs[i].ox, obs[i].oy, obs[i].ow, obs[i].oh);
  }
  if (!base) {
    dibujo_rect(0, suelo, ancho_px, 3, color_suelo);
    dibujo_rect(0, suelo + 12, ancho_px, alto_px - suelo - 12, color_tierra);
    base = 1;
  }
  dibujar_suelo();
  for (i = 0; i < n_nubes; i++)
    dibujar_nube(i);
  dibujar_dino();
  for (i = 0; i < n_obs; i++) {
    if (obs[i].activo)
      dibujar_cactus(&obs[i]);
    else
      obs[i].viejo = 0;
  }
  hud(e);
}

static const juego_t dino_ops = { "dino", dino_iniciar, dino_actualizar, dino_dibujar };

const juego_t *juego_dino(void)
{
  return &dino_ops;
}
