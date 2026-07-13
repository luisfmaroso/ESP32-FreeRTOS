#include "shared_state.h"

#include <assert.h>

system_state_t g_state = {0};
system_stats_t g_stats = {0};

/* One mutex guards both structs: they are small, always accessed briefly,
 * and a single lock keeps the locking story trivially deadlock-free (no
 * ordering rules to get wrong). A mutex — not a binary semaphore — because
 * mutexes give us PRIORITY INHERITANCE: if the low-priority stats task holds
 * the lock when the high-priority button task wants it, the stats task is
 * temporarily boosted so it releases quickly instead of being starved by
 * medium-priority tasks (the classic priority-inversion problem). */
static SemaphoreHandle_t s_mutex;

void shared_state_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    assert(s_mutex != NULL);
}

void shared_state_lock(void)
{
    /* portMAX_DELAY: every critical section in this project is a handful of
     * assignments, so waiting forever is safe and simpler than plumbing
     * timeout errors through every caller. */
    xSemaphoreTake(s_mutex, portMAX_DELAY);
}

void shared_state_unlock(void)
{
    xSemaphoreGive(s_mutex);
}

void shared_state_snapshot(system_state_t *state_out, system_stats_t *stats_out)
{
    shared_state_lock();
    if (state_out != NULL) {
        *state_out = g_state;
    }
    if (stats_out != NULL) {
        *stats_out = g_stats;
    }
    shared_state_unlock();
}
