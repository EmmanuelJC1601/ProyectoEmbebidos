/* util.h - reloj, espera y creacion de hilos. los mensajes van a stderr
 * para que journalctl -u arcade los guarde. */
#ifndef util_h
#define util_h
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define log_info(fmt, ...) fprintf(stderr, "[arcade] " fmt "\n", ##__VA_ARGS__)
#define log_aviso(fmt, ...) fprintf(stderr, "[arcade][aviso] " fmt "\n", ##__VA_ARGS__)
#define log_error(fmt, ...) fprintf(stderr, "[arcade][error] %s:%d " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#define log_errno(que) log_error("%s: %s", (que), strerror(errno))

/* clock_monotonic en nanosegundos. 0 solo si el reloj fallo (queda en el log). */
int64_t ahora_ns(void);
int64_t ahora_ms(void);
/* si una senal corta el sueno, regresa. el llamador revisa salir. */
void dormir_ms(unsigned ms);
/* duerme hasta un instante absoluto. no acumula atraso si el plazo ya paso. */
void dormir_hasta(int64_t plazo_ns);
/* prio > 0 pide sched_fifo. sin permiso (a mano, sin el servicio) sigue en cfs y avisa.
 * bitacora clase 8: la entrada tiene que poder adelantarse al dibujo. */
int iniciar_hilo(pthread_t *hilo, int prio, void *(*fn)(void *), void *arg);
#endif
