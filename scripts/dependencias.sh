#!/bin/sh
# paquetes para compilar. no corre raspi-config ni escribe config.txt.
set -eu
if [ "$(id -u)" -ne 0 ]; then
  echo "ejecuta: sudo sh scripts/dependencias.sh" >&2
  exit 1
fi
apt-get update
apt-get install -y build-essential pkg-config libgpiod-dev
echo "libgpiod $(pkg-config --modversion libgpiod)"
echo "no se cambio el sistema: spi e i2c quedan como estaban."
