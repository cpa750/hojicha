#include <drivers/serial.h>
#include <drivers/tty.h>
#include <hlog.h>
#include <kernel/g_kernel.h>
#include <multitask/scheduler.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct pending_log pending_log_t;
struct pending_log {
  char* buf;
  process_block_t* proc;
  hlog_level_t level;
  pending_log_t* next;
};

#if defined(__printf_serial)
static bool use_console = true;
#else
static bool use_console = false;
#endif

void add_log_internal(hlogger_t* logger,
                      hlog_level_t level,
                      process_block_t* proc,
                      const char* restrict format,
                      va_list args);
uint64_t write_log_internal(hlog_level_t level,
                            hlogger_t* logger,
                            process_block_t* proc,
                            const char* restrict format,
                            va_list args);
uint64_t print_level(hlog_level_t level);
uint64_t write_log_record(hlog_level_t level, process_block_t* proc, char* buf);
uint64_t serial_write_log_record(hlog_level_t level,
                                 process_block_t* proc,
                                 char* buf);

hlogger_t* hlog_new(hlog_level_t level, uint64_t bufsize) {
  hlogger_t* logger = (hlogger_t*)calloc(1, sizeof(hlogger_t));
  if (logger == NULL) {
    free(logger);
    return NULL;
  }

  logger->max_level = level;
  logger->bufsize = bufsize;
  logger->pending_logs_head = NULL;
  logger->pending_logs_tail = NULL;
  return logger;
}

uint64_t hlog_free_logger(hlogger_t* logger) {
  uint64_t bytes_written = hlog_commit_logger(logger);
  free(logger);
  return bytes_written;
}

void hlog_add(hlog_level_t level, const char* restrict format, ...) {
  hlogger_t* logger = sched_pb_get_logger(g_kernel.current_process);
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
  vsnprintf(buf, logger->bufsize, format, args);
  log->buf = buf;

  if (logger->pending_logs_head == NULL) {
    logger->pending_logs_head = log;
  } else {
    logger->pending_logs_tail->next = log;
  }
  logger->pending_logs_tail = log;
}

uint64_t hlog_commit(void) {
  return hlog_commit_logger(sched_pb_get_logger(g_kernel.current_process));
}

uint64_t hlog_commit_logger(hlogger_t* logger) {
  uint64_t bytes_written = 0;
  pending_log_t* log = logger->pending_logs_head;
  while (log != NULL) {
    bytes_written += write_log_record(log->level, log->proc, log->buf);
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
  hlogger_t* logger = sched_pb_get_logger(g_kernel.current_process);
  if (level > logger->max_level) { return 0; }

  va_list args;
  va_start(args, format);
  uint64_t bytes_written =
      write_log_internal(level, logger, g_kernel.current_process, format, args);
  va_end(args);
  return bytes_written;
}

uint64_t hlog_write_logger(hlogger_t* logger,
                           hlog_level_t level,
                           const char* restrict format,
                           ...) {
  if (level > logger->max_level) { return 0; }

  va_list args;
  va_start(args, format);
  uint64_t bytes_written =
      write_log_internal(level, logger, g_kernel.current_process, format, args);
  va_end(args);
  return bytes_written;
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

void hlog_disable_console(void) { use_console = false; }
void hlog_enable_console(void) {
#if defined(__printf_serial)
  use_console = true;
#else
  use_console = false;
#endif
}

uint64_t write_log_internal(hlog_level_t level,
                            hlogger_t* logger,
                            process_block_t* proc,
                            const char* restrict format,
                            va_list args) {
  char* buf = (char*)malloc(sizeof(char) * logger->bufsize);
  if (buf == NULL) { return 0; }

  vsnprintf(buf, logger->bufsize, format, args);
  uint64_t bytes_written = write_log_record(level, proc, buf);
  free(buf);
  return bytes_written;
}

uint64_t write_log_record(hlog_level_t level,
                          process_block_t* proc,
                          char* buf) {
  if (!use_console) { return serial_write_log_record(level, proc, buf); }

  uint64_t bytes_written = 0;
  bytes_written += print_level(level);
  bytes_written += printf(
      "[%s] (PID: %d): ", sched_pb_get_name(proc), sched_pb_get_pid(proc));
  bytes_written += printf("%s\n", buf);
  return bytes_written;
}

uint64_t serial_write_log_record(hlog_level_t level,
                                 process_block_t* proc,
                                 char* buf) {
  char pid_string_buf[64];
  itoa(sched_pb_get_pid(proc), pid_string_buf, 10);
  char* pid_string = pid_string_buf;
  if (pid_string_buf[0] == '0' && pid_string_buf[1] == 'd') {
    pid_string = pid_string_buf + 2;
  }

  uint64_t bytes_written = 0;
  serial_write_string("[");
  bytes_written++;
  serial_write_string(hlog_level_to_string(level));
  bytes_written += strlen(hlog_level_to_string(level));
  serial_write_string("] [");
  bytes_written += 3;
  serial_write_string(sched_pb_get_name(proc));
  bytes_written += strlen(sched_pb_get_name(proc));
  serial_write_string("] (PID: ");
  bytes_written += 8;
  serial_write_string(pid_string);
  bytes_written += strlen(pid_string);
  serial_write_string("): ");
  bytes_written += 3;
  serial_write_string(buf);
  bytes_written += strlen(buf);
  serial_write_string("\n");
  bytes_written++;
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
