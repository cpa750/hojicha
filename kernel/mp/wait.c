#include <errno.h>
#include <mp/scheduler.h>
#include <mp/wait.h>
#include <mp/wait_queue.h>
#include <stdint.h>

#include "mp_utils.h"

#define WAITPID_WNOHANG 1

static long process_wait_collect(proc_t* parent,
                                 proc_t* child,
                                 uint64_t child_slot,
                                 int* wstatus);
static bool wait_child_find(proc_t* parent,
                            long pid,
                            proc_t** child_out,
                            uint64_t* child_slot_out);

long waitpid_proc(proc_t* process, long pid, int* wstatus, int options) {
  if (process == NULL || process->children == NULL) { return -ECHILD; }
  if ((options & ~WAITPID_WNOHANG) != 0 || pid == 0 || pid < -1) {
    return -EINVAL;
  }

  sched_postpone();

  proc_t* child = NULL;
  uint64_t child_slot = 0;
  bool has_child = wait_child_find(process, pid, &child, &child_slot);

  if (!has_child) {
    sched_resume();
    return -ECHILD;
  }

  if (child->status == PROC_STATUS_READY_TO_DIE) {
    long ret = process_wait_collect(process, child, child_slot, wstatus);
    sched_resume();
    return ret;
  }

  if ((options & WAITPID_WNOHANG) != 0) {
    sched_resume();
    return 0;
  }

  if (pid == -1) {
    wait_queue_sleep_postponed(&process->child_waiters);
    has_child = wait_child_find(process, pid, &child, &child_slot);
  } else {
    wait_queue_sleep_postponed(&child->exit_waiters);
  }

  if (!has_child || child == NULL ||
      child->status != PROC_STATUS_READY_TO_DIE) {
    sched_resume();
    return -ECHILD;
  }

  long ret = process_wait_collect(process, child, child_slot, wstatus);
  sched_resume();
  return ret;
}

static long process_wait_collect(proc_t* parent,
                                 proc_t* child,
                                 uint64_t child_slot,
                                 int* wstatus) {
  if (parent == NULL || child == NULL || parent->children == NULL) {
    return -ECHILD;
  }
  if (child->status != PROC_STATUS_READY_TO_DIE) { return -ECHILD; }

  long child_pid = child->pid;
  if (wstatus != NULL) { *wstatus = (child->exit_code & 0xFF) << 8; }

  parent->children[child_slot] = NULL;
  child->parent = NULL;
  mp_ready_to_die_remove(child);
  proc_free(child);
  return child_pid;
}

static bool wait_child_find(proc_t* parent,
                            long pid,
                            proc_t** child_out,
                            uint64_t* child_slot_out) {
  if (parent == NULL || parent->children == NULL) { return false; }

  proc_t* first_matching_child = NULL;
  uint64_t first_matching_child_slot = 0;
  for (uint64_t child_slot = 1; child_slot < MAX_CHILDREN; ++child_slot) {
    proc_t* child = parent->children[child_slot];
    if (child == NULL) { continue; }
    if (pid != -1 && child->pid != (uint64_t)pid) { continue; }

    if (child->status == PROC_STATUS_READY_TO_DIE) {
      if (child_out != NULL) { *child_out = child; }
      if (child_slot_out != NULL) { *child_slot_out = child_slot; }
      return true;
    }

    if (first_matching_child == NULL) {
      first_matching_child = child;
      first_matching_child_slot = child_slot;
    }
  }

  if (first_matching_child == NULL) { return false; }

  if (child_out != NULL) { *child_out = first_matching_child; }
  if (child_slot_out != NULL) { *child_slot_out = first_matching_child_slot; }
  return true;
}
