# Como probar el hardware (en orden; no avances si un paso falla)

0. Permisos (una sola vez) y reinicio de sesion:
   `sudo usermod -aG gpio,i2c,spi $USER` y vuelve a entrar por SSH.
1. `./scripts/hw_check.sh`
   - `/dev/i2c-1`, `/dev/spidev0.0`, `/dev/gpiochip0` existen.
   - `i2cdetect` muestra **27** o **3f** (la LCD). Si no sale nada: SDA/SCL, 5 V o I2C deshabilitado.
   - libgpiod puede ser `1.6.x` (Bookworm) o `2.x` (Trixie). Las dos sirven.
2. Botones solos, sin mi programa.
   - Bookworm (libgpiod 1.6): `gpiomon -r -f gpiochip0 5 6`
   - Trixie (libgpiod 2): `gpiomon -e both -c gpiochip0 5 6`
   Cada pulsacion debe imprimir RISING y al soltar FALLING. Ctrl+C para salir.
3. Todo en consola, sin LCD: `./build/arcade --no-lcd`
   - Gira cada perilla: solo cambia su valor (P1 o P2), de ~0 a ~1023.
   - Pulsa cada boton: `#` mientras lo mantienes y el contador sube de 1 en 1.
4. Con LCD: `./build/arcade` (o `--lcd-addr 0x3F`). Debe verse lo mismo que en el paso 3.
   Si ves cuadros negros sin texto: ajusta el potenciometro de contraste de la mochila.
5. Latencia: tras 20 pulsaciones, Ctrl+C. El log imprime `latencia de entrada: ... maxima N us`. Debe ser < 50000.
   En Bookworm el filtro de 20 ms es del programa, asi que la maxima ronda los 20000 us. Sigue siendo valida.
6. Apagado: manten ambos botones 3 s. La LCD dice "Apagando..." y la Pi se apaga
   (solo funciona via servicio systemd; ejecutado a mano termina con codigo 42).
7. Fallas: desconecta la LCD en marcha (el resto sigue), reconectala (se recupera en 2 s).
8. Arranque automatico: `./scripts/install_service.sh && sudo reboot`, sin abrir SSH:
   la LCD debe mostrar lecturas sola. Luego `./scripts/measure_boot.sh` (< 15 s).
9. Llena `docs/pruebas.md` con lo obtenido.

## Si algo falla
| Sintoma | Causa probable |
|---|---|
| `Device or resource busy` | otra instancia, `gpiomon` o el servicio activo: `sudo systemctl stop arcade` |
| `Permission denied` | falta `usermod -aG gpio,i2c,spi` y volver a entrar |
| Pot siempre 0 | cursor no llega al canal, o MISO/CS mal; siempre 1023: MISO flotando o VREF sin 3.3 V |
| Los dos pots se mueven juntos | ambos cursores en el mismo canal |
| Boton siempre `#` | falta resistencia a GND; siempre `-`: no le llega 3.3 V |
| LCD con luz pero sin texto | contraste, o direccion equivocada (0x27 vs 0x3F) |
| `SCHED_FIFO ... no disponible` | solo aviso si lo ejecutas a mano; con el servicio si tiene permiso |
