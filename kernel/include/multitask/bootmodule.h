#ifndef BOOTMODULE_H
#define BOOTMODULE_H

#include <stdbool.h>
#include <stdint.h>

struct bootmodule {
  char* name;
  void* address;
  uint64_t size;
};
typedef struct bootmodule bootmodule_t;

/* Captures Limine module response pointers before VMM initialization. */
bool bootmodule_capture_early();

/*
 * Maps captured Limine response pages and deep-copies module names and bytes
 * into kernel-owned structures.
 * Must be called after VMM and kmalloc initialization.
 */
bool bootmodule_finalize_cache();

/* Returns a boot module with the specified `name`.
 * Returns the bootmodule, or NULL if not found.
 * The caller is reponsible for calling `bootmodule_free`
 * when finished with the handle.
 */
bootmodule_t* bootmodule_get(const char* name);

/*
 * Frees a previously obtained bootmodule structure.
 * Trying to access the bootmodule handle after a call to this function
 * is undefined behavior.
 * The underlying `name` and `address` fields of the handle are not affected.
 */
void bootmodule_free(bootmodule_t* bootmodule);

#endif  // BOOTMODULE_H
