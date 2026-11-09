#!/bin/sh
# lo llama arcade.service al terminar. el prefijo + del unit lo corre como root.
# el codigo 42 es el combo de los dos botones: sync y poweroff.
# cualquier otra salida solo hace sync. systemd reinicia si fue fallo.
set -u
sync
if [ "${EXIT_STATUS:-}" = "42" ]; then
  /sbin/poweroff
fi
exit 0
