#!/bin/bash
# Mide el requisito "arranque < 15 s". Ejecutar tras un reinicio real.
echo "== systemd-analyze =="; systemd-analyze
echo "== cadena critica de arcade.service =="; systemd-analyze critical-chain arcade.service --no-pager
t_us=$(systemctl show arcade.service -p ActiveEnterTimestampMonotonic --value)
if [ -n "$t_us" ] && [ "$t_us" != "0" ]; then
    awk -v t="$t_us" 'BEGIN { printf "arcade.service activo a los %.2f s del inicio del kernel (no incluye ~2-5 s de EEPROM/firmware)\n", t/1e6 }'
else
    echo "arcade.service no esta activo"
fi
