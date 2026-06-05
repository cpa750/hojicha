#include <multitask/mutex.h>
#include <stdlib.h>
#include <utils/ringbuffer.h>
#include <utils/set_out.h>

typedef struct ringbuffer ringbuffer_t;
struct ringbuffer {
  char* buf;
  uint64_t read_pos;
  uint64_t write_pos;
  uint64_t size;
  mutex_t* mtx;
};

void ringbuffer_new(uint64_t size, ringbuffer_t** out) {
  ringbuffer_t* ret = calloc(1, sizeof(ringbuffer_t));
  char* buffer = calloc(size + 1, sizeof(char));
  mutex_t* mtx = mutex_create();
  if (ret == NULL || buffer == NULL || mtx == NULL) {
    free(ret);
    free(buffer);
    mutex_destroy(mtx);
    SET_OUT_NULL(out);
  }

  ret->buf = buffer;
  ret->read_pos = 0;
  ret->write_pos = 0;
  ret->size = size;
  ret->mtx = mtx;
  SET_OUT(out, ret);
}

void ringbuffer_free(ringbuffer_t* r) {
  free(r->buf);
  free(r);
}

void ringbuffer_write(ringbuffer_t* r, char value) {
  mutex_lock(r->mtx);
  if (r->write_pos == 0) { r->write_pos = 1; }
  r->buf[r->write_pos++] = value;
  if (r->write_pos > r->size) { r->write_pos = 1; }
  mutex_unlock(r->mtx);
}

bool ringbuffer_read(ringbuffer_t* r, char* out) {
  if (r->read_pos == r->write_pos) { return false; }
  mutex_lock(r->mtx);
  if (r->read_pos == 0) { r->read_pos = 1; }
  SET_OUT(out, r->buf[r->read_pos++]);
  if (r->read_pos > r->size) { r->read_pos = 1; }
  mutex_unlock(r->mtx);
  return true;
}

