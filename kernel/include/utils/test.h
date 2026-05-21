#ifndef HTEST_H
#define HTEST_H

#include <assert.h>
#include <hlog.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct htest_ctx htest_ctx_t;
struct htest_ctx {
  const char* suite_name;
  const char* case_name;
  uint64_t total_asserts;
  uint64_t case_asserts;
  uint64_t case_count;
};

static inline void htest_suite_begin(htest_ctx_t* ctx, const char* suite_name) {
  if (ctx == NULL) { return; }

  ctx->suite_name = suite_name;
  ctx->case_name = NULL;
  ctx->total_asserts = 0;
  ctx->case_asserts = 0;
  ctx->case_count = 0;
  hlog_write(HLOG_INFO, "Starting test suite: %s", suite_name);
}

static inline void htest_case_begin(htest_ctx_t* ctx, const char* case_name) {
  if (ctx == NULL) { return; }

  ctx->case_name = case_name;
  ctx->case_asserts = 0;
  ctx->case_count++;
  hlog_write(
      HLOG_DEBUG, "[%s] starting test case: %s", ctx->suite_name, case_name);
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
    hlog_write(HLOG_ERROR,
               "[%s:%s] assertion failed at %s (%d): %s",
               suite_name,
               case_name,
               file,
               line,
               expr);
    assert_false(expr, file, line);
  }

  if (ctx != NULL) {
    ctx->total_asserts++;
    ctx->case_asserts++;
  }
  hlog_write(
      HLOG_DEBUG, "[%s:%s] assertion passed: %s", suite_name, case_name, expr);
}

static inline void htest_suite_pass(htest_ctx_t* ctx) {
  if (ctx == NULL) { return; }

  hlog_write(HLOG_INFO,
             "Test suite passed: %s (%d cases, %d asserts)",
             ctx->suite_name,
             ctx->case_count,
             ctx->total_asserts);
}

#define HTEST_ASSERT(ctx, cond)                                                \
  htest_assert_impl((ctx), (cond), #cond, __FILE__, __LINE__)

#endif  // HTEST_H
