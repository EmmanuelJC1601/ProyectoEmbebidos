# Tabla de pruebas del Avance 2 (llenar la columna "Obtenido" en la Pi)

| # | Prueba | Esperado | Obtenido |
|---|--------|----------|----------|
| 1 | `make test` en la Pi | 30 pruebas, 0 fallidas | |
| 2 | Girar P1 de un extremo a otro | Fila 0 de la LCD: P1 va de ~0 a ~1023 | |
| 3 | Girar P2 de un extremo a otro | P2 va de ~0 a ~1023, P1 no cambia | |
| 4 | Pulsar boton A una vez | `B1:#` mientras se mantiene; contador +1 | |
| 5 | Pulsar B con rebote (pulsar rapido) | Cuenta una sola vez por pulsacion (debounce 20 ms) | |
| 6 | Mantener ambos botones 3 s | LCD "Apagando...", Linux se apaga | |
| 7 | `sudo systemctl start arcade` sin sesion SSH abierta tras reboot | Servicio activo, LCD muestra lecturas sin intervencion | |
| 8 | Latencia de entrada (log al cerrar con Ctrl+C) | maxima < 50000 us | |
| 9 | Tiempo de arranque (`scripts/measure_boot.sh`) | < 15 s | |
| 10 | **FALLA**: desconectar la LCD con el programa corriendo | Botones y pots siguen leyendose (`--console`); el log dice "LCD no disponible"; al reconectar se recupera en <= 2 s | |
| 11 | **FALLA**: desconectar CE0/MISO del MCP3008 | Tras 20 errores seguidos el programa sale con error; systemd lo reinicia; no se cuelga | |
| 12 | **FALLA**: `--lcd-addr 0x99` | Rechaza la direccion y termina con codigo 2 | |
| 13 | **FALLA**: ejecutar dos copias a la vez | La segunda falla al pedir los GPIO (mensaje claro; pines ocupados) | |
