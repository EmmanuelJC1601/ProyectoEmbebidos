# Cableado de la entrega 3

Raspberry Pi 3 Model B, header de 40 pines. Toda señal hacia la Pi es de 3.3 V. Los botones, las resistencias de 10 kΩ y el MCP3008 quedan como en la entrega 2. Cambia la pantalla: sale la LCD1602 y entra la TFT ST7796S 3.5" 320×480 por SPI.

Desconecta la mochila de la LCD antes de alimentar. Usaba 5 V (pin 2), SDA (GPIO2, pin 3) y SCL (GPIO3, pin 5). El programa ya no abre `/dev/i2c-1`. Si la dejas puesta no pelea con el SPI, pero el pin 2 queda ocupado y el diagrama no coincide con el hardware.

## TFT ST7796S en el mismo SPI0

El MCP3008 sigue en CE0 (`/dev/spidev0.0`, GPIO8). La TFT usa el otro chip select del mismo controlador: CE1 (`/dev/spidev0.1`, GPIO7). MOSI y SCLK son comunes. MISO de la TFT no se conecta: el DOUT del MCP3008 ya está en GPIO9 y los dos no pueden hablar a la vez.

No pidas CE0, CE1, MOSI, MISO ni SCLK con libgpiod. Esos pines los mueve el driver `spi-bcm2835`. El programa solo pide DC, RST y la luz.

### Serigrafía de este módulo (9 pines)

El header de la placa no dice MOSI ni SCK. Dice nombres de I2C. No los lleves a los pines I2C de la Pi (pin 3 y pin 5): eso era la LCD. Los dos conectores marcados «puerto paralelo» son el flex del panel, ya soldado. No se cablean. La ficha de este módulo admite 3.3 V o 5 V en VIN; la guía del mismo pinout alimenta VIN con 5 V.

| Serigrafía | Qué es | Pin de la Pi |
|---|---|---|
| GND | tierra | 6, 9, 14… |
| VIN | alimentación | 2 (5 V) |
| SCL | reloj SPI, no I2C | 23, GPIO11 |
| SDA | MOSI, no I2C | 19, GPIO10 |
| RST | reset | 13, GPIO27 |
| DC | dato o comando | 22, GPIO25 |
| CS | chip select | 26, GPIO7 |
| BL | luz de fondo | 12, GPIO18 |
| SDA-0 | salida SPI del panel | sin cable |

### Alimentación del módulo

Mira la placa del módulo antes de elegir el pin.

- Si junto a VCC hay un regulador (AMS1117 o parecido), VCC va a **5 V, pin 2**. La lógica del panel sigue en 3.3 V; no pongas 5 V en DC, RST, CS, MOSI, SCLK ni LED.
- Si el serigrafiado dice 3V3 y no hay regulador, VCC va a **3.3 V, pin 1**. 5 V en ese módulo lo quema.

GND del módulo a un GND de la Pi (pin 6, 9, 14, 20, 25, 30, 34 o 39), el mismo que el MCP3008.

### Luz de fondo

LED o BL del módulo a **GPIO18, pin 12**. El programa lo pone en alto para encender (`luz_encendida = 1` en [include/constantes.h](../include/constantes.h)). La mayoría de estos módulos traen transistor y el pin es entrada lógica.

Si el pin LED es el ánodo de la tira y no hay transistor, no lo conectes al GPIO: la Pi no debe alimentar la tira. En ese caso únelo a 3.3 V (pin 1) y acepta que la luz no se apague a los 60 s. Si la luz queda invertida (encendida cuando el programa la cree apagada), cambia `luz_encendida` a 0. No uses PWM: el overlay de PWM modifica el sistema y aquí no hay buzzer.

### Tabla pin a pin

| Señal | Pin físico | BCM | Hacia dónde |
|---|---|---|---|
| 3.3 V | 1 y 17 | — | Extremo de B1, B2, P1 y P2. VDD (16) y VREF (15) del MCP3008. VCC de la TFT solo si el módulo es de 3.3 V. |
| 5 V | 2 | — | VCC de la TFT solo si el módulo tiene regulador. Ya no va a la LCD. |
| GND | 6, 9, 14, 20, 25 | — | GND de TFT, AGND (14) y DGND (9) del MCP3008, extremo bajo de P1 y P2, 10 kΩ de cada botón. |
| MOSI | 19 | GPIO10 | DIN del MCP3008 (11) y SDI/MOSI de la TFT. |
| MISO | 21 | GPIO9 | Solo DOUT del MCP3008 (12). MISO de la TFT sin cable. |
| SCLK | 23 | GPIO11 | CLK del MCP3008 (13) y SCK de la TFT. |
| CE0 | 24 | GPIO8 | CS del MCP3008 (10). Lo baja spidev. |
| CE1 | 26 | GPIO7 | CS de la TFT. Lo baja spidev al abrir `/dev/spidev0.1`. |
| DC | 22 | GPIO25 | DC, RS o A0 de la TFT. |
| RST | 13 | GPIO27 | RESET de la TFT. |
| Luz | 12 | GPIO18 | LED o BL de la TFT. Salida GPIO, activo en alto. |
| B1 | 29 | GPIO5 | Botón: 3.3 V, contacto, GPIO. 10 kΩ de ese nodo a GND. Activo en alto. |
| B2 | 31 | GPIO6 | Igual que B1. |
| P1 | — | — | Un extremo a 3.3 V, el otro a GND, cursor a CH0 del MCP3008 (pin 1). |
| P2 | — | — | Igual, cursor a CH1 (pin 2). |

CH2 a CH7 del MCP3008 siguen sin conectar. GPIO2 y GPIO3 quedan libres.

## Si la imagen sale mal

No cambies cables para probar orientación. En [include/constantes.h](../include/constantes.h):

- Volteada o con los ejes cambiados: `madctl_paisaje` está en `0x28` (MV|BGR). Prueba `0xE8` o `0x48`.
- Colores en negativo: `invertir_color` está en 1. Pásalo a 0.

El reloj pedido es 32 MHz. El divisor del SPI de la Pi 3 lo redondea (a menudo cerca de 31 o 25 MHz). No hace falta tocarlo para la primera prueba.
