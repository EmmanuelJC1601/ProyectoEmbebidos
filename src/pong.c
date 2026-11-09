#include "pong.h"
#include "juego.h"
#include "filtro.h"
#include "dibujo.h"
#include "util.h"
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

enum {
  campo_y = 28,
  campo_h = 276,
  paleta_w = 8,
  paleta_h = 56,
  paleta_x = 12,
  bola = 8,
  vel = 8,
  red_x = 239,
  saca_izq = 0,
  saca_der = 1,
  vy_saque = 0
};

static unsigned visto1, visto2;
static int listo1, listo2;
static int bx, by, vx, vy;
static int izquierda, derecha;
static int espera;
static int servidor;
static int marco_izq, marco_der;
static int bola_ox, bola_oy, bola_vista;
static int izq_oy, der_oy, izq_vista, der_vista;
static int aviso;

int pelota_x(void) { return bx; }
int pelota_y(void) { return by; }
int pelota_vy(void) { return vy; }
unsigned puntos_pong(int lado) { return lado ? (unsigned)derecha : (unsigned)izquierda; }

void forzar_pelota(int x, int y, int vx0, int vy0)
{
  bx = x;
  by = y;
  vx = vx0;
  vy = vy0;
  espera = 0;
  aviso = 0;
}

static int paleta_y(unsigned crudo)
{
  uint32_t recorrido = (uint32_t)(campo_h - paleta_h);
  return campo_y + (int)escalar_adc((uint16_t)crudo, recorrido);
}

/* y de la pelota para que su centro coincida con el de la paleta. */
static int centro_y(int py)
{
  int y = py + paleta_h / 2 - bola / 2;
  int tope = campo_y + campo_h - bola;
  if (y < campo_y)
    return campo_y;
  if (y > tope)
    return tope;
  return y;
}

/* deja la pelota pegada al centro de quien saca. no apaga bola_vista:
 * el dibujo tiene que tapar la pelota que estaba en juego. */
static void poner_saque(int py1, int py2)
{
  if (servidor == saca_izq) {
    bx = paleta_x + paleta_w;
    by = centro_y(py1);
    vx = vel;
  } else {
    bx = ancho_px - paleta_x - paleta_w - bola;
    by = centro_y(py2);
    vx = -vel;
  }
  vy = vy_saque;
}

static void pedir_saque(int lado, int py1, int py2)
{
  servidor = lado;
  espera = 1;
  aviso = 0;
  poner_saque(py1, py2);
}

/* quien abre la partida. un byte de /dev/urandom. si el archivo no se puede
 * leer, vale el nanosegundo impar. el resto de los puntos no usa esto:
 * saca quien dejo pasar la pelota. */
static int lado_azar(void)
{
  uint8_t b = 0;
  int fd = open("/dev/urandom", O_RDONLY);
  ssize_t n;
  if (fd < 0) {
    log_errno("open urandom");
    return (ahora_ns() & 1) ? saca_der : saca_izq;
  }
  n = read(fd, &b, 1);
  if (close(fd) != 0)
    log_errno("close urandom");
  if (n != 1) {
    if (n < 0)
      log_errno("read urandom");
    return (ahora_ns() & 1) ? saca_der : saca_izq;
  }
  return (b & 1u) ? saca_der : saca_izq;
}

static void reparar_red(int y, int h)
{
  int yy;
  int y1 = y + h;
  if (y < campo_y)
    y = campo_y;
  if (y1 > campo_y + campo_h)
    y1 = campo_y + campo_h;
  for (yy = y; yy < y1; yy++) {
    uint16_t c = ((yy / 8) & 1) ? color_pong : color_red;
    dibujo_pixel(red_x, yy, c);
    dibujo_pixel(red_x + 1, yy, c);
  }
  if (y1 > y)
    dibujo_marcar(red_x, y, 2, y1 - y);
}

static void pong_iniciar(estado_t *e)
{
  listo1 = 0;
  listo2 = 0;
  izquierda = 0;
  derecha = 0;
  marco_izq = -1;
  marco_der = -1;
  izq_vista = 0;
  der_vista = 0;
  bola_vista = 0;
  dibujo_limpiar(color_pong);
  reparar_red(campo_y, campo_h);
  pedir_saque(lado_azar(), paleta_y(atomic_load(&e->pot[idx_p1])), paleta_y(atomic_load(&e->pot[idx_p2])));
}

static int golpea(int py, int ny)
{
  return ny + bola > py && ny < py + paleta_h;
}

static void pong_actualizar(estado_t *e)
{
  unsigned b1 = atomic_load(&e->pulsos[idx_b1]);
  unsigned b2 = atomic_load(&e->pulsos[idx_b2]);
  int ambos = atomic_load(&e->nivel[idx_b1]) && atomic_load(&e->nivel[idx_b2]);
  int py1 = paleta_y(atomic_load(&e->pot[idx_p1]));
  int py2 = paleta_y(atomic_load(&e->pot[idx_p2]));
  int nx, ny;
  int borde_izq = paleta_x + paleta_w;
  int borde_der = ancho_px - paleta_x - paleta_w;
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
  if (espera) {
    poner_saque(py1, py2);
    if (!pulso_nuevo(b1, &visto1, &listo1))
      return;
    espera = 0;
  }
  ny = by + vy;
  if (ny < campo_y) {
    vy = -vy;
    ny = campo_y;
  } else if (ny + bola > campo_y + campo_h) {
    vy = -vy;
    ny = campo_y + campo_h - bola;
  }
  nx = bx + vx;
  if (vx < 0 && bx >= borde_izq && nx < borde_izq) {
    if (golpea(py1, ny)) {
      vx = vel;
      vy = (ny + bola / 2 - (py1 + paleta_h / 2)) / 4;
      if (vy == 0)
        vy = 1;
      if (vy > 8)
        vy = 8;
      if (vy < -8)
        vy = -8;
      nx = borde_izq;
    } else {
      derecha++;
      pedir_saque(saca_izq, py1, py2);
      return;
    }
  } else if (vx > 0 && bx + bola <= borde_der && nx + bola > borde_der) {
    if (golpea(py2, ny)) {
      vx = -vel;
      vy = (ny + bola / 2 - (py2 + paleta_h / 2)) / 4;
      if (vy == 0)
        vy = -1;
      if (vy > 8)
        vy = 8;
      if (vy < -8)
        vy = -8;
      nx = borde_der - bola;
    } else {
      izquierda++;
      pedir_saque(saca_der, py1, py2);
      return;
    }
  }
  bx = nx;
  by = ny;
}

static void dibujar_paleta(int x, int y, int *oy, int *vista)
{
  if (*vista && *oy != y)
    dibujo_rect(x, *oy, paleta_w, paleta_h, color_pong);
  if (!*vista || *oy != y)
    dibujo_rect(x, y, paleta_w, paleta_h, color_blanco);
  *oy = y;
  *vista = 1;
}

/* tapa la pelota anterior antes de mover las paletas, para no dejarle un hueco. */
static void borrar_bola(void)
{
  if (!bola_vista || (bola_ox == bx && bola_oy == by))
    return;
  dibujo_rect(bola_ox, bola_oy, bola, bola, color_pong);
  if (bola_ox <= red_x + 1 && bola_ox + bola >= red_x)
    reparar_red(bola_oy, bola);
}

static void pintar_bola(void)
{
  dibujo_rect(bx, by, bola, bola, color_oro);
  bola_ox = bx;
  bola_oy = by;
  bola_vista = 1;
}

static void pong_dibujar(estado_t *e)
{
  int y1 = paleta_y(atomic_load(&e->pot[idx_p1]));
  int y2 = paleta_y(atomic_load(&e->pot[idx_p2]));
  char buf[32];
  if (marco_izq != izquierda || marco_der != derecha) {
    dibujo_rect(0, 4, ancho_px, 18, color_pong);
    snprintf(buf, sizeof buf, "P1 %02d    P2 %02d", izquierda % 100, derecha % 100);
    dibujo_texto(140, 4, buf, color_marca, 2);
    marco_izq = izquierda;
    marco_der = derecha;
  }
  if (espera) {
    if (!aviso) {
      dibujo_texto(96, 40, servidor == saca_der ? "P2 SACA   B2 MENU" : "P1 SACA   B2 MENU", color_blanco, 2);
      aviso = 1;
    }
  } else if (aviso) {
    dibujo_rect(90, 38, 320, 20, color_pong);
    reparar_red(38, 22);
    aviso = 0;
  }
  borrar_bola();
  dibujar_paleta(paleta_x, y1, &izq_oy, &izq_vista);
  dibujar_paleta(ancho_px - paleta_x - paleta_w, y2, &der_oy, &der_vista);
  pintar_bola();
}

static const juego_t pong_ops = { "pong", pong_iniciar, pong_actualizar, pong_dibujar };

const juego_t *juego_pong(void)
{
  return &pong_ops;
}
