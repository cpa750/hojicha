#ifndef HOJICHA_VMA_H
#define HOJICHA_VMA_H

#include <haddr.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct vma vma_t;

bool vma_insert(vma_t** head,
                haddr_t start,
                haddr_t end,
                uint64_t access,
                uint64_t flags,
                uint64_t offset);
bool vma_remove(vma_t** head, haddr_t start, haddr_t end);
bool vma_copy_list(vma_t** dst, vma_t* src);
void vma_clear(vma_t** head);

#endif  // HOJICHA_VMA_H
