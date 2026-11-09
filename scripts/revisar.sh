#!/bin/sh
# evidencia de la placa, sin modificar nada. el codigo de salida es 1 si falta algo.
set -u
cd "$(dirname "$0")/.." || exit 1
falla=0
ok() {
  echo "[ok] $1"
}
mal() {
  echo "[falta] $1"
  falla=1
}
echo "== placa =="
if [ -r /proc/device-tree/model ]; then
  tr -d '\0' < /proc/device-tree/model
  echo
else
  mal "no hay /proc/device-tree/model (esto no es la raspberry)"
fi
echo "== libgpiod =="
if ver=$(pkg-config --modversion libgpiod 2>/dev/null); then
  case "$ver" in
    1.*) ok "libgpiod $ver" ;;
    *) mal "libgpiod $ver (hace falta la 1.x, bookworm trae 1.6.3)" ;;
  esac
else
  mal "pkg-config no ve libgpiod"
fi
echo "== dispositivos =="
if [ -e /dev/gpiochip0 ]; then
  ls -l /dev/gpiochip0
  ok "/dev/gpiochip0"
else
  mal "/dev/gpiochip0"
fi
if [ -e /dev/spidev0.0 ]; then
  ls -l /dev/spidev0.0
  ok "/dev/spidev0.0 (mcp3008, ce0)"
else
  mal "/dev/spidev0.0"
fi
if [ -e /dev/spidev0.1 ]; then
  ls -l /dev/spidev0.1
  ok "/dev/spidev0.1 (tft, ce1)"
else
  mal "/dev/spidev0.1: con dtparam=spi=on el kernel crea los dos. no edites config.txt sin revisarlo"
fi
if [ -e /dev/i2c-1 ]; then
  ls -l /dev/i2c-1
  echo "[aviso] /dev/i2c-1 sigue ahi. el juego ya no lo usa. desconecta la lcd."
fi
echo "== grupos =="
usuario="${ARCADE_USUARIO:-chema}"
if id "$usuario" >/dev/null 2>&1; then
  grupos=$(id -nG "$usuario")
  echo "$usuario: $grupos"
  echo "$grupos" | tr ' ' '\n' | grep -qx gpio || mal "$usuario no esta en gpio"
  echo "$grupos" | tr ' ' '\n' | grep -qx spi || mal "$usuario no esta en spi"
else
  mal "no existe el usuario $usuario"
fi
echo "== volatile en el arbol =="
grep -n volatile src/*.c include/*.h || true
if [ "$falla" -ne 0 ]; then
  echo "hay faltantes."
  exit 1
fi
echo "la placa tiene lo que el binario abre. no se cambio ningun archivo del sistema."
exit 0
