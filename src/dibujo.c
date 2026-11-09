#include "dibujo.h"
#include <string.h>

static uint16_t fb[alto_px * ancho_px];
static zona_t zonas[zonas_max];
static int nzonas;
static int todo;
static unsigned ultimo_bytes;

#include "fuente.inc"

static int dentro(int x, int y)
{
  return x >= 0 && y >= 0 && x < ancho_px && y < alto_px;
}

void dibujo_pixel(int x, int y, uint16_t color)
{
  if (!dentro(x, y))
    return;
  fb[(size_t)y * (size_t)ancho_px + (size_t)x] = color;
}

static void recortar(int *x, int *y, int *w, int *h)
{
  if (*w <= 0 || *h <= 0) {
    *w = 0;
    *h = 0;
    return;
  }
  if (*x < 0) {
    *w += *x;
    *x = 0;
  }
  if (*y < 0) {
    *h += *y;
    *y = 0;
  }
  if (*x >= ancho_px || *y >= alto_px) {
    *w = 0;
    *h = 0;
    return;
  }
  if (*x + *w > ancho_px)
    *w = ancho_px - *x;
  if (*y + *h > alto_px)
    *h = alto_px - *y;
  if (*w < 0)
    *w = 0;
  if (*h < 0)
    *h = 0;
}

void dibujo_marcar(int x, int y, int w, int h)
{
  recortar(&x, &y, &w, &h);
  if (w == 0 || h == 0 || todo)
    return;
  if (nzonas >= zonas_max) {
    todo = 1;
    nzonas = 0;
    return;
  }
  zonas[nzonas].x = x;
  zonas[nzonas].y = y;
  zonas[nzonas].w = w;
  zonas[nzonas].h = h;
  nzonas++;
}

static void rellenar(int x, int y, int w, int h, uint16_t color)
{
  int fila;
  int col;
  for (fila = 0; fila < h; fila++) {
    uint16_t *p = fb + ((size_t)(y + fila) * (size_t)ancho_px + (size_t)x);
    for (col = 0; col < w; col++)
      p[col] = color;
  }
}

void dibujo_rect(int x, int y, int w, int h, uint16_t color)
{
  recortar(&x, &y, &w, &h);
  if (w == 0 || h == 0)
    return;
  rellenar(x, y, w, h, color);
  dibujo_marcar(x, y, w, h);
}

void dibujo_limpiar(uint16_t color)
{
  size_t i;
  size_t n = (size_t)ancho_px * (size_t)alto_px;
  for (i = 0; i < n; i++)
    fb[i] = color;
  nzonas = 0;
  todo = 1;
}

void dibujo_todo(void)
{
  nzonas = 0;
  todo = 1;
}

static const uint8_t *buscar_glifo(char c)
{
  const char *p;
  if (c >= 'a' && c <= 'z')
    c = (char)(c - 'a' + 'A');
  p = strchr(alfabeto, c);
  if (!p)
    return NULL;
  return glifo[(size_t)(p - alfabeto)];
}

int dibujo_ancho(const char *texto, int escala)
{
  size_t n;
  if (escala < 1)
    escala = 1;
  n = strlen(texto);
  if (n == 0)
    return 0;
  return (int)n * (glifo_ancho + 1) * escala;
}

int dibujo_texto(int x, int y, const char *texto, uint16_t color, int escala)
{
  int cx = x;
  int alto;
  const char *t;
  if (escala < 1)
    escala = 1;
  alto = glifo_alto * escala;
  for (t = texto; *t; t++) {
    const uint8_t *filas = buscar_glifo(*t);
    int fy;
    if (filas) {
      for (fy = 0; fy < glifo_alto; fy++) {
        uint8_t bits = filas[fy];
        int fx;
        for (fx = 0; fx < glifo_ancho; fx++) {
          int px;
          int py;
          int pw;
          int ph;
          if ((bits & (uint8_t)(0x10u >> fx)) == 0)
            continue;
          px = cx + fx * escala;
          py = y + fy * escala;
          pw = escala;
          ph = escala;
          recortar(&px, &py, &pw, &ph);
          if (pw > 0 && ph > 0)
            rellenar(px, py, pw, ph, color);
        }
      }
    }
    cx += (glifo_ancho + 1) * escala;
  }
  /* una sola zona para toda la frase. un rect por pixel llenaria la lista
   * y obligaria a mandar la pantalla entera. */
  if (cx > x)
    dibujo_marcar(x, y, cx - x, alto);
  return cx - x;
}

uint16_t dibujo_leer(int x, int y)
{
  if (!dentro(x, y))
    return 0;
  return fb[(size_t)y * (size_t)ancho_px + (size_t)x];
}

/* junta dos rectangulos si el resultado sigue siendo un sprite.
 * la franja del suelo no se mezcla con un cactus: el area se pasa del tope. */
static int se_juntan(const zona_t *a, const zona_t *b)
{
  int x0 = a->x < b->x ? a->x : b->x;
  int y0 = a->y < b->y ? a->y : b->y;
  int x1 = a->x + a->w > b->x + b->w ? a->x + a->w : b->x + b->w;
  int y1 = a->y + a->h > b->y + b->h ? a->y + a->h : b->y + b->h;
  int uw = x1 - x0;
  int uh = y1 - y0;
  enum { hueco = 12, area_tope = 6000 };
  if (uw <= 0 || uh <= 0)
    return 0;
  if (uw > a->w + b->w + hueco || uh > a->h + b->h + hueco)
    return 0;
  if (uw * uh > area_tope)
    return 0;
  return 1;
}

static void absorber(zona_t *a, const zona_t *b)
{
  int x1 = a->x + a->w > b->x + b->w ? a->x + a->w : b->x + b->w;
  int y1 = a->y + a->h > b->y + b->h ? a->y + a->h : b->y + b->h;
  if (b->x < a->x)
    a->x = b->x;
  if (b->y < a->y)
    a->y = b->y;
  a->w = x1 - a->x;
  a->h = y1 - a->y;
}

static int fundir(zona_t *z, int n)
{
  int cambio = 1;
  while (cambio) {
    int i;
    int j;
    cambio = 0;
    for (i = 0; i < n; i++) {
      for (j = i + 1; j < n; j++) {
        if (!se_juntan(&z[i], &z[j]))
          continue;
        absorber(&z[i], &z[j]);
        z[j] = z[n - 1];
        n--;
        j--;
        cambio = 1;
      }
    }
  }
  return n;
}

int dibujo_zonas(zona_t *salida, int tope)
{
  int n;
  if (tope < 1)
    return 0;
  if (todo) {
    salida[0].x = 0;
    salida[0].y = 0;
    salida[0].w = ancho_px;
    salida[0].h = alto_px;
    todo = 0;
    nzonas = 0;
    ultimo_bytes = dibujo_bytes(salida, 1);
    return 1;
  }
  n = nzonas;
  if (n > tope)
    n = tope;
  if (n > 1)
    n = fundir(zonas, n);
  if (n > 0)
    memcpy(salida, zonas, (size_t)n * sizeof(zona_t));
  nzonas = 0;
  ultimo_bytes = dibujo_bytes(salida, n);
  return n;
}

unsigned dibujo_bytes(const zona_t *z, int n)
{
  unsigned total = 0;
  int i;
  for (i = 0; i < n; i++) {
    if (z[i].w > 0 && z[i].h > 0)
      total += (unsigned)z[i].w * (unsigned)z[i].h * 2u;
  }
  return total;
}

unsigned dibujo_ultimo(void)
{
  return ultimo_bytes;
}

const uint16_t *dibujo_buffer(void)
{
  return fb;
}
