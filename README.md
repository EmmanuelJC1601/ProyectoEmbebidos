# Consola arcade Versión 3.0 (entrega 3)

Raspberry Pi 3 Model B Rev 1.2, Raspberry Pi OS 12 (bookworm), libgpiod 1.6.
Un proceso: botones, MCP3008 y TFT ST7796S. Juegos: dino y pong.

## Qué hay

| Ruta | Para qué |
|---|---|
| [src/](src) | Firmware. El binario de la placa es `build/arcade`. |
| [docs/CABLEADO.md](docs/CABLEADO.md) | Cómo conectar la ST7796S sin soltar el MCP3008. |
| [docs/DECISIONES.md](docs/DECISIONES.md) | Cifras nuevas, bitácora y lo pendiente. |
| [systemd/arcade.service](systemd/arcade.service) | Arranque solo, reinicio a 1 s, código 42 apaga. |
| [scripts/](scripts) | Dependencias, revisión, instalación y parada. |

## En la Raspberry

```sh
sudo sh scripts/dependencias.sh
sh scripts/revisar.sh
make
sudo sh scripts/instalar.sh
sudo systemctl start arcade
journalctl -u arcade -f
```

`revisar.sh` tiene que ver `/dev/spidev0.0` y `/dev/spidev0.1`. Con `dtparam=spi=on` (entrega 2) el kernel crea los dos. 

## Controles

Los dos botones a la vez, 3 s, apagan. 

| Dónde | B1 (GPIO5) | B2 (GPIO6) | P1 (CH0) | P2 (CH1) |
|---|---|---|---|---|
| Menú | Siguiente | Entrar | — | — |
| Dino | Salto / reinicio | Menú | — | — |
| Pong | Sacar | Menú | Paleta izquierda | Paleta derecha |
| Prueba | Se ve el nivel | Se ve el nivel. Los dos a la vez, un toque, vuelven al menú | Barra | Barra |

60 s sin tocar nada apagan la luz de fondo (GPIO18). El siguiente toque la enciende.

Arranques y récord quedan en `marcador`, dentro de esta carpeta. El servicio usa esta carpeta como directorio de trabajo. Para ponerlos en cero: `printf '0 0\n' > marcador` con el servicio parado.

## Dos fallas para la demo

1. `sudo kill -9 $(pidof arcade)`. `systemctl status arcade` muestra el reinicio al segundo. `marcador` conserva el récord y sube el arranque.
2. `printf 'basura\n' > marcador` y `sudo systemctl restart arcade`. El archivo no se interpreta. El juego arranca igual y vuelve a escribir dos enteros. Un corte a mitad de esa escritura deja el archivo viejo o el nuevo: el `rename` va después del `fsync`.

Desconectar el SPI casi nunca devuelve error en el `ioctl`.

## Cifras

La latencia y los fps salen en el journal al terminar el proceso (`scripts/medir_arranque.sh` dice dónde). 
