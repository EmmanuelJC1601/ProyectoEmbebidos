# Decisiones de la entrega 3

Lo planeado en las entregas 1 y 2 se construyó, menos lo que esta entrega no puede tocar: el sistema ya instalado en la tarjeta. Cada diferencia queda con la causa y la cifra nueva. Los números de diseño viven en [include/constantes.h](../include/constantes.h). No hay constantes sueltas en el juego.

La latencia y los fps los imprime el proceso (`journalctl -u arcade`). 

## Qué cambió y por qué

| Tema | Entrega 1 o 2 | Ahora | Por qué |
|---|---|---|---|
| Placa | Entrega 1: Pi 4, BCM2711. Entrega 2: Pi 3. | Pi 3 Model B Rev 1.2, BCM2837. | La placa real es la de la entrega 2. El PWM del buzzer se había escrito para el BCM2711. |
| Pantalla | LCD1602 + PCF8574, I2C `0x27`, texto de 16 columnas, refresco 50 ms. | ST7796S, SPI0 CE1, paisaje 480×320, RGB565, paso de 33 ms. | El módulo TFT reemplaza a la LCD. 16×2 no porque no dibuja bien pong ni dino. |
| Driver de video | SPI o HDMI, y en la entrega 1 un framebuffer de 1.5 MB. | Driver de usuario sobre `/dev/spidev0.1`. Framebuffer estático de 300 KiB. | Un overlay `fbtft` o un `dtoverlay` escribe el sistema. 480×320×2 = 307200 bytes = 300 KiB. |
| Bus | SPI solo para el ADC. I2C para la LCD. | SPI0 compartido: CE0 el ADC a 1 MHz, CE1 la TFT a 32 MHz pedidos. Mutex con herencia de prioridad. | Un solo controlador. El mutex está en el código porque la bitácora pide ver la sección crítica; el kernel también serializa el controlador. |
| Tope de transferencia | No estaba. | Bloques de 4096 bytes. | `spidev.bufsiz` de fábrica es 4096. Subirlo sería un parámetro del kernel. Entre bloque y bloque se suelta el mutex para que el ADC no espere el cuadro. |
| Audio | Hilo PWM, GPIO18 ALT5, beep de 1 kHz. | No hay hilo de audio. GPIO18 es la luz de la TFT, en GPIO, activo en alto. | No hay buzzer en el hardware de esta entrega. El overlay de PWM cambia el sistema. |
| Tareas | 4 hilos, pila 128 KiB × 4. | 1 proceso, 3 tareas. Pila de 128 KiB solo en los dos hilos de tiempo real. El principal no fija la pila. | Se quitó el audio. El dibujo se queda en el hilo principal: un hilo de render aparte volvería a compartir las coordenadas. |
| Estado compartido | Mutex o atómico sobre el botón y las coordenadas del personaje. | Atómicos de un solo escritor: nivel, pulsos, potenciómetros, récord, banderas. Las coordenadas son estáticas del hilo principal. | El hilo del juego no espera un mutex para leer un botón. Una coordenada que solo escribe un hilo no es recurso compartido. |
| Debounce | 20 ms en software. libgpiod 1.6 no filtra en el kernel. | Igual. La latencia publicada incluye esos 20 ms y el requisito sigue siendo < 50 ms. | El pin tiene que quedarse quieto 20 ms y una lectura confirma el nivel. |
| ADC | 1 MHz, modo 0, trama de 3 bytes, muestra cada 10 ms, filtro ±2 y paso 1/4. | Mediana de 5 muestras y banda ±16, luego el mismo paso de 1/4. | El ±2 de la entrega 2 dejaba pasar el ruido del cable: la paleta se movía sin tocar el potenciometro. Un pico de una muestra ya no entra. La muestra sigue siendo cada 10 ms. |
| Datos que sobreviven | Récord, volumen y dificultad. Partición `/` de solo lectura y `/data` aparte. Pérdida 0 s de récord. | Solo arranques y récord del dino, archivo `marcador` en la carpeta del proyecto. El servicio tiene esa carpeta como directorio de trabajo. | Volumen y dificultad eran del buzzer y no hay perilla de eso. Otra partición o root de solo lectura reformatea o remonta la tarjeta que ya usamos. |
| Escritura | `fsync` + `rename`. | Temporal, `fsync` del archivo, `rename`, `fsync` del directorio. | Sin el `fsync` del directorio el nombre nuevo puede quedar solo en caché. Un corte deja el archivo viejo o el nuevo entero. |
| Apagado | Dos botones 3 s, salida 42, `poweroff`. | Igual. Además, 60 s sin entrada apagan solo la luz. | El estado de inactividad de la bitácora no puede atenuar por PWM. Apagar el GPIO sí. |
| Arranque ≤ 15 s | Imagen mínima, sin red ni escritorio. | El servicio arranca en `multi-user.target`, después de `local-fs.target`, sin esperar red. | `scripts/medir_arranque.sh` imprime lo que `systemd-analyze` diga en la Pi. |
| Consumo ≤ 15 W | Presupuesto, sin medición. | No una medición. Pi 3 con este proceso (sin ethernet a plena carga ni GPU) más la luz del panel y el MCP3008 (microamperios) queda debajo de 15 W. Los dos potenciómetros de 10 kΩ a 3.3 V son ~0.7 mA. | Hay que medirlo con un medidor USB para poder afirmar. |
| Memoria | Reserva al arrancar, 0 malloc en partida. FB 1.5 MB, sprites ≤ 0.5 MB, 8 obstáculos, pila 128 KiB × 4. | FB 300 KiB estático, fuente 5×7 dentro del binario (0 archivos de imagen), 8 obstáculos, bloque SPI 4 KiB, 96 zonas (1536 bytes), pila 128 KiB × 2. `mlockall` solo si el servicio dio `CAP_IPC_LOCK`. | Misma estrategia, menos reserva: no hay audio y el panel es RGB565, no un buffer de 1.5 MB. |
| Reintento | LCD: 2000 ms si el write falla. | Igual para la TFT. 20 fallos seguidos del ADC o de los botones: el proceso sale 1 y systemd lo levanta a 1 s. La TFT caída no mata el proceso: se reabre cada 2 s. | Un panel que tarda en arrancar no debe tumbar los botones. |

## Bitácora: clases 8, 9 y 11

### Clase 8. Tareas y prioridades

Sigue habiendo planificación preemptiva en la entrada. Si no, un volcado SPI se come los 50 ms. Lo que cambió es que ya no hay cuarta tarea.

| Tarea | Prioridad | Dónde | Diferencia |
|---|---|---|---|
| Botones. Se bloquea en `gpiod_line_event_wait_bulk`. El kernel despierta con el flanco. Debounce 20 ms. Combo de 3 s en este hilo. | `SCHED_FIFO` 50 | [src/hw.c](../src/hw.c) `tarea_botones` (línea 24), [src/botones.c](../src/botones.c) espera (línea 106) y debounce (línea 20), [src/util.c](../src/util.c) `iniciar_hilo` (línea 41), [include/constantes.h](../include/constantes.h) `prio_botones` (línea 71) | Igual que lo planeado. |
| ADC. Periodo absoluto de 10 ms. Publica el potenciómetro ya filtrado. | `SCHED_FIFO` 40 | [src/hw.c](../src/hw.c) `tarea_adc` (línea 55), [include/constantes.h](../include/constantes.h) `ms_adc` (línea 40) | Igual. |
| Juego y dibujo. Paso de 33 ms. CFS, prioridad normal. | Normal | [src/principal.c](../src/principal.c) bucle (línea 153) y comentario de cabecera (líneas 1-5) | La entrega 1 separaba lógica y render. Van en el mismo hilo porque las coordenadas no se comparten. |
| Audio PWM | Baja | — | Eliminada. No hay buzzer y GPIO18 es la luz. Causa en la tabla de arriba. |

Sin `CAP_SYS_NICE` el hilo se crea igual en CFS.

### Clase 9. Recurso compartido y sincronización

Lo planeado mezclaba el botón y las coordenadas en un mutex. Construido:

- Cada dato compartido tiene un escritor. Botones, pulsos y latencia: el hilo de entrada. Potenciómetros: el hilo del ADC. Récord, arranques, salir, apagar, fallo: el hilo principal, salvo `salir` y `fallo`, que también los ponen los hilos de entrada si se pasan de `errores_tope`. Esos son `atomic_*`. Declarados en [include/estado.h](../include/estado.h) líneas 1-26.
- Las coordenadas del dino y de la pelota no salen de su archivo. No hay mutex del personaje.
- El recurso que sí es de dos hilos es el bus SPI. Mutex POSIX con `PTHREAD_PRIO_INHERIT`: si el juego (CFS) tiene el bus y el ADC (FIFO 40) lo pide, el juego hereda y suelta antes. [include/spi.h](../include/spi.h) líneas 1-7, [src/spi.c](../src/spi.c) `spi_iniciar` línea 17. Se suelta entre bloques de 4 KiB en [src/pantalla.c](../src/pantalla.c) `mandar_rect` línea 131.

### Clase 11. IPC

No hay dos procesos del juego. Un proceso, tres hilos, memoria común. Cabecera de [src/principal.c](../src/principal.c) líneas 1-5. systemd no es IPC del juego: es quien lo arranca, lo reinicia si sale mal y apaga la máquina si sale 42.

## Clase 20. volatile

El único `volatile` del código es `static volatile sig_atomic_t senal_parar` en [src/principal.c](../src/principal.c) línea 23. Lo escribe el manejador de `SIGINT`/`SIGTERM` y lo lee el bucle. `volatile` no hace atómica una escritura de juego ni ordena dos núcleos: por eso el estado compartido es `atomic_*` y no `volatile`. No hay `mmap` de registros, así que no hay `volatile` de periférico.

## Clase 21. Driver de cada sensor y actuador

Ninguno se programa por `mmap`. La temporización que el proceso no puede sostener la hace el periférico a través de su driver de kernel, y el proceso hace una syscall por evento o por muestra.

| Pieza | Quién genera el tiempo | Qué abre el programa | Evidencia en código |
|---|---|---|---|
| Botones GPIO5 y GPIO6 | Interrupción del GPIO. Driver `pinctrl-bcm2835`, nodo `/dev/gpiochip0`. libgpiod 1.6. | Una syscall por flanco: `gpiod_line_event_wait_bulk`. | [include/botones.h](../include/botones.h) líneas 1-8 |
| MCP3008 | Reloj SPI del hardware, 1 MHz, modo 0. Driver `spi-bcm2835`. No hay driver del chip. | `/dev/spidev0.0`. Un `ioctl` por canal. Canal ≥ 8 devuelve `EINVAL` sin `ioctl`. | [include/adc.h](../include/adc.h) líneas 1-5, [src/adc.c](../src/adc.c) `adc_abrir` línea 10 |
| TFT ST7796S | Reloj SPI del mismo controlador, otro chip select, 32 MHz pedidos. No se carga `fbtft`. | `/dev/spidev0.1`. DC, RST y luz por libgpiod. | [include/pantalla.h](../include/pantalla.h) líneas 1-5, [src/pantalla.c](../src/pantalla.c) `pantalla_abrir` línea 278 |
| Luz de fondo | No es PWM. Es un nivel GPIO. | GPIO18, salida. | [include/constantes.h](../include/constantes.h) `pin_luz` línea 57 |
| Buzzer | No existe en este hardware. | — | La tarea de la entrega 1 no se compiló. |

En la Pi, `sh scripts/revisar.sh` imprime los nodos y la versión de libgpiod. Eso es la evidencia de terminal: tienen que aparecer `/dev/gpiochip0`, `/dev/spidev0.0` y `/dev/spidev0.1`, y libgpiod 1.6.x. El listado de la entrega 2 solo mencionaba `spidev0.0` porque la LCD iba por I2C.

## Robustez (dos fallas)

1. **Proceso muerto.** `kill -9` no pasa por el `fsync`. systemd ve muerte por señal, `Restart=on-failure`, `RestartSec=1s`. Al volver a abrir, el arranque suma 1 y el récord se lee del archivo que ya estaba en disco. Código: [systemd/arcade.service](../systemd/arcade.service), carga en [src/principal.c](../src/principal.c) `cargar` línea 47.
2. **Archivo dañado o directorio sin escritura.** `datos` con texto que no son dos enteros se rechaza ([src/archivo.c](../src/archivo.c) `cargar_datos`). El juego sigue con récord 0. Si el `open` del temporal falla, no hay `rename` y el archivo anterior no se toca. Un corte durante el `write` borra el temporal en el siguiente arranque; el nombre definitivo solo cambia después del `fsync`.

La TFT que no abre no es una de esas dos: el proceso sigue y `hw_recuperar` reintenta cada 2 s ([src/hw.c](../src/hw.c) línea 172). Desenchufar el SPI no hace fallar el `ioctl`, porque el controlador no recibe acuse.

## Medición contra la entrega 1

El requisito que el propio programa compara es el de tiempo:

- Entrada: latencia del flanco del kernel hasta el nivel publicado, tope `us_latencia_max` = 50000. Incluye los 20000 µs de debounce. Al salir imprime última, máxima y si cabe. [src/botones.c](../src/botones.c) `anotar` línea 9, [src/principal.c](../src/principal.c) `resumen` línea 70.
- Cuadro: fps del último segundo contra `fps_minimo` = 30, y el peor cuadro en microsegundos. 

La lógica de trama, filtro, combo de 3 s, archivo y zonas se probó antes. El zip de la placa ya no trae ese programa de pruebas ni el lector de potenciómetros. La pantalla PRUEBA del menú sigue.

## Pendiente para el avance 4

- Confirmar en la Pi `/dev/spidev0.1`. Si la imagen sale espejada, una constante (`madctl_paisaje` o `invertir_color`), no un overlay.
- Si el fps medido en el panel baja de 30, el primer cambio es `hz_tft` (probar 48000000). Un cuadro sucio grande se acerca al presupuesto de 33 ms a 32 MHz.
- Arranque < 15 s y la partición `/data` con root de solo lectura. Causa de no haberlos hecho: esta entrega no podía cambiar el sistema.
- Buildroot queda como idea de producto de la entrega 2. No es el sistema de la demo.

## Uso de IA

Se usó un asistente para reconstruir el firmware de la entrega 3 (TFT ST7796S, juegos, servicio y esta bitácora) a partir de las entregas 1 y 2 y del código del módulo de botones, ADC y LCD. Las cifras de la placa (latencia, fps, arranque, consumo) no las produjo el asistente: se midieron en la Raspberry.
