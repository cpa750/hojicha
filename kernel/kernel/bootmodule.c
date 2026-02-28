#include <haddr.h>
#include <hlog.h>
#include <kernel/bootmodule.h>
#include <limine.h>
#include <memory/vmm.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kernel/kernel_state.h"

__attribute__((
    used,
    section(".limine_requests"))) static volatile struct limine_module_request
    module_request = {.id = LIMINE_MODULE_REQUEST, .revision = 0};

#define PAGE_SIZE 4096ULL

static haddr_t hhdm_offset;
static struct limine_module_response* captured_response = NULL;
static uint64_t captured_count = 0;
static bool early_capture_ready = false;

static bootmodule_t* module_cache = NULL;
static uint64_t module_cache_count = 0;
static bool module_cache_ready = false;

static bool streq(const char* a, const char* b) {
  if (a == NULL || b == NULL) { return false; }
  return strcmp(a, b) == 0;
}

static const char* basename_ptr(const char* path) {
  if (path == NULL) { return NULL; }
  const char* basename = path;
  for (const char* c = path; *c != '\0'; ++c) {
    if (*c == '/') { basename = c + 1; }
  }
  return basename;
}

static bool map_page_containing(haddr_t addr) {
  if (addr < hhdm_offset) { return true; }
  haddr_t page = addr & ~(PAGE_SIZE - 1);
  haddr_t phys = page - hhdm_offset;
  return vmm_map_at_paddr(page, phys, PAGE_PRESENT | PAGE_WRITABLE) != 0;
}

static bool map_range(haddr_t addr, uint64_t size) {
  if (size == 0) { return true; }
  if (addr < hhdm_offset) { return true; }

  haddr_t start = addr & ~(PAGE_SIZE - 1);
  haddr_t end = (addr + size - 1) & ~(PAGE_SIZE - 1);
  for (haddr_t page = start; page <= end; page += PAGE_SIZE) {
    haddr_t phys = page - hhdm_offset;
    if (vmm_map_at_paddr(page, phys, PAGE_PRESENT | PAGE_WRITABLE) == 0) {
      return false;
    }
  }
  return true;
}

static bool map_cstring(const char* str) {
  if (str == NULL) { return false; }
  haddr_t page = ((haddr_t)str) & ~(PAGE_SIZE - 1);
  for (;;) {
    if (!map_page_containing(page)) { return false; }
    const char* s = (const char*)page;
    for (haddr_t i = 0; i < PAGE_SIZE; ++i) {
      if (s[i] == '\0') { return true; }
    }
    page += PAGE_SIZE;
  }
}

static char* copy_cstring(const char* src) {
  size_t len = strlen(src) + 1;
  char* dst = malloc(len);
  if (dst == NULL) { return NULL; }
  memcpy(dst, src, len);
  return dst;
}

bool bootmodule_capture_early() {
  if (module_request.response == NULL) { return false; }

  captured_response = module_request.response;
  captured_count = captured_response->module_count;
  early_capture_ready = true;
  module_cache_ready = false;

  return true;
}

bool bootmodule_finalize_cache() {
  hhdm_offset = vmm_state_get_kernel_offset(g_kernel.vmm);
  if (!early_capture_ready) { return false; }

  if (!map_range((haddr_t)captured_response,
                 sizeof(struct limine_module_response))) {
    return false;
  }

  if (!map_range((haddr_t)captured_response->modules,
                 captured_count * sizeof(struct limine_file*))) {
    return false;
  }

  module_cache = malloc(captured_count * sizeof(bootmodule_t));
  if (module_cache == NULL) { return false; }

  module_cache_count = 0;

  for (uint64_t i = 0; i < captured_count; ++i) {
    struct limine_file* module = captured_response->modules[i];
    if (module == NULL) { continue; }

    if (!map_range((haddr_t)module, sizeof(struct limine_file))) { continue; }
    if (module->address == NULL || module->size == 0) { continue; }

    const char* name_src = module->string;
    if (name_src != NULL) {
      if (!map_cstring(name_src)) { continue; }
    } else if (module->path != NULL) {
      if (!map_cstring(module->path)) { continue; }
      name_src = basename_ptr(module->path);
    } else {
      continue;
    }

    if (!map_range((haddr_t)module->address, module->size)) { continue; }

    char* name_copy = copy_cstring(name_src);
    if (name_copy == NULL) { return false; }

    void* payload_copy = malloc(module->size);
    if (payload_copy == NULL) { return false; }
    memcpy(payload_copy, module->address, module->size);

    module_cache[module_cache_count].name = name_copy;
    module_cache[module_cache_count].address = payload_copy;
    module_cache[module_cache_count].size = module->size;
    ++module_cache_count;
  }

  module_cache_ready = true;
  return true;
}

bootmodule_t* bootmodule_get(const char* name) {
  if (!module_cache_ready) { return NULL; }

  for (uint64_t i = 0; i < module_cache_count; ++i) {
    if (streq(name, module_cache[i].name)) { return &module_cache[i]; }
  }

  return NULL;
}

void bootmodule_free(bootmodule_t* bootmodule) { (void)bootmodule; }
