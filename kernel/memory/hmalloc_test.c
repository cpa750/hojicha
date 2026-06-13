#include <haddr.h>
#include <hmalloc.h>
#include <memory/hmalloc_test.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HTEST_USE_PRINTF
#include <utils/test.h>

#if defined(__stress_hmalloc)
#define STRESS_HMALLOC_BLOCK_OVERHEAD     33
#define STRESS_HMALLOC_INITIAL_HEAP_PAGES 5
#define STRESS_HMALLOC_PAGE_SIZE          4096
#define STRESS_HMALLOC_INITIAL_HEAP_USABLE                                     \
  ((STRESS_HMALLOC_INITIAL_HEAP_PAGES * STRESS_HMALLOC_PAGE_SIZE) -            \
   STRESS_HMALLOC_BLOCK_OVERHEAD)
#define STRESS_HMALLOC_INITIAL_EDGE_CAP 256
#define STRESS_HMALLOC_INITIAL_PREFIX_ALLOC                                    \
  (STRESS_HMALLOC_INITIAL_HEAP_USABLE - STRESS_HMALLOC_BLOCK_OVERHEAD -        \
   STRESS_HMALLOC_INITIAL_EDGE_CAP)
#define STRESS_HMALLOC_INITIAL_EDGE_ALLOC                                      \
  (STRESS_HMALLOC_INITIAL_EDGE_CAP - STRESS_HMALLOC_BLOCK_OVERHEAD)
#define STRESS_HMALLOC_INITIAL_GROW_PAGES 10
#define STRESS_HMALLOC_GROWN_HEAP_USABLE                                       \
  ((STRESS_HMALLOC_INITIAL_GROW_PAGES * STRESS_HMALLOC_PAGE_SIZE) -            \
   STRESS_HMALLOC_BLOCK_OVERHEAD)
#define STRESS_HMALLOC_BORDER_ALLOC  192
#define STRESS_HMALLOC_LAST_FREE_CAP 256
#define STRESS_HMALLOC_POST_BORDER_FREE                                        \
  (STRESS_HMALLOC_GROWN_HEAP_USABLE - STRESS_HMALLOC_BORDER_ALLOC -            \
   STRESS_HMALLOC_BLOCK_OVERHEAD)
#define STRESS_HMALLOC_TAIL_FILLER_ALLOC                                       \
  (STRESS_HMALLOC_POST_BORDER_FREE - STRESS_HMALLOC_BLOCK_OVERHEAD -           \
   STRESS_HMALLOC_LAST_FREE_CAP)
#define STRESS_HMALLOC_TAIL_GROW_ALLOC                                         \
  (STRESS_HMALLOC_LAST_FREE_CAP + STRESS_HMALLOC_BORDER_ALLOC)
#define STRESS_HMALLOC_SCAN_SIZE         768
#define STRESS_HMALLOC_SCAN_MAX          192
#define STRESS_HMALLOC_CHURN_ROUNDS      16
#define STRESS_HMALLOC_GROW_ALLOCS       384
#define STRESS_HMALLOC_REUSE_ALLOCS      96
#define STRESS_HMALLOC_MIN_GROW_DISTANCE (32 * 4096)

typedef struct st_hmalloc_border_scan st_hmalloc_border_scan_t;
struct st_hmalloc_border_scan {
  char* before;
  char* after;
  size_t before_size;
  size_t after_size;
  uint8_t before_pattern;
  uint8_t after_pattern;
  char* allocations[STRESS_HMALLOC_SCAN_MAX];
  uint64_t count;
};

static size_t st_hmalloc_grow_size(int idx) {
  static const size_t sizes[] = {1536, 2048, 3072, 3584, 1024, 2560};
  return sizes[idx % (sizeof(sizes) / sizeof(sizes[0]))];
}

static uint8_t st_hmalloc_pattern(int idx) {
  return (uint8_t)(0x31 + ((idx * 17) & 0x3F));
}

static void st_hmalloc_fill(char* ptr, size_t size, uint8_t pattern) {
  memset(ptr, pattern, size);
}

static bool st_hmalloc_verify(char* ptr, size_t size, uint8_t pattern) {
  for (size_t idx = 0; idx < size; ++idx) {
    if ((uint8_t)ptr[idx] != pattern) { return false; }
  }
  return true;
}

static haddr_t st_hmalloc_next_user_addr(char* ptr, size_t block_size) {
  return (haddr_t)ptr + block_size + STRESS_HMALLOC_BLOCK_OVERHEAD;
}

static void st_hmalloc_occupy_tail(htest_ctx_t* ctx,
                                   size_t desired_size,
                                   uint8_t pattern) {
  for (int attempt = 0; attempt < STRESS_HMALLOC_SCAN_MAX; ++attempt) {
    if (!hmalloc_debug_last_block_is_free()) {
      char* tail = (char*)hmalloc_debug_last_block_user();
      st_hmalloc_fill(tail, hmalloc_debug_last_block_size(), pattern);
      return;
    }

    char* tail_before = (char*)hmalloc_debug_last_block_user();
    size_t tail_size = hmalloc_debug_last_block_size();
    size_t alloc_size = tail_size < desired_size ? tail_size : desired_size;

    if (tail_size > desired_size + STRESS_HMALLOC_BLOCK_OVERHEAD) {
      alloc_size = tail_size - desired_size - STRESS_HMALLOC_BLOCK_OVERHEAD;
    }
    if (alloc_size == 0) { alloc_size = 1; }

    char* allocation = (char*)malloc(alloc_size);
    HTEST_ASSERT(ctx, allocation != NULL);

    if (allocation == tail_before) {
      size_t fill_size =
          hmalloc_debug_last_block_is_free() ? alloc_size
                                             : hmalloc_debug_last_block_size();
      st_hmalloc_fill(allocation, fill_size, pattern);
    } else {
      st_hmalloc_fill(allocation, alloc_size, (uint8_t)(pattern - 1));
    }
  }

  HTEST_ASSERT(ctx, !hmalloc_debug_last_block_is_free());
}

static void st_hmalloc_find_growth_border(htest_ctx_t* ctx,
                                          st_hmalloc_border_scan_t* scan,
                                          uint8_t pattern_base) {
  scan->before = NULL;
  scan->after = NULL;
  scan->before_size = 0;
  scan->after_size = 0;
  scan->before_pattern = (uint8_t)(pattern_base + 0x3E);
  scan->after_pattern = 0;
  scan->count = 0;

  st_hmalloc_occupy_tail(ctx,
                         STRESS_HMALLOC_SCAN_SIZE,
                         scan->before_pattern);

  for (int idx = 0; idx < STRESS_HMALLOC_SCAN_MAX; ++idx) {
    uint64_t footer_before = hmalloc_debug_last_footer();
    char* tail_before = (char*)hmalloc_debug_last_block_user();
    size_t tail_before_size = hmalloc_debug_last_block_size();

    char* current = (char*)malloc(STRESS_HMALLOC_SCAN_SIZE);
    HTEST_ASSERT(ctx, current != NULL);

    scan->allocations[idx] = current;
    scan->count++;
    uint8_t current_pattern = (uint8_t)(pattern_base + (idx & 0x1F));
    st_hmalloc_fill(current,
                    STRESS_HMALLOC_SCAN_SIZE,
                    current_pattern);

    if (hmalloc_debug_last_footer() != footer_before) {
      scan->before = tail_before;
      scan->after = current;
      scan->before_size = tail_before_size;
      scan->after_size = STRESS_HMALLOC_SCAN_SIZE;
      scan->after_pattern = current_pattern;
      HTEST_ASSERT(ctx,
                   (haddr_t)scan->after ==
                       st_hmalloc_next_user_addr(scan->before,
                                                 scan->before_size));
      return;
    }
  }

  HTEST_ASSERT(ctx, scan->before != NULL);
  HTEST_ASSERT(ctx, scan->after != NULL);
}
#endif

void hmalloc_test() {
  htest_ctx_t ctx = {0};
  htest_suite_begin(&ctx, "hmalloc");

  htest_case_begin(&ctx, "basic allocation reuse");
  char* a = (char*)malloc(sizeof(char) * 20);
  strcpy(a, "hello, world!");

  char* b = (char*)malloc(sizeof(char) * 30);
  strcpy(b, "this is so cool!!!");

  HTEST_ASSERT(&ctx, (haddr_t)b == ((haddr_t)a) + 33 + 20);

  char* c = (char*)malloc(sizeof(char) * 40);
  strcpy(c, "look at me ma, no stack!");

  HTEST_ASSERT(&ctx, (haddr_t)c == ((haddr_t)b) + 33 + 30);

  free(b);

  // Should be allocated in old b
  char* d = (char*)malloc(sizeof(char) * 10);
  strcpy(d, "test\000");

  HTEST_ASSERT(&ctx, (haddr_t)d == ((haddr_t)b));

  free(a);

  htest_case_begin(&ctx, "heap growth and free list reuse");
  // Test we can grow the heap
  for (int i = 0; i < 500; ++i) {
    char* n = (char*)malloc(sizeof(char) * 2000);
    strcpy(n, "grow");
    memset(n, 0xFF, 2000);
  }

  char *e, *f, *g, *h, *i, *j, *k, *l, *m, *n, *o, *p;
  e = (char*)malloc(sizeof(char) * 10);
  f = (char*)malloc(sizeof(char) * 10);
  g = (char*)malloc(sizeof(char) * 10);
  h = (char*)malloc(sizeof(char) * 10);
  i = (char*)malloc(sizeof(char) * 10);
  j = (char*)malloc(sizeof(char) * 10);
  k = (char*)malloc(sizeof(char) * 10);
  l = (char*)malloc(sizeof(char) * 10);
  m = (char*)malloc(sizeof(char) * 10);
  n = (char*)malloc(sizeof(char) * 10);
  o = (char*)malloc(sizeof(char) * 10);
  p = (char*)malloc(sizeof(char) * 10);

  HTEST_ASSERT(&ctx, (haddr_t)e == (haddr_t)a);
  HTEST_ASSERT(&ctx, (haddr_t)g == ((haddr_t)f) + 33 + 10);
  HTEST_ASSERT(&ctx, (haddr_t)h == ((haddr_t)g) + 33 + 10);
  HTEST_ASSERT(&ctx, (haddr_t)i == ((haddr_t)h) + 33 + 10);
  HTEST_ASSERT(&ctx, (haddr_t)j == ((haddr_t)i) + 33 + 10);
  HTEST_ASSERT(&ctx, (haddr_t)k == ((haddr_t)j) + 33 + 10);
  HTEST_ASSERT(&ctx, (haddr_t)l == ((haddr_t)k) + 33 + 10);
  HTEST_ASSERT(&ctx, (haddr_t)m == ((haddr_t)l) + 33 + 10);
  HTEST_ASSERT(&ctx, (haddr_t)n == ((haddr_t)m) + 33 + 10);
  HTEST_ASSERT(&ctx, (haddr_t)o == ((haddr_t)n) + 33 + 10);
  HTEST_ASSERT(&ctx, (haddr_t)p == ((haddr_t)o) + 33 + 10);

  free(f);
  free(e);

  free(h);
  free(j);
  free(i);

  free(l);
  free(m);

  free(o);
  free(p);

  char* q = (char*)malloc(10);
  char* r = (char*)malloc(10);
  char* s = (char*)malloc(30);
  char* t = (char*)malloc(20);
  char* u = (char*)malloc(20);
  char* v = (char*)malloc(20);

  HTEST_ASSERT(&ctx, (haddr_t)q == (haddr_t)e);
  HTEST_ASSERT(&ctx, (haddr_t)r == (haddr_t)f);
  HTEST_ASSERT(&ctx, (haddr_t)s == (haddr_t)h);
  HTEST_ASSERT(&ctx, (haddr_t)t == (haddr_t)h + 33 + 30);
  HTEST_ASSERT(&ctx, (haddr_t)u == (haddr_t)l);
  HTEST_ASSERT(&ctx, (haddr_t)v == (haddr_t)o);

  htest_case_begin(&ctx, "consecutive allocator scans");
  char* chain[16] = {0};
  for (int idx = 0; idx < 16; ++idx) {
    chain[idx] = (char*)malloc(sizeof(char) * 16);
    memset(chain[idx], idx, 16);
  }

  free(chain[0]);
  free(chain[15]);

  char* scan_reuse = (char*)malloc(sizeof(char) * 16);
  HTEST_ASSERT(&ctx, (haddr_t)scan_reuse == (haddr_t)chain[0]);
  char* backwards_scan_reuse = (char*)malloc(sizeof(char) * 16);
  HTEST_ASSERT(&ctx, (haddr_t)backwards_scan_reuse == (haddr_t)chain[15]);

  htest_suite_pass(&ctx);
}

#if defined(__stress_hmalloc)
void hmalloc_stress_test() {
  htest_ctx_t ctx = {0};
  htest_suite_begin(&ctx, "hmalloc stress");

  htest_case_begin(&ctx, "initial heap growth border");
  char* initial_prefix = (char*)malloc(STRESS_HMALLOC_INITIAL_PREFIX_ALLOC);
  HTEST_ASSERT(&ctx, initial_prefix != NULL);
  st_hmalloc_fill(initial_prefix, STRESS_HMALLOC_INITIAL_PREFIX_ALLOC, 0xB0);

  char* old_edge = (char*)malloc(STRESS_HMALLOC_INITIAL_EDGE_ALLOC);
  HTEST_ASSERT(&ctx, old_edge != NULL);
  HTEST_ASSERT(&ctx,
               (haddr_t)old_edge ==
                   st_hmalloc_next_user_addr(
                       initial_prefix, STRESS_HMALLOC_INITIAL_PREFIX_ALLOC));
  st_hmalloc_fill(old_edge, STRESS_HMALLOC_INITIAL_EDGE_ALLOC, 0xB1);

  char* new_edge = (char*)malloc(STRESS_HMALLOC_BORDER_ALLOC);
  HTEST_ASSERT(&ctx, new_edge != NULL);
  HTEST_ASSERT(
      &ctx,
      (haddr_t)new_edge ==
          st_hmalloc_next_user_addr(old_edge, STRESS_HMALLOC_INITIAL_EDGE_CAP));
  st_hmalloc_fill(new_edge, STRESS_HMALLOC_BORDER_ALLOC, 0xB2);

  HTEST_ASSERT(&ctx,
               st_hmalloc_verify(
                   initial_prefix, STRESS_HMALLOC_INITIAL_PREFIX_ALLOC, 0xB0));
  HTEST_ASSERT(
      &ctx,
      st_hmalloc_verify(old_edge, STRESS_HMALLOC_INITIAL_EDGE_ALLOC, 0xB1));
  HTEST_ASSERT(&ctx,
               st_hmalloc_verify(new_edge, STRESS_HMALLOC_BORDER_ALLOC, 0xB2));

  free(old_edge);
  char* old_edge_reuse = (char*)malloc(STRESS_HMALLOC_INITIAL_EDGE_ALLOC);
  HTEST_ASSERT(&ctx, old_edge_reuse != NULL);
  HTEST_ASSERT(&ctx, (haddr_t)old_edge_reuse == (haddr_t)old_edge);
  st_hmalloc_fill(old_edge_reuse, STRESS_HMALLOC_INITIAL_EDGE_ALLOC, 0xB3);
  HTEST_ASSERT(&ctx,
               st_hmalloc_verify(new_edge, STRESS_HMALLOC_BORDER_ALLOC, 0xB2));

  free(new_edge);
  char* new_edge_reuse = (char*)malloc(STRESS_HMALLOC_BORDER_ALLOC);
  HTEST_ASSERT(&ctx, new_edge_reuse != NULL);
  HTEST_ASSERT(&ctx, (haddr_t)new_edge_reuse == (haddr_t)new_edge);
  st_hmalloc_fill(new_edge_reuse, STRESS_HMALLOC_BORDER_ALLOC, 0xB4);
  HTEST_ASSERT(&ctx,
               st_hmalloc_verify(
                   old_edge_reuse, STRESS_HMALLOC_INITIAL_EDGE_ALLOC, 0xB3));

  char* tail_filler = (char*)malloc(STRESS_HMALLOC_TAIL_FILLER_ALLOC);
  HTEST_ASSERT(&ctx, tail_filler != NULL);
  HTEST_ASSERT(
      &ctx,
      (haddr_t)tail_filler == st_hmalloc_next_user_addr(
                                  new_edge_reuse, STRESS_HMALLOC_BORDER_ALLOC));
  st_hmalloc_fill(tail_filler, STRESS_HMALLOC_TAIL_FILLER_ALLOC, 0xB6);

  char* tail_grown = (char*)malloc(STRESS_HMALLOC_TAIL_GROW_ALLOC);
  HTEST_ASSERT(&ctx, tail_grown != NULL);
  HTEST_ASSERT(&ctx,
               (haddr_t)tail_grown ==
                   st_hmalloc_next_user_addr(tail_filler,
                                             STRESS_HMALLOC_TAIL_FILLER_ALLOC));
  st_hmalloc_fill(tail_grown, STRESS_HMALLOC_TAIL_GROW_ALLOC, 0xB7);
  HTEST_ASSERT(
      &ctx,
      st_hmalloc_verify(tail_filler, STRESS_HMALLOC_TAIL_FILLER_ALLOC, 0xB6));
  HTEST_ASSERT(&ctx,
               st_hmalloc_verify(
                   old_edge_reuse, STRESS_HMALLOC_INITIAL_EDGE_ALLOC, 0xB3));

  free(old_edge_reuse);
  free(new_edge_reuse);
  char* merged_initial_border = (char*)malloc(
      STRESS_HMALLOC_INITIAL_EDGE_ALLOC + STRESS_HMALLOC_BORDER_ALLOC +
      STRESS_HMALLOC_BLOCK_OVERHEAD);
  HTEST_ASSERT(&ctx, merged_initial_border != NULL);
  HTEST_ASSERT(&ctx, (haddr_t)merged_initial_border == (haddr_t)old_edge);
  st_hmalloc_fill(merged_initial_border,
                  STRESS_HMALLOC_INITIAL_EDGE_ALLOC +
                      STRESS_HMALLOC_BORDER_ALLOC +
                      STRESS_HMALLOC_BLOCK_OVERHEAD,
                  0xB5);
  HTEST_ASSERT(&ctx,
               st_hmalloc_verify(
                   initial_prefix, STRESS_HMALLOC_INITIAL_PREFIX_ALLOC, 0xB0));

  htest_case_begin(&ctx, "growth border free new then old");
  static st_hmalloc_border_scan_t new_then_old_scan = {0};
  st_hmalloc_find_growth_border(&ctx, &new_then_old_scan, 0xC0);
  HTEST_ASSERT(&ctx, new_then_old_scan.before != NULL);
  HTEST_ASSERT(&ctx, new_then_old_scan.after != NULL);
  free(new_then_old_scan.after);
  free(new_then_old_scan.before);
  size_t new_then_old_merged_size = new_then_old_scan.before_size +
                                    new_then_old_scan.after_size +
                                    STRESS_HMALLOC_BLOCK_OVERHEAD;
  char* new_then_old_merged = (char*)malloc(new_then_old_merged_size);
  HTEST_ASSERT(&ctx, new_then_old_merged != NULL);
  HTEST_ASSERT(
      &ctx, (haddr_t)new_then_old_merged == (haddr_t)new_then_old_scan.before);
  st_hmalloc_fill(new_then_old_merged, new_then_old_merged_size, 0xC1);

  htest_case_begin(&ctx, "growth border free old then new");
  static st_hmalloc_border_scan_t old_then_new_scan = {0};
  st_hmalloc_find_growth_border(&ctx, &old_then_new_scan, 0xD0);
  HTEST_ASSERT(&ctx, old_then_new_scan.before != NULL);
  HTEST_ASSERT(&ctx, old_then_new_scan.after != NULL);
  free(old_then_new_scan.before);
  free(old_then_new_scan.after);
  size_t old_then_new_merged_size = old_then_new_scan.before_size +
                                    old_then_new_scan.after_size +
                                    STRESS_HMALLOC_BLOCK_OVERHEAD;
  char* old_then_new_merged = (char*)malloc(old_then_new_merged_size);
  HTEST_ASSERT(&ctx, old_then_new_merged != NULL);
  HTEST_ASSERT(
      &ctx, (haddr_t)old_then_new_merged == (haddr_t)old_then_new_scan.before);
  st_hmalloc_fill(old_then_new_merged, old_then_new_merged_size, 0xD1);

  htest_case_begin(&ctx, "growth border split and remerge");
  static st_hmalloc_border_scan_t split_scan = {0};
  st_hmalloc_find_growth_border(&ctx, &split_scan, 0xE0);
  HTEST_ASSERT(&ctx, split_scan.before != NULL);
  HTEST_ASSERT(&ctx, split_scan.after != NULL);
  free(split_scan.before);
  size_t split_old_size = split_scan.before_size / 2;
  if (split_old_size == 0) { split_old_size = 1; }
  char* split_old_edge = (char*)malloc(split_old_size);
  HTEST_ASSERT(&ctx, split_old_edge != NULL);
  HTEST_ASSERT(&ctx, (haddr_t)split_old_edge == (haddr_t)split_scan.before);
  st_hmalloc_fill(split_old_edge, split_old_size, 0xE1);
  free(split_scan.after);
  HTEST_ASSERT(&ctx, st_hmalloc_verify(split_old_edge, split_old_size, 0xE1));
  free(split_old_edge);
  size_t split_remerged_size =
      split_scan.before_size + split_scan.after_size +
      STRESS_HMALLOC_BLOCK_OVERHEAD;
  char* split_remerged = (char*)malloc(split_remerged_size);
  HTEST_ASSERT(&ctx, split_remerged != NULL);
  HTEST_ASSERT(&ctx, (haddr_t)split_remerged == (haddr_t)split_scan.before);
  st_hmalloc_fill(split_remerged, split_remerged_size, 0xE2);

  htest_case_begin(&ctx, "growth border repeated edge churn");
  static st_hmalloc_border_scan_t churn_scan = {0};
  st_hmalloc_find_growth_border(&ctx, &churn_scan, 0xF0);
  HTEST_ASSERT(&ctx, churn_scan.before != NULL);
  HTEST_ASSERT(&ctx, churn_scan.after != NULL);
  uint8_t churn_before_pattern = churn_scan.before_pattern;
  size_t churn_before_size = churn_scan.before_size;
  size_t churn_after_size = churn_scan.after_size;
  for (int round = 0; round < STRESS_HMALLOC_CHURN_ROUNDS; ++round) {
    free(churn_scan.after);
    char* after_reuse = (char*)malloc(churn_after_size);
    HTEST_ASSERT(&ctx, after_reuse != NULL);
    HTEST_ASSERT(&ctx, (haddr_t)after_reuse == (haddr_t)churn_scan.after);
    st_hmalloc_fill(after_reuse, churn_after_size, (uint8_t)(0x70 + round));
    HTEST_ASSERT(
        &ctx,
        st_hmalloc_verify(churn_scan.before, churn_before_size,
                          churn_before_pattern));

    free(churn_scan.before);
    char* before_reuse = (char*)malloc(churn_before_size);
    HTEST_ASSERT(&ctx, before_reuse != NULL);
    HTEST_ASSERT(&ctx, (haddr_t)before_reuse == (haddr_t)churn_scan.before);
    st_hmalloc_fill(before_reuse, churn_before_size, (uint8_t)(0x90 + round));
    churn_before_pattern = (uint8_t)(0x90 + round);
    HTEST_ASSERT(
        &ctx,
        st_hmalloc_verify(after_reuse, churn_after_size,
                          (uint8_t)(0x70 + round)));

    churn_scan.before = before_reuse;
    churn_scan.after = after_reuse;
  }

  free(churn_scan.before);
  free(churn_scan.after);
  size_t churn_merged_size =
      churn_before_size + churn_after_size + STRESS_HMALLOC_BLOCK_OVERHEAD;
  char* churn_merged = (char*)malloc(churn_merged_size);
  HTEST_ASSERT(&ctx, churn_merged != NULL);
  HTEST_ASSERT(&ctx, (haddr_t)churn_merged == (haddr_t)churn_scan.before);
  st_hmalloc_fill(churn_merged, churn_merged_size, 0xF1);

  htest_case_begin(&ctx, "heap growing");
  static char* allocations[STRESS_HMALLOC_GROW_ALLOCS] = {0};
  static size_t sizes[STRESS_HMALLOC_GROW_ALLOCS] = {0};
  static uint8_t patterns[STRESS_HMALLOC_GROW_ALLOCS] = {0};

  for (int idx = 0; idx < STRESS_HMALLOC_GROW_ALLOCS; ++idx) {
    sizes[idx] = st_hmalloc_grow_size(idx);
    patterns[idx] = st_hmalloc_pattern(idx);
    allocations[idx] = (char*)malloc(sizes[idx]);

    HTEST_ASSERT(&ctx, allocations[idx] != NULL);
    if (idx > 0) {
      HTEST_ASSERT(&ctx,
                   (haddr_t)allocations[idx] > (haddr_t)allocations[idx - 1]);
    }
    st_hmalloc_fill(allocations[idx], sizes[idx], patterns[idx]);
  }

  HTEST_ASSERT(&ctx,
               (haddr_t)allocations[STRESS_HMALLOC_GROW_ALLOCS - 1] -
                       (haddr_t)allocations[0] >
                   STRESS_HMALLOC_MIN_GROW_DISTANCE);

  for (int idx = 0; idx < STRESS_HMALLOC_GROW_ALLOCS; ++idx) {
    HTEST_ASSERT(
        &ctx, st_hmalloc_verify(allocations[idx], sizes[idx], patterns[idx]));
  }

  htest_case_begin(&ctx, "grown heap free list reuse");
  static haddr_t freed_addrs[STRESS_HMALLOC_REUSE_ALLOCS] = {0};
  for (int idx = 0; idx < STRESS_HMALLOC_REUSE_ALLOCS; ++idx) {
    int allocation_idx = idx * 2;
    freed_addrs[idx] = (haddr_t)allocations[allocation_idx];
    free(allocations[allocation_idx]);
    allocations[allocation_idx] = NULL;
  }

  for (int idx = 0; idx < STRESS_HMALLOC_REUSE_ALLOCS; ++idx) {
    int allocation_idx = idx * 2;
    char* reused = (char*)malloc(sizes[allocation_idx]);
    HTEST_ASSERT(&ctx, reused != NULL);
    HTEST_ASSERT(&ctx, (haddr_t)reused == freed_addrs[idx]);
    memset(reused, 0xA5, sizes[allocation_idx]);
  }

  for (int idx = 1; idx < STRESS_HMALLOC_GROW_ALLOCS; idx += 2) {
    HTEST_ASSERT(
        &ctx, st_hmalloc_verify(allocations[idx], sizes[idx], patterns[idx]));
  }

  htest_suite_pass(&ctx);
}
#endif
