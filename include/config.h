/* config.h - Todas las constantes del sistema en un solo lugar.
 * Cambiar un pin, un periodo o una direccion I2C se hace aqui y en ningun otro archivo. */
#ifndef CONFIG_H
#define CONFIG_H

#define NS_PER_SEC            1000000000LL
#define NS_PER_MS             1000000LL

/* ---- Botones (libgpiod, circuito de la P7: 3.3 V -> boton -> GPIO, 10k a GND, activo en alto) ---- */
#define GPIO_CHIP_PATH        "/dev/gpiochip0"
#define GPIO_CONSUMER         "arcade"
#define BUTTON_COUNT          2u
#define PIN_BTN_A             5u            /* BCM, pin fisico 29 */
#define PIN_BTN_B             6u            /* BCM, pin fisico 31 */
#define BUTTON_DEBOUNCE_US    20000u        /* Requisito: filtro de 20 ms (lo hace el kernel) */
#define BUTTON_WAIT_NS        (50 * NS_PER_MS) /* Despierta cada 50 ms para revisar bandera de salida */
#define BUTTON_EVENT_BUF      16u
#define SHUTDOWN_HOLD_MS      3000          /* Ambos botones 3 s = apagado controlado */

/* ---- ADC MCP3008 por SPI (P3) ---- */
#define SPI_DEVICE_PATH       "/dev/spidev0.0"
#define SPI_SPEED_HZ          1000000u
#define SPI_BITS_PER_WORD     8u
#define ADC_CHANNELS          8u
#define ADC_MAX               1023u
#define POT_COUNT             2u
#define POT_CHANNEL_P1        0u            /* Jugador 1 / Pong paleta izquierda */
#define POT_CHANNEL_P2        1u            /* Jugador 2 / Pong paleta derecha */
#define ADC_PERIOD_MS         10            /* 100 muestras/s por canal */
#define ADC_FILTER_SHIFT      2u            /* Filtro: avanza 1/4 de la diferencia */
#define ADC_DEADBAND          2u            /* Ruido de +-2 cuentas se ignora */

/* ---- LCD1602 con mochila PCF8574 por I2C ---- */
#define I2C_BUS_PATH          "/dev/i2c-1"
#define LCD_I2C_ADDR_DEFAULT  0x27u         /* Algunas mochilas usan 0x3F: ver i2cdetect -y 1 */
#define LCD_COLS              16u
#define LCD_ROWS              2u
#define LCD_RETRY_MS          2000          /* Si la LCD falla, reintenta reiniciarla cada 2 s */

/* ---- Interfaz y tareas ---- */
#define UI_PERIOD_MS          50            /* 20 actualizaciones/s de pantalla */
#define RT_PRIO_INPUT         50            /* SCHED_FIFO: botones, tiempo critico */
#define RT_PRIO_ADC           40            /* SCHED_FIFO: potenciometros */
#define THREAD_STACK_BYTES    (128u * 1024u) /* Decision de bitacora: stack 128 KB por hilo */
#define MAX_CONSECUTIVE_ERRORS 20u          /* Errores seguidos antes de abortar un hilo */

/* ---- Persistencia (P15: ext4 con journal, escritura atomica + fsync) ---- */
#define DATA_DIR              "/var/lib/arcade"
#define BOOTCOUNT_FILE        DATA_DIR "/bootcount"
#define PATH_MAX_LEN          256u

/* ---- Codigo de salida que systemd interpreta como "apagar la Pi" (ver arcade.service) ---- */
#define EXIT_POWEROFF         42

#endif
