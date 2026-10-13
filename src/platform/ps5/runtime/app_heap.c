// ps5-homebrew-ui - Process-lifetime heap for the OpenGL runtime.
// Copyright (C) 2026 BlackBearReloaded
// Regie5: the largest of 256, 192 or 128 MiB the console grants, regie5_heap_state/stats/attempts
// for the log, a report on any failed allocation (2026).
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Adapted from ps5-opengl native-app/app_heap.c. tools/build.sh links with
// --wrap for the malloc family whenever this file exists, routing allocations
// into a fixed sceLibcMspace that is never unmapped.

#include <errno.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

void *__real_malloc(size_t size);
void *__real_calloc(size_t count, size_t size);
void *__real_realloc(void *address, size_t size);
void __real_free(void *address);
int __real_posix_memalign(void **address, size_t alignment, size_t size);
size_t __real_malloc_usable_size(const void *address);

void *sceLibcMspaceCreate(const char *name, void *base, size_t size, unsigned flags);
void *sceLibcMspaceMalloc(void *mspace, size_t size);
void *sceLibcMspaceCalloc(void *mspace, size_t count, size_t size);
void *sceLibcMspaceRealloc(void *mspace, void *address, size_t size);
void sceLibcMspaceFree(void *mspace, void *address);
int sceLibcMspacePosixMemalign(void *mspace, void **address, size_t alignment, size_t size);
size_t sceLibcMspaceMallocUsableSize(const void *address);

/* Fixed, process-lifetime heap: do not unmap underneath late C++ destructors.
 * Regie5: 512 MiB was refused on a 13.60 console, 128 MiB was granted; the
 * sizes are tried from the largest down and the first granted is kept. */
static const size_t hui_heap_sizes[] = {256u << 20, 192u << 20, 128u << 20};
#define HUI_HEAP_TRIES (sizeof hui_heap_sizes / sizeof hui_heap_sizes[0])

static atomic_int hui_heap_state;
static void *hui_heap_base;
static void *hui_heap_mspace;
static size_t hui_heap_size;
/* What each size gave, for the log: 0 granted, -1 the mspace refused it,
 * else mmap's errno. Written once, before the state is published. */
static int hui_heap_tried[HUI_HEAP_TRIES];
static int hui_heap_tries;
/* Owned-heap usable bytes only; relaxed counters are observations, not fences. */
static atomic_size_t hui_heap_live_bytes, hui_heap_peak_bytes, hui_heap_blocks;
static atomic_size_t hui_heap_failures;

/* Regie5: told when the heap cannot give a block (platform/ps5/system_ps5.cpp). */
void regie5_alloc_failed(size_t asked, size_t live_bytes, int system) __attribute__((weak));

static void hui_heap_failed(size_t asked, int system)
{
    if (regie5_alloc_failed)
        regie5_alloc_failed(asked,
                            atomic_load_explicit(&hui_heap_live_bytes, memory_order_relaxed), system);
}

static void hui_heap_resize_stats(size_t before, size_t after)
{
    size_t live = after >= before ? atomic_fetch_add_explicit(&hui_heap_live_bytes, after - before,
                                                              memory_order_relaxed) +
                                        after - before
                                  : atomic_fetch_sub_explicit(&hui_heap_live_bytes, before - after,
                                                              memory_order_relaxed) -
                                        (before - after);
    size_t peak = atomic_load_explicit(&hui_heap_peak_bytes, memory_order_relaxed);
    while (live > peak &&
           !atomic_compare_exchange_weak_explicit(&hui_heap_peak_bytes, &peak, live,
                                                  memory_order_relaxed, memory_order_relaxed))
    {
    }
}

static void *hui_heap_record_allocation(void *address, size_t asked)
{
    if (address)
    {
        hui_heap_resize_stats(0, sceLibcMspaceMallocUsableSize(address));
        atomic_fetch_add_explicit(&hui_heap_blocks, 1, memory_order_relaxed);
    }
    else if (asked)
    {
        atomic_fetch_add_explicit(&hui_heap_failures, 1, memory_order_relaxed);
        hui_heap_failed(asked, 0);
    }
    return address;
}

/* Regie5: the system's malloc, used only while the heap is not up, is small
 * on a console: a failure there is reported too (a failed new traps at once). */
static void *hui_heap_system_result(void *address, size_t asked)
{
    if (address == NULL && asked)
        hui_heap_failed(asked, 1);
    return address;
}

static int hui_heap_ready(void)
{
    int state = atomic_load_explicit(&hui_heap_state, memory_order_acquire);
    if (state == 2)
        return 1;
    if (state != 0)
        return 0;

    int expected = 0;
    if (!atomic_compare_exchange_strong_explicit(&hui_heap_state, &expected, 1,
                                                 memory_order_acq_rel, memory_order_acquire))
        return expected == 2;

    for (size_t i = 0; i < HUI_HEAP_TRIES; i++)
    {
        size_t size = hui_heap_sizes[i];
        hui_heap_tries = (int)i + 1;
        errno = 0;
        void *base = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
        if (base == MAP_FAILED)
        {
            hui_heap_tried[i] = errno ? errno : 1;
            continue;
        }
        void *mspace = sceLibcMspaceCreate("regie5", base, size, 0);
        if (mspace == NULL)
        {
            hui_heap_tried[i] = -1;
            munmap(base, size);
            continue;
        }
        hui_heap_tried[i] = 0;
        hui_heap_base = base;
        hui_heap_size = size;
        hui_heap_mspace = mspace;
        break;
    }
    if (hui_heap_mspace == NULL)
    {
        atomic_store_explicit(&hui_heap_state, -1, memory_order_release);
        return 0;
    }

    atomic_store_explicit(&hui_heap_state, 2, memory_order_release);
    return 1;
}

static int hui_heap_owns(const void *address)
{
    /* Acquire publication before reading non-atomic heap metadata. */
    if (atomic_load_explicit(&hui_heap_state, memory_order_acquire) != 2)
        return 0;
    uintptr_t value = (uintptr_t)address;
    uintptr_t base = (uintptr_t)hui_heap_base;
    return value >= base && value - base < hui_heap_size;
}

void *__wrap_malloc(size_t size)
{
    return hui_heap_ready()
               ? hui_heap_record_allocation(sceLibcMspaceMalloc(hui_heap_mspace, size), size)
               : hui_heap_system_result(__real_malloc(size), size);
}

void *__wrap_calloc(size_t count, size_t size)
{
    return hui_heap_ready()
               ? hui_heap_record_allocation(sceLibcMspaceCalloc(hui_heap_mspace, count, size),
                                            count * size)
               : hui_heap_system_result(__real_calloc(count, size), count * size);
}

void *__wrap_realloc(void *address, size_t size)
{
    if (address == NULL)
        return __wrap_malloc(size);
    if (!hui_heap_owns(address))
        return hui_heap_system_result(__real_realloc(address, size), size);
    size_t before = sceLibcMspaceMallocUsableSize(address);
    void *result = sceLibcMspaceRealloc(hui_heap_mspace, address, size);
    if (result)
        hui_heap_resize_stats(before, sceLibcMspaceMallocUsableSize(result));
    else if (size)
    {
        atomic_fetch_add_explicit(&hui_heap_failures, 1, memory_order_relaxed);
        hui_heap_failed(size, 0);
    }
    return result;
}

void __wrap_free(void *address)
{
    if (hui_heap_owns(address))
    {
        hui_heap_resize_stats(sceLibcMspaceMallocUsableSize(address), 0);
        atomic_fetch_sub_explicit(&hui_heap_blocks, 1, memory_order_relaxed);
        sceLibcMspaceFree(hui_heap_mspace, address);
    }
    else
    {
        __real_free(address);
    }
}

int __wrap_posix_memalign(void **address, size_t alignment, size_t size)
{
    if (!hui_heap_ready())
    {
        int result = __real_posix_memalign(address, alignment, size);
        if (result == ENOMEM)
            hui_heap_failed(size, 1);
        return result;
    }
    int result = sceLibcMspacePosixMemalign(hui_heap_mspace, address, alignment, size);
    if (result == 0)
        hui_heap_record_allocation(*address, size);
    else if (result == ENOMEM)
    {
        atomic_fetch_add_explicit(&hui_heap_failures, 1, memory_order_relaxed);
        hui_heap_failed(size, 0);
    }
    return result;
}

size_t __wrap_malloc_usable_size(const void *address)
{
    return hui_heap_owns(address) ? sceLibcMspaceMallocUsableSize(address)
                                  : __real_malloc_usable_size(address);
}

/* Diagnostics: current heap counters for logs and the diagnostics screen. */
void hui_heap_stats(size_t *live_bytes, size_t *peak_bytes, size_t *blocks, size_t *failures)
{
    if (live_bytes)
        *live_bytes = atomic_load_explicit(&hui_heap_live_bytes, memory_order_relaxed);
    if (peak_bytes)
        *peak_bytes = atomic_load_explicit(&hui_heap_peak_bytes, memory_order_relaxed);
    if (blocks)
        *blocks = atomic_load_explicit(&hui_heap_blocks, memory_order_relaxed);
    if (failures)
        *failures = atomic_load_explicit(&hui_heap_failures, memory_order_relaxed);
}

/* Regie5: whether the heap is up (2), failed (-1) or not asked yet (0). */
int regie5_heap_state(void)
{
    hui_heap_ready();
    return atomic_load_explicit(&hui_heap_state, memory_order_acquire);
}

/* Regie5: the heap's size, what is in use, the most ever in use, the blocks
 * and the failed requests. */
void regie5_heap_stats(size_t *size, size_t *live, size_t *peak, size_t *blocks, size_t *failures)
{
    *size = atomic_load_explicit(&hui_heap_state, memory_order_acquire) == 2 ? hui_heap_size : 0;
    *live = atomic_load_explicit(&hui_heap_live_bytes, memory_order_relaxed);
    *peak = atomic_load_explicit(&hui_heap_peak_bytes, memory_order_relaxed);
    *blocks = atomic_load_explicit(&hui_heap_blocks, memory_order_relaxed);
    *failures = atomic_load_explicit(&hui_heap_failures, memory_order_relaxed);
}

/* Regie5: the sizes tried, in order, and what each gave (0 granted, -1 the
 * mspace refused it, else mmap's errno). Returns how many were tried. */
int regie5_heap_attempts(size_t *sizes, int *results, int room)
{
    hui_heap_ready();
    if (atomic_load_explicit(&hui_heap_state, memory_order_acquire) == 1)
        return 0;   /* another thread is still trying */
    int n = hui_heap_tries < room ? hui_heap_tries : room;
    for (int i = 0; i < n; i++)
    {
        sizes[i] = hui_heap_sizes[i];
        results[i] = hui_heap_tried[i];
    }
    return n;
}
