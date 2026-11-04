#ifndef UTIL_H
#define UTIL_H
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Los mensajes van a stderr: con systemd quedan en journalctl -u arcade */
#define LOG_INFO(fmt, ...)  fprintf(stderr, "[arcade] " fmt "\n", ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  fprintf(stderr, "[arcade][WARN] " fmt "\n", ##__VA_ARGS__)
#define LOG_ERR(fmt, ...)   fprintf(stderr, "[arcade][ERR] %s:%d " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#define LOG_ERRNO(what)     LOG_ERR("%s: %s", (what), strerror(errno))

int64_t now_ns(void);                 /* CLOCK_MONOTONIC */
int64_t now_ms(void);
void sleep_ms(unsigned ms);
void sleep_until_ns(int64_t deadline_ns); /* Periodo sin deriva: usa TIMER_ABSTIME */

/* Crea un hilo con stack fijo; con prio > 0 intenta SCHED_FIFO y si no hay permiso cae a normal. */
int thread_start(pthread_t *t, int rt_prio, void *(*fn)(void *), void *arg);
#endif
