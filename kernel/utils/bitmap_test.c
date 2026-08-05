#include <utils/bitmap.h>
#include <utils/bitmap_test.h>
#include <utils/test.h>

void bitmap_test(void) {
  htest_ctx_t ctx = {0};
  htest_suite_begin(&ctx, "bitmap");

  htest_case_begin(&ctx, "init and basic bit operations");
  {
    uint8_t storage[bitmap_storage_size(16)];
    bitmap_t bitmap;
    uint64_t clear_idx = 99;

    bitmap_init(&bitmap, storage, 16, false);
    HTEST_ASSERT(&ctx, bitmap_find_clear(&bitmap, &clear_idx) == true);
    HTEST_ASSERT(&ctx, clear_idx == 0);
    HTEST_ASSERT(&ctx, bitmap_get(&bitmap, 3) == false);
    HTEST_ASSERT(&ctx, bitmap_set(&bitmap, 3) == true);
    HTEST_ASSERT(&ctx, bitmap_get(&bitmap, 3) == true);
    HTEST_ASSERT(&ctx, bitmap_clear(&bitmap, 3) == true);
    HTEST_ASSERT(&ctx, bitmap_get(&bitmap, 3) == false);
  }

  htest_case_begin(&ctx, "find clear skips set bits");
  {
    uint8_t storage[bitmap_storage_size(10)];
    bitmap_t bitmap;
    uint64_t clear_idx = 99;

    bitmap_init(&bitmap, storage, 10, false);
    for (uint64_t idx = 0; idx < 9; ++idx) {
      HTEST_ASSERT(&ctx, bitmap_set(&bitmap, idx) == true);
    }
    HTEST_ASSERT(&ctx, bitmap_find_clear(&bitmap, &clear_idx) == true);
    HTEST_ASSERT(&ctx, clear_idx == 9);
    HTEST_ASSERT(&ctx, bitmap_set(&bitmap, 9) == true);
    HTEST_ASSERT(&ctx, bitmap_find_clear(&bitmap, &clear_idx) == false);
  }

  htest_case_begin(&ctx, "clear reopens low and high indices");
  {
    uint8_t storage[bitmap_storage_size(9)];
    bitmap_t bitmap;
    uint64_t clear_idx = 99;

    bitmap_init(&bitmap, storage, 9, true);
    HTEST_ASSERT(&ctx, bitmap_find_clear(&bitmap, &clear_idx) == false);
    HTEST_ASSERT(&ctx, bitmap_clear(&bitmap, 8) == true);
    HTEST_ASSERT(&ctx, bitmap_find_clear(&bitmap, &clear_idx) == true);
    HTEST_ASSERT(&ctx, clear_idx == 8);
    HTEST_ASSERT(&ctx, bitmap_clear(&bitmap, 0) == true);
    HTEST_ASSERT(&ctx, bitmap_find_clear(&bitmap, &clear_idx) == true);
    HTEST_ASSERT(&ctx, clear_idx == 0);
    HTEST_ASSERT(&ctx, bitmap_set(&bitmap, 0) == true);
    HTEST_ASSERT(&ctx, bitmap_find_clear(&bitmap, &clear_idx) == true);
    HTEST_ASSERT(&ctx, clear_idx == 8);
  }

  htest_case_begin(&ctx, "repeated operations preserve search bounds");
  {
    uint8_t storage[bitmap_storage_size(4)];
    bitmap_t bitmap;
    uint64_t clear_idx = 99;

    bitmap_init(&bitmap, storage, 4, false);
    HTEST_ASSERT(&ctx, bitmap_set(&bitmap, 0) == true);
    HTEST_ASSERT(&ctx, bitmap_set(&bitmap, 0) == true);
    HTEST_ASSERT(&ctx, bitmap_find_clear(&bitmap, &clear_idx) == true);
    HTEST_ASSERT(&ctx, clear_idx == 1);

    HTEST_ASSERT(&ctx, bitmap_clear(&bitmap, 3) == true);
    HTEST_ASSERT(&ctx, bitmap_clear(&bitmap, 3) == true);
    HTEST_ASSERT(&ctx, bitmap_find_clear(&bitmap, &clear_idx) == true);
    HTEST_ASSERT(&ctx, clear_idx == 1);
  }

  htest_case_begin(&ctx, "bounds checks");
  {
    uint8_t storage[bitmap_storage_size(1)];
    bitmap_t bitmap;
    uint64_t clear_idx = 99;

    bitmap_init(&bitmap, storage, 1, false);
    HTEST_ASSERT(&ctx, bitmap_get(&bitmap, 1) == false);
    HTEST_ASSERT(&ctx, bitmap_set(&bitmap, 1) == false);
    HTEST_ASSERT(&ctx, bitmap_clear(&bitmap, 1) == false);
    HTEST_ASSERT(&ctx, bitmap_find_clear(&bitmap, &clear_idx) == true);
    HTEST_ASSERT(&ctx, clear_idx == 0);
  }

  htest_suite_pass(&ctx);
}
