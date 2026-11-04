#include "game.h"
#include <string.h>

void frame_set_row(frame_t *f, unsigned row, const char *text)
{
    if (row >= LCD_ROWS) return;
    size_t n = strlen(text);
    if (n > LCD_COLS) n = LCD_COLS;
    memset(f->row[row], ' ', LCD_COLS);
    memcpy(f->row[row], text, n);
    f->row[row][LCD_COLS] = '\0';
}

static const game_ops_t *const REGISTRY[] = { &GAME_DIAG /*, &GAME_DINO, &GAME_PONG */ };

const game_ops_t *game_registry(size_t *count)
{
    *count = sizeof(REGISTRY) / sizeof(REGISTRY[0]);
    return REGISTRY[0]; /* el primero es el modo por defecto; el menu vendra con los juegos */
}
