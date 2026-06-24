#include <haddr.h>
#include <memory/vma.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "vma_internal.h"

static bool vma_range_valid(haddr_t start, haddr_t end);
static bool vma_overlaps(vma_t* vma, haddr_t start, haddr_t end);
static haddr_t vma_align_up(haddr_t addr);
static bool vma_range_fits(haddr_t start,
                           haddr_t min,
                           haddr_t max,
                           haddr_t length,
                           haddr_t* end_out);
static void vma_unlink(vma_t** head, vma_t* node);
static void vma_link_before(vma_t** head, vma_t* before, vma_t* node);

bool vma_insert(vma_t** head,
                haddr_t start,
                haddr_t end,
                uint64_t access,
                uint64_t flags,
                uint64_t offset) {
  if (head == NULL || !vma_range_valid(start, end)) { return false; }

  vma_t* current = *head;
  while (current != NULL && current->end < start) {
    if (current->end == (haddr_t)-1) { return false; }
    current = current->next;
  }

  if (current != NULL && vma_overlaps(current, start, end)) { return false; }

  vma_t* node = calloc(1, sizeof(vma_t));
  if (node == NULL) { return false; }

  node->start = start;
  node->end = end;
  node->access = access;
  node->flags = flags;
  node->offset = offset;
  vma_link_before(head, current, node);
  return true;
}

bool vma_remove(vma_t** head, haddr_t start, haddr_t end) {
  if (head == NULL || !vma_range_valid(start, end)) { return false; }

  vma_t* current = *head;
  while (current != NULL && current->end < start) { current = current->next; }

  if (current == NULL || current->start > start) { return true; }

  haddr_t remove_end = end < current->end ? end : current->end;

  if (start == current->start && remove_end == current->end) {
    vma_unlink(head, current);
  } else if (start == current->start) {
    current->start = remove_end + 1;
  } else if (remove_end == current->end) {
    current->end = start - 1;
  } else {
    vma_t* split = calloc(1, sizeof(vma_t));
    if (split == NULL) { return false; }

    split->start = remove_end + 1;
    split->end = current->end;
    split->access = current->access;
    split->flags = current->flags;
    split->offset = current->offset;

    current->end = start - 1;
    vma_link_before(head, current->next, split);
  }

  return true;
}

bool vma_copy_list(vma_t** dst, vma_t* src) {
  if (dst == NULL || *dst != NULL) { return false; }

  for (vma_t* current = src; current != NULL; current = current->next) {
    if (!vma_insert(dst,
                    current->start,
                    current->end,
                    current->access,
                    current->flags,
                    current->offset)) {
      vma_clear(dst);
      return false;
    }
  }

  return true;
}

void vma_clear(vma_t** head) {
  if (head == NULL) { return; }

  while (*head != NULL) {
    vma_t* next = (*head)->next;
    free(*head);
    *head = next;
  }
}

bool vma_find_free_forward(vma_t* head,
                           haddr_t hint,
                           haddr_t min,
                           haddr_t max,
                           haddr_t length,
                           haddr_t* out) {
  if (out == NULL || length == 0 || max < min) { return false; }
  if ((length & (VMA_PAGE_SIZE - 1)) != 0) { return false; }

  haddr_t candidate = hint == 0 ? min : hint;
  if ((candidate & (VMA_PAGE_SIZE - 1)) != 0) {
    if (candidate > (haddr_t)-1 - (VMA_PAGE_SIZE - 1)) { return false; }
    candidate = vma_align_up(candidate);
  }
  if (candidate < min) { candidate = min; }

  haddr_t end = 0;
  if (!vma_range_fits(candidate, min, max, length, &end)) { return false; }

  for (vma_t* current = head; current != NULL; current = current->next) {
    if (current->end < candidate) { continue; }
    if (end < current->start) {
      *out = candidate;
      return true;
    }

    if (current->end == (haddr_t)-1) { return false; }
    candidate = current->end + 1;
    if (!vma_range_fits(candidate, min, max, length, &end)) { return false; }
  }

  *out = candidate;
  return true;
}

bool vma_find_free_fixed(vma_t* head,
                         haddr_t addr,
                         haddr_t min,
                         haddr_t max,
                         haddr_t length,
                         haddr_t* out) {
  if (out == NULL || length == 0 || max < min) { return false; }
  if ((addr & (VMA_PAGE_SIZE - 1)) != 0) { return false; }
  if ((length & (VMA_PAGE_SIZE - 1)) != 0) { return false; }

  haddr_t end = 0;
  if (!vma_range_fits(addr, min, max, length, &end)) { return false; }

  for (vma_t* current = head; current != NULL; current = current->next) {
    if (current->start > end) { break; }
    if (vma_overlaps(current, addr, end)) { return false; }
  }

  *out = addr;
  return true;
}

static bool vma_range_valid(haddr_t start, haddr_t end) {
  if (end < start) { return false; }
  if ((start & (VMA_PAGE_SIZE - 1)) != 0) { return false; }
  if (((end + 1) & (VMA_PAGE_SIZE - 1)) != 0) { return false; }
  return true;
}

static bool vma_overlaps(vma_t* vma, haddr_t start, haddr_t end) {
  return vma->start <= end && start <= vma->end;
}

static haddr_t vma_align_up(haddr_t addr) {
  return (addr + VMA_PAGE_SIZE - 1) & ~(VMA_PAGE_SIZE - 1);
}

static bool vma_range_fits(haddr_t start,
                           haddr_t min,
                           haddr_t max,
                           haddr_t length,
                           haddr_t* end_out) {
  if (start < min || start > max) { return false; }
  if (length - 1 > (haddr_t)-1 - start) { return false; }

  haddr_t end = start + length - 1;
  if (end > max) { return false; }
  if (end_out != NULL) { *end_out = end; }
  return true;
}

static void vma_unlink(vma_t** head, vma_t* node) {
  if (node->prev != NULL) {
    node->prev->next = node->next;
  } else {
    *head = node->next;
  }

  if (node->next != NULL) { node->next->prev = node->prev; }
  free(node);
}

static void vma_link_before(vma_t** head, vma_t* before, vma_t* node) {
  if (before == NULL) {
    vma_t* tail = *head;
    if (tail == NULL) {
      *head = node;
      return;
    }

    while (tail->next != NULL) { tail = tail->next; }

    tail->next = node;
    node->prev = tail;
    return;
  }

  node->next = before;
  node->prev = before->prev;
  before->prev = node;

  if (node->prev != NULL) {
    node->prev->next = node;
  } else {
    *head = node;
  }
}
