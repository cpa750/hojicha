#include <haddr.h>
#include <memory/kmalloc.h>
#include <stdlib.h>
#include <string.h>

#define HTEST_USE_PRINTF
#include <utils/test.h>

void kmalloc_test() {
  htest_ctx_t ctx = {0};
  htest_suite_begin(&ctx, "kmalloc");

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

  htest_suite_pass(&ctx);
}
