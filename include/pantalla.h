/* pantalla.h - driver de usuario del st7796s por /dev/spidev0.1.
 * bitacora clase 21: no usamos fbtft ni un overlay en config.txt, porque eso
 * cambiaria el sistema y ademas el kernel se quedaria con el spi del adc.
 * dc, reset y luz son gpio por libgpiod. cs lo baja el propio spidev (ce1).
 * la luz es salida gpio, no pwm: el overlay de pwm tambien cambiaria el so. */
#ifndef pantalla_h
#define pantalla_h
int pantalla_abrir(void);
/* manda las zonas sucias. 0 bien, -1 si el spi fallo (el juego sigue). */
int pantalla_volcar(void);
void pantalla_luz(int encendida);
int pantalla_viva(void);
void pantalla_cerrar(void);
#endif
