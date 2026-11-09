#include "archivo.h"
#include "constantes.h"
#include "util.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static const char *ruta_activa;

/* fsync del directorio: sin esto el rename puede quedar solo en cache. */
static int sincronizar_dir(const char *ruta)
{
  char dir[largo_ruta];
  char *corte;
  int fd;
  int rc;
  if (strlen(ruta) >= sizeof dir) {
    errno = ENAMETOOLONG;
    return -1;
  }
  memcpy(dir, ruta, strlen(ruta) + 1u);
  corte = strrchr(dir, '/');
  if (corte)
    *corte = '\0';
  else
    memcpy(dir, ".", 2u);
  fd = open(dir, O_RDONLY | O_DIRECTORY);
  if (fd < 0)
    return -1;
  rc = fsync(fd);
  if (close(fd) != 0 && rc == 0)
    rc = -1;
  return rc;
}

void archivo_usar(const char *ruta)
{
  ruta_activa = ruta;
}

int guardar_datos(const char *ruta, uint32_t arranques, uint32_t record)
{
  char tmp[largo_ruta];
  char num[largo_num];
  int ntmp;
  int nnum;
  int fd;
  ntmp = snprintf(tmp, sizeof tmp, "%s.tmp", ruta);
  if (ntmp < 0 || (size_t)ntmp >= sizeof tmp) {
    errno = ENAMETOOLONG;
    return -1;
  }
  nnum = snprintf(num, sizeof num, "%u %u\n", arranques, record);
  if (nnum < 0 || (size_t)nnum >= sizeof num) {
    errno = EOVERFLOW;
    return -1;
  }
  fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd < 0) {
    log_errno("open tmp");
    return -1;
  }
  if (write(fd, num, (size_t)nnum) != nnum) {
    log_errno("write");
    close(fd);
    unlink(tmp);
    return -1;
  }
  if (fsync(fd) != 0) {
    log_errno("fsync");
    close(fd);
    unlink(tmp);
    return -1;
  }
  if (close(fd) != 0) {
    log_errno("close");
    unlink(tmp);
    return -1;
  }
  if (rename(tmp, ruta) != 0) {
    log_errno("rename");
    unlink(tmp);
    return -1;
  }
  if (sincronizar_dir(ruta) != 0) {
    log_errno("fsync dir");
    return -1;
  }
  return 0;
}

int guardar_record(uint32_t arranques, uint32_t record)
{
  if (!ruta_activa)
    return 0;
  return guardar_datos(ruta_activa, arranques, record);
}

static int leer_texto(const char *ruta, char *buf, size_t nbuf)
{
  int fd;
  ssize_t n;
  fd = open(ruta, O_RDONLY);
  if (fd < 0)
    return -1;
  n = read(fd, buf, nbuf - 1u);
  if (close(fd) != 0) {
    log_errno("close lectura");
    return -1;
  }
  if (n < 0)
    return -1;
  if (n == 0) {
    errno = EINVAL;
    return -1;
  }
  buf[n] = '\0';
  return 0;
}

int cargar_datos(const char *ruta, uint32_t *arranques, uint32_t *record)
{
  char buf[largo_num];
  char *fin = NULL;
  char *fin2 = NULL;
  unsigned long a;
  unsigned long r;
  if (leer_texto(ruta, buf, sizeof buf) != 0)
    return -1;
  errno = 0;
  a = strtoul(buf, &fin, 10);
  if (errno != 0 || fin == buf || *fin != ' ' || a > UINT32_MAX) {
    errno = EINVAL;
    return -1;
  }
  errno = 0;
  r = strtoul(fin + 1, &fin2, 10);
  if (errno != 0 || fin2 == fin + 1 || (*fin2 != '\n' && *fin2 != '\0') || r > UINT32_MAX) {
    errno = EINVAL;
    return -1;
  }
  *arranques = (uint32_t)a;
  *record = (uint32_t)r;
  return 0;
}

int cargar_entero(const char *ruta, uint32_t *valor)
{
  char buf[largo_num];
  char *fin = NULL;
  unsigned long v;
  if (leer_texto(ruta, buf, sizeof buf) != 0)
    return -1;
  errno = 0;
  v = strtoul(buf, &fin, 10);
  if (errno != 0 || fin == buf || (*fin != '\n' && *fin != '\0') || v > UINT32_MAX) {
    errno = EINVAL;
    return -1;
  }
  *valor = (uint32_t)v;
  return 0;
}

int asegurar_dir(const char *dir)
{
  if (mkdir(dir, 0755) == 0)
    return 0;
  if (errno == EEXIST)
    return 0;
  log_errno("mkdir");
  return -1;
}
