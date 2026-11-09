/* archivo.h - lo que debe sobrevivir a un corte: arranques y record del dino.
 * escritura: temporal, fsync del archivo, rename, fsync del directorio.
 * un corte deja el archivo viejo o el nuevo entero, nunca una mezcla.
 * bitacora: no hay particion /data aparte. el archivo marcador vive en la
 * carpeta del proyecto (el directorio de trabajo del servicio). */
#ifndef archivo_h
#define archivo_h
#include <stdint.h>

/* 0 bien, -1 si no se pudo. no crea directorios: el llamador lo hace. */
int guardar_datos(const char *ruta, uint32_t arranques, uint32_t record);
/* -1 si no existe o el texto no es exactamente dos enteros. no adivina. */
int cargar_datos(const char *ruta, uint32_t *arranques, uint32_t *record);
/* el mismo formato de un solo entero que uso la entrega 2. */
int cargar_entero(const char *ruta, uint32_t *valor);
/* mkdir. si ya existe, no es error. */
int asegurar_dir(const char *dir);
/* si ruta es null, guardar_record no escribe. lo usan las pruebas. */
void archivo_usar(const char *ruta);
int guardar_record(uint32_t arranques, uint32_t record);
#endif
