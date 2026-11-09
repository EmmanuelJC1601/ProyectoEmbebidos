/* principal.c - un proceso, tres tareas.
 * bitacora clase 8: botones sched_fifo 50, adc sched_fifo 40, juego y dibujo
 * en este hilo con prioridad normal. la entrada es preemptiva: si no, un
 * volcado spi largo pasaria de los 50 ms.
 * bitacora clase 11: no hay ipc. los hilos comparten este proceso.
 * bitacora clase 20: senal_parar es el unico volatile. es sig_atomic_t porque
 * la escribe el manejador y la lee este hilo. el resto del estado compartido
 * es atomic_*: volatile no hace atomica una escritura ni ordena dos nucleos.
 * no hay volatile en registros: no se hace mmap del hardware. */
#include "archivo.h"
#include "constantes.h"
#include "dibujo.h"
#include "estado.h"
#include "hw.h"
#include "juego.h"
#include "util.h"
#include <signal.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

static volatile sig_atomic_t senal_parar;
static unsigned bytes_seg;
static unsigned bytes_peor;

static void al_senal(int sig)
{
  (void)sig;
  senal_parar = 1;
}

static int instalar_senales(void)
{
  struct sigaction sa;
  memset(&sa, 0, sizeof sa);
  sa.sa_handler = al_senal;
  if (sigemptyset(&sa.sa_mask) != 0) {
    log_errno("sigemptyset");
    return -1;
  }
  if (sigaction(SIGINT, &sa, NULL) < 0 || sigaction(SIGTERM, &sa, NULL) < 0) {
    log_errno("sigaction");
    return -1;
  }
  return 0;
}

static void cargar(estado_t *e, int simular)
{
  uint32_t arranques = 0;
  uint32_t record = 0;
  if (simular)
    return;
  if (asegurar_dir(dir_datos) != 0)
    log_aviso("no existe %s o no se puede crear", dir_datos);
  if (cargar_datos(ruta_datos, &arranques, &record) != 0) {
    record = 0;
    if (cargar_entero(ruta_bootcount, &arranques) != 0)
      arranques = 0;
  }
  arranques++;
  atomic_store(&e->arranques, arranques);
  atomic_store(&e->record, record);
  archivo_usar(ruta_datos);
  if (guardar_datos(ruta_datos, arranques, record) != 0)
    log_aviso("el arranque queda solo en ram");
  else
    log_info("arranque %u record %u", arranques, record);
}

static void resumen(estado_t *e)
{
  unsigned ultima = atomic_load(&e->latencia_us);
  unsigned maxima = atomic_load(&e->latencia_max);
  unsigned fps = atomic_load(&e->fps);
  unsigned cuadro = atomic_load(&e->cuadro_us);
  if (maxima == 0)
    log_info("latencia: sin flancos en esta corrida (requisito < %u us)", (unsigned)us_latencia_max);
  else
    log_info("latencia ultima %u us maxima %u us (requisito < %u us) %s",
             ultima, maxima, (unsigned)us_latencia_max,
             maxima < (unsigned)us_latencia_max ? "ok" : "no");
  if (fps == 0)
    log_info("fps: no cerro un segundo (peor cuadro %u us, paso %u ms)", cuadro, (unsigned)ms_cuadro);
  else
    log_info("fps %u peor cuadro %u us spi %u bytes (requisito >= %u) %s",
             fps, cuadro, bytes_seg, (unsigned)fps_minimo,
             fps >= (unsigned)fps_minimo ? "ok" : "no");
  log_info("errores adc %u tft %u", atomic_load(&e->errores_adc), atomic_load(&e->errores_tft));
}

static int uso(const char *nombre)
{
  fprintf(stderr, "uso: %s\n", nombre);
  return codigo_uso;
}

int main(int argc, char **argv)
{
  estado_t estado;
  int codigo;
  int64_t plazo;
  int64_t marca_fps;
  int64_t toque;
  int64_t reintento = 0;
  unsigned cuadros = 0;
  unsigned peor = 0;
  unsigned mem_p1 = 0, mem_p2 = 0, mem_b1 = 0, mem_b2 = 0;
  if (argc > 1)
    return uso(argv[0]);
  estado_iniciar(&estado);
  if (instalar_senales() != 0)
    return 1;
  /* mlockall evita que un fallo de pagina alargue un cuadro. sin el
   * permiso del servicio solo se avisa: el juego sigue. */
  if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0)
    log_aviso("mlockall (%s): sigue sin bloquear la ram", strerror(errno));
  cargar(&estado, 0);
  if (hw_abrir(&estado, 0) != 0)
    return 1;
  {
    int64_t limite = ahora_ms() + ms_espera_adc;
    while (!atomic_load(&estado.adc_listo) && ahora_ms() < limite && !senal_parar)
      dormir_ms(10);
  }
  juegos_iniciar(&estado);
  plazo = ahora_ns();
  marca_fps = ahora_ms();
  toque = marca_fps;
  while (!senal_parar && !atomic_load(&estado.salir)) {
    const juego_t *juego;
    int64_t t0;
    int64_t t1;
    unsigned us;
    unsigned p1, p2, b1, b2;
    hw_entrada(&estado);
    if (ahora_ms() >= reintento) {
      if (!hw_recuperar())
        reintento = ahora_ms() + ms_reintento;
      else
        reintento = ahora_ms() + ms_por_s;
    }
    t0 = ahora_ns();
    juego = juego_activo();
    juego->actualizar(&estado);
    juego_activo()->dibujar(&estado);
    if (hw_volcar() != 0) {
      atomic_fetch_add(&estado.errores_tft, 1u);
      reintento = ahora_ms() + ms_reintento;
    }
    t1 = ahora_ns();
    us = t1 > t0 ? (unsigned)((t1 - t0) / 1000) : 0u;
    if (us > peor)
      peor = us;
    if (dibujo_ultimo() > bytes_peor)
      bytes_peor = dibujo_ultimo();
    cuadros++;
    if (ahora_ms() - marca_fps >= ms_por_s) {
      atomic_store(&estado.fps, cuadros);
      atomic_store(&estado.cuadro_us, peor);
      bytes_seg = bytes_peor;
      cuadros = 0;
      peor = 0;
      bytes_peor = 0;
      marca_fps = ahora_ms();
    }
    p1 = atomic_load(&estado.pot[idx_p1]);
    p2 = atomic_load(&estado.pot[idx_p2]);
    b1 = atomic_load(&estado.pulsos[idx_b1]);
    b2 = atomic_load(&estado.pulsos[idx_b2]);
    if (p1 != mem_p1 || p2 != mem_p2 || b1 != mem_b1 || b2 != mem_b2) {
      mem_p1 = p1;
      mem_p2 = p2;
      mem_b1 = b1;
      mem_b2 = b2;
      toque = ahora_ms();
      hw_luz(1);
    } else if (ahora_ms() - toque >= ms_inactivo) {
      hw_luz(0);
    }
    plazo += (int64_t)ms_cuadro * ns_por_ms;
    if (plazo < ahora_ns())
      plazo = ahora_ns();
    else
      dormir_hasta(plazo);
  }
  if (atomic_load(&estado.apagar)) {
    dibujo_limpiar(color_negro);
    dibujo_texto((ancho_px - dibujo_ancho("APAGANDO", 3)) / 2, 140, "APAGANDO", color_oro, 3);
    hw_volcar();
    dormir_ms(400);
  }
  codigo = atomic_load(&estado.fallo) ? 1 : atomic_load(&estado.apagar) ? codigo_apagar : 0;
  resumen(&estado);
  hw_cerrar();
  return codigo;
}
