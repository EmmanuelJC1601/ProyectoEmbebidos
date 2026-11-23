#!/bin/sh
# quita del arranque lo que la consola no usa.
# no toca config.txt, overlays ni particiones.
# el cable sigue en dhcp y el ssh sigue. el wifi no se apaga.
# se puede correr dos veces.
set -eu
if [ "$(id -u)" -ne 0 ]; then
  echo "ejecuta: sudo sh scripts/aligerar_arranque.sh" >&2
  exit 1
fi

# man-db reindexa los manuales. no hace falta para jugar.
# wait-online retrasa el arranque hasta que haya ip; la red sigue.
# cloud-init solo hacia falta el primer encendido (usuario, ssh, red).
# modem, bluetooth y avahi no los usa el juego.
# e2scrub y apt-daily son mantenimiento.
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
cloud-init-main.service
cloud-init-local.service
cloud-config.service
cloud-final.service
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

# el generador de cloud-init arranca aunque el servicio este disabled.
if [ -d /etc/cloud ]; then
  touch /etc/cloud/cloud-init.disabled
  echo "  cloud-init desactivado"
fi

# no tiene seccion install: disable no le hace nada. mask si.
if systemctl list-unit-files rpi-resize-swap-file.service 2>/dev/null | grep -q '^rpi-resize-swap-file.service'; then
  systemctl mask rpi-resize-swap-file.service >/dev/null 2>&1 || true
  echo "  mask rpi-resize-swap-file.service"
fi

# netplan generate llama a systemctl daemon-reload.
# en el arranque systemd espera a networkmanager y networkmanager espera
# ese reload: unos 30 s. el generador de verdad tarda una fraccion.
# el dhcp del cable no depende del reload.
if [ -x /usr/libexec/netplan/generate ] && [ -e /usr/sbin/netplan ]; then
  if [ -e /usr/sbin/netplan.real ]; then
    echo "  netplan ya tiene el puente"
  else
    mv /usr/sbin/netplan /usr/sbin/netplan.real
    cat > /usr/sbin/netplan << 'EOF'
#!/bin/sh
if [ "$1" = generate ]; then
  exec /usr/libexec/netplan/generate
fi
exec /usr/sbin/netplan.real "$@"
EOF
    chmod 755 /usr/sbin/netplan
    echo "  puente netplan: generate ya no recarga systemd"
  fi
else
  echo "  no esta netplan; no se puso el puente"
fi

# netplan avisa si el yaml lo puede leer cualquiera. no es el retraso de 30 s.
chmod 600 /lib/netplan/*.yaml /etc/netplan/*.yaml 2>/dev/null || true

# lite ya arranca en multi-user. si la imagen trajera escritorio, no se espera.
systemctl set-default multi-user.target >/dev/null
echo "  default multi-user.target"

echo
echo "el ssh y el dhcp del cable siguen. el wifi no se toco."
echo "avahi quedo apagado: chema.local puede no resolver."
echo "para el nombre: systemctl enable --now avahi-daemon.service"
echo "para quitar el puente: mv /usr/sbin/netplan.real /usr/sbin/netplan"
echo "reinicia y mide: sudo reboot"
echo "despues: sh scripts/medir_arranque.sh"
