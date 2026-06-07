#include <stdlib.h>
#include <utils/ringbuffer.h>
#include <utils/set_out.h>

#define RINGBUFFER_READ_LOCK(r)                                                \
  if (r->lock != NULL && r->read_lock_fn && r->read_unlock_fn != NULL) {       \
    r->read_lock_fn(r->lock);                                                  \
  }
#define RINGBUFFER_READ_UNLOCK(r)                                              \
  if (r->lock != NULL && r->read_lock_fn && r->read_unlock_fn != NULL) {       \
    r->read_unlock_fn(r->lock);                                                \
  }
#define RINGBUFFER_WRITE_LOCK(r)                                               \
  if (r->lock != NULL && r->write_lock_fn && r->write_unlock_fn != NULL) {     \
    r->write_lock_fn(r->lock);                                                 \
  }
#define RINGBUFFER_WRITE_UNLOCK(r)                                             \
  if (r->lock != NULL && r->write_lock_fn && r->write_unlock_fn != NULL) {     \
    r->write_unlock_fn(r->lock);                                               \
  }

typedef struct ringbuffer ringbuffer_t;
struct ringbuffer {
  char* buf;
  uint64_t read_pos;
  uint64_t write_pos;
  uint64_t size;
  void* lock;
  ringbuffer_lock_fn_t read_lock_fn;
  ringbuffer_unlock_fn_t read_unlock_fn;
  ringbuffer_lock_fn_t write_lock_fn;
  ringbuffer_unlock_fn_t write_unlock_fn;
};

void ringbuffer_new(uint64_t size,
                    ringbuffer_t** out,
                    void* lock,
                    ringbuffer_lock_fn_t read_lock_fn,
                    ringbuffer_unlock_fn_t read_unlock_fn,
                    ringbuffer_lock_fn_t write_lock_fn,
                    ringbuffer_unlock_fn_t write_unlock_fn) {
  ringbuffer_t* ret = calloc(1, sizeof(ringbuffer_t));
  char* buffer = calloc(size + 1, sizeof(char));
  if (ret == NULL || buffer == NULL) {
    free(ret);
    free(buffer);
    SET_OUT_NULL(out);
    return;
  }

  ret->buf = buffer;
  ret->read_pos = 0;
  ret->write_pos = 0;
  ret->size = size;
  ret->lock = lock;
  ret->read_lock_fn = read_lock_fn;
  ret->read_unlock_fn = read_unlock_fn;
  ret->write_lock_fn = write_lock_fn;
  ret->write_unlock_fn = write_unlock_fn;
  SET_OUT(out, ret);
}

void ringbuffer_free(ringbuffer_t* r) {
  free(r->buf);
  free(r);
}

void ringbuffer_write(ringbuffer_t* r, char value) {
  RINGBUFFER_WRITE_LOCK(r);
  if (r->write_pos == 0) { r->write_pos = 1; }
  r->buf[r->write_pos++] = value;
  if (r->write_pos > r->size) { r->write_pos = 1; }
  RINGBUFFER_WRITE_UNLOCK(r);
}

bool ringbuffer_read(ringbuffer_t* r, char* out) {
  RINGBUFFER_READ_LOCK(r);
  if (r->read_pos == r->write_pos) {
    RINGBUFFER_READ_UNLOCK(r);
    return false;
  }
  if (r->read_pos == 0) { r->read_pos = 1; }
  SET_OUT(out, r->buf[r->read_pos++]);
  if (r->read_pos > r->size) { r->read_pos = 1; }
  RINGBUFFER_READ_UNLOCK(r);
  return true;
}
