# Decisiones

Consola en una Raspberry Pi 3 Model B Rev 1.2 con Raspberry Pi OS Lite de 64 bits, Debian 13 (Trixie) y libgpiod 2. Un proceso lee dos botones y dos potenciómetros, dibuja en una TFT ST7796S y arranca solo con systemd.

Las constantes con nombre están en [include/constantes.h](../include/constantes.h). 

La latencia, los cuadros, la CPU, la memoria y el arranque se midieron en la Pi. 

## Mediciones

| Requisito | Criterio | Resultado |
|---|---|---|
| Entrada | Menos de 50 ms | 20.0 ms la última, 20.1 ms la máxima. Incluye los 20 ms de antirrebote. |
| Cuadros | 30 fps o más | 31 fps con el dinosaurio en marcha. Peor trabajo del segundo: 16.5 ms. |
| Arranque | Menos de 15 s hasta el menú | 16.6 s hasta que `arcade.service` queda activo (núcleo 5.682 s, servicio a los 10.952 s de espacio de usuario). El arranque completo del sistema fue 20.5 s. |
| Consumo | 15 W o menos | No medido. Presupuesto de diseño: 2.76 W a 5 V. |
| CPU y memoria | Sin tope numérico en el diseño | Con el menú en pantalla: 1.5 % de CPU, 2840 KB el proceso, 134 Mi de 905 Mi en la Pi. |

La latencia y los fps los imprime el proceso al salir. Se leen con `journalctl -u arcade -n 40 --no-pager` después de `systemctl stop arcade`. El arranque lo imprime `scripts/medir_arranque.sh`.

## Qué cambió respecto al diseño inicial

| Tema | Diseño inicial | Sistema actual | Causa |
|---|---|---|---|
| Placa | Primero una Pi 4 (BCM2711); después la Pi 3. | Pi 3 Model B Rev 1.2, BCM2837. | Es la placa del prototipo. |
| Pantalla | LCD1602 con PCF8574, I2C `0x27`, 16 columnas, refresco de 50 ms. | ST7796S en SPI0 CE1, 480×320, RGB565, paso de 33 ms. | 16×2 no alcanza para los dos juegos. |
| Video | SPI o HDMI, con un framebuffer estimado en 1.5 MB. | Dibujo en espacio de usuario sobre `/dev/spidev0.1`. Framebuffer estático de 300 KiB. | Un overlay de framebuffer modifica el sistema. 480×320×2 = 307200 bytes. |
| Bus | SPI solo para el conversor. I2C para la LCD. | SPI0 compartido. CE0: MCP3008 a 1 MHz. CE1: TFT a 32 MHz. Mutex con herencia de prioridad. | Hay un solo controlador y dos relojes. Entre bloques de 4 KiB se suelta el mutex para que una muestra no espere el cuadro. |
| Cuadro completo | 30 fps por SPI o HDMI, sin presupuesto de bytes. | Un cuadro entero a 32 MHz son unos 77 ms de bits, cerca de 13 fps. No se manda. | Se mandan zonas, con tope de 96. El dinosaurio medido quedó en 32374 bytes. |
| Tamaño de transferencia | No estaba definido. | Bloques de 4096 bytes. | Es el `spidev.bufsiz` de fábrica. Subirlo sería un parámetro del núcleo. |
| Audio | Hilo PWM en GPIO18, tono de 1 kHz. | No hay hilo de audio. GPIO18 es la luz, en nivel alto. | El hardware no tiene audio. Un overlay de PWM escribe el sistema. |
| Tareas | Cuatro hilos, pila de 128 KiB cada uno. | Un proceso, tres hilos. Pila de 128 KiB solo en los dos de tiempo real. | Se eliminó el audio. El dibujo se quedó en el hilo principal: las coordenadas las escribe solo ese hilo. |
| Estado compartido | Mutex sobre el botón y las coordenadas. | Atómicos de un solo escritor. Las coordenadas no salen del hilo principal. | El juego lee un botón sin tomar un mutex. Una coordenada con un solo escritor no es un recurso compartido. |
| Antirrebote | 20 ms en el programa. | Igual. libgpiod 2 no lo filtra. | El nivel se publica cuando el pin lleva 20 ms quieto y una lectura lo confirma. Por eso la latencia medida sale en 20 ms. |
| Conversor | 1 MHz, modo 0, tres bytes, muestra cada 10 ms, banda de ±2 y paso de 1/4. | Mediana de cinco muestras, banda de ±16, y después el mismo paso. | La banda de ±2 dejaba pasar el ruido del cable y la paleta se movía sola. La muestra sigue cada 10 ms. |
| Datos persistentes | Récord, volumen y dificultad. Raíz de solo lectura y partición `/data`. Pérdida de 0 s. | Arranques y récord del dinosaurio, en `marcador`, dentro del directorio de trabajo del servicio. | Volumen y dificultad eran de otra tarea. Reparticionar la tarjeta del prototipo no aporta al juego y pone en riesgo la demostración. |
| Escritura | `fsync` y `rename`. | Temporal, `fsync` del archivo, `rename`, `fsync` del directorio. | Sin el último, el nombre nuevo puede quedarse solo en caché. |
| Apagado | Los dos botones durante 3 s, salida 42, `poweroff`. | Igual. A los 60 s sin entrada se apaga solo la luz. | No hay PWM para atenuar. Bajar el GPIO sí se puede. |
| Arranque | Imagen mínima, menos de 15 s, sin red ni escritorio. | Raspberry Pi OS Lite, sin escritorio. `scripts/aligerar_arranque.sh` quita servicios que el juego no usa. El servicio arranca en `multi-user.target`, después de `local-fs.target`, sin esperar la red. | No se construyó una imagen con Buildroot. En Trixie, `netplan generate` se quedaba unos 30 s esperando un reload de NetworkManager; el script evita esa llamada. Aun así el menú sale a los 16.6 s. |
| Consumo | Presupuesto, sin medición, tope 15 W. | Sigue sin medición. Presupuesto: 2.76 W. | Pi 3 en este proceso, 2.00 W. TFT con luz, 0.75 W. MCP3008, dos potenciómetros de 10 kΩ y dos botones, unos 0.01 W entre los tres. |
| Memoria | Reserva al arrancar, sin `malloc` durante la partida. Framebuffer de 1.5 MB, ocho obstáculos, pila de 128 KiB por cuatro hilos. | Framebuffer de 300 KiB, fuente en el binario, ocho obstáculos, bloque SPI de 4 KiB, 96 zonas. `mlockall` solo si el servicio dio `CAP_IPC_LOCK`. | Misma idea, menos reserva: no hay audio y el color es RGB565. Medido: 2840 KB. |
| Paso del juego | 50 ms, el de la LCD. | 33 ms fijos (`ms_cuadro`). La física no usa un dt variable. | 1000/33 queda en 30 cuadros. Un dt variable haría que un cuadro largo atraviese la paleta. |
| Reintento | 2000 ms si la escritura a la LCD falla. | Igual para la TFT. Veinte fallos seguidos del conversor o de los botones: el proceso sale con 1 y systemd lo levanta al segundo. Una TFT que no abre no mata el proceso. | Un panel que tarda en encender no debe tumbar la entrada. |

## Planificación

La entrada es preemptiva. Si el volcado del SPI no se puede interrumpir, un cuadro largo se come los 50 ms.

| Hilo | Prioridad | Qué hace |
|---|---|---|
| Botones | `SCHED_FIFO` 50 | Se bloquea en el flanco del GPIO. El núcleo lo despierta. Antirrebote de 20 ms. El combo de 3 s para apagar vive aquí, para que un cuadro largo no retrase el `poweroff`. |
| Conversor | `SCHED_FIFO` 40 | Periodo absoluto de 10 ms. Publica la posición ya filtrada. |
| Juego y dibujo | Planificador normal | Paso de 33 ms. Las coordenadas no se comparten con otro hilo. |


## Sincronización

Cada dato compartido tiene un escritor. Los botones, los pulsos y la latencia los escribe el hilo de entrada. Los potenciómetros, el del conversor. El récord, los arranques y el pedido de apagado, el hilo principal. `salir` y `fallo` también los pueden poner los hilos de entrada al pasar de `errores_tope`. Están declarados como `atomic_*` en [include/estado.h](../include/estado.h). El juego lee un botón sin esperar.

Las coordenadas del dinosaurio y de la pelota no salen de su archivo.

El recurso que sí usan dos hilos es el bus SPI. El mutex es POSIX, con `PTHREAD_PRIO_INHERIT`. Si el juego tiene el bus y el conversor lo pide, el juego hereda la prioridad 40 y suelta. La creación está en [src/spi.c](../src/spi.c). Se suelta entre bloques de 4 KiB en [src/pantalla.c](../src/pantalla.c). 

## Procesos
Un proceso, tres hilos, la misma memoria. systemd lo arranca, lo reinicia si sale mal y apaga la Pi si el código de salida es 42. 

## `volatile`

El único `volatile` del árbol es `senal_parar`, de tipo `sig_atomic_t`, en [src/principal.c](../src/principal.c). Lo escribe el manejador de `SIGINT` y `SIGTERM`. Lo lee el bucle. `volatile` no hace atómica una escritura ni ordena dos núcleos: el estado compartido es `atomic_*`. No hay `mmap` de registros, así que no hay `volatile` de periférico.

## Controladores

| Pieza | Quién genera el tiempo | Qué abre el programa |
|---|---|---|
| Botones, GPIO5 y GPIO6 | El flanco del GPIO. Nodo `/dev/gpiochip0`, libgpiod 2. | Una espera por flanco. No es un bucle preguntando el pin. |
| MCP3008 | El reloj SPI, 1 MHz, modo 0. Driver `spi-bcm2835`. No hay driver del chip. | `/dev/spidev0.0`. Un `ioctl` por canal. |
| TFT ST7796S | El mismo controlador, otro chip select, 32 MHz pedidos. No se carga `fbtft`. | `/dev/spidev0.1`. DC en GPIO25, reset en GPIO27, luz en GPIO18. |
| Luz | No es PWM. Es un nivel. | GPIO18, salida, activo en alto. |

`sh scripts/revisar.sh` tiene que mostrar `/dev/gpiochip0`, `/dev/spidev0.0`, `/dev/spidev0.1` y libgpiod 2.x. 

## Persistencia y fallas

`marcador` guarda dos enteros: arranques y récord del dinosaurio. El récord se escribe cuando la partida termina y supera al anterior. La escritura va a un temporal, hace `fsync`, renombra y sincroniza el directorio. Está en [src/archivo.c](../src/archivo.c).

Eso fija qué sobrevive a un corte de corriente. Si el dinosaurio sigue vivo, los puntos de esa partida están en RAM y se pierden. Si la escritura ya terminó, el récord queda. Si el corte cae a mitad, el archivo anterior sigue siendo el válido, porque el nuevo no lo reemplaza hasta el `rename`.

Dos fallas provocadas a propósito:

1. **Proceso muerto.** `kill -9` no pasa por el `fsync`. systemd ve la señal, `Restart=on-failure` y `RestartSec=1s`. 
2. **Archivo dañado.** Con el servicio parado se escribió `basura` en `marcador`. El programa no interpreta esa línea, abre igual y la siguiente escritura deja otra vez dos enteros. El récord de esa copia se pierde.

## Límites

- El menú no aparece en menos de 15 s. Medido: 16.6 s hasta el servicio, 20.5 s el sistema completo. Bajar de eso implica otra imagen.
- El consumo no está medido. 2.76 W es la suma de las hojas de datos y del consumo típico de la Pi 3 con un solo proceso, no una lectura de la fuente.
- No hay partición de datos ni raíz de solo lectura. Un archivo dañado pierde el récord de esa copia.
- No hay imagen de Buildroot ni de Yocto. Para el prototipo pesaba poder instalar, medir y corregir. 

