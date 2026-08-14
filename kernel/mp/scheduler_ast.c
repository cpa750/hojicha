#include <drivers/pit.h>
#include <hlog.h>
#include <kernel/g_kernel.h>
#include <mp/proc.h>
#include <mp/scheduler.h>
#include <mp/semaphore.h>
#include <mp/wait_queue.h>
#include <stdbool.h>
#include <stdint.h>

#define AST_SCHEDULER_LOG(level, format, ...)                                  \
  hlog_add(level,                                                              \
           "AST_SCHEDULER:%d: " format,                                        \
           ast_scheduler_timestamp(),                                          \
           ##__VA_ARGS__)

static semaphore_t* ast_scheduler_sem;
static wait_queue_t ast_scheduler_wait_queue;
static wait_queue_t ast_scheduler_postponed_wait_queue;
static volatile bool ast_scheduler_awake_1 = false;
static volatile bool ast_scheduler_awake_2 = false;
static volatile bool ast_scheduler_postponed_waiter_woke = false;
static volatile uint64_t ast_scheduler_log_sequence = 0;
static proc_t* ast_scheduler_proc_1;
static proc_t* ast_scheduler_proc_2;

static uint64_t ast_scheduler_timestamp(void) {
  uint64_t time = pit_get_ns_elapsed_since_init(g_kernel.pit);
  uint64_t sequence =
      __atomic_fetch_add(&ast_scheduler_log_sequence, 1, __ATOMIC_RELAXED);
  return (time << 16) | (sequence & 0xFFFF);
}

static void ast_scheduler_commit_proc_list(proc_t* proc) {
  for (; proc != NULL; proc = proc_get_next(proc)) {
    hlog_commit_logger(proc_get_logger(proc));
  }
}

static void ast_scheduler_commit_logs(void) {
  sched_postpone();
  hlog_commit_logger(proc_get_logger(g_kernel.current_process));
  ast_scheduler_commit_proc_list(sched_state_get_ready_head(g_kernel.sched));
  ast_scheduler_commit_proc_list(sched_state_get_sleeping_head(g_kernel.sched));
  ast_scheduler_commit_proc_list(
      sched_state_get_ready_to_die_head(g_kernel.sched));
  ast_scheduler_commit_proc_list(ast_scheduler_wait_queue.head);
  ast_scheduler_commit_proc_list(ast_scheduler_postponed_wait_queue.head);
  sched_resume();
}

static void ast_scheduler_watch_1(void) {
  while (1) {
    if (ast_scheduler_awake_1) {
      AST_SCHEDULER_LOG(HLOG_INFO, "watch_1 observed wake");
      ast_scheduler_awake_1 = false;
    }
  }
}

static void ast_scheduler_watch_2(void) {
  while (1) {
    if (ast_scheduler_awake_2) {
      AST_SCHEDULER_LOG(HLOG_INFO, "watch_2 observed wake");
      ast_scheduler_awake_2 = false;
    }
  }
}

static void ast_scheduler_sleep_once_1(void) {
  sched_current_sleep(7);
  AST_SCHEDULER_LOG(HLOG_INFO, "sleep_once_1 complete");
}

static void ast_scheduler_sleep_once_2(void) {
  sched_current_sleep(7);
  AST_SCHEDULER_LOG(HLOG_INFO, "sleep_once_2 complete");
}

static void ast_scheduler_sem_owner(void) {
  AST_SCHEDULER_LOG(HLOG_INFO, "sem_owner lock requested");
  semaphore_lock(ast_scheduler_sem);
  AST_SCHEDULER_LOG(HLOG_INFO, "sem_owner lock acquired");
  sched_current_sleep(7);
  AST_SCHEDULER_LOG(HLOG_INFO, "sem_owner unlock");
  semaphore_unlock(ast_scheduler_sem);
}

static void ast_scheduler_sem_waiter(void) {
  AST_SCHEDULER_LOG(HLOG_INFO, "sem_waiter lock requested");
  semaphore_lock(ast_scheduler_sem);
  AST_SCHEDULER_LOG(HLOG_INFO, "sem_waiter lock acquired");
  AST_SCHEDULER_LOG(HLOG_INFO, "sem_waiter unlock");
  semaphore_unlock(ast_scheduler_sem);
}

static void ast_scheduler_try_fail(void) {
  AST_SCHEDULER_LOG(HLOG_INFO, "try_fail lock requested");
  bool success = semaphore_try_lock(ast_scheduler_sem);
  if (success) {
    AST_SCHEDULER_LOG(HLOG_ERROR, "try_fail unexpectedly acquired");
    semaphore_unlock(ast_scheduler_sem);
  } else {
    AST_SCHEDULER_LOG(HLOG_INFO, "try_fail lock denied");
  }
}

static void ast_scheduler_try_success(void) {
  sched_current_sleep(20);
  AST_SCHEDULER_LOG(HLOG_INFO, "try_success lock requested");
  bool success = semaphore_try_lock(ast_scheduler_sem);
  if (!success) {
    AST_SCHEDULER_LOG(HLOG_ERROR, "try_success lock denied");
  } else {
    AST_SCHEDULER_LOG(HLOG_INFO, "try_success lock acquired");
    semaphore_unlock(ast_scheduler_sem);
  }
}

static void ast_scheduler_waiter_1(void) {
  AST_SCHEDULER_LOG(HLOG_INFO, "waitq_waiter_1 sleeping");
  wait_queue_sleep(&ast_scheduler_wait_queue);
  AST_SCHEDULER_LOG(HLOG_INFO, "waitq_waiter_1 woke");
}

static void ast_scheduler_waiter_2(void) {
  AST_SCHEDULER_LOG(HLOG_INFO, "waitq_waiter_2 sleeping");
  wait_queue_sleep(&ast_scheduler_wait_queue);
  AST_SCHEDULER_LOG(HLOG_INFO, "waitq_waiter_2 woke");
}

static void ast_scheduler_waitq_waker(void) {
  sched_current_sleep(3);
  AST_SCHEDULER_LOG(HLOG_INFO, "waitq wake one");
  wait_queue_wake_one(&ast_scheduler_wait_queue);
  sched_current_sleep(3);
  AST_SCHEDULER_LOG(HLOG_INFO, "waitq wake all");
  wait_queue_wake_all(&ast_scheduler_wait_queue);
}

static void ast_scheduler_waitq_postponed_waiter(void) {
  AST_SCHEDULER_LOG(HLOG_INFO, "waitq_postponed_waiter sleeping");
  sched_postpone();
  wait_queue_sleep_postponed(&ast_scheduler_postponed_wait_queue);
  sched_resume();
  ast_scheduler_postponed_waiter_woke = true;
  AST_SCHEDULER_LOG(HLOG_INFO, "waitq_postponed_waiter woke");
}

static void ast_scheduler_waitq_postponed_waker(void) {
  sched_current_sleep(3);
  if (ast_scheduler_postponed_waiter_woke) {
    AST_SCHEDULER_LOG(HLOG_ERROR, "waitq_postponed waiter woke before wake");
  }

  AST_SCHEDULER_LOG(HLOG_INFO, "waitq_postponed wake all");
  wait_queue_wake_all(&ast_scheduler_postponed_wait_queue);

  sched_current_sleep(1);
  if (ast_scheduler_postponed_waiter_woke) {
    AST_SCHEDULER_LOG(HLOG_INFO, "waitq_postponed waiter woke after wake");
  } else {
    AST_SCHEDULER_LOG(HLOG_ERROR, "waitq_postponed waiter did not wake");
  }
}

static void ast_scheduler_waker(void) {
  while (1) {
    sched_current_sleep(5);
    AST_SCHEDULER_LOG(HLOG_INFO, "waker awake");
    ast_scheduler_awake_1 = true;
    ast_scheduler_awake_2 = true;
  }
}

static void ast_scheduler_monitor(void) {
  uint64_t count = 0;
  while (1) {
    sched_current_sleep(1);
    ++count;
    AST_SCHEDULER_LOG(HLOG_INFO, "monitor tick %d", count);

    if (count == 15) {
      AST_SCHEDULER_LOG(HLOG_WARN, "terminating watch_2");
      hlog_commit_logger(proc_get_logger(ast_scheduler_proc_2));
      sched_proc_terminate(ast_scheduler_proc_2);
    }

    if (count == 21) {
      AST_SCHEDULER_LOG(HLOG_WARN, "terminating watch_1");
      hlog_commit_logger(proc_get_logger(ast_scheduler_proc_1));
      sched_proc_terminate(ast_scheduler_proc_1);
    }

    if (count == 22) {
      ast_scheduler_commit_logs();
      return;
    }
  }
}

static void ast_scheduler_add(char* name, proc_entry_t entry) {
  proc_t* proc =
      sched_kproc_new(name, entry, proc_get_cr3(g_kernel.current_process));
  sched_add_proc(proc);
  if (entry == ast_scheduler_watch_1) { ast_scheduler_proc_1 = proc; }
  if (entry == ast_scheduler_watch_2) { ast_scheduler_proc_2 = proc; }
}

void ast_scheduler(void) {
  AST_SCHEDULER_LOG(HLOG_INFO, "starting");
  ast_scheduler_sem = semaphore_create(1);
  wait_queue_init(&ast_scheduler_wait_queue);
  wait_queue_init(&ast_scheduler_postponed_wait_queue);

  ast_scheduler_add("ast_sched_waker", ast_scheduler_waker);
  ast_scheduler_add("ast_sched_watch_1", ast_scheduler_watch_1);
  ast_scheduler_add("ast_sched_watch_2", ast_scheduler_watch_2);
  ast_scheduler_add("ast_sched_sleep_1", ast_scheduler_sleep_once_1);
  ast_scheduler_add("ast_sched_sleep_2", ast_scheduler_sleep_once_2);
  ast_scheduler_add("ast_sched_sem_owner", ast_scheduler_sem_owner);
  ast_scheduler_add("ast_sched_sem_waiter", ast_scheduler_sem_waiter);
  ast_scheduler_add("ast_sched_try_fail", ast_scheduler_try_fail);
  ast_scheduler_add("ast_sched_try_success", ast_scheduler_try_success);
  ast_scheduler_add("ast_sched_waitq_waiter_1", ast_scheduler_waiter_1);
  ast_scheduler_add("ast_sched_waitq_waiter_2", ast_scheduler_waiter_2);
  ast_scheduler_add("ast_sched_waitq_waker", ast_scheduler_waitq_waker);
  ast_scheduler_add("ast_sched_waitq_post_waiter",
                    ast_scheduler_waitq_postponed_waiter);
  ast_scheduler_add("ast_sched_waitq_post_waker",
                    ast_scheduler_waitq_postponed_waker);
  ast_scheduler_add("ast_sched_monitor", ast_scheduler_monitor);
  AST_SCHEDULER_LOG(HLOG_INFO, "processes added");
}
