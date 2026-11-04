# Consola arcade - Avance 2 (FSE, UNAM FI)

Raspberry Pi 3 Model B. Probado con libgpiod **1.6** (Raspberry Pi OS Bookworm). Este avance lee **2 botones** (GPIO5, GPIO6) y **2 potenciometros** (MCP3008, canales 0 y 1)
y los muestra en una **LCD1602 I2C**. Los juegos (Dino y Pong 2 jugadores) se conectan despues sin cambiar el hardware ni los hilos: ver `include/game.h`.

## Compilar y ejecutar
```bash
./scripts/install_deps.sh        # libgpiod 1.6 o 2.x, i2c-tools, habilita I2C y SPI
./scripts/hw_check.sh            # LCD debe salir en 0x27 o 0x3F
make                             # produce build/arcade
make test                        # 30 pruebas de logica (no necesita hardware)
./build/arcade                   # LCD en 0x27
./build/arcade --lcd-addr 0x3F   # otra direccion
./build/arcade --console         # ademas imprime la pantalla en la terminal
./build/arcade --no-lcd          # solo consola (depuracion sin LCD)
```
Ctrl+C sale limpio y muestra la latencia de entrada medida. Mantener **ambos botones 3 s** apaga la Pi.

## Arranque automatico
```bash
./scripts/install_service.sh && sudo reboot
./scripts/measure_boot.sh        # requisito: < 15 s
./scripts/boot_optimize.sh       # candidatos a deshabilitar; --apply los aplica
```

Guia completa de pruebas y fallas comunes: `docs/COMO_PROBAR.md`.

## Estructura
| Archivo | Funcion |
|---|---|
| `include/config.h` | Todas las constantes (pines, periodos, direcciones, prioridades) |
| `src/buttons.c` | libgpiod 1.6 o 2, eventos de flanco, debounce de 20 ms, mide latencia |
| `src/mcp3008.c` | SPI (spidev), trama de 3 bytes de la P3 |
| `src/lcd1602.c` | HD44780 4 bits por PCF8574; cache de filas; caracteres personalizados |
| `src/filter.c` | Filtro de potenciometro y deteccion de combo (logica pura, probada) |
| `src/storage.c` | Escritura atomica con fsync (archivo temporal + rename) |
| `src/game*.c` | Interfaz de juegos + modo diagnostico |
| `src/main.c` | Hilos, senales, pantalla con recuperacion |

## Decisiones de la bitacora aplicadas en el codigo
- Una syscall por evento, hilo bloqueado en libgpiod, sin polling: `buttons.c`.
- Debounce de 20 ms: en API 2 lo hace el kernel (`BUTTON_DEBOUNCE_US`); en Bookworm (API 1.6) el mismo tiempo lo confirma `buttons.c` leyendo el pin cuando lleva 20 ms estable.
- Atomicos en vez de mutex para el estado compartido: `shared_state.h`.
- Stack de 128 KB por hilo y cero `malloc` en el ciclo principal: `thread_start` en `util.c`.
- SCHED_FIFO para entrada y ADC: `RT_PRIO_INPUT`, `RT_PRIO_ADC`.
- Datos que sobreviven a un corte con `fsync`: `storage.c`.
- Arranque directo al juego con systemd: `systemd/arcade.service`.
- Apagado controlado: combo de botones -> exit 42 -> `post_stop.sh`.

