/* estado.h - lo que comparten los hilos.
 * bitacora clase 9: cada dato tiene un solo escritor. por eso bastan atomicos
 * y el hilo del juego nunca espera un mutex para leer un boton o una perilla.
 * el bus spi es la excepcion y vive en spi.c, no aqui. */
#ifndef estado_h
#define estado_h
#include <stdatomic.h>
#include "constantes.h"

typedef struct {
  atomic_int nivel[n_botones];     /* 1 = presionado, ya con debounce */
  atomic_uint pulsos[n_botones];   /* flancos de subida aceptados */
  atomic_uint pot[n_potes];        /* 0..1023 ya filtrado */
  atomic_uint latencia_us;        /* flanco del kernel -> nivel publicado */
  atomic_uint latencia_max;
  atomic_uint errores_adc;
  atomic_uint errores_tft;
  atomic_uint fps;
  atomic_uint cuadro_us;          /* peor cuadro del ultimo segundo */
  atomic_uint record;
  atomic_uint arranques;
  atomic_int adc_listo;           /* 1 tras la primera pareja de muestras */
  atomic_int salir;
  atomic_int apagar;
  atomic_int fallo;
} estado_t;

/* deja el estado en cero. no reserva memoria: el llamador pone la struct. */
void estado_iniciar(estado_t *e);
#endif
