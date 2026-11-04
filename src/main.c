/* main.c - Consola arcade, Avance 2: lectura de 2 botones + 2 potenciometros y muestra en LCD1602.
 *
 * Hilos (decision del Avance 1: un proceso, varios hilos, estado compartido atomico):
 *   input_task  SCHED_FIFO 50  bloqueado en libgpiod; el kernel lo despierta por interrupcion (sin polling)
 *   adc_task    SCHED_FIFO 40  muestreo periodico del MCP3008 a 100 Hz con filtro
 *   main        prioridad normal: logica de juego a paso fijo + pantalla */
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#include "buttons.h"
#include "config.h"
#include "filter.h"
#include "game.h"
#include "lcd1602.h"
#include "mcp3008.h"
#include "shared_state.h"
#include "storage.h"
#include "util.h"

static volatile sig_atomic_t g_stop = 0;
static shared_t g_shared;

typedef struct {
    unsigned lcd_addr;
    bool     use_lcd;
    bool     console;
} options_t;

static void on_signal(int sig) { (void)sig; g_stop = 1; }

static int install_signals(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal;       /* sin SA_RESTART: las esperas se interrumpen y se revisa g_stop */
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGINT, &sa, NULL) < 0 || sigaction(SIGTERM, &sa, NULL) < 0) {
        LOG_ERRNO("sigaction");
        return -1;
    }
    return 0;
}

/* ---------------- Tarea de entrada: botones ---------------- */
static void *input_task(void *arg)
{
    buttons_t *b = arg;
    hold_t combo;
    memset(&combo, 0, sizeof(combo));
    unsigned errors = 0;

    while (!atomic_load(&g_shared.quit)) {
        if (buttons_service(b, &g_shared) < 0) {
            if (++errors >= MAX_CONSECUTIVE_ERRORS) {
                LOG_ERR("demasiados errores en botones: se aborta");
                atomic_store(&g_shared.failed, 1);
                atomic_store(&g_shared.quit, 1);
                break;
            }
            sleep_ms(10);
            continue;
        }
        errors = 0;

        bool both = atomic_load(&g_shared.btn_level[0]) && atomic_load(&g_shared.btn_level[1]);
        if (hold_update(&combo, both, now_ms(), SHUTDOWN_HOLD_MS)) {
            LOG_INFO("combo de apagado detectado (ambos botones %d ms)", SHUTDOWN_HOLD_MS);
            atomic_store(&g_shared.poweroff, 1);
            atomic_store(&g_shared.quit, 1);
        }
    }
    return NULL;
}

/* ---------------- Tarea de ADC: potenciometros ---------------- */
static void *adc_task(void *arg)
{
    mcp3008_t *m = arg;
    static const unsigned channels[POT_COUNT] = { POT_CHANNEL_P1, POT_CHANNEL_P2 };
    pot_filter_t filt[POT_COUNT];
    memset(filt, 0, sizeof(filt));
    unsigned errors = 0;
    int64_t next = now_ns();

    while (!atomic_load(&g_shared.quit)) {
        bool ok = true;
        for (unsigned i = 0; i < POT_COUNT; i++) {
            uint16_t raw;
            if (mcp3008_read(m, channels[i], &raw) < 0) {
                ok = false;
                break;
            }
            atomic_store(&g_shared.pot_raw[i], pot_filter_step(&filt[i], raw));
        }
        if (!ok) {
            LOG_ERRNO("lectura MCP3008");
            atomic_fetch_add(&g_shared.adc_errors, 1u);
            if (++errors >= MAX_CONSECUTIVE_ERRORS) {
                LOG_ERR("demasiados errores en ADC: se aborta");
                atomic_store(&g_shared.failed, 1);
                atomic_store(&g_shared.quit, 1);
                break;
            }
        } else {
            errors = 0;
        }
        next += ADC_PERIOD_MS * NS_PER_MS;
        sleep_until_ns(next);
    }
    return NULL;
}

/* ---------------- Pantalla con recuperacion ---------------- */
typedef struct {
    lcd_t   lcd;
    bool    ok;
    int64_t retry_at_ms;
} display_t;

static void display_try_open(display_t *d, const options_t *o)
{
    d->ok = (lcd_open(&d->lcd, I2C_BUS_PATH, o->lcd_addr) == 0);
    d->retry_at_ms = now_ms() + LCD_RETRY_MS;
    if (d->ok)
        LOG_INFO("LCD lista en 0x%02X", o->lcd_addr);
    else
        LOG_WARN("LCD no disponible en 0x%02X: el sistema sigue sin pantalla", o->lcd_addr);
}

static void display_present(display_t *d, const options_t *o, const frame_t *f)
{
    if (o->use_lcd) {
        if (!d->ok && now_ms() >= d->retry_at_ms)
            display_try_open(d, o);          /* LCD desconectada en caliente: se recupera sola */
        if (d->ok) {
            for (unsigned r = 0; r < LCD_ROWS; r++) {
                if (lcd_print_row(&d->lcd, r, f->row[r]) < 0) {
                    LOG_WARN("fallo al escribir en la LCD: se reintentara");
                    lcd_close(&d->lcd);
                    d->ok = false;
                    d->retry_at_ms = now_ms() + LCD_RETRY_MS;
                    break;
                }
            }
        }
    }
    if (o->console) {
        static char last[LCD_ROWS][LCD_COLS + 1u];
        if (strcmp(last[0], f->row[0]) != 0 || strcmp(last[1], f->row[1]) != 0) {
            printf("|%s|\n|%s|\n", f->row[0], f->row[1]);
            fflush(stdout);
            memcpy(last, f->row, sizeof(last));
        }
    }
}

static int parse_options(int argc, char **argv, options_t *o)
{
    o->lcd_addr = LCD_I2C_ADDR_DEFAULT;
    o->use_lcd = true;
    o->console = false;
    static const struct option longopts[] = {
        { "lcd-addr", required_argument, NULL, 'a' },
        { "no-lcd",   no_argument,       NULL, 'n' },
        { "console",  no_argument,       NULL, 'c' },
        { "help",     no_argument,       NULL, 'h' },
        { NULL, 0, NULL, 0 }
    };
    int c;
    while ((c = getopt_long(argc, argv, "a:nch", longopts, NULL)) != -1) {
        switch (c) {
        case 'a': {
            char *end = NULL;
            unsigned long v = strtoul(optarg, &end, 0);
            if (*optarg == '\0' || *end != '\0' || v < 0x03ul || v > 0x77ul) {
                fprintf(stderr, "direccion I2C invalida: %s\n", optarg);
                return -1;
            }
            o->lcd_addr = (unsigned)v;
            break;
        }
        case 'n': o->use_lcd = false; o->console = true; break;
        case 'c': o->console = true; break;
        default:
            fprintf(stderr, "uso: %s [--lcd-addr 0x27|0x3F] [--no-lcd] [--console]\n", argv[0]);
            return -1;
        }
    }
    return 0;
}

static void count_boot(void)
{
    uint32_t boots = 0;
    if (storage_load_u32(BOOTCOUNT_FILE, &boots) < 0)
        boots = 0; /* primera vez, o archivo corrupto: se reinicia el conteo */
    boots++;
    if (storage_save_u32(BOOTCOUNT_FILE, boots) < 0)
        LOG_WARN("no se pudo guardar " BOOTCOUNT_FILE " (existe " DATA_DIR " y es escribible?)");
    else
        LOG_INFO("arranque numero %u", boots);
}

int main(int argc, char **argv)
{
    options_t opt;
    if (parse_options(argc, argv, &opt) < 0)
        return 2;

    shared_init(&g_shared);
    if (install_signals() < 0)
        return 1;

    /* Mejora (no vista en clase): bloquear la memoria evita fallos de pagina durante la partida. */
    if (mlockall(MCL_CURRENT | MCL_FUTURE) < 0)
        LOG_WARN("mlockall fallo (%s): continua sin bloquear memoria", strerror(errno));

    count_boot();

    buttons_t btn;
    mcp3008_t adc;
    display_t disp;
    memset(&disp, 0, sizeof(disp));
    int exit_code = 1;
    bool input_started = false, adc_started = false;
    pthread_t t_input, t_adc;

    if (buttons_open(&btn, &g_shared) < 0) return 1;
    if (mcp3008_open(&adc, SPI_DEVICE_PATH) < 0) { buttons_close(&btn); return 1; }
    if (opt.use_lcd)
        display_try_open(&disp, &opt);

    if (thread_start(&t_input, RT_PRIO_INPUT, input_task, &btn) < 0) goto cleanup;
    input_started = true;
    if (thread_start(&t_adc, RT_PRIO_ADC, adc_task, &adc) < 0) goto cleanup;
    adc_started = true;

    size_t n_games = 0;
    const game_ops_t *game = game_registry(&n_games);
    LOG_INFO("modo: %s", game->name);
    game->start();

    frame_t frame;
    int64_t last = now_ms();
    while (!g_stop && !atomic_load(&g_shared.quit)) {
        int64_t t = now_ms();
        game->update(&g_shared, (uint32_t)(t - last));
        last = t;
        game->render(&g_shared, &frame);
        display_present(&disp, &opt, &frame);
        sleep_ms(UI_PERIOD_MS);
    }

    if (atomic_load(&g_shared.poweroff) && disp.ok) {
        lcd_print_row(&disp.lcd, 0, "Apagando...");
        lcd_print_row(&disp.lcd, 1, "");
        sleep_ms(500);
    }
    exit_code = atomic_load(&g_shared.failed) ? 1
              : atomic_load(&g_shared.poweroff) ? EXIT_POWEROFF : 0;

cleanup:
    atomic_store(&g_shared.quit, 1);
    if (input_started) pthread_join(t_input, NULL);
    if (adc_started) pthread_join(t_adc, NULL);
    LOG_INFO("latencia de entrada: ultima %u us, maxima %u us (requisito < 50000 us)",
             atomic_load(&g_shared.latency_last_us), atomic_load(&g_shared.latency_max_us));
    if (disp.ok) lcd_close(&disp.lcd);
    mcp3008_close(&adc);
    buttons_close(&btn);
    return exit_code;
}
