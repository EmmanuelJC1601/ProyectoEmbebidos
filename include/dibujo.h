#ifndef dibujo_h
#define dibujo_h
#include <stdint.h>
#include "constantes.h"

#define color_rgb(r, g, b) ((uint16_t)((((unsigned)(r) & 0xf8u) << 8) | (((unsigned)(g) & 0xfcu) << 3) | ((unsigned)(b) >> 3)))
#define color_cielo color_rgb(235, 235, 235)
#define color_tierra color_rgb(90, 80, 60)
#define color_suelo color_rgb(70, 70, 70)
#define color_dino color_rgb(70, 70, 70)
#define color_ojo color_rgb(255, 255, 255)
#define color_cactus color_rgb(36, 130, 58)
#define color_nube color_rgb(255, 255, 255)
#define color_texto color_rgb(30, 30, 30)
#define color_negro color_rgb(0, 0, 0)
#define color_blanco color_rgb(240, 240, 240)
#define color_menu color_rgb(12, 18, 42)
#define color_sel color_rgb(255, 196, 40)
#define color_dim color_rgb(130, 140, 160)
#define color_oro color_rgb(255, 210, 70)
#define color_pong color_rgb(8, 10, 22)
#define color_red color_rgb(50, 60, 80)
#define color_marca color_rgb(120, 220, 160)
#define color_aviso color_rgb(180, 40, 40)
#define color_panel color_rgb(20, 24, 32)

typedef struct {
  int x;
  int y;
  int w;
  int h;
} zona_t;

enum { glifo_ancho = 5, glifo_alto = 7 };

void dibujo_limpiar(uint16_t color);
void dibujo_todo(void);
void dibujo_rect(int x, int y, int w, int h, uint16_t color);
void dibujo_pixel(int x, int y, uint16_t color);
void dibujo_marcar(int x, int y, int w, int h);
int dibujo_texto(int x, int y, const char *texto, uint16_t color, int escala);
int dibujo_ancho(const char *texto, int escala);
uint16_t dibujo_leer(int x, int y);
int dibujo_zonas(zona_t *salida, int tope);
unsigned dibujo_bytes(const zona_t *zonas, int n);
unsigned dibujo_ultimo(void);
const uint16_t *dibujo_buffer(void);
#endif
