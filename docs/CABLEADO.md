# Cableado del sistema

Raspberry Pi 3 Model B, header de 40 pines. Toda señal hacia la Pi es de 3.3 V. Conexión de los botones, las resistencias de 10 kΩ, MCP3008, potenciometros y pantalla TFT ST7796S 3.5" 480×320 por SPI.

### Tabla pin a pin

| Señal | Pin físico | BCM | Hacia dónde |
|---|---|---|---|
| 3.3 V | 17 | — | Extremo de B1, B2, P1 y P2. VDD (16) y VREF (15) del MCP3008. |
| 3.3 V | 1 | — | VCC de la TFT. |
| GND | 6, 9, 14, 20, 25 | — | GND de TFT, AGND (14) y DGND (9) del MCP3008, extremo bajo de P1 y P2, 10 kΩ de cada botón. |
| MOSI | 19 | GPIO10 | DIN del MCP3008 (11) y SDI/MOSI de la TFT. |
| MISO | 21 | GPIO9 | Solo DOUT del MCP3008 (12). |
| SCLK | 23 | GPIO11 | CLK del MCP3008 (13) y SCK de la TFT. |
| CE0 | 24 | GPIO8 | CS del MCP3008 (10). Lo baja spidev. |
| CE1 | 26 | GPIO7 | CS de la TFT. Lo baja spidev al abrir `/dev/spidev0.1`. |
| DC | 22 | GPIO25 | DC de la TFT. |
| RST | 13 | GPIO27 | RESET de la TFT. |
| Luz | 12 | GPIO18 | BL de la TFT. Salida GPIO, activo en alto. |
| B1 | 29 | GPIO5 | Botón: 3.3 V, contacto, GPIO. 10 kΩ de ese nodo a GND. Activo en alto. |
| B2 | 31 | GPIO6 | Igual que B1. |
| P1 | — | — | Un extremo a 3.3 V, el otro a GND, cursor a CH0 del MCP3008 (pin 1). |
| P2 | — | — | Igual, cursor a CH1 (pin 2). |

CH2 a CH7 del MCP3008 sin conectar. 

## TFT ST7796S en el mismo SPI0

El MCP3008 sigue en CE0 (`/dev/spidev0.0`, GPIO8). La TFT usa el otro chip select del mismo controlador: CE1 (`/dev/spidev0.1`, GPIO7). MOSI y SCLK son comunes. 

### Serigrafía de este módulo (9 pines)

| Serigrafía | Qué es | Pin de la Pi |
|---|---|---|
| GND | tierra | 6, 9, 14… |
| VIN | alimentación | 1 (3.3 V) |
| SCL | reloj SPI, no I2C | 23, GPIO11 |
| SDA | MOSI, no I2C | 19, GPIO10 |
| RST | reset | 13, GPIO27 |
| DC | dato o comando | 22, GPIO25 |
| CS | chip select | 26, GPIO7 |
| BL | luz de fondo | 12, GPIO18 |
| SDA-0 | salida SPI del panel | sin cable |

### Luz de fondo

BL del módulo a **GPIO18, pin 12**. El programa lo pone en alto para encender, si queda al revés (encendida cuando el programa la cree apagada) cambia `luz_encendida = 1` a 0 en [include/constantes.h](../include/constantes.h). 

### Si la imagen sale mal

En [include/constantes.h](../include/constantes.h):

- Volteada o con los ejes cambiados: `madctl_paisaje` está en `0x28` (MV|BGR). Prueba `0xE8` o `0x48`.
- Colores en negativo: `invertir_color` está en 1. Pásalo a 0.

