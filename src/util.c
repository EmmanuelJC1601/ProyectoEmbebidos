#include "util.h"
#include "constantes.h"
#include <sched.h>
#include <time.h>

/* lee clock_monotonic. es el mismo reloj que pone el kernel en el flanco del gpio,
 * asi la latencia no mezcla relojes. */
int64_t ahora_ns(void)
{
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
    log_errno("clock_gettime");
    return 0;
  }
  return (int64_t)ts.tv_sec * ns_por_s + (int64_t)ts.tv_nsec;
}

int64_t ahora_ms(void)
{
  return ahora_ns() / ns_por_ms;
}

void dormir_ms(unsigned ms)
{
  struct timespec ts;
  ts.tv_sec = (time_t)(ms / (unsigned)ms_por_s);
  ts.tv_nsec = (long)(ms % (unsigned)ms_por_s) * (long)ns_por_ms;
  if (nanosleep(&ts, NULL) != 0 && errno != EINTR)
    log_errno("nanosleep");
}

void dormir_hasta(int64_t plazo_ns)
{
  struct timespec ts;
  ts.tv_sec = (time_t)(plazo_ns / ns_por_s);
  ts.tv_nsec = (long)(plazo_ns % ns_por_s);
  if (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, NULL) != 0 && errno != EINTR)
    log_errno("clock_nanosleep");
}

int iniciar_hilo(pthread_t *hilo, int prio, void *(*fn)(void *), void *arg)
{
  pthread_attr_t attr;
  int rc = pthread_attr_init(&attr);
  if (rc != 0) {
    errno = rc;
    log_errno("pthread_attr_init");
    return -1;
  }
  rc = pthread_attr_setstacksize(&attr, (size_t)pila_hilo);
  if (rc != 0) {
    errno = rc;
    log_errno("pthread_attr_setstacksize");
    pthread_attr_destroy(&attr);
    return -1;
  }
  /* bitacora clase 8: sched_fifo en la entrada. un cuadro largo de spi no puede
   * comerse los 50 ms. si no hay cap_sys_nice el programa avisa y sigue. */
  if (prio > 0) {
    struct sched_param sp;
    sp.sched_priority = prio;
    if (pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED) != 0 ||
        pthread_attr_setschedpolicy(&attr, SCHED_FIFO) != 0 ||
        pthread_attr_setschedparam(&attr, &sp) != 0) {
      log_aviso("no se pudo armar sched_fifo %d", prio);
    }
    rc = pthread_create(hilo, &attr, fn, arg);
    if (rc == 0) {
      pthread_attr_destroy(&attr);
      return 0;
    }
    log_aviso("sched_fifo %d no disponible (%s): sigue el planificador normal", prio, strerror(rc));
    pthread_attr_setinheritsched(&attr, PTHREAD_INHERIT_SCHED);
  }
  rc = pthread_create(hilo, &attr, fn, arg);
  pthread_attr_destroy(&attr);
  if (rc != 0) {
    errno = rc;
    log_errno("pthread_create");
    return -1;
  }
  return 0;
}
