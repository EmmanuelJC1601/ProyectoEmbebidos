#!/bin/sh
# instala el binario y el servicio. no toca particiones, config.txt ni overlays.
set -eu
raiz=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
usuario="${ARCADE_USUARIO:-chema}"
if [ "$(id -u)" -ne 0 ]; then
  echo "ejecuta: sudo sh scripts/instalar.sh" >&2
  exit 1
fi
if ! id "$usuario" >/dev/null 2>&1; then
  echo "no existe el usuario $usuario (export ARCADE_USUARIO=otro)" >&2
  exit 1
fi
make -C "$raiz"
install -d /usr/local/bin /usr/local/lib/arcade /etc/systemd/system
install -m 755 "$raiz/build/arcade" /usr/local/bin/arcade
install -m 755 "$raiz/scripts/al_parar.sh" /usr/local/lib/arcade/al_parar.sh
sed -e "s/^User=.*/User=$usuario/" -e "s|^WorkingDirectory=.*|WorkingDirectory=$raiz|" \
  "$raiz/systemd/arcade.service" > /etc/systemd/system/arcade.service
if [ ! -f "$raiz/marcador" ] && [ -f /var/lib/arcade/datos ]; then
  cp /var/lib/arcade/datos "$raiz/marcador"
  chown "$usuario:$usuario" "$raiz/marcador"
  echo "marcador copiado desde /var/lib/arcade/datos"
fi
systemctl daemon-reload
systemctl enable arcade.service
echo "servicio habilitado para $usuario."
echo "arranca con: systemctl start arcade"
echo "log: journalctl -u arcade -f"
echo "no se escribio config.txt ni se creo otra particion."
