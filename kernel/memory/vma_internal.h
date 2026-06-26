#ifndef HOJICHA_VMA_INTERNAL_H
#define HOJICHA_VMA_INTERNAL_H

#include <haddr.h>
#include <memory/vma.h>
#include <stdint.h>

#define VMA_PAGE_SIZE 4096ULL

struct vma {
  haddr_t start;
  haddr_t end;
  uint64_t access;
  uint64_t flags;
  uint64_t offset;
  vma_t* next;
  vma_t* prev;
};

#endif  // HOJICHA_VMA_INTERNAL_H
