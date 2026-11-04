#!/bin/bash
# Compila, instala en /opt/arcade y habilita el arranque automatico.
set -euo pipefail
cd "$(dirname "$0")/.."
make
USER_NAME="${SUDO_USER:-$USER}"
sudo install -d /opt/arcade/bin /opt/arcade/scripts
sudo install -m 755 build/arcade /opt/arcade/bin/arcade
sudo install -m 755 scripts/post_stop.sh /opt/arcade/scripts/post_stop.sh
sed "s/@USER@/${USER_NAME}/" systemd/arcade.service | sudo tee /etc/systemd/system/arcade.service >/dev/null
sudo systemctl daemon-reload
sudo systemctl enable arcade.service
echo "Instalado. Probar sin reiniciar: sudo systemctl start arcade.service ; journalctl -u arcade -f"
echo "Quitar:   sudo systemctl disable --now arcade.service   (recuerda: stop + disable, como en la P4)"
