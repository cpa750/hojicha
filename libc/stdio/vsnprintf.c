#include <internal/__stdio.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

struct snprintf_ctx {
  char* buffer;
  size_t size;
  size_t written;
};

static int snprintf_write(void* raw_ctx, const char* data, size_t len) {
  struct snprintf_ctx* ctx = (struct snprintf_ctx*)raw_ctx;

  for (size_t i = 0; i < len; ++i) {
    if (ctx->size > 0 && ctx->written < ctx->size - 1) {
      ctx->buffer[ctx->written] = data[i];
    }
    ++ctx->written;
  }

  return 0;
}

int vsnprintf(char* restrict buffer,
              size_t size,
              const char* restrict format,
              va_list parameters) {
  struct snprintf_ctx ctx = {
      .buffer = buffer,
      .size = size,
      .written = 0,
  };

  int ret = __hojicha_vformat(snprintf_write, &ctx, format, parameters);
  if (ret < 0) { return ret; }

  if (size > 0) {
    size_t terminator = (size_t)ret < size ? (size_t)ret : size - 1;
    buffer[terminator] = '\0';
  }

  return ret;
}
