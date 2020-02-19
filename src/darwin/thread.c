/*****************************************************************************
 * thread.c : pthread back-end for LibVLC
 *****************************************************************************
 * Copyright (C) 1999-2013 VLC authors and VideoLAN
 *
 * Authors: Jean-Marc Dressler <polux@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *          Gildas Bazin <gbazin@netcourrier.com>
 *          Clément Sténac
 *          Rémi Denis-Courmont
 *          Felix Paul Kühne <fkuehne # videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_atomic.h>

#include "libvlc.h"
#include <signal.h>
#include <errno.h>
#include <assert.h>

#include <pthread.h>
#include <mach/mach_init.h> /* mach_task_self in semaphores */
#include <mach/mach_time.h>
#include <execinfo.h>

static struct {
    uint32_t quotient;
    uint32_t remainder;
    uint32_t divider;
} vlc_clock_conversion;

static void vlc_clock_setup_once (void)
{
    mach_timebase_info_data_t timebase;
    if (unlikely(mach_timebase_info (&timebase) != 0))
        abort ();
    lldiv_t d = lldiv (timebase.numer, timebase.denom);
    vlc_clock_conversion.quotient = (uint32_t)d.quot;
    vlc_clock_conversion.remainder = (uint32_t)d.rem;
    vlc_clock_conversion.divider = timebase.denom;
}

static pthread_once_t vlc_clock_once = PTHREAD_ONCE_INIT;

#define vlc_clock_setup() \
    pthread_once(&vlc_clock_once, vlc_clock_setup_once)

/* Print a backtrace to the standard error for debugging purpose. */
void vlc_trace (const char *fn, const char *file, unsigned line)
{
     fprintf (stderr, "at %s:%u in %s\n", file, line, fn);
     fflush (stderr); /* needed before switch to low-level I/O */
     void *stack[20];
     int len = backtrace (stack, sizeof (stack) / sizeof (stack[0]));
     backtrace_symbols_fd (stack, len, 2);
     fsync (2);
}

#ifndef NDEBUG
/* Reports a fatal error from the threading layer, for debugging purposes. */
static void
vlc_thread_fatal (const char *action, int error,
                  const char *function, const char *file, unsigned line)
{
    int canc = vlc_savecancel ();
    fprintf (stderr, "LibVLC fatal error %s (%d) in thread %lu ",
             action, error, vlc_thread_id ());
    vlc_trace (function, file, line);

    char buf[1000];
    const char *msg;

    switch (strerror_r (error, buf, sizeof (buf)))
    {
        case 0:
            msg = buf;
            break;
        case ERANGE: /* should never happen */
            msg = "unknown (too big to display)";
            break;
        default:
            msg = "unknown (invalid error number)";
            break;
    }
    fprintf (stderr, " Error message: %s\n", msg);
    fflush (stderr);

    vlc_restorecancel (canc);
    abort ();
}

# define VLC_THREAD_ASSERT( action ) \
    if (unlikely(val)) \
        vlc_thread_fatal (action, val, __func__, __FILE__, __LINE__)
#else
# define VLC_THREAD_ASSERT( action ) ((void)val)
#endif

/* Initializes a fast mutex. */
void vlc_mutex_init( vlc_mutex_t *p_mutex )
{
    pthread_mutexattr_t attr;

    if (unlikely(pthread_mutexattr_init (&attr)))
        abort();
#ifdef NDEBUG
    pthread_mutexattr_settype (&attr, PTHREAD_MUTEX_DEFAULT);
#else
    pthread_mutexattr_settype (&attr, PTHREAD_MUTEX_ERRORCHECK);
#endif
    if (unlikely(pthread_mutex_init (p_mutex, &attr)))
        abort();
    pthread_mutexattr_destroy( &attr );
}

void vlc_mutex_init_recursive( vlc_mutex_t *p_mutex )
{
    pthread_mutexattr_t attr;

    if (unlikely(pthread_mutexattr_init (&attr)))
        abort();
    pthread_mutexattr_settype (&attr, PTHREAD_MUTEX_RECURSIVE);
    if (unlikely(pthread_mutex_init (p_mutex, &attr)))
        abort();
    pthread_mutexattr_destroy( &attr );
}


void vlc_mutex_destroy (vlc_mutex_t *p_mutex)
{
    int val = pthread_mutex_destroy( p_mutex );
    VLC_THREAD_ASSERT ("destroying mutex");
}

void vlc_mutex_lock (vlc_mutex_t *p_mutex)
{
    int val = pthread_mutex_lock( p_mutex );
    VLC_THREAD_ASSERT ("locking mutex");
    vlc_mutex_mark(p_mutex);
}

int vlc_mutex_trylock (vlc_mutex_t *p_mutex)
{
    int val = pthread_mutex_trylock( p_mutex );

    if (val != EBUSY) {
        VLC_THREAD_ASSERT ("locking mutex");
        vlc_mutex_mark(p_mutex);
    }
    return val;
}

void vlc_mutex_unlock (vlc_mutex_t *p_mutex)
{
    int val = pthread_mutex_unlock( p_mutex );
    /* FIXME: We can't check for the success of the unlock
     * here as due to a bug in Apple pthread implementation.
     * The `pthread_cond_wait` function does not behave like
     * it should According to POSIX, pthread_cond_wait is a
     * cancellation point and when a thread is cancelled while
     * in a condition wait, the mutex is re-acquired before
     * calling the first cancellation cleanup handler:
     *
     * > The effect is as if the thread were unblocked, allowed
     * > to execute up to the point of returning from the call to
     * > pthread_cond_timedwait() or pthread_cond_wait(), but at
     * > that point notices the cancellation request and instead
     * > of returning to the caller of pthread_cond_timedwait()
     * > or pthread_cond_wait(), starts the thread cancellation
     * > activities, which includes calling cancellation cleanup
     * > handlers.
     *
     * Unfortunately the mutex is not locked sometimes, causing
     * the call to `pthread_mutex_unlock` to fail.
     * Until this is fixed, enabling this assertion would lead to
     * spurious test failures and VLC crashes when compiling with
     * debug enabled, which would make it nearly impossible to
     * proeprly test with debug builds on macOS.
     * This was reported to Apple as FB6152751.
     */
#ifndef NDEBUG
    if (val != EPERM)
        VLC_THREAD_ASSERT ("unlocking mutex");
#endif
    vlc_mutex_unmark(p_mutex);
}

void vlc_rwlock_init (vlc_rwlock_t *lock)
{
    if (unlikely(pthread_rwlock_init (lock, NULL)))
        abort ();
}

void vlc_rwlock_destroy (vlc_rwlock_t *lock)
{
    int val = pthread_rwlock_destroy (lock);
    VLC_THREAD_ASSERT ("destroying R/W lock");
}

void vlc_rwlock_rdlock (vlc_rwlock_t *lock)
{
    int val = pthread_rwlock_rdlock (lock);
    VLC_THREAD_ASSERT ("acquiring R/W lock for reading");
}

void vlc_rwlock_wrlock (vlc_rwlock_t *lock)
{
    int val = pthread_rwlock_wrlock (lock);
    VLC_THREAD_ASSERT ("acquiring R/W lock for writing");
}

void vlc_rwlock_unlock (vlc_rwlock_t *lock)
{
    int val = pthread_rwlock_unlock (lock);
    VLC_THREAD_ASSERT ("releasing R/W lock");
}

void vlc_once(vlc_once_t *once, void (*cb)(void))
{
    int val = pthread_once(once, cb);
    VLC_THREAD_ASSERT("initializing once");
}

int vlc_threadvar_create (vlc_threadvar_t *key, void (*destr) (void *))
{
    return pthread_key_create (key, destr);
}

void vlc_threadvar_delete (vlc_threadvar_t *p_tls)
{
    pthread_key_delete (*p_tls);
}

int vlc_threadvar_set (vlc_threadvar_t key, void *value)
{
    return pthread_setspecific (key, value);
}

void *vlc_threadvar_get (vlc_threadvar_t key)
{
    return pthread_getspecific (key);
}

void vlc_threads_setup (libvlc_int_t *p_libvlc)
{
    (void) p_libvlc;
}

static int vlc_clone_attr (vlc_thread_t *th, pthread_attr_t *attr,
                           void *(*entry) (void *), void *data, int priority)
{
    int ret;

    sigset_t oldset;
    {
        sigset_t set;
        sigemptyset (&set);
        sigdelset (&set, SIGHUP);
        sigaddset (&set, SIGINT);
        sigaddset (&set, SIGQUIT);
        sigaddset (&set, SIGTERM);

        sigaddset (&set, SIGPIPE); /* We don't want this one, really! */
        pthread_sigmask (SIG_BLOCK, &set, &oldset);
    }

    (void) priority;

#define VLC_STACKSIZE (128 * sizeof (void *) * 1024)

#ifdef VLC_STACKSIZE
    ret = pthread_attr_setstacksize (attr, VLC_STACKSIZE);
    assert (ret == 0); /* fails iif VLC_STACKSIZE is invalid */
#endif

    ret = pthread_create (th, attr, entry, data);
    pthread_sigmask (SIG_SETMASK, &oldset, NULL);
    pthread_attr_destroy (attr);
    return ret;
}

int vlc_clone (vlc_thread_t *th, void *(*entry) (void *), void *data,
               int priority)
{
    pthread_attr_t attr;

    pthread_attr_init (&attr);
    return vlc_clone_attr (th, &attr, entry, data, priority);
}

void vlc_join (vlc_thread_t handle, void **result)
{
    int val = pthread_join (handle, result);
    VLC_THREAD_ASSERT ("joining thread");
}

int vlc_clone_detach (vlc_thread_t *th, void *(*entry) (void *), void *data,
                      int priority)
{
    vlc_thread_t dummy;
    pthread_attr_t attr;

    if (th == NULL)
        th = &dummy;

    pthread_attr_init (&attr);
    pthread_attr_setdetachstate (&attr, PTHREAD_CREATE_DETACHED);
    return vlc_clone_attr (th, &attr, entry, data, priority);
}

vlc_thread_t vlc_thread_self (void)
{
    return pthread_self ();
}

unsigned long vlc_thread_id (void)
{
    return -1;
}

int vlc_set_priority (vlc_thread_t th, int priority)
{
    (void) th; (void) priority;
    return VLC_SUCCESS;
}

void vlc_cancel (vlc_thread_t thread_id)
{
    pthread_cancel (thread_id);
}

int vlc_savecancel (void)
{
    int state;
    int val = pthread_setcancelstate (PTHREAD_CANCEL_DISABLE, &state);

    VLC_THREAD_ASSERT ("saving cancellation");
    return state;
}

void vlc_restorecancel (int state)
{
#ifndef NDEBUG
    int oldstate, val;

    val = pthread_setcancelstate (state, &oldstate);
    VLC_THREAD_ASSERT ("restoring cancellation");

    if (unlikely(oldstate != PTHREAD_CANCEL_DISABLE))
         vlc_thread_fatal ("restoring cancellation while not disabled", EINVAL,
                           __func__, __FILE__, __LINE__);
#else
    pthread_setcancelstate (state, NULL);
#endif
}

void vlc_testcancel (void)
{
    pthread_testcancel ();
}

vlc_tick_t vlc_tick_now (void)
{
    vlc_clock_setup();
    uint64_t date = mach_absolute_time();

    date = date * vlc_clock_conversion.quotient +
        date * vlc_clock_conversion.remainder / vlc_clock_conversion.divider;
    return VLC_TICK_FROM_NS(date);
}

#undef vlc_tick_wait
void vlc_tick_wait (vlc_tick_t deadline)
{
    deadline -= vlc_tick_now ();
    if (deadline > 0)
        vlc_tick_sleep (deadline);
}

#undef vlc_tick_sleep
void vlc_tick_sleep (vlc_tick_t delay)
{
    struct timespec ts = timespec_from_vlc_tick (delay);

    /* nanosleep uses mach_absolute_time and mach_wait_until internally,
       but also handles kernel errors. Thus we use just this. */
    while (nanosleep (&ts, &ts) == -1)
        assert (errno == EINTR);
}

unsigned vlc_GetCPUCount(void)
{
    return sysconf(_SC_NPROCESSORS_CONF);
}
