#ifndef STORAGE_H
#define STORAGE_H
#include <stdint.h>
/* Escritura atomica: archivo temporal + fsync + rename + fsync del directorio.
 * Un corte de energia deja el valor viejo o el nuevo completo, nunca uno a medias. */
int storage_save_u32(const char *path, uint32_t value);
int storage_load_u32(const char *path, uint32_t *value);  /* -1 si falta o esta corrupto */
#endif
