/* game.h - Interfaz comun para los juegos (Dino/Pong) y el modo diagnostico.
 * Un juego solo lee el estado compartido y dibuja en un frame de texto; no toca hardware.
 * Asi, agregar Pong o Dino es crear un game_xxx.c y registrarlo en game_registry(). */
#ifndef GAME_H
#define GAME_H
#include <stddef.h>
#include <stdint.h>
#include "config.h"
#include "shared_state.h"

typedef struct { char row[LCD_ROWS][LCD_COLS + 1u]; } frame_t;

typedef struct {
    const char *name;
    void (*start)(void);                                     /* reinicia el juego */
    void (*update)(shared_t *s, uint32_t dt_ms);             /* logica a paso fijo */
    void (*render)(shared_t *s, frame_t *f);                 /* dibuja en el frame */
} game_ops_t;

void frame_set_row(frame_t *f, unsigned row, const char *text);
const game_ops_t *game_registry(size_t *count);
extern const game_ops_t GAME_DIAG;
/* Futuro: extern const game_ops_t GAME_DINO, GAME_PONG; */
#endif
