#ifndef HLOG_H
#define HLOG_H

#include <stdint.h>

#ifndef DEFAULT_HLOG_LEVEL
#define DEFAULT_HLOG_LEVEL INFO
#endif

#define DEFAULT_HLOG_BUFSIZE 1024

enum hlog_level { WARN, ERROR, FATAL, INFO, DEBUG, VERBOSE };
typedef enum hlog_level hlog_level_t;

typedef struct pending_log pending_log_t;
struct hlogger {
  hlog_level_t max_level;
  uint64_t bufsize;
  pending_log_t* pending_logs_head;
  pending_log_t* pending_logs_tail;
};
typedef struct hlogger hlogger_t;

// TODO: async logger commits with a dedicated commit process

/*
 * Creates a new logger. Logs below `min_level` will not be written.
 * `bufsize` controls the size of the buffer used to store the formatted
 * log strings after a log has been added but not yet committed. One buffer
 * per log.
 */
hlogger_t* hlog_new(hlog_level_t min_level, uint64_t bufsize);

/*
 * Frees a `logger` previously created with `hlog_new()`
 */
uint64_t hlog_free_logger(hlogger_t* logger);

/*
 * The `hlog_add()` family of functions adds a log to the queue of log
 * messages to be submitted. Logs below the logger's `min_level` will not
 * be written.
 *
 * Logs added with the `hlog_add()` family of functions will
 * not be written immediately - they can be written with the `hlog_commit()`
 * family of functions.
 *
 * These functions block until the log string is formatted.
 *
 * `hlog_add_logger()` take a `logger` argument - in which case, the log
 * will be added to that specific logger's instance. Otherwise, `hlog_add()`
 * will default to the current running process' logger.
 */
void hlog_add(hlog_level_t level, const char* restrict format, ...);
void hlog_add_logger(hlogger_t* logger,
                     hlog_level_t level,
                     const char* restrict format,
                     ...);

/*
 * The `hlog_commit()` family of functions writes out logs previously
 * added with the `hlog_add()` family of functions. These functions return
 * the total number of bytes written.
 *
 * These functions block until the commit completes.
 *
 * `hlog_commit_logger()` takes a `logger` - in which case, the queued
 * logs in `logger` will be written out. Otherwise, `hlog_commit()` will default
 * to committing the logs belonging to the current process' logger.
 */
uint64_t hlog_commit(void);
uint64_t hlog_commit_logger(hlogger_t* logger);

/*
 * The `hlog_write()` family of functions simultaneously adds and commits
 * a log, skipping the queue of logs and blocking until the commit completes.
 *
 * `hlog_write_logger()` takes a `logger` argument to write against,
 * whereas `hlog_write()` will default to the current running process' logger.
 */
uint64_t hlog_write(hlog_level_t level, const char* restrict format, ...);
uint64_t hlog_write_logger(hlogger_t* logger,
                           hlog_level_t level,
                           const char* restrict format,
                           ...);

char* hlog_level_to_string(hlog_level_t level);
#endif  // HLOG_H

