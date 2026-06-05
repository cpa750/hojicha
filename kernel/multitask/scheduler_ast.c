#include <hlog.h>
#include <kernel/g_kernel.h>
#include <multitask/scheduler.h>
#include <multitask/semaphore.h>
#include <stdbool.h>
#include <stdint.h>

static semaphore_t* ast_scheduler_sem;
static bool ast_scheduler_awake_1 = false;
static bool ast_scheduler_awake_2 = false;
static process_block_t* ast_scheduler_proc_1;
static process_block_t* ast_scheduler_proc_2;

static void ast_scheduler_watch_1(void) {
  ast_scheduler_awake_1 = true;
  while (1) {
    if (ast_scheduler_awake_1) {
      hlog_write(HLOG_INFO, "AST_SCHEDULER: watch_1 observed wake");
      ast_scheduler_awake_1 = false;
    }
  }
}

static void ast_scheduler_watch_2(void) {
  ast_scheduler_awake_2 = true;
  while (1) {
    if (ast_scheduler_awake_2) {
      hlog_write(HLOG_INFO, "AST_SCHEDULER: watch_2 observed wake");
      ast_scheduler_awake_2 = false;
    }
  }
}

static void ast_scheduler_sleep_once_1(void) {
  sched_current_sleep(7);
  hlog_write(HLOG_INFO, "AST_SCHEDULER: sleep_once_1 complete");
}

static void ast_scheduler_sleep_once_2(void) {
  sched_current_sleep(7);
  hlog_write(HLOG_INFO, "AST_SCHEDULER: sleep_once_2 complete");
}

static void ast_scheduler_sem_owner(void) {
  hlog_write(HLOG_INFO, "AST_SCHEDULER: sem_owner lock requested");
  semaphore_lock(ast_scheduler_sem);
  hlog_write(HLOG_INFO, "AST_SCHEDULER: sem_owner lock acquired");
  sched_current_sleep(7);
  hlog_write(HLOG_INFO, "AST_SCHEDULER: sem_owner unlock");
  semaphore_unlock(ast_scheduler_sem);
}

static void ast_scheduler_sem_waiter(void) {
  hlog_write(HLOG_INFO, "AST_SCHEDULER: sem_waiter lock requested");
  semaphore_lock(ast_scheduler_sem);
  hlog_write(HLOG_INFO, "AST_SCHEDULER: sem_waiter lock acquired");
  hlog_write(HLOG_INFO, "AST_SCHEDULER: sem_waiter unlock");
  semaphore_unlock(ast_scheduler_sem);
}

static void ast_scheduler_try_fail(void) {
  hlog_write(HLOG_INFO, "AST_SCHEDULER: try_fail lock requested");
  bool success = semaphore_try_lock(ast_scheduler_sem);
  if (success) {
    hlog_write(HLOG_ERROR, "AST_SCHEDULER: try_fail unexpectedly acquired");
    semaphore_unlock(ast_scheduler_sem);
  } else {
    hlog_write(HLOG_INFO, "AST_SCHEDULER: try_fail lock denied");
  }
}

static void ast_scheduler_try_success(void) {
  sched_current_sleep(20);
  hlog_write(HLOG_INFO, "AST_SCHEDULER: try_success lock requested");
  bool success = semaphore_try_lock(ast_scheduler_sem);
  if (!success) {
    hlog_write(HLOG_ERROR, "AST_SCHEDULER: try_success lock denied");
  } else {
    hlog_write(HLOG_INFO, "AST_SCHEDULER: try_success lock acquired");
    semaphore_unlock(ast_scheduler_sem);
  }
}

static void ast_scheduler_waker(void) {
  while (1) {
    sched_current_sleep(5);
    hlog_write(HLOG_INFO, "AST_SCHEDULER: waker awake");
    ast_scheduler_awake_1 = true;
    ast_scheduler_awake_2 = true;
  }
}

static void ast_scheduler_monitor(void) {
  uint64_t count = 0;
  while (1) {
    sched_current_sleep(1);
    ++count;
    hlog_write(HLOG_INFO, "AST_SCHEDULER: monitor tick %d", count);

    if (count == 15) {
      hlog_write(HLOG_WARN, "AST_SCHEDULER: terminating watch_2");
      sched_proc_terminate(ast_scheduler_proc_2);
    }

    if (count == 21) {
      hlog_write(HLOG_WARN, "AST_SCHEDULER: terminating watch_1");
      sched_proc_terminate(ast_scheduler_proc_1);
    }
  }
}

static void ast_scheduler_add(char* name, proc_entry_t entry) {
  process_block_t* proc =
      sched_kproc_new(name, entry, sched_pb_get_cr3(g_kernel.current_process));
  sched_add_proc(proc);
  if (entry == ast_scheduler_watch_1) { ast_scheduler_proc_1 = proc; }
  if (entry == ast_scheduler_watch_2) { ast_scheduler_proc_2 = proc; }
}

void ast_scheduler(void) {
  hlog_write(HLOG_INFO, "AST_SCHEDULER: starting");
  ast_scheduler_sem = semaphore_create(1);

  ast_scheduler_add("ast_sched_waker", ast_scheduler_waker);
  ast_scheduler_add("ast_sched_watch_1", ast_scheduler_watch_1);
  ast_scheduler_add("ast_sched_watch_2", ast_scheduler_watch_2);
  ast_scheduler_add("ast_sched_sleep_1", ast_scheduler_sleep_once_1);
  ast_scheduler_add("ast_sched_sleep_2", ast_scheduler_sleep_once_2);
  ast_scheduler_add("ast_sched_sem_owner", ast_scheduler_sem_owner);
  ast_scheduler_add("ast_sched_sem_waiter", ast_scheduler_sem_waiter);
  ast_scheduler_add("ast_sched_try_fail", ast_scheduler_try_fail);
  ast_scheduler_add("ast_sched_try_success", ast_scheduler_try_success);
  ast_scheduler_add("ast_sched_monitor", ast_scheduler_monitor);
  hlog_write(HLOG_INFO, "AST_SCHEDULER: processes added");
}
