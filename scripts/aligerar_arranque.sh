#!/bin/sh
# quita del arranque servicios que la consola no usa.
# no toca config.txt, overlays ni particiones. se puede revertir
# con systemctl enable de cada unidad.
set -eu
if [ "$(id -u)" -ne 0 ]; then
  echo "ejecuta: sudo sh scripts/aligerar_arranque.sh" >&2
  exit 1
fi
# man-db reindexa los manuales. no hace falta para jugar.
# wait-online solo retrasa el arranque hasta que haya ip; la red sigue.
# modem, bluetooth y avahi no tienen hardware en esta consola.
# e2scrub y apt-daily son mantenimiento, no el juego.
# rp1-test es de la pi 5. udisks2 monta discos usb.
unidades="
man-db.service
man-db.timer
NetworkManager-wait-online.service
ModemManager.service
bluetooth.service
avahi-daemon.service
e2scrub_reap.service
e2scrub_all.timer
apt-daily.service
apt-daily.timer
apt-daily-upgrade.service
apt-daily-upgrade.timer
rp1-test.service
udisks2.service
"
echo "se desactivan, si existen:"
for u in $unidades; do
  if systemctl list-unit-files "$u" >/dev/null 2>&1 && systemctl list-unit-files "$u" | grep -q "^$u"; then
    systemctl disable --now "$u" || true
    echo "  off $u"
  else
    echo "  no esta $u"
  fi
done
echo
echo "el escritorio sigue. para bajar de 15 s hay que no esperarlo:"
echo "  sudo systemctl set-default multi-user.target"
echo "la consola y el juego arrancan igual. vuelve al escritorio con:"
echo "  sudo systemctl set-default graphical.target"
echo
echo "reinicia y mide: sudo reboot"
echo "despues: sh scripts/medir_arranque.sh"
