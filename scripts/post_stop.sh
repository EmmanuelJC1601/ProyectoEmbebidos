#!/bin/bash
# Lo ejecuta systemd (con privilegios, por el prefijo "+") al terminar arcade.service.
# Si el programa salio con codigo 42, hace un apagado controlado: sync + poweroff.
set -u
if [ "${EXIT_STATUS:-}" = "42" ]; then
    logger -t arcade "apagado controlado solicitado por el usuario"
    sync
    systemctl poweroff
fi
