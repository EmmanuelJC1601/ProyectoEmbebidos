#include "botones.h"
#include "util.h"
#include <errno.h>
#include <gpiod.h>
#include <string.h>

static const unsigned pines[n_botones] = { pin_boton1, pin_boton2 };

static void anotar(estado_t *e, int64_t flanco_ns)
{
  int64_t dt = ahora_ns() - flanco_ns;
  unsigned us = dt > 0 ? (unsigned)(dt / 1000) : 0u;
  unsigned maximo;
  atomic_store(&e->latencia_us, us);
  maximo = atomic_load(&e->latencia_max);
  while (us > maximo && !atomic_compare_exchange_weak(&e->latencia_max, &maximo, us))
    ;
}

static int confirmar(botones_t *b, estado_t *e, unsigned i)
{
  int valor;
  int64_t ventana = (int64_t)us_debounce * 1000;
  if (!b->pendiente[i])
    return 0;
  if (ahora_ns() - b->flanco_ns[i] < ventana)
    return 0;
  valor = gpiod_line_get_value(b->linea[i]);
  if (valor < 0) {
    log_errno("gpiod_line_get_value");
    return -1;
  }
  b->pendiente[i] = 0;
  /* el pin volvio al nivel de antes: fue rebote, no un pulso. */
  if (valor == atomic_load(&e->nivel[i]))
    return 0;
  atomic_store(&e->nivel[i], valor ? 1 : 0);
  if (valor)
    atomic_fetch_add(&e->pulsos[i], 1u);
  anotar(e, b->flanco_ns[i]);
  return 0;
}

static void llenar(botones_t *b, struct gpiod_line_bulk *bulk)
{
  unsigned i;
  gpiod_line_bulk_init(bulk);
  for (i = 0; i < n_botones; i++)
    gpiod_line_bulk_add(bulk, b->linea[i]);
}

int botones_abrir(botones_t *b, estado_t *e)
{
  struct gpiod_line_bulk bulk;
  struct gpiod_line_request_config cfg;
  unsigned i;
  memset(b, 0, sizeof *b);
  b->chip = gpiod_chip_open(ruta_gpio);
  if (!b->chip) {
    log_errno("gpiod_chip_open");
    return -1;
  }
  gpiod_line_bulk_init(&bulk);
  for (i = 0; i < n_botones; i++) {
    b->linea[i] = gpiod_chip_get_line(b->chip, pines[i]);
    if (!b->linea[i]) {
      log_errno("gpiod_chip_get_line");
      botones_cerrar(b);
      return -1;
    }
    gpiod_line_bulk_add(&bulk, b->linea[i]);
  }
  cfg.consumer = nombre_gpio;
  cfg.request_type = GPIOD_LINE_REQUEST_EVENT_BOTH_EDGES;
  cfg.flags = GPIOD_LINE_REQUEST_FLAG_BIAS_PULL_DOWN;
  if (gpiod_line_request_bulk(&bulk, &cfg, NULL) < 0) {
    if (errno != EINVAL && errno != EOPNOTSUPP && errno != ENOTSUP) {
      log_errno("gpiod_line_request_bulk");
      botones_cerrar(b);
      return -1;
    }
    /* el circuito ya trae 10k a gnd. algunos kernel rechazan el pull interno. */
    log_aviso("pull interno no disponible (%s): se usa la resistencia de 10k", strerror(errno));
    cfg.flags = 0;
    if (gpiod_line_request_bulk(&bulk, &cfg, NULL) < 0) {
      log_errno("gpiod_line_request_bulk");
      botones_cerrar(b);
      return -1;
    }
  }
  b->pedido = 1;
  for (i = 0; i < n_botones; i++) {
    int valor = gpiod_line_get_value(b->linea[i]);
    if (valor < 0) {
      log_errno("gpiod_line_get_value");
      botones_cerrar(b);
      return -1;
    }
    atomic_store(&e->nivel[i], valor ? 1 : 0);
  }
  log_info("botones gpio %u y %u, debounce %u us en el programa (libgpiod 1.6)",
           (unsigned)pin_boton1, (unsigned)pin_boton2, (unsigned)us_debounce);
  return 0;
}

int botones_esperar(botones_t *b, estado_t *e)
{
  struct gpiod_line_bulk bulk;
  struct gpiod_line_bulk listos;
  struct timespec espera;
  int64_t tope = (int64_t)ms_espera_boton * ns_por_ms;
  int64_t ahora;
  int64_t ventana = (int64_t)us_debounce * 1000;
  unsigned i;
  unsigned nlineas;
  int rc;
  for (i = 0; i < n_botones; i++) {
    if (confirmar(b, e, i) != 0)
      return -1;
  }
  ahora = ahora_ns();
  for (i = 0; i < n_botones; i++) {
    int64_t falta;
    if (!b->pendiente[i])
      continue;
    falta = b->flanco_ns[i] + ventana - ahora;
    if (falta < 0)
      falta = 0;
    if (falta < tope)
      tope = falta;
  }
  llenar(b, &bulk);
  gpiod_line_bulk_init(&listos);
  espera.tv_sec = (time_t)(tope / ns_por_s);
  espera.tv_nsec = (long)(tope % ns_por_s);
  rc = gpiod_line_event_wait_bulk(&bulk, &espera, &listos);
  if (rc < 0) {
    if (errno == EINTR)
      return 0;
    log_errno("gpiod_line_event_wait_bulk");
    return -1;
  }
  if (rc == 0) {
    for (i = 0; i < n_botones; i++) {
      if (confirmar(b, e, i) != 0)
        return -1;
    }
    return 0;
  }
  nlineas = gpiod_line_bulk_num_lines(&listos);
  for (i = 0; i < nlineas; i++) {
    struct gpiod_line *linea = gpiod_line_bulk_get_line(&listos, i);
    struct gpiod_line_event ev[n_eventos];
    int idx = -1;
    int fd;
    int n;
    unsigned k;
    int evi;
    for (k = 0; k < n_botones; k++) {
      if (b->linea[k] == linea)
        idx = (int)k;
    }
    if (idx < 0)
      continue;
    fd = gpiod_line_event_get_fd(linea);
    n = gpiod_line_event_read_fd_multiple(fd, ev, (unsigned)n_eventos);
    if (n < 0) {
      log_errno("gpiod_line_event_read_fd_multiple");
      return -1;
    }
    for (evi = 0; evi < n; evi++) {
      /* cada flanco reinicia los 20 ms. el nivel tiene que quedarse quieto. */
      b->pendiente[idx] = 1;
      b->flanco_ns[idx] = (int64_t)ev[evi].ts.tv_sec * ns_por_s + (int64_t)ev[evi].ts.tv_nsec;
    }
  }
  for (i = 0; i < n_botones; i++) {
    if (confirmar(b, e, i) != 0)
      return -1;
  }
  return 0;
}

void botones_cerrar(botones_t *b)
{
  if (b->pedido) {
    struct gpiod_line_bulk bulk;
    llenar(b, &bulk);
    gpiod_line_release_bulk(&bulk);
    b->pedido = 0;
  }
  if (b->chip)
    gpiod_chip_close(b->chip);
  b->chip = NULL;
}
