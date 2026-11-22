#include "botones.h"
#include "util.h"
#include <errno.h>
#include <gpiod.h>
#include <string.h>

static const unsigned pines[n_botones] = { pin_boton1, pin_boton2 };

/* guarda cuantos us pasaron del flanco del kernel al nivel publicado.
 * el reloj es el mismo (monotonic), asi que no se mezclan fuentes. */
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

/* si ya pasaron 20 ms desde el ultimo flanco, lee el pin.
 * si sigue igual que el nivel publicado, era rebote y no hay pulso. */
static int confirmar(botones_t *b, estado_t *e, unsigned i)
{
  enum gpiod_line_value valor;
  int alto;
  int64_t ventana = (int64_t)us_debounce * 1000;
  if (!b->pendiente[i])
    return 0;
  if (ahora_ns() - b->flanco_ns[i] < ventana)
    return 0;
  valor = gpiod_line_request_get_value(b->pedido, pines[i]);
  if (valor == GPIOD_LINE_VALUE_ERROR) {
    log_errno("valor boton");
    return -1;
  }
  b->pendiente[i] = 0;
  alto = valor == GPIOD_LINE_VALUE_ACTIVE;
  if (alto == atomic_load(&e->nivel[i]))
    return 0;
  atomic_store(&e->nivel[i], alto);
  if (alto)
    atomic_fetch_add(&e->pulsos[i], 1u);
  anotar(e, b->flanco_ns[i]);
  return 0;
}

/* pide los dos pines. si el kernel no acepta el pull, se reintenta sin el:
 * el circuito ya trae 10k a gnd. */
static int pedir_lineas(botones_t *b, int con_pull)
{
  struct gpiod_line_settings *ajuste = gpiod_line_settings_new();
  struct gpiod_line_config *lineas = gpiod_line_config_new();
  struct gpiod_request_config *cfg = gpiod_request_config_new();
  unsigned offsets[n_botones];
  int rc = -1;
  offsets[idx_b1] = pin_boton1;
  offsets[idx_b2] = pin_boton2;
  if (!ajuste || !lineas || !cfg) {
    log_errno("gpiod botones");
    goto fin;
  }
  if (gpiod_line_settings_set_direction(ajuste, GPIOD_LINE_DIRECTION_INPUT) < 0 ||
      gpiod_line_settings_set_edge_detection(ajuste, GPIOD_LINE_EDGE_BOTH) < 0) {
    log_errno("ajuste botones");
    goto fin;
  }
  if (con_pull && gpiod_line_settings_set_bias(ajuste, GPIOD_LINE_BIAS_PULL_DOWN) < 0) {
    log_errno("pull botones");
    goto fin;
  }
  if (gpiod_line_config_add_line_settings(lineas, offsets, n_botones, ajuste) < 0) {
    log_errno("lineas botones");
    goto fin;
  }
  gpiod_request_config_set_consumer(cfg, nombre_gpio);
  b->pedido = gpiod_chip_request_lines(b->chip, cfg, lineas);
  if (!b->pedido) {
    if (con_pull && (errno == EINVAL || errno == EOPNOTSUPP || errno == ENOTSUP))
      rc = 1;
    else
      log_errno("request botones");
    goto fin;
  }
  rc = 0;
fin:
  if (ajuste)
    gpiod_line_settings_free(ajuste);
  if (lineas)
    gpiod_line_config_free(lineas);
  if (cfg)
    gpiod_request_config_free(cfg);
  return rc;
}

int botones_abrir(botones_t *b, estado_t *e)
{
  unsigned i;
  int rc;
  memset(b, 0, sizeof *b);
  b->eventos = gpiod_edge_event_buffer_new(n_eventos);
  if (!b->eventos) {
    log_errno("buffer flancos");
    botones_cerrar(b);
    return -1;
  }
  b->chip = gpiod_chip_open(ruta_gpio);
  if (!b->chip) {
    log_errno("gpiod_chip_open");
    botones_cerrar(b);
    return -1;
  }
  rc = pedir_lineas(b, 1);
  if (rc > 0) {
    log_aviso("pull interno no disponible (%s): se usa la resistencia de 10k", strerror(errno));
    rc = pedir_lineas(b, 0);
  }
  if (rc != 0) {
    botones_cerrar(b);
    return -1;
  }
  for (i = 0; i < n_botones; i++) {
    enum gpiod_line_value valor = gpiod_line_request_get_value(b->pedido, pines[i]);
    if (valor == GPIOD_LINE_VALUE_ERROR) {
      log_errno("valor boton");
      botones_cerrar(b);
      return -1;
    }
    atomic_store(&e->nivel[i], valor == GPIOD_LINE_VALUE_ACTIVE);
  }
  log_info("botones gpio %u y %u, debounce %u us en el programa (libgpiod 2)",
           (unsigned)pin_boton1, (unsigned)pin_boton2, (unsigned)us_debounce);
  return 0;
}

/* espera el proximo flanco o el fin de la ventana de 20 ms.
 * cada flanco nuevo reinicia esa ventana: el pin tiene que quedarse quieto. */
int botones_esperar(botones_t *b, estado_t *e)
{
  int64_t tope = (int64_t)ms_espera_boton * ns_por_ms;
  int64_t ahora;
  int64_t ventana = (int64_t)us_debounce * 1000;
  unsigned i;
  int rc;
  int n;
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
  rc = gpiod_line_request_wait_edge_events(b->pedido, tope);
  if (rc < 0) {
    if (errno == EINTR)
      return 0;
    log_errno("espera flanco");
    return -1;
  }
  if (rc == 0) {
    for (i = 0; i < n_botones; i++) {
      if (confirmar(b, e, i) != 0)
        return -1;
    }
    return 0;
  }
  n = gpiod_line_request_read_edge_events(b->pedido, b->eventos, n_eventos);
  if (n < 0) {
    log_errno("leer flancos");
    return -1;
  }
  for (i = 0; i < (unsigned)n; i++) {
    struct gpiod_edge_event *ev = gpiod_edge_event_buffer_get_event(b->eventos, i);
    unsigned off = gpiod_edge_event_get_line_offset(ev);
    unsigned idx = off == (unsigned)pin_boton2 ? (unsigned)idx_b2 : (unsigned)idx_b1;
    b->pendiente[idx] = 1;
    b->flanco_ns[idx] = (int64_t)gpiod_edge_event_get_timestamp_ns(ev);
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
    gpiod_line_request_release(b->pedido);
    b->pedido = NULL;
  }
  if (b->chip) {
    gpiod_chip_close(b->chip);
    b->chip = NULL;
  }
  if (b->eventos) {
    gpiod_edge_event_buffer_free(b->eventos);
    b->eventos = NULL;
  }
}
