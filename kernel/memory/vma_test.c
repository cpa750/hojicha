#include <memory/vma.h>
#include <memory/vma_test.h>
#include <utils/test.h>

#include "vma_internal.h"

#define PAGE       VMA_PAGE_SIZE
#define VMA_ACCESS 1
#define VMA_FLAGS  2
#define VMA_OFFSET 3

static uint64_t vma_test_count(vma_t* head) {
  uint64_t count = 0;
  while (head != NULL) {
    count++;
    head = head->next;
  }
  return count;
}

static void vma_test_cleanup(htest_ctx_t* ctx, vma_t** head) {
  while (*head != NULL) {
    HTEST_ASSERT(ctx, vma_remove(head, (*head)->start, (*head)->end));
  }
}

void vma_test(void) {
  htest_ctx_t ctx = {0};
  htest_suite_begin(&ctx, "vma");

  htest_case_begin(&ctx, "insert single range");
  {
    vma_t* head = NULL;

    HTEST_ASSERT(
        &ctx,
        vma_insert(
            &head, PAGE, (2 * PAGE) - 1, VMA_ACCESS, VMA_FLAGS, VMA_OFFSET));
    HTEST_ASSERT(&ctx, head != NULL);
    HTEST_ASSERT(&ctx, head->start == PAGE);
    HTEST_ASSERT(&ctx, head->end == (2 * PAGE) - 1);
    HTEST_ASSERT(&ctx, head->access == VMA_ACCESS);
    HTEST_ASSERT(&ctx, head->flags == VMA_FLAGS);
    HTEST_ASSERT(&ctx, head->offset == VMA_OFFSET);
    HTEST_ASSERT(&ctx, head->next == NULL);
    HTEST_ASSERT(&ctx, head->prev == NULL);

    vma_test_cleanup(&ctx, &head);
  }

  htest_case_begin(&ctx, "insert requires page boundaries");
  {
    vma_t* head = NULL;

    HTEST_ASSERT(&ctx, !vma_insert(&head, PAGE + 1, (2 * PAGE) - 1, 0, 0, 0));
    HTEST_ASSERT(&ctx, head == NULL);
    HTEST_ASSERT(&ctx, !vma_insert(&head, PAGE, (2 * PAGE), 0, 0, 0));
    HTEST_ASSERT(&ctx, head == NULL);

    vma_test_cleanup(&ctx, &head);
  }

  htest_case_begin(&ctx, "insert rejects overlaps");
  {
    vma_t* head = NULL;

    HTEST_ASSERT(&ctx, vma_insert(&head, PAGE, (3 * PAGE) - 1, 0, 0, 0));
    HTEST_ASSERT(&ctx, !vma_insert(&head, 2 * PAGE, (4 * PAGE) - 1, 0, 0, 0));
    HTEST_ASSERT(&ctx, vma_test_count(head) == 1);
    HTEST_ASSERT(&ctx, head->start == PAGE);
    HTEST_ASSERT(&ctx, head->end == (3 * PAGE) - 1);

    vma_test_cleanup(&ctx, &head);
  }

  htest_case_begin(&ctx, "delete whole range");
  {
    vma_t* head = NULL;

    HTEST_ASSERT(&ctx, vma_insert(&head, PAGE, (2 * PAGE) - 1, 0, 0, 0));
    HTEST_ASSERT(&ctx, vma_remove(&head, PAGE, (2 * PAGE) - 1));
    HTEST_ASSERT(&ctx, head == NULL);

    vma_test_cleanup(&ctx, &head);
  }

  htest_case_begin(&ctx, "delete requires page boundaries");
  {
    vma_t* head = NULL;

    HTEST_ASSERT(&ctx, vma_insert(&head, PAGE, (3 * PAGE) - 1, 0, 0, 0));
    HTEST_ASSERT(&ctx, !vma_remove(&head, PAGE + 1, (2 * PAGE) - 1));
    HTEST_ASSERT(&ctx, !vma_remove(&head, PAGE, (2 * PAGE)));
    HTEST_ASSERT(&ctx, vma_test_count(head) == 1);
    HTEST_ASSERT(&ctx, head->start == PAGE);
    HTEST_ASSERT(&ctx, head->end == (3 * PAGE) - 1);

    vma_test_cleanup(&ctx, &head);
  }

  htest_case_begin(&ctx, "delete leading page");
  {
    vma_t* head = NULL;

    HTEST_ASSERT(&ctx, vma_insert(&head, PAGE, (3 * PAGE) - 1, 0, 0, 0));
    HTEST_ASSERT(&ctx, vma_remove(&head, PAGE, (2 * PAGE) - 1));
    HTEST_ASSERT(&ctx, vma_test_count(head) == 1);
    HTEST_ASSERT(&ctx, head->start == 2 * PAGE);
    HTEST_ASSERT(&ctx, head->end == (3 * PAGE) - 1);

    vma_test_cleanup(&ctx, &head);
  }

  htest_case_begin(&ctx, "delete ignores start outside range");
  {
    vma_t* head = NULL;

    HTEST_ASSERT(&ctx, vma_insert(&head, 2 * PAGE, (5 * PAGE) - 1, 0, 0, 0));
    HTEST_ASSERT(&ctx, vma_remove(&head, PAGE, (3 * PAGE) - 1));
    HTEST_ASSERT(&ctx, vma_test_count(head) == 1);
    HTEST_ASSERT(&ctx, head->start == 2 * PAGE);
    HTEST_ASSERT(&ctx, head->end == (5 * PAGE) - 1);

    vma_test_cleanup(&ctx, &head);
  }

  htest_case_begin(&ctx, "delete trailing page");
  {
    vma_t* head = NULL;

    HTEST_ASSERT(&ctx, vma_insert(&head, PAGE, (3 * PAGE) - 1, 0, 0, 0));
    HTEST_ASSERT(&ctx, vma_remove(&head, 2 * PAGE, (3 * PAGE) - 1));
    HTEST_ASSERT(&ctx, vma_test_count(head) == 1);
    HTEST_ASSERT(&ctx, head->start == PAGE);
    HTEST_ASSERT(&ctx, head->end == (2 * PAGE) - 1);

    vma_test_cleanup(&ctx, &head);
  }

  htest_case_begin(&ctx, "delete clips trailing overrun");
  {
    vma_t* head = NULL;

    HTEST_ASSERT(&ctx, vma_insert(&head, 2 * PAGE, (5 * PAGE) - 1, 0, 0, 0));
    HTEST_ASSERT(&ctx, vma_remove(&head, 4 * PAGE, (7 * PAGE) - 1));
    HTEST_ASSERT(&ctx, vma_test_count(head) == 1);
    HTEST_ASSERT(&ctx, head->start == 2 * PAGE);
    HTEST_ASSERT(&ctx, head->end == (4 * PAGE) - 1);

    vma_test_cleanup(&ctx, &head);
  }

  htest_case_begin(&ctx, "delete middle page splits range");
  {
    vma_t* head = NULL;
    vma_t* second = NULL;

    HTEST_ASSERT(&ctx, vma_insert(&head, PAGE, (4 * PAGE) - 1, 0, 0, 0));
    HTEST_ASSERT(&ctx, vma_remove(&head, 2 * PAGE, (3 * PAGE) - 1));
    second = head->next;
    HTEST_ASSERT(&ctx, vma_test_count(head) == 2);
    HTEST_ASSERT(&ctx, head->start == PAGE);
    HTEST_ASSERT(&ctx, head->end == (2 * PAGE) - 1);
    HTEST_ASSERT(&ctx, second != NULL);
    HTEST_ASSERT(&ctx, second->start == 3 * PAGE);
    HTEST_ASSERT(&ctx, second->end == (4 * PAGE) - 1);
    HTEST_ASSERT(&ctx, second->prev == head);

    vma_test_cleanup(&ctx, &head);
  }

  htest_case_begin(&ctx, "delete clips at starting range");
  {
    vma_t* head = NULL;
    vma_t* second = NULL;

    HTEST_ASSERT(&ctx, vma_insert(&head, PAGE, (3 * PAGE) - 1, 0, 0, 0));
    HTEST_ASSERT(&ctx, vma_insert(&head, 3 * PAGE, (6 * PAGE) - 1, 0, 0, 0));
    HTEST_ASSERT(&ctx, vma_remove(&head, 2 * PAGE, (5 * PAGE) - 1));
    second = head->next;
    HTEST_ASSERT(&ctx, vma_test_count(head) == 2);
    HTEST_ASSERT(&ctx, head->start == PAGE);
    HTEST_ASSERT(&ctx, head->end == (2 * PAGE) - 1);
    HTEST_ASSERT(&ctx, second != NULL);
    HTEST_ASSERT(&ctx, second->start == 3 * PAGE);
    HTEST_ASSERT(&ctx, second->end == (6 * PAGE) - 1);
    HTEST_ASSERT(&ctx, second->prev == head);

    vma_test_cleanup(&ctx, &head);
  }

  htest_suite_pass(&ctx);
}
