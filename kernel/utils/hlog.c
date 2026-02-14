#include <hlog.h>
#include <kernel/kernel_state.h>
#include <kernel/multitask.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "drivers/tty.h"

typedef struct pending_log pending_log_t;
struct pending_log {
  char* buf;
  process_block_t* proc;
  hlog_level_t level;
  pending_log_t* next;
};

void add_log_internal(hlogger_t* logger,
                      hlog_level_t level,
                      process_block_t* proc,
                      const char* restrict format,
                      va_list args);
uint64_t write_log_internal(hlog_level_t level,
                            process_block_t* proc,
                            const char* restrict format,
                            va_list args);
uint64_t print_level(hlog_level_t level);

hlogger_t* hlog_new(hlog_level_t level, uint64_t bufsize) {
  hlogger_t* logger = (hlogger_t*)malloc(sizeof(hlogger_t));
  logger->max_level = level;
  logger->bufsize = bufsize;
  return logger;
}

uint64_t hlog_free_logger(hlogger_t* logger) {
  uint64_t bytes_written = hlog_commit_logger(logger);
  free(logger);
  return bytes_written;
}

void hlog_add(hlog_level_t level, const char* restrict format, ...) {
  hlogger_t* logger = multitask_pb_get_logger(g_kernel.current_process);
  if (level > logger->max_level) { return; }
  va_list args;
  va_start(args, format);
  add_log_internal(logger, level, g_kernel.current_process, format, args);
  va_end(args);
}

void hlog_add_logger(hlogger_t* logger,
                     hlog_level_t level,
                     const char* restrict format,
                     ...) {
  if (level > logger->max_level) { return; }

  va_list args;
  va_start(args, format);
  add_log_internal(logger, level, g_kernel.current_process, format, args);
  va_end(args);
}

void add_log_internal(hlogger_t* logger,
                      hlog_level_t level,
                      process_block_t* proc,
                      const char* restrict format,
                      va_list args) {
  pending_log_t* log = (pending_log_t*)malloc(sizeof(pending_log_t));
  char* buf = (char*)malloc(sizeof(char) * logger->bufsize);
  log->level = level;
  log->next = NULL;
  log->proc = proc;
  vsnprintf(buf, format, args);
  log->buf = buf;

  if (logger->pending_logs_head == NULL) {
    logger->pending_logs_head = log;
  } else {
    logger->pending_logs_tail->next = log;
  }
  logger->pending_logs_tail = log;
}

uint64_t hlog_commit(void) {
  return hlog_commit_logger(multitask_pb_get_logger(g_kernel.current_process));
}

uint64_t hlog_commit_logger(hlogger_t* logger) {
  uint64_t bytes_written = 0;
  pending_log_t* log = logger->pending_logs_head;
  while (log != NULL) {
    // TODO wire in cool colour effects for log levels here
    bytes_written += print_level(log->level);
    bytes_written += printf("[%s] (PID: %d): ",
                            multitask_pb_get_name(log->proc),
                            multitask_pb_get_pid(log->proc));
    // TODO: update bytes written
    printf("%s\n", log->buf);
    free(log->buf);
    pending_log_t* old = log;
    log = log->next;
    free(old);
  }
  logger->pending_logs_head = NULL;
  logger->pending_logs_tail = NULL;
  return bytes_written;
}

uint64_t hlog_write(hlog_level_t level, const char* restrict format, ...) {
  hlogger_t* logger = multitask_pb_get_logger(g_kernel.current_process);
  if (level > logger->max_level) { return 0; }

  va_list args;
  va_start(args, format);
  write_log_internal(level, g_kernel.current_process, format, args);
  va_end(args);
  // TODO: return bytes written
}

uint64_t hlog_write_logger(hlogger_t* logger,
                           hlog_level_t level,
                           const char* restrict format,
                           ...) {
  if (level > logger->max_level) { return 0; }

  va_list args;
  va_start(args, format);
  write_log_internal(level, g_kernel.current_process, format, args);
  va_end(args);
  // TODO: return bytes written
}

char* hlog_level_to_string(hlog_level_t level) {
  switch (level) {
    case HLOG_INFO:
      return "INFO";
    case HLOG_WARN:
      return "WARN";
    case HLOG_ERROR:
      return "ERROR";
    case HLOG_FATAL:
      return "FATAL";
    case HLOG_DEBUG:
      return "DEBUG";
    case HLOG_VERBOSE:
      return "VERBOSE";
  }
}

uint64_t write_log_internal(hlog_level_t level,
                            process_block_t* proc,
                            const char* restrict format,
                            va_list args) {
  uint64_t bytes_written = 0;
  char pid_string_buf[64];
  itoa(multitask_pb_get_pid(proc), pid_string_buf, 10);
  // TODO wire in cool colour effects for log levels here
  bytes_written += print_level(level);
  bytes_written += printf("[%s] (PID: %d): ",
                          multitask_pb_get_name(proc),
                          multitask_pb_get_pid(proc));
  vprintf(format, args);
  printf("\n");
  return bytes_written;
}

uint64_t print_level(hlog_level_t level) {
  uint32_t old_color = terminal_get_fg();
  uint32_t color = 0x0;
  switch (level) {
    case HLOG_INFO:
      color = 0x00FF00;
      break;
    case HLOG_WARN:
      color = 0xFFFB00;
      break;
    case HLOG_ERROR:
      color = 0xFF5B00;
      break;
    case HLOG_FATAL:
      color = 0xFF0000;
      break;
    case HLOG_DEBUG:
      color = 0x00FFFF;
      break;
    case HLOG_VERBOSE:
      color = 0xF000FF;
      break;
    default:
      color = old_color;
  }
  uint64_t total_bytes = 0;
  total_bytes += printf("[");
  terminal_set_fg(color);
  total_bytes += printf("%s", hlog_level_to_string(level));
  terminal_set_fg(old_color);
  total_bytes += printf("] ");
  return total_bytes;
}

