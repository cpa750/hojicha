#include <utils/ringbuffer.h>
#include <utils/ringbuffer_test.h>
#include <utils/test.h>

#define RINGBUFFER_SIZE 3

void ringbuffer_test(void) {
  htest_ctx_t ctx = {0};
  htest_suite_begin(&ctx, "ringbuffer");

  htest_case_begin(&ctx, "simple write + read");
  {
    ringbuffer_t* rb = NULL;
    char a;
    ringbuffer_new(RINGBUFFER_SIZE, &rb);
    HTEST_ASSERT(&ctx, rb != NULL);
    ringbuffer_write(rb, 'a');

    HTEST_ASSERT(&ctx, ringbuffer_read(rb, &a) == true);
    HTEST_ASSERT(&ctx, a == 'a');
    ringbuffer_free(rb);
  }

  htest_case_begin(&ctx, "starved read");
  {
    ringbuffer_t* rb = NULL;
    char a;
    ringbuffer_new(RINGBUFFER_SIZE, &rb);
    HTEST_ASSERT(&ctx, rb != NULL);
    ringbuffer_write(rb, 'a');
    ringbuffer_read(rb, &a);
    char b = 'c';
    HTEST_ASSERT(&ctx, ringbuffer_read(rb, &b) == false);
    HTEST_ASSERT(&ctx, b == 'c');

    ringbuffer_write(rb, 'b');
    HTEST_ASSERT(&ctx, ringbuffer_read(rb, &b) == true);
    HTEST_ASSERT(&ctx, b == 'b');

    ringbuffer_free(rb);
  }

  htest_case_begin(&ctx, "overflow");
  {
    ringbuffer_t* rb = NULL;
    char a;
    char b;
    char c;
    char d;

    ringbuffer_new(RINGBUFFER_SIZE, &rb);
    HTEST_ASSERT(&ctx, rb != NULL);
    ringbuffer_write(rb, 'a');
    ringbuffer_write(rb, 'b');
    ringbuffer_write(rb, 'c');
    ringbuffer_read(rb, &a);
    ringbuffer_read(rb, &b);

    HTEST_ASSERT(&ctx, ringbuffer_read(rb, &c) == true);
    HTEST_ASSERT(&ctx, c == 'c');

    ringbuffer_write(rb, 'd');
    HTEST_ASSERT(&ctx, ringbuffer_read(rb, &d) == true);
    HTEST_ASSERT(&ctx, d == 'd');

    ringbuffer_free(rb);
  }

  htest_case_begin(&ctx, "overwrite");
  {
    ringbuffer_t* rb = NULL;
    char d;

    ringbuffer_new(RINGBUFFER_SIZE, &rb);
    HTEST_ASSERT(&ctx, rb != NULL);
    ringbuffer_write(rb, 'a');
    ringbuffer_write(rb, 'b');
    ringbuffer_write(rb, 'c');
    ringbuffer_write(rb, 'd');

    HTEST_ASSERT(&ctx, ringbuffer_read(rb, &d) == true);  // First read
    HTEST_ASSERT(&ctx, d == 'd');  // overflow replaces unread chars

    ringbuffer_free(rb);
  }

  htest_suite_pass(&ctx);
}
