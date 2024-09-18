#ifndef _UTIL_QUEUE_H_
#define _UTIL_QUEUE_H_

#include <circle/types.h>
#include <circle/spinlock.h>
#include <circle/atomic.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    CSpinLock spinlock;
    u8 *data;
    u16 wptr;
    u16 rptr;
    u16 element_size;
    u16 element_count;
} queue_t ;

void queue_init(queue_t *q, u16 element_size, u16 element_count) ;
void queue_free(queue_t *q) ;

static inline u16 queue_get_level_unsafe(queue_t *q) {
    u16 rc = q->wptr - q->rptr;
    if (rc < 0) {
        rc += q->element_count + 1;
    }
    return rc ;
}

inline u16 queue_get_level(queue_t *q) {
    q->spinlock.Acquire() ;
    u16 rc = queue_get_level_unsafe(q) ;
    q->spinlock.Release() ;
    return rc ;
}

inline bool queue_is_empty(queue_t *q) {
    return queue_get_level(q) == 0 ;
}

inline bool queue_is_full(queue_t *q) {
    return queue_get_level(q) == q->element_count;
}

bool queue_try_add(queue_t *q, const void *data) ;
bool queue_try_remove(queue_t *q, void *data) ;

#ifdef __cplusplus
}
#endif

#endif
