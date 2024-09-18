#include "queue.h"

#include <circle/alloc.h>
#include <circle/util.h>

void queue_init(queue_t *q, u16 element_size, u16 element_count) {
    q->data = (u8*)calloc(element_count + 1, element_size) ;
    q->element_count = element_count ;
    q->element_size = element_size ;
    q->wptr = 0 ;
    q->rptr = 0 ;
}

void queue_free(queue_t *q) {
    free(q->data) ;
}

static inline void *element_ptr(queue_t *q, u16 index) {
    assert(index <= q->element_count);
    return q->data + index * q->element_size;
}

static inline u16 inc_index(queue_t *q, u16 index) {
    if (++index > q->element_count) {
        index = 0;
    }
    return index;
}

bool queue_try_add(queue_t *q, const void *data) {
    do {
        q->spinlock.Acquire() ;
        if (queue_get_level_unsafe(q) != q->element_count) {
            memcpy(element_ptr(q, q->wptr), data, q->element_size);
            q->wptr = inc_index(q, q->wptr);
            q->spinlock.Release() ;
            return true;
        }
        q->spinlock.Release() ;
        return false;
    } while (true) ;
}

bool queue_try_remove(queue_t *q, void *data) {
    do {
        q->spinlock.Acquire() ;
        if (queue_get_level_unsafe(q) != 0) {
            memcpy(data, element_ptr(q, q->rptr), q->element_size);
            q->rptr = inc_index(q, q->rptr);
            q->spinlock.Release() ;
            return true;
        }
        q->spinlock.Release() ;
        return false;
    } while (true) ;
}
