#ifndef VMM_H
#define VMM_H

#include <haddr.h>

#define PAGE_PRESENT        0x1
#define PAGE_WRITABLE       0x2
#define PAGE_USER_ACCESIBLE 0x4

struct vmm;
typedef struct vmm vmm_t;
haddr_t vmm_get_kernel_offset(vmm_t* vmm);
haddr_t vmm_get_first_available_vaddr(vmm_t* vmm);
haddr_t vmm_get_last_available_vaddr(vmm_t* vmm);
void vmm_dump(vmm_t* v);

/*
 * Bootstraps the kernel's VMM. Should only be called once, at kernel init.
 */
void initialize_vmm();

/*
 * Creates a new VMM for use in a process. `flags` can optionally be passed,
 * which will be used in the mapping of the page directories. The page
 * directories will always be mapped as PAGE_PRESENT and PAGE_WRITABLE,
 * regardless of the value of this argument.
 * Can only be called after initializing kmalloc.
 * The caller is responsible for freeing the handle with `vmm_free()`.
 */
vmm_t* vmm_new(haddr_t flags);

void vmm_free(vmm_t* vmm);

haddr_t vmm_map_at_paddr(vmm_t* vmm, haddr_t virt, haddr_t phys, haddr_t flags);
haddr_t vmm_map_single(vmm_t* vmm, haddr_t virt, haddr_t flags);
haddr_t vmm_map(vmm_t* vmm, haddr_t virt, haddr_t size, haddr_t flags);
haddr_t vmm_unmap(vmm_t* vmm, haddr_t virt);

#endif  // VMM_H
