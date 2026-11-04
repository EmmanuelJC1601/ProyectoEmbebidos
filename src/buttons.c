#include "buttons.h"
#include "util.h"
#include <gpiod.h>

static const unsigned BUTTON_PINS[BUTTON_COUNT] = { PIN_BTN_A, PIN_BTN_B };

static void note_latency(shared_t *s, int64_t ev_ns)
{
    /* Del flanco (timestamp del kernel, CLOCK_MONOTONIC) a este punto. */
    int64_t dt_ns = now_ns() - ev_ns;
    unsigned lat_us = dt_ns > 0 ? (unsigned)(dt_ns / 1000) : 0u;
    atomic_store(&s->latency_last_us, lat_us);
    unsigned max = atomic_load(&s->latency_max_us);
    while (lat_us > max &&
           !atomic_compare_exchange_weak(&s->latency_max_us, &max, lat_us))
        ;
}

#if defined(GPIOD_API_V2)

static int pin_to_index(unsigned offset)
{
    for (unsigned i = 0; i < BUTTON_COUNT; i++)
        if (BUTTON_PINS[i] == offset)
            return (int)i;
    return -1;
}

int buttons_open(buttons_t *b, shared_t *s)
{
    struct gpiod_line_settings *ls = NULL;
    struct gpiod_line_config *lc = NULL;
    struct gpiod_request_config *rc = NULL;
    int ret = -1;

    b->chip = NULL; b->req = NULL; b->buf = NULL;

    b->chip = gpiod_chip_open(GPIO_CHIP_PATH);
    if (!b->chip) { LOG_ERRNO("gpiod_chip_open " GPIO_CHIP_PATH); goto out; }

    ls = gpiod_line_settings_new();
    lc = gpiod_line_config_new();
    rc = gpiod_request_config_new();
    if (!ls || !lc || !rc) { LOG_ERR("sin memoria para configurar GPIO"); goto out; }

    if (gpiod_line_settings_set_direction(ls, GPIOD_LINE_DIRECTION_INPUT) < 0 ||
        gpiod_line_settings_set_edge_detection(ls, GPIOD_LINE_EDGE_BOTH) < 0 ||
        gpiod_line_settings_set_bias(ls, GPIOD_LINE_BIAS_PULL_DOWN) < 0) {
        LOG_ERRNO("gpiod_line_settings_set_*");
        goto out;
    }
    gpiod_line_settings_set_debounce_period_us(ls, BUTTON_DEBOUNCE_US); /* filtro de 20 ms en el kernel */

    if (gpiod_line_config_add_line_settings(lc, BUTTON_PINS, BUTTON_COUNT, ls) < 0) {
        LOG_ERRNO("gpiod_line_config_add_line_settings");
        goto out;
    }
    gpiod_request_config_set_consumer(rc, GPIO_CONSUMER);

    b->req = gpiod_chip_request_lines(b->chip, rc, lc);
    if (!b->req) { LOG_ERRNO("gpiod_chip_request_lines (otro programa usa los pines?)"); goto out; }

    b->buf = gpiod_edge_event_buffer_new(BUTTON_EVENT_BUF);
    if (!b->buf) { LOG_ERR("sin memoria para buffer de eventos"); goto out; }

    /* Nivel inicial: si arranca con el boton presionado, no lo perdemos */
    for (unsigned i = 0; i < BUTTON_COUNT; i++) {
        int v = gpiod_line_request_get_value(b->req, BUTTON_PINS[i]);
        if (v < 0) { LOG_ERRNO("gpiod_line_request_get_value"); goto out; }
        atomic_store(&s->btn_level[i], v == GPIOD_LINE_VALUE_ACTIVE ? 1 : 0);
    }
    LOG_INFO("botones: libgpiod API 2, debounce %u us en el kernel", BUTTON_DEBOUNCE_US);
    ret = 0;
out:
    gpiod_request_config_free(rc);
    gpiod_line_config_free(lc);
    gpiod_line_settings_free(ls);
    if (ret < 0)
        buttons_close(b);
    return ret;
}

int buttons_service(buttons_t *b, shared_t *s)
{
    int w = gpiod_line_request_wait_edge_events(b->req, BUTTON_WAIT_NS);
    if (w < 0) {
        if (errno == EINTR)
            return 0;
        LOG_ERRNO("gpiod_line_request_wait_edge_events");
        return -1;
    }
    if (w == 0)
        return 0; /* timeout: nada que hacer, el llamador revisa quit y el combo de apagado */

    int n = gpiod_line_request_read_edge_events(b->req, b->buf, BUTTON_EVENT_BUF);
    if (n < 0) {
        LOG_ERRNO("gpiod_line_request_read_edge_events");
        return -1;
    }
    for (int i = 0; i < n; i++) {
        struct gpiod_edge_event *ev = gpiod_edge_event_buffer_get_event(b->buf, (unsigned long)i);
        if (!ev) continue;
        int idx = pin_to_index(gpiod_edge_event_get_line_offset(ev));
        if (idx < 0) continue;

        int pressed = gpiod_edge_event_get_event_type(ev) == GPIOD_EDGE_EVENT_RISING_EDGE;
        atomic_store(&s->btn_level[idx], pressed);
        if (pressed)
            atomic_fetch_add(&s->btn_presses[idx], 1u);
        note_latency(s, (int64_t)gpiod_edge_event_get_timestamp_ns(ev));
    }
    return 0;
}

void buttons_close(buttons_t *b)
{
    if (b->buf)  gpiod_edge_event_buffer_free(b->buf);
    if (b->req)  gpiod_line_request_release(b->req);
    if (b->chip) gpiod_chip_close(b->chip);
    b->buf = NULL; b->req = NULL; b->chip = NULL;
}

#else /* libgpiod 1.6: Bookworm. No hay debounce en el kernel; se confirma el nivel a los 20 ms. */

static int64_t debounce_ns(void)
{
    return (int64_t)BUTTON_DEBOUNCE_US * 1000;
}

static void publish_level(shared_t *s, unsigned idx, int pressed, int64_t ev_ns)
{
    int prev = atomic_load(&s->btn_level[idx]);
    atomic_store(&s->btn_level[idx], pressed);
    if (pressed && !prev)
        atomic_fetch_add(&s->btn_presses[idx], 1u);
    note_latency(s, ev_ns);
}

/* Publica el cambio solo si el pin lleva BUTTON_DEBOUNCE_US sin nuevos flancos
 * y la lectura confirma un nivel distinto. Un rebote que vuelve al nivel anterior no cuenta. */
static void commit_pending(buttons_t *b, shared_t *s, unsigned idx)
{
    if (!b->pending[idx])
        return;
    if (now_ns() - b->pending_ns[idx] < debounce_ns())
        return;

    int v = gpiod_line_get_value(b->line[idx]);
    if (v < 0) {
        LOG_ERRNO("gpiod_line_get_value");
        return;
    }
    b->pending[idx] = 0;
    if (v == atomic_load(&s->btn_level[idx]))
        return;
    publish_level(s, idx, v, b->pending_ns[idx]);
}

static void fill_bulk(buttons_t *b, struct gpiod_line_bulk *bulk)
{
    gpiod_line_bulk_init(bulk);
    for (unsigned i = 0; i < BUTTON_COUNT; i++)
        gpiod_line_bulk_add(bulk, b->line[i]);
}

int buttons_open(buttons_t *b, shared_t *s)
{
    int ret = -1;
    memset(b, 0, sizeof(*b));

    b->chip = gpiod_chip_open(GPIO_CHIP_PATH);
    if (!b->chip) { LOG_ERRNO("gpiod_chip_open " GPIO_CHIP_PATH); goto out; }

    struct gpiod_line_bulk bulk;
    gpiod_line_bulk_init(&bulk);
    for (unsigned i = 0; i < BUTTON_COUNT; i++) {
        b->line[i] = gpiod_chip_get_line(b->chip, BUTTON_PINS[i]);
        if (!b->line[i]) { LOG_ERRNO("gpiod_chip_get_line"); goto out; }
        gpiod_line_bulk_add(&bulk, b->line[i]);
    }

    struct gpiod_line_request_config cfg = {
        .consumer = GPIO_CONSUMER,
        .request_type = GPIOD_LINE_REQUEST_EVENT_BOTH_EDGES,
        .flags = GPIOD_LINE_REQUEST_FLAG_BIAS_PULL_DOWN,
    };
    if (gpiod_line_request_bulk(&bulk, &cfg, NULL) < 0) {
        if (errno != EINVAL && errno != EOPNOTSUPP && errno != ENOTSUP) {
            LOG_ERRNO("gpiod_line_request_bulk (otro programa usa los pines?)");
            goto out;
        }
        /* El circuito ya trae 10 kOhm a GND; algunos kernel rechazan el pull interno. */
        LOG_WARN("pull-down interno no disponible (%s); se usa la resistencia externa", strerror(errno));
        cfg.flags = 0;
        if (gpiod_line_request_bulk(&bulk, &cfg, NULL) < 0) {
            LOG_ERRNO("gpiod_line_request_bulk (otro programa usa los pines?)");
            goto out;
        }
    }
    b->requested = 1;

    for (unsigned i = 0; i < BUTTON_COUNT; i++) {
        int v = gpiod_line_get_value(b->line[i]);
        if (v < 0) { LOG_ERRNO("gpiod_line_get_value"); goto out; }
        atomic_store(&s->btn_level[i], v ? 1 : 0);
    }
    LOG_INFO("botones: libgpiod API 1.6, debounce %u us en el programa", BUTTON_DEBOUNCE_US);
    ret = 0;
out:
    if (ret < 0)
        buttons_close(b);
    return ret;
}

int buttons_service(buttons_t *b, shared_t *s)
{
    for (unsigned i = 0; i < BUTTON_COUNT; i++)
        commit_pending(b, s, i);

    /* Despierta en el timeout normal o justo cuando vence el debounce, lo que ocurra antes.
     * Asi la latencia medida queda ~20 ms y no se come los 50 ms del timeout. */
    int64_t wait_ns = BUTTON_WAIT_NS;
    int64_t now = now_ns();
    for (unsigned i = 0; i < BUTTON_COUNT; i++) {
        if (!b->pending[i])
            continue;
        int64_t left = b->pending_ns[i] + debounce_ns() - now;
        if (left < 0)
            left = 0;
        if (left < wait_ns)
            wait_ns = left;
    }

    struct gpiod_line_bulk bulk, ready;
    fill_bulk(b, &bulk);
    gpiod_line_bulk_init(&ready);
    struct timespec ts = {
        .tv_sec = (time_t)(wait_ns / NS_PER_SEC),
        .tv_nsec = (long)(wait_ns % NS_PER_SEC),
    };
    int w = gpiod_line_event_wait_bulk(&bulk, &ts, &ready);
    if (w < 0) {
        if (errno == EINTR)
            return 0;
        LOG_ERRNO("gpiod_line_event_wait_bulk");
        return -1;
    }
    if (w == 0) {
        for (unsigned i = 0; i < BUTTON_COUNT; i++)
            commit_pending(b, s, i);
        return 0;
    }

    unsigned nlines = gpiod_line_bulk_num_lines(&ready);
    for (unsigned li = 0; li < nlines; li++) {
        struct gpiod_line *line = gpiod_line_bulk_get_line(&ready, li);
        int idx = -1;
        for (unsigned i = 0; i < BUTTON_COUNT; i++)
            if (b->line[i] == line)
                idx = (int)i;
        if (idx < 0)
            continue;

        struct gpiod_line_event evs[BUTTON_EVENT_BUF];
        int fd = gpiod_line_event_get_fd(line);
        int n = gpiod_line_event_read_fd_multiple(fd, evs, BUTTON_EVENT_BUF);
        if (n < 0) {
            LOG_ERRNO("gpiod_line_event_read_fd_multiple");
            return -1;
        }
        for (int e = 0; e < n; e++) {
            /* Cada flanco reinicia la ventana: el nivel tiene que quedarse quieto 20 ms. */
            b->pending[idx] = 1;
            b->pending_ns[idx] = (int64_t)evs[e].ts.tv_sec * NS_PER_SEC
                               + (int64_t)evs[e].ts.tv_nsec;
        }
    }
    for (unsigned i = 0; i < BUTTON_COUNT; i++)
        commit_pending(b, s, i);
    return 0;
}

void buttons_close(buttons_t *b)
{
    if (b->requested) {
        struct gpiod_line_bulk bulk;
        fill_bulk(b, &bulk);
        gpiod_line_release_bulk(&bulk);
        b->requested = 0;
    }
    if (b->chip)
        gpiod_chip_close(b->chip);
    b->chip = NULL;
    for (unsigned i = 0; i < BUTTON_COUNT; i++)
        b->line[i] = NULL;
}

#endif
