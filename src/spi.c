#define _GNU_SOURCE
#include "spi.h"
#include "constantes.h"
#include "util.h"
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>

static pthread_mutex_t bus;
static int lista;

/* bitacora clase 9: mutex con herencia de prioridad.
 * si el hilo del juego (prioridad normal) tiene el bus y el adc (fifo 40)
 * lo necesita, el juego hereda la prioridad y suelta el bus antes. */
int spi_iniciar(void)
{
  pthread_mutexattr_t attr;
  int rc;
  if (lista)
    return 0;
  if (pthread_mutexattr_init(&attr) != 0) {
    log_error("pthread_mutexattr_init");
    return -1;
  }
  if (pthread_mutexattr_setprotocol(&attr, PTHREAD_PRIO_INHERIT) != 0)
    log_aviso("mutex sin herencia de prioridad");
  rc = pthread_mutex_init(&bus, &attr);
  if (pthread_mutexattr_destroy(&attr) != 0)
    log_aviso("pthread_mutexattr_destroy");
  if (rc != 0) {
    errno = rc;
    log_errno("pthread_mutex_init");
    return -1;
  }
  lista = 1;
  return 0;
}

void spi_cerrar(void)
{
  if (!lista)
    return;
  if (pthread_mutex_destroy(&bus) != 0)
    log_aviso("pthread_mutex_destroy");
  lista = 0;
}

int spi_tomar(void)
{
  int rc = pthread_mutex_lock(&bus);
  if (rc != 0) {
    errno = rc;
    log_errno("pthread_mutex_lock");
    return -1;
  }
  return 0;
}

void spi_soltar(void)
{
  int rc = pthread_mutex_unlock(&bus);
  if (rc != 0) {
    errno = rc;
    log_errno("pthread_mutex_unlock");
  }
}

int spi_preparar(int fd, uint32_t hz)
{
  uint8_t modo = SPI_MODE_0;
  uint8_t bits = (uint8_t)bits_spi;
  uint32_t reloj = hz;
  if (ioctl(fd, SPI_IOC_WR_MODE, &modo) < 0)
    return -1;
  if (ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0)
    return -1;
  if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &reloj) < 0)
    return -1;
  return 0;
}

/* la tft escribe sin leer. si rx es null el fifo del controlador se queda
 * con basura y la muestra siguiente del mcp3008 sale en cero. por eso aqui
 * siempre hay buffer de lectura. el llamador ya tiene el mutex del bus. */
int spi_transferir(int fd, const uint8_t *tx, uint8_t *rx, size_t n, uint32_t hz)
{
  static uint8_t descarte[tam_bloque];
  struct spi_ioc_transfer tr;
  int intentos = 0;
  if (n == 0)
    return 0;
  if (n > sizeof descarte) {
    errno = EINVAL;
    return -1;
  }
  if (!rx)
    rx = descarte;
  memset(&tr, 0, sizeof tr);
  tr.tx_buf = (unsigned long)tx;
  tr.rx_buf = (unsigned long)rx;
  tr.len = (unsigned)n;
  tr.speed_hz = hz;
  tr.bits_per_word = (uint8_t)bits_spi;
  while (intentos < 3) {
    if (ioctl(fd, SPI_IOC_MESSAGE(1), &tr) >= 0)
      return 0;
    if (errno != EINTR)
      return -1;
    intentos++;
  }
  return -1;
}
