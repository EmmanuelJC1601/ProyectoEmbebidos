# Consola arcade

Raspberry Pi 3 Model B. Raspberry Pi OS Lite de 64 bits, Debian 13 (trixie), libgpiod 2.
Un proceso: botones, MCP3008 y TFT ST7796S. Juegos: dino y pong.

## Qué hay

| Ruta | Para qué |
|---|---|
| [src/](src) | Firmware. El binario de la placa es `build/arcade`. |
| [docs/CABLEADO.md](docs/CABLEADO.md) | Cómo conectar la ST7796S sin soltar el MCP3008. |
| [docs/DECISIONES.md](docs/DECISIONES.md) | Cifras nuevas, bitácora y lo pendiente. |
| [systemd/arcade.service](systemd/arcade.service) | Arranque solo, reinicio a 1 s, código 42 apaga. |
| [scripts/](scripts) | Dependencias, revisión, instalación y parada. |

## Cómo probar

En Raspberry Pi Imager, para la Pi 3, selecciona **Raspberry Pi OS Lite (64-bit)**. Versión Trixie y sin escritorio.

En la personalización: SSH activado.

Conectate por SSH a la raspberry. Al primer arranque ejecuta:

```sh
sudo apt update
sudo raspi-config nonint do_spi 0
sudo usermod -aG spi,gpio "$USER"
sudo reboot
```

Conectate nuevamente a la raspberry, copia este directorio a la placa y sigue con la siguiente sección. 

## En la Raspberry
Verifica las dependencias y apaga los servicios que no se necesitan:

```sh
sudo sh scripts/dependencias.sh
sh scripts/revisar.sh
make
sudo sh scripts/instalar.sh
sudo sh scripts/aligerar_arranque.sh
sudo reboot
```

Con `revisar.sh` se tiene que ver `/dev/gpiochip0`, `/dev/spidev0.0` y `/dev/spidev0.1`.

Para ver el registro usa: `journalctl -u arcade -f`.

Para ver la cifra del arranque, después de ese reinicio: `sh scripts/medir_arranque.sh`.

## Controles

Los dos botones a la vez, 3 s, apagan la Pi.

| Dónde | B1 (GPIO5) | B2 (GPIO6) | P1 (CH0) | P2 (CH1) |
|---|---|---|---|---|
| Menú | Siguiente | Entrar | — | — |
| Dino | Salto / reinicio | Menú | — | — |
| Pong | Saca si saca P1 | Saca si saca P2. Los dos a la vez vuelven al menú | Paleta izquierda | Paleta derecha |
| Prueba | Se ve el nivel | Se ve el nivel. Los dos a la vez, un toque, vuelven al menú | Barra | Barra |

60 s sin tocar nada apagan la luz de fondo (GPIO18). El siguiente toque la enciende.

Arranques y récord quedan en `marcador`, en esta carpeta. Para ponerlos en cero, con el servicio parado: `printf '0 0\n' > marcador`.

## Dos fallas para la demo

1. `sudo kill -9 $(pidof arcade)`. systemd lo levanta al segundo. El récord que ya estaba en `marcador` se queda.
2. Con el servicio parado, `printf 'basura\n' > marcador` y `sudo systemctl start arcade`. El archivo no se interpreta, el juego abre igual y la siguiente escritura vuelve a dejar dos enteros.


## Cifras

La latencia y los fps salen en el journal al terminar el proceso. `sudo systemctl stop arcade` y luego `journalctl -u arcade -n 40 --no-pager`.