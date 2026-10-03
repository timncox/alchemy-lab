/* Mark's engine on the host behaves as on the module (src/host_stubs.c):
 * no threads, no dynamic loading, no directories. With real pthreads the
 * engine's FX-loader thread starts on the Mac -- it never does on the M7,
 * where pthread_create returns EAGAIN -- and races the audio thread over the
 * bump allocator (seen as a garbage fx_pending pointer and a segfault in
 * fx_apply_pending). Force-included into mark_core_alchemy.c only. */
#pragma once
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>
#include <stddef.h>
#include <sys/stat.h>

static inline int emu_mark_pthread_create(pthread_t* t, const pthread_attr_t* a,
                                          void* (*fn)(void*), void* arg)
{
    (void)t; (void)a; (void)fn; (void)arg;
    return EAGAIN;
}
static inline void* emu_mark_dlopen(const char* p, int f) { (void)p; (void)f; return NULL; }
static inline DIR*  emu_mark_opendir(const char* p) { (void)p; errno = ENOENT; return NULL; }
static inline int   emu_mark_mkdir(const char* p, mode_t m) { (void)p; (void)m; errno = ENOSYS; return -1; }
#define pthread_create emu_mark_pthread_create
#define dlopen         emu_mark_dlopen
#define opendir        emu_mark_opendir
#define mkdir          emu_mark_mkdir
