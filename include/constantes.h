/* constantes.h - cifras del sistema. un numero de diseno vive aqui, no suelto en el codigo.
 * las macros de la libc y del kernel (o_rdwr, spi_ioc_*, sched_fifo, gpiod_*) se escriben
 * como las declara el encabezado: ahi la mayuscula es obligatoria. */
#ifndef constantes_h
#define constantes_h
#include <stdint.h>

enum {
  ns_por_s = 1000000000,
  ns_por_ms = 1000000,
  us_por_ms = 1000,
  ms_por_s = 1000
};

/* botones. circuito de la entrega 2: 3.3 v -> boton -> gpio, 10k a gnd, activo en alto. */
enum {
  n_botones = 2,
  idx_b1 = 0,
  idx_b2 = 1,
  idx_p1 = 0,
  idx_p2 = 1,
  pin_boton1 = 5,          /* bcm, header pin 29 */
  pin_boton2 = 6,          /* bcm, header pin 31 */
  us_debounce = 20000,     /* requisito: filtrar rebote 20 ms */
  ms_espera_boton = 50,    /* tope de bloqueo para revisar salir y el combo */
  n_eventos = 16,
  ms_apagado = 3000,       /* ambos botones estables: apagado controlado */
  us_latencia_max = 50000  /* requisito de la entrega 1 */
};

/* mcp3008 en spi0 ce0. 1 mhz ya medido en la entrega 2 con vdd 3.3 v. */
enum {
  hz_adc = 1000000,
  bits_spi = 8,
  n_canales = 8,
  adc_max = 1023,
  n_potes = 2,
  canal_p1 = 0,
  canal_p2 = 1,
  ms_adc = 10,             /* 100 hz. el requisito de entrada es 50 ms */
  paso_filtro = 2,         /* avanza 1/4 de lo que ya paso la mediana */
  banda_muerta = 16,       /* +-16 cuentas: el ruido del cable no mueve la paleta */
  fondo_filtro = 5,        /* mediana de 5 muestras (50 ms). un pico no entra */
  n_trama = 3,
  bit_inicio = 0x01,
  bit_simple = 0x08,       /* un solo extremo contra gnd */
  desplaza_canal = 4,
  mascara_alta = 0x03
};

/* st7796s 3.5 pulgadas. paisaje 480x320, rgb565. ce1 para no pisar el mcp3008. */
enum {
  ancho_px = 480,
  alto_px = 320,
  hz_tft = 32000000,       /* el driver redondea al divisor par del reloj spi */
  pin_dc = 25,             /* header pin 22. comando o dato */
  pin_rst = 27,            /* header pin 13 */
  pin_luz = 18,            /* header pin 12. gpio, no pwm: el overlay cambiaria el so */
  luz_encendida = 1,       /* modulos habituales: led en alto enciende */
  ms_reset = 20,
  ms_panel = 120,          /* la hoja del st7796 pide espera tras salir de sleep */
  tam_bloque = 4096,       /* tope de fabrica de spidev.bufsiz. no lo cambiamos */
  x_origen = 0,
  y_origen = 0,
  madctl_paisaje = 0x28,  /* mv | bgr. si sale volteada, probar 0xe8 o 0x48 */
  invertir_color = 1,
  zonas_max = 96
};

/* tareas. bitacora clase 8: fifo en la entrada, el dibujo en el hilo principal. */
enum {
  prio_botones = 50,
  prio_adc = 40,
  pila_hilo = 128 * 1024,  /* 128 kib por hilo de tiempo real. caso b de memoria */
  ms_cuadro = 33,          /* 30 hz estables. requisito: al menos 30 fps */
  ms_reintento = 2000,     /* pantalla o spi caidos: se vuelve a abrir */
  ms_espera_adc = 500,     /* al arrancar, un margen para la primera muestra */
  errores_tope = 20,       /* fallos seguidos: el proceso sale y systemd lo reinicia */
  ms_inactivo = 60000,     /* sin tocar nada: se apaga la luz de fondo */
  fps_minimo = 30
};

enum {
  codigo_apagar = 42,      /* arcade.service no reinicia este codigo: hace poweroff */
  codigo_uso = 2
};

#define ruta_gpio "/dev/gpiochip0"
#define ruta_spi_adc "/dev/spidev0.0"
#define ruta_spi_tft "/dev/spidev0.1"
#define nombre_gpio "arcade"
/* el servicio arranca con el directorio de trabajo en la carpeta del proyecto.
 * marcador queda ahi: arranques y record, no en /var/lib. */
#define dir_datos "."
#define ruta_datos "marcador"
#define ruta_bootcount "bootcount"
#define largo_ruta 256
#define largo_num 48

#endif
