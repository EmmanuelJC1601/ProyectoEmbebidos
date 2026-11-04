#include "util.h"
#include "config.h"
#include <sched.h>
#include <time.h>

int64_t now_ns(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        LOG_ERRNO("clock_gettime");
        return 0;
    }
    return (int64_t)ts.tv_sec * NS_PER_SEC + (int64_t)ts.tv_nsec;
}

int64_t now_ms(void)
{
    return now_ns() / NS_PER_MS;
}

void sleep_ms(unsigned ms)
{
    struct timespec ts = { .tv_sec = (time_t)(ms / 1000u),
                           .tv_nsec = (long)(ms % 1000u) * 1000000L };
    nanosleep(&ts, NULL); /* EINTR (senal de salida) es aceptable: el llamador revisa la bandera */
}

void sleep_until_ns(int64_t deadline_ns)
{
    struct timespec ts = { .tv_sec = (time_t)(deadline_ns / NS_PER_SEC),
                           .tv_nsec = (long)(deadline_ns % NS_PER_SEC) };
    clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, NULL);
}

int thread_start(pthread_t *t, int rt_prio, void *(*fn)(void *), void *arg)
{
    pthread_attr_t attr;
    int rc = pthread_attr_init(&attr);
    if (rc != 0) {
        errno = rc;
        LOG_ERRNO("pthread_attr_init");
        return -1;
    }
    rc = pthread_attr_setstacksize(&attr, THREAD_STACK_BYTES);
    if (rc != 0) {
        errno = rc;
        LOG_ERRNO("pthread_attr_setstacksize");
        pthread_attr_destroy(&attr);
        return -1;
    }

    if (rt_prio > 0) {
        struct sched_param sp = { .sched_priority = rt_prio };
        pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
        pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
        pthread_attr_setschedparam(&attr, &sp);
        rc = pthread_create(t, &attr, fn, arg);
        if (rc == 0) {
            pthread_attr_destroy(&attr);
            return 0;
        }
        LOG_WARN("SCHED_FIFO prio %d no disponible (%s): se usa planificacion normal",
                 rt_prio, strerror(rc));
        pthread_attr_setinheritsched(&attr, PTHREAD_INHERIT_SCHED);
    }

    rc = pthread_create(t, &attr, fn, arg);
    pthread_attr_destroy(&attr);
    if (rc != 0) {
        errno = rc;
        LOG_ERRNO("pthread_create");
        return -1;
    }
    return 0;
}
