/* Pruebas de logica pura (se ejecutan en cualquier PC). Cada caso imprime esperado vs obtenido.
 * Los casos marcados FALLA verifican que el codigo rechaza entradas invalidas. */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "config.h"
#include "filter.h"
#include "game.h"
#include "mcp3008.h"
#include "storage.h"

static int g_total, g_fail;

#define CHECK(name, cond) do { g_total++; \
    if (cond) printf("[ OK ] %s\n", name); \
    else { g_fail++; printf("[FAIL] %s\n", name); } } while (0)

static void test_mcp3008(void)
{
    uint8_t tx[3], rx[3];
    mcp3008_build_tx(0, tx);
    CHECK("MCP3008 trama canal 0 = 01 80 00", tx[0] == 0x01 && tx[1] == 0x80 && tx[2] == 0x00);
    mcp3008_build_tx(1, tx);
    CHECK("MCP3008 trama canal 1 = 01 90 00", tx[1] == 0x90);
    rx[0] = 0; rx[1] = 0x03; rx[2] = 0xFF;
    CHECK("MCP3008 decode extremo alto = 1023", mcp3008_decode(rx) == 1023);
    rx[1] = 0xFC; rx[2] = 0x00;  /* bits basura arriba: la mascara los quita */
    CHECK("MCP3008 decode ignora bits no validos = 0", mcp3008_decode(rx) == 0);
    rx[1] = 0x02; rx[2] = 0x00;
    CHECK("MCP3008 decode 512", mcp3008_decode(rx) == 512);
    mcp3008_t m = { .fd = -1 };
    uint16_t v;
    CHECK("FALLA: canal 8 invalido -> EINVAL", mcp3008_read(&m, 8, &v) == -1 && errno == EINVAL);
}

static void test_filter(void)
{
    pot_filter_t f; memset(&f, 0, sizeof(f));
    CHECK("Filtro: primera muestra pasa directa", pot_filter_step(&f, 500) == 500);
    CHECK("Filtro: ruido +2 se ignora", pot_filter_step(&f, 502) == 500);
    CHECK("Filtro: ruido -2 se ignora", pot_filter_step(&f, 498) == 500);
    uint16_t y = pot_filter_step(&f, 600);
    CHECK("Filtro: salto grande avanza 1/4 (525)", y == 525);
    for (int i = 0; i < 40; i++) y = pot_filter_step(&f, 600);
    CHECK("Filtro: converge a +-2 del objetivo", y >= 598 && y <= 600);
    for (int i = 0; i < 80; i++) y = pot_filter_step(&f, 5000);  /* fuera de rango */
    CHECK("FALLA: muestra > 1023 se satura a 1023", y <= ADC_MAX && y >= ADC_MAX - ADC_DEADBAND);
    CHECK("adc_scale 0 -> 0", adc_scale(0, 100) == 0);
    CHECK("adc_scale 1023 -> 100", adc_scale(1023, 100) == 100);
    CHECK("adc_scale 512 -> 50", adc_scale(512, 100) == 50);
}

static void test_hold(void)
{
    hold_t h; memset(&h, 0, sizeof(h));
    CHECK("Combo: inicia sin disparar", !hold_update(&h, true, 1000, 3000));
    CHECK("Combo: 2 s no dispara", !hold_update(&h, true, 3000, 3000));
    CHECK("Combo: 3 s dispara", hold_update(&h, true, 4000, 3000));
    CHECK("Combo: dispara una sola vez", !hold_update(&h, true, 5000, 3000));
    hold_update(&h, false, 5100, 3000);
    hold_update(&h, true, 6000, 3000);
    CHECK("FALLA: soltar y volver a presionar reinicia la cuenta", !hold_update(&h, true, 8000, 3000));
}

static void test_frame(void)
{
    frame_t f;
    frame_set_row(&f, 0, "hola");
    CHECK("Frame: rellena con espacios a 16", strlen(f.row[0]) == LCD_COLS && f.row[0][4] == ' ');
    frame_set_row(&f, 1, "texto demasiado largo para la pantalla");
    CHECK("FALLA: texto largo se trunca a 16", strlen(f.row[1]) == LCD_COLS);

    shared_t s; shared_init(&s);
    atomic_store(&s.pot_raw[0], 1023u);
    atomic_store(&s.pot_raw[1], 7u);
    atomic_store(&s.btn_level[0], 1);
    atomic_store(&s.btn_presses[1], 12u);
    size_t n; const game_ops_t *g = game_registry(&n);
    g->render(&s, &f);
    CHECK("Diag fila 0 = 'P1:1023 P2:   7'", strcmp(f.row[0], "P1:1023 P2:   7 ") == 0);
    CHECK("Diag fila 1 = 'B1:#0   B2:-12 '", strcmp(f.row[1], "B1:#0   B2:-12  ") == 0);
}

static void test_storage(void)
{
    char path[] = "/tmp/arcade_test_XXXXXX";
    int fd = mkstemp(path); close(fd);
    uint32_t v = 0;
    CHECK("Storage: guardar 1234", storage_save_u32(path, 1234) == 0);
    CHECK("Storage: leer 1234", storage_load_u32(path, &v) == 0 && v == 1234);
    CHECK("Storage: sin archivo .tmp residual", access("/tmp/arcade_test.tmp", F_OK) != 0);
    FILE *fp = fopen(path, "w"); fputs("basura\n", fp); fclose(fp);
    CHECK("FALLA: archivo corrupto se rechaza", storage_load_u32(path, &v) == -1);
    unlink(path);
    CHECK("FALLA: archivo inexistente se rechaza", storage_load_u32(path, &v) == -1);
    CHECK("FALLA: directorio inexistente al guardar", storage_save_u32("/no/existe/x", 1) == -1);
}

int main(void)
{
    test_mcp3008(); test_filter(); test_hold(); test_frame(); test_storage();
    printf("\n%d pruebas, %d fallidas\n", g_total, g_fail);
    return g_fail ? 1 : 0;
}
