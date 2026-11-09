#include "hw.h"
#include "adc.h"
#include "botones.h"
#include "constantes.h"
#include "dibujo.h"
#include "filtro.h"
#include "pantalla.h"
#include "spi.h"
#include "util.h"
#include <string.h>

static estado_t *estado;
static botones_t botones;
static adc_t adc;
static pthread_t hilo_botones;
static pthread_t hilo_adc;
static int botones_ok;
static int adc_ok;
static int hilos_ok;

/* bitacora clase 8: prioridad muy alta. se bloquea en el gpio y el kernel
 * lo despierta. no hay bucle de lectura. el combo de 3 s vive aqui para que
 * un cuadro largo no retrase el apagado. */
static void *tarea_botones(void *arg)
{
  combo_t combo;
  unsigned errores = 0;
  (void)arg;
  memset(&combo, 0, sizeof combo);
  while (!atomic_load(&estado->salir)) {
    int ambos;
    if (botones_esperar(&botones, estado) != 0) {
      if (++errores >= (unsigned)errores_tope) {
        log_error("demasiados fallos de botones");
        atomic_store(&estado->fallo, 1);
        atomic_store(&estado->salir, 1);
        break;
      }
      dormir_ms(10);
      continue;
    }
    errores = 0;
    ambos = atomic_load(&estado->nivel[idx_b1]) && atomic_load(&estado->nivel[idx_b2]);
    if (combo_mantener(&combo, ambos, ahora_ms(), ms_apagado)) {
      log_info("combo de apagado (%d ms)", ms_apagado);
      atomic_store(&estado->apagar, 1);
      atomic_store(&estado->salir, 1);
    }
  }
  return NULL;
}

/* bitacora clase 8: prioridad media-alta, periodo fijo de ms_adc.
 * bitacora clase 11: no hay otro proceso. el adc publica en el estado atomico. */
static void *tarea_adc(void *arg)
{
  filtro_t filtro[n_potes];
  static const unsigned canal[n_potes] = { canal_p1, canal_p2 };
  unsigned errores = 0;
  unsigned vistos = 0;
  int aviso = 0;
  uint16_t crudo[n_potes];
  uint16_t base0 = 0;
  uint16_t base1 = 0;
  int64_t siguiente;
  (void)arg;
  memset(filtro, 0, sizeof filtro);
  memset(crudo, 0, sizeof crudo);
  siguiente = ahora_ns();
  while (!atomic_load(&estado->salir)) {
    unsigned i;
    int ok = 1;
    for (i = 0; i < n_potes; i++) {
      if (adc_leer(&adc, canal[i], &crudo[i]) != 0) {
        ok = 0;
        break;
      }
      atomic_store(&estado->pot[i], filtrar_pot(&filtro[i], crudo[i]));
    }
    if (!ok) {
      atomic_fetch_add(&estado->errores_adc, 1u);
      log_errno("lectura mcp3008");
      if (++errores >= (unsigned)errores_tope) {
        log_error("demasiados fallos del adc");
        atomic_store(&estado->fallo, 1);
        atomic_store(&estado->salir, 1);
        break;
      }
    } else {
      errores = 0;
      atomic_store(&estado->adc_listo, 1);
      if (vistos == 0) {
        base0 = crudo[0];
        base1 = crudo[1];
        log_info("adc primera muestra p1 %u p2 %u", (unsigned)base0, (unsigned)base1);
      }
      vistos++;
      if (!aviso && vistos >= 100) {
        int movio = crudo[0] > base0 + 8 || base0 > crudo[0] + 8 ||
                    crudo[1] > base1 + 8 || base1 > crudo[1] + 8;
        if (movio)
          log_info("adc se mueve p1 %u p2 %u", (unsigned)crudo[0], (unsigned)crudo[1]);
        else if (crudo[0] == 0 && crudo[1] == 0)
          log_aviso("adc en cero. gira el pote; si no cambia, revisa ch0 ch1 y el miso, y deja sda-0 suelto");
        aviso = 1;
      }
    }
    siguiente += (int64_t)ms_adc * ns_por_ms;
    if (siguiente < ahora_ns())
      siguiente = ahora_ns();
    dormir_hasta(siguiente);
  }
  return NULL;
}

int hw_abrir(estado_t *e, int simular)
{
  estado = e;
  adc.fd = -1;
  (void)simular;
  if (spi_iniciar() != 0)
    return -1;
  if (botones_abrir(&botones, e) != 0) {
    spi_cerrar();
    return -1;
  }
  botones_ok = 1;
  if (adc_abrir(&adc, ruta_spi_adc) != 0) {
    hw_cerrar();
    return -1;
  }
  adc_ok = 1;
  if (pantalla_abrir() != 0)
    log_aviso("la pantalla no abrio: el juego sigue y reintenta");
  else
    pantalla_luz(1);
  if (iniciar_hilo(&hilo_botones, prio_botones, tarea_botones, NULL) != 0) {
    hw_cerrar();
    return -1;
  }
  if (iniciar_hilo(&hilo_adc, prio_adc, tarea_adc, NULL) != 0) {
    atomic_store(&e->salir, 1);
    if (pthread_join(hilo_botones, NULL) != 0)
      log_aviso("pthread_join botones");
    hilos_ok = 0;
    botones_ok = 1;
    hw_cerrar();
    return -1;
  }
  hilos_ok = 1;
  return 0;
}

void hw_cerrar(void)
{
  if (estado)
    atomic_store(&estado->salir, 1);
  if (hilos_ok) {
    if (pthread_join(hilo_botones, NULL) != 0)
      log_aviso("pthread_join botones");
    if (pthread_join(hilo_adc, NULL) != 0)
      log_aviso("pthread_join adc");
    hilos_ok = 0;
  }
  pantalla_cerrar();
  if (adc_ok) {
    adc_cerrar(&adc);
    adc_ok = 0;
  }
  if (botones_ok) {
    botones_cerrar(&botones);
    botones_ok = 0;
  }
  spi_cerrar();
}

int hw_volcar(void)
{
  if (!pantalla_viva())
    return 0;
  return pantalla_volcar();
}

void hw_luz(int encendida)
{
  pantalla_luz(encendida);
}

int hw_recuperar(void)
{
  if (pantalla_viva())
    return 1;
  if (pantalla_abrir() != 0)
    return 0;
  dibujo_todo();
  pantalla_luz(1);
  log_info("tft recuperada");
  return 1;
}

void hw_entrada(estado_t *e)
{
  (void)e;
}
