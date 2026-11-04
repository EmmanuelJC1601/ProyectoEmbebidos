#!/bin/bash
# Instala dependencias de compilacion. libgpiod debe ser 2.x (Raspberry Pi OS Trixie).
set -euo pipefail
sudo apt update
sudo apt install -y build-essential pkg-config libgpiod-dev gpiod i2c-tools
ver=$(pkg-config --modversion libgpiod)
echo "libgpiod instalada: $ver"
case "$ver" in
    2.*) echo "OK: version 2.x" ;;
    *)   echo "ERROR: este proyecto usa la API 2.x de libgpiod y tienes $ver (usa Trixie o compila libgpiod 2.x)"; exit 1 ;;
esac
# I2C y SPI (igual que la P3)
sudo raspi-config nonint do_i2c 0
sudo raspi-config nonint do_spi 0
echo "Reinicia la Pi si acabas de habilitar I2C/SPI."
