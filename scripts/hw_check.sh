#!/bin/bash
# Verifica el hardware antes de arrancar el programa.
echo "== placa y sistema =="; tr -d '\0' < /proc/device-tree/model; echo; uname -m; grep PRETTY_NAME /etc/os-release
echo "== dispositivos =="; ls -l /dev/i2c-1 /dev/spidev0.0 /dev/gpiochip0 2>&1
echo "== grupos de $USER (deben incluir gpio i2c spi) =="; groups
echo "== libgpiod =="; pkg-config --modversion libgpiod 2>&1
echo "== I2C (la LCD debe aparecer en 0x27 o 0x3F) =="; i2cdetect -y 1
echo "== GPIO 5 y 6 (botones) =="; gpioinfo -c gpiochip0 5 6 2>/dev/null || gpioinfo gpiochip0 | grep -E "line +(5|6):"
echo "== servicio =="; systemctl is-active arcade.service 2>&1
