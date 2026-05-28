#ifndef HTEST_H
#define HTEST_H

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if defined(HTEST_USE_PRINTF)
#include <stdio.h>
#else
#include <hlog.h>
#endif

typedef struct htest_ctx htest_ctx_t;
struct htest_ctx {
  const char* suite_name;
  const char* case_name;
  uint64_t total_asserts;
  uint64_t case_asserts;
  uint64_t case_count;
};

static inline void htest_log_suite_begin(const char* suite_name) {
#if defined(HTEST_USE_PRINTF)
  printf("[TEST] Starting test suite: %s\n", suite_name);
#else
  hlog_write(HLOG_INFO, "Starting test suite: %s", suite_name);
#endif
}

static inline void htest_log_case_begin(const char* suite_name,
                                        const char* case_name) {
#if defined(HTEST_USE_PRINTF)
  printf("[TEST] [%s] starting test case: %s\n", suite_name, case_name);
#else
  hlog_write(HLOG_DEBUG, "[%s] starting test case: %s", suite_name, case_name);
#endif
}

static inline void htest_log_assert_fail(const char* suite_name,
                                         const char* case_name,
                                         const char* file,
                                         int line,
                                         const char* expr) {
#if defined(HTEST_USE_PRINTF)
  printf("[TEST] [%s:%s] assertion failed at %s (%d): %s\n",
         suite_name,
         case_name,
         file,
         line,
         expr);
#else
  hlog_write(HLOG_ERROR,
             "[%s:%s] assertion failed at %s (%d): %s",
             suite_name,
             case_name,
             file,
             line,
             expr);
#endif
}

static inline void htest_log_assert_pass(const char* suite_name,
                                         const char* case_name,
                                         const char* expr) {
#if !defined(HTEST_USE_PRINTF)
  hlog_write(HLOG_DEBUG, "[%s:%s] assertion passed: %s", suite_name, case_name,
             expr);
#else
  (void)suite_name;
  (void)case_name;
  (void)expr;
#endif
}

static inline void htest_log_suite_pass(const char* suite_name,
                                        uint64_t case_count,
                                        uint64_t total_asserts) {
#if defined(HTEST_USE_PRINTF)
  printf("[TEST] Test suite passed: %s (%d cases, %d asserts)\n",
         suite_name,
         case_count,
         total_asserts);
#else
  hlog_write(HLOG_INFO,
             "Test suite passed: %s (%d cases, %d asserts)",
             suite_name,
             case_count,
             total_asserts);
#endif
}

static inline void htest_suite_begin(htest_ctx_t* ctx, const char* suite_name) {
  if (ctx == NULL) { return; }

  ctx->suite_name = suite_name;
  ctx->case_name = NULL;
  ctx->total_asserts = 0;
  ctx->case_asserts = 0;
  ctx->case_count = 0;
  htest_log_suite_begin(suite_name);
}

static inline void htest_case_begin(htest_ctx_t* ctx, const char* case_name) {
  if (ctx == NULL) { return; }

  ctx->case_name = case_name;
  ctx->case_asserts = 0;
  ctx->case_count++;
  htest_log_case_begin(ctx->suite_name, case_name);
}

static inline void htest_assert_impl(htest_ctx_t* ctx,
                                     bool cond,
                                     const char* expr,
                                     const char* file,
                                     int line) {
  const char* suite_name =
      ctx != NULL && ctx->suite_name != NULL ? ctx->suite_name : "test";
  const char* case_name =
      ctx != NULL && ctx->case_name != NULL ? ctx->case_name : "ungrouped";

  if (!cond) {
    htest_log_assert_fail(suite_name, case_name, file, line, expr);
    assert_false(expr, file, line);
  }

  if (ctx != NULL) {
    ctx->total_asserts++;
    ctx->case_asserts++;
  }
  htest_log_assert_pass(suite_name, case_name, expr);
}

static inline void htest_suite_pass(htest_ctx_t* ctx) {
  if (ctx == NULL) { return; }

  htest_log_suite_pass(ctx->suite_name, ctx->case_count, ctx->total_asserts);
}

#define HTEST_ASSERT(ctx, cond)                                                \
  htest_assert_impl((ctx), (cond), #cond, __FILE__, __LINE__)

#endif  // HTEST_H
