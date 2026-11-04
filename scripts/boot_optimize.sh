#!/bin/bash
# Candidatos para acortar el arranque (P4, Parte F). Por defecto solo MUESTRA que haria.
# Con --apply deshabilita los servicios listados. NUNCA toca ssh ni la red: no perder acceso remoto.
set -u
CANDIDATES="bluetooth.service hciuart.service triggerhappy.service ModemManager.service avahi-daemon.service apt-daily.timer apt-daily-upgrade.timer man-db.timer"
echo "Tiempo actual:"; systemd-analyze
for s in $CANDIDATES; do
    if systemctl list-unit-files "$s" --no-legend 2>/dev/null | grep -q "$s"; then
        if [ "${1:-}" = "--apply" ]; then sudo systemctl disable --now "$s" && echo "deshabilitado: $s"
        else echo "candidato: $s"; fi
    fi
done
cat <<'TXT'

Ajustes manuales sugeridos en /boot/firmware/config.txt (medir antes y despues):
  boot_delay=0
  disable_splash=1
  dtoverlay=disable-bt        # solo si no usaras Bluetooth
Y en /boot/firmware/cmdline.txt agregar: quiet
Despues: sudo reboot && ./scripts/measure_boot.sh
TXT
