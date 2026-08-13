#include <internal/__stdio.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

#if defined(__is_libk) && defined(__printf_serial)
#include <drivers/serial.h>
#endif

static int printf_write(void* ctx, const char* data, size_t len) {
  (void)ctx;

  for (size_t i = 0; i < len; ++i) {
#if defined(__is_libk) && defined(__printf_serial)
    serial_write_char((unsigned char)data[i]);
#endif
    if (putchar((unsigned char)data[i]) == EOF) { return -1; }
  }

  return 0;
}

int vprintf(const char* restrict format, va_list parameters) {
  return __hojicha_vformat(printf_write, NULL, format, parameters);
}
