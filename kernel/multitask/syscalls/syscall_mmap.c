#include <errno.h>
#include <fs/vfs.h>
#include <haddr.h>
#include <kernel/g_kernel.h>
#include <memory/pmm.h>
#include <memory/vmm.h>
#include <multitask/scheduler.h>
#include <multitask/syscall_callbacks.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>

#define MMAP_SUPPORTED_FLAGS                                                   \
  (MAP_PRIVATE | MAP_SHARED | MAP_FIXED | MAP_ANONYMOUS)
#define MMAP_SUPPORTED_PROT (PROT_READ | PROT_WRITE | PROT_EXEC)

static bool mmap_length_to_pages(unsigned long length,
                                 haddr_t* rounded_len_out,
                                 haddr_t* page_count_out);
static haddr_t mmap_prot_to_vmm_flags(int prot);
static long mmap_find_addr(vmm_t* vmm,
                           void* addr,
                           haddr_t rounded_len,
                           int flags,
                           haddr_t* addr_out);
static long mmap_file_into_region(int fd,
                                  vfs_file_t* file,
                                  long offset,
                                  haddr_t mapped_addr,
                                  unsigned long length);
static long mmap_unmap_pages(vmm_t* vmm, haddr_t addr, haddr_t page_count);

long syscall_mmap(void* addr,
                  unsigned long length,
                  int prot,
                  int flags,
                  int fd,
                  long offset) {
  process_mem_t* mem = sched_pb_get_mem(g_kernel.current_process);
  if (mem == NULL || mem->vmm == NULL) { return -EINVAL; }

  if ((flags & ~MMAP_SUPPORTED_FLAGS) != 0) { return -EINVAL; }
  if ((prot & ~MMAP_SUPPORTED_PROT) != 0) { return -EINVAL; }
  if ((flags & MAP_PRIVATE) == 0 && (flags & MAP_SHARED) == 0) {
    return -EINVAL;
  }
  if ((flags & MAP_PRIVATE) != 0 && (flags & MAP_SHARED) != 0) {
    return -EINVAL;
  }
  if (offset < 0 || ((haddr_t)offset & (PAGE_SIZE - 1)) != 0) {
    return -EINVAL;
  }

  haddr_t rounded_len = 0;
  haddr_t page_count = 0;
  if (!mmap_length_to_pages(length, &rounded_len, &page_count)) {
    return -EINVAL;
  }

  vfs_file_t* file = NULL;
  bool file_mmap = false;
  if ((flags & MAP_ANONYMOUS) == 0) {
    if (fd < 0) { return -EBADF; }

    vfs_status_t status = vfs_resolve_fd(fd, &file);
    if (status != VFS_STATUS_OK) { return -vfs_status_to_errno(status); }
    file_mmap = file->ops != NULL && file->ops->mmap != NULL;
  }

  haddr_t search_len = rounded_len;
  if (file_mmap) {
    if (rounded_len > (haddr_t)-1 - PAGE_SIZE) { return -EINVAL; }
    search_len += PAGE_SIZE;
  }

  haddr_t mapped_addr = 0;
  long addr_status =
      mmap_find_addr(mem->vmm, addr, search_len, flags, &mapped_addr);
  if (addr_status < 0) { return addr_status; }

  if (file_mmap) {
    vfs_status_t status = file->ops->mmap(
        file, mem->vmm, &mapped_addr, rounded_len, prot, flags, offset);
    return status == VFS_STATUS_OK ? (long)mapped_addr
                                   : -vfs_status_to_errno(status);
  }

  haddr_t vmm_flags = mmap_prot_to_vmm_flags(prot);
  if (vmm_map(mem->vmm, mapped_addr, page_count, vmm_flags) == 0) {
    return -ENOMEM;
  }

  memset((void*)mapped_addr, 0, rounded_len);

  if ((flags & MAP_ANONYMOUS) == 0) {
    long file_status = mmap_file_into_region(
        fd, file, offset, mapped_addr, length);
    if (file_status < 0) {
      mmap_unmap_pages(mem->vmm, mapped_addr, page_count);
      return file_status;
    }
  }

  return (long)mapped_addr;
}

long syscall_munmap(void* addr, unsigned long length) {
  process_mem_t* mem = sched_pb_get_mem(g_kernel.current_process);
  if (mem == NULL || mem->vmm == NULL) { return -EINVAL; }

  haddr_t start = (haddr_t)addr;
  if ((start & (PAGE_SIZE - 1)) != 0) { return -EINVAL; }

  haddr_t rounded_len = 0;
  haddr_t page_count = 0;
  if (!mmap_length_to_pages(length, &rounded_len, &page_count)) {
    return -EINVAL;
  }
  (void)rounded_len;

  long status = mmap_unmap_pages(mem->vmm, start, page_count);
  return status < 0 ? status : 0;
}

static bool mmap_length_to_pages(unsigned long length,
                                 haddr_t* rounded_len_out,
                                 haddr_t* page_count_out) {
  if (length == 0 || rounded_len_out == NULL || page_count_out == NULL) {
    return false;
  }
  if (length > (haddr_t)-1 - (PAGE_SIZE - 1)) { return false; }

  haddr_t rounded_len = (length + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
  haddr_t page_count = rounded_len / PAGE_SIZE;
  if (rounded_len == 0 || page_count == 0) { return false; }

  *rounded_len_out = rounded_len;
  *page_count_out = page_count;
  return true;
}

static haddr_t mmap_prot_to_vmm_flags(int prot) {
  haddr_t flags = PAGE_PRESENT | PAGE_USER_ACCESIBLE | PAGE_WRITABLE;
  if ((prot & PROT_EXEC) != 0) { flags |= PAGE_EXECUTABLE; }
  return flags;
}

static long mmap_find_addr(vmm_t* vmm,
                           void* addr,
                           haddr_t rounded_len,
                           int flags,
                           haddr_t* addr_out) {
  haddr_t hint = (haddr_t)addr;
  bool ok = false;

  if ((flags & MAP_FIXED) != 0) {
    ok = vmm_find_free_region_fixed(vmm, hint, rounded_len, addr_out);
    return ok ? 0 : -EINVAL;
  } else {
    ok = vmm_find_free_region_forward(vmm, hint, rounded_len, addr_out);
  }

  return ok ? 0 : -ENOMEM;
}

static long mmap_file_into_region(int fd,
                                  vfs_file_t* file,
                                  long offset,
                                  haddr_t mapped_addr,
                                  unsigned long length) {
  if (fd < 0) { return -EBADF; }
  if (file == NULL) { return -EBADF; }

  uint64_t old_offset = 0;
  vfs_status_t status = vfs_seek(file, 0, VFS_SEEK_CUR, &old_offset);
  if (status != VFS_STATUS_OK) { return -vfs_status_to_errno(status); }

  status = vfs_seek(file, offset, VFS_SEEK_SET, NULL);
  if (status != VFS_STATUS_OK) {
    vfs_seek(file, old_offset, VFS_SEEK_SET, NULL);
    return -vfs_status_to_errno(status);
  }

  uint64_t total_read = 0;
  while (total_read < length) {
    uint64_t bytes_read = 0;
    status = vfs_read(file,
                      (void*)(mapped_addr + total_read),
                      length - total_read,
                      &bytes_read);
    if (status != VFS_STATUS_OK) {
      vfs_seek(file, old_offset, VFS_SEEK_SET, NULL);
      return -vfs_status_to_errno(status);
    }
    if (bytes_read == 0) { break; }
    total_read += bytes_read;
  }

  status = vfs_seek(file, old_offset, VFS_SEEK_SET, NULL);
  if (status != VFS_STATUS_OK) { return -vfs_status_to_errno(status); }
  return 0;
}

static long mmap_unmap_pages(vmm_t* vmm, haddr_t addr, haddr_t page_count) {
  for (haddr_t page = 0; page < page_count; ++page) {
    if (vmm_unmap(vmm, addr + (page * PAGE_SIZE)) == 0) { return -EINVAL; }
  }
  return 0;
}
