#!/bin/sh
# cifras de arranque que ya da este sistema. no las inventa y no lo optimiza.
# el requisito de 15 s de la entrega 1 no se afirma: el so no se modifico.
set -u
echo "== systemd-analyze =="
if command -v systemd-analyze >/dev/null 2>&1; then
  systemd-analyze || true
  echo "== blame (primeras lineas) =="
  systemd-analyze blame 2>/dev/null | head -n 15 || true
else
  echo "systemd-analyze no esta en este equipo."
fi
echo "== servicio =="
if command -v systemctl >/dev/null 2>&1; then
  systemctl show arcade -p ActiveState -p SubState -p NRestarts -p ExecMainStatus -p ExecMainStartTimestamp 2>/dev/null || true
fi
echo
echo "la latencia y los fps los imprime el propio proceso al salir:"
echo "  journalctl -u arcade -n 40 --no-pager"
echo "busca las lineas 'latencia' y 'fps'. el requisito de entrada es < 50000 us."
echo "el requisito de cuadro es >= 30 fps. si fps sale 0, la corrida duro menos de 1 s."
echo "el consumo no lo mide este script: hace falta un medidor usb en la alimentacion."
echo "el arranque de 15 s es de todo el sistema, no del juego."
echo "man-db (el indice de los manuales) no hace falta. para quitarlo y otros que no usa la consola:"
echo "  sudo sh scripts/aligerar_arranque.sh"
