#include <hlog.h>
#include <kernel/kernel_state.h>
#include <kernel/multitask.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

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
    bytes_written += printf("[%s] [%s] (PID: %d): ",
                            hlog_level_to_string(log->level),
                            multitask_pb_get_name(log->proc),
                            multitask_pb_get_pid(log->proc));
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
}

uint64_t write_log_internal(hlog_level_t level,
                            process_block_t* proc,
                            const char* restrict format,
                            va_list args) {
  uint64_t bytes_written = 0;
  char pid_string_buf[64];
  itoa(multitask_pb_get_pid(proc), pid_string_buf, 10);
  // TODO wire in cool colour effects for log levels here
  bytes_written += printf("[%s] [%s] (PID: %d): ",
                          hlog_level_to_string(level),
                          multitask_pb_get_name(proc),
                          multitask_pb_get_pid(proc));
  vprintf(format, args);
  return bytes_written;
}

char* hlog_level_to_string(hlog_level_t level) {
  switch (level) {
    case INFO:
      return "INFO";
    case WARN:
      return "WARN";
    case ERROR:
      return "ERROR";
    case FATAL:
      return "FATAL";
    case DEBUG:
      return "DEBUG";
    case VERBOSE:
      return "VERBOSE";
  }
}

