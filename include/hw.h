/* hw.h - abre dispositivos y lanza los dos hilos. */
#ifndef hw_h
#define hw_h
#include "estado.h"
int hw_abrir(estado_t *e, int simular);
void hw_cerrar(void);
int hw_volcar(void);
void hw_luz(int encendida);
void hw_entrada(estado_t *e);
/* si la tft se cayo, intenta abrirla otra vez. 1 si quedo lista. */
int hw_recuperar(void);
#endif
