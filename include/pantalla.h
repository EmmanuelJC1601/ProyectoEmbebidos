#ifndef pantalla_h
#define pantalla_h
int pantalla_abrir(void);
/* manda las zonas sucias. 0 bien, -1 si el spi fallo (el juego sigue). */
int pantalla_volcar(void);
void pantalla_luz(int encendida);
int pantalla_viva(void);
void pantalla_cerrar(void);
#endif
