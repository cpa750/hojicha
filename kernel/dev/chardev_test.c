#include <dev/chardev_test.h>

#include <fs/vfs.h>
#include <kernel/g_kernel.h>
#include <multitask/scheduler.h>
#include <stdint.h>
#include <string.h>
#include <utils/test.h>

static void assert_all_zero(htest_ctx_t* ctx, const char* buffer, uint64_t len);

void chardev_test(void) {
  htest_ctx_t ctx = {0};
  htest_suite_begin(&ctx, "chardev");

  htest_case_begin(&ctx, "null semantics");
  vfs_file_t* null_file = NULL;
  HTEST_ASSERT(&ctx,
               vfs_open("/dev/null",
                        VFS_OPEN_READ | VFS_OPEN_WRITE,
                        &null_file,
                        NULL) ==
                   VFS_STATUS_OK);

  char null_write_buf[] = "send this to null";
  uint64_t bytes_written = 0;
  HTEST_ASSERT(&ctx,
               vfs_write(null_file,
                         null_write_buf,
                         sizeof(null_write_buf) - 1,
                         &bytes_written) == VFS_STATUS_OK);
  HTEST_ASSERT(&ctx, bytes_written == sizeof(null_write_buf) - 1);

  char null_read_buf[16] = "still here";
  uint64_t bytes_read = 99;
  HTEST_ASSERT(&ctx,
               vfs_read(null_file,
                        null_read_buf,
                        sizeof(null_read_buf),
                        &bytes_read) == VFS_STATUS_OK);
  HTEST_ASSERT(&ctx, bytes_read == 0);
  HTEST_ASSERT(&ctx, vfs_close(null_file) == VFS_STATUS_OK);

  htest_case_begin(&ctx, "zero semantics");
  vfs_file_t* zero_file = NULL;
  HTEST_ASSERT(&ctx,
               vfs_open("/dev/zero",
                        VFS_OPEN_READ | VFS_OPEN_WRITE,
                        &zero_file,
                        NULL) ==
                   VFS_STATUS_OK);

  char zero_write_buf[] = "discard this too";
  bytes_written = 99;
  HTEST_ASSERT(&ctx,
               vfs_write(zero_file,
                         zero_write_buf,
                         sizeof(zero_write_buf) - 1,
                         &bytes_written) == VFS_STATUS_OK);
  HTEST_ASSERT(&ctx, bytes_written == 0);

  char zero_read_buf[] = "occupied";
  bytes_read = 0;
  HTEST_ASSERT(&ctx,
               vfs_read(zero_file,
                        zero_read_buf,
                        sizeof(zero_read_buf),
                        &bytes_read) == VFS_STATUS_OK);
  HTEST_ASSERT(&ctx, bytes_read == sizeof(zero_read_buf));
  assert_all_zero(&ctx, zero_read_buf, sizeof(zero_read_buf));
  HTEST_ASSERT(&ctx, vfs_close(zero_file) == VFS_STATUS_OK);

  htest_case_begin(&ctx, "devfs parent-relative lookup");
  vfs_file_t* dev_dir = NULL;
  HTEST_ASSERT(&ctx,
               vfs_open("/dev/", VFS_OPEN_DIRECTORY, &dev_dir, NULL) ==
                   VFS_STATUS_OK);

  vfs_node_t* parent_walk_dir = NULL;
  HTEST_ASSERT(&ctx,
               vfs_mkdir(dev_dir->vnode,
                         "parent_walk_test",
                         sizeof("parent_walk_test") - 1,
                         &parent_walk_dir) == VFS_STATUS_OK);

  vfs_node_t* looked_up = NULL;
  HTEST_ASSERT(&ctx,
               vfs_symlink("null", "/dev/null-link") == VFS_STATUS_OK);
  char link_target[16] = {0};
  uint64_t link_target_len = 0;
  HTEST_ASSERT(&ctx,
               vfs_readlink("/dev/null-link",
                            link_target,
                            sizeof(link_target),
                            &link_target_len) == VFS_STATUS_OK);
  HTEST_ASSERT(&ctx, link_target_len == sizeof("null") - 1);
  HTEST_ASSERT(&ctx, memcmp(link_target, "null", link_target_len) == 0);

  looked_up = NULL;
  HTEST_ASSERT(&ctx, vfs_lookup("/dev/null-link", &looked_up) == VFS_STATUS_OK);
  vfs_node_t* direct_null = NULL;
  HTEST_ASSERT(&ctx, vfs_lookup("/dev/null", &direct_null) == VFS_STATUS_OK);
  HTEST_ASSERT(&ctx, looked_up == direct_null);
  vfs_vnode_release(looked_up);
  vfs_vnode_release(direct_null);

  HTEST_ASSERT(&ctx,
               vfs_symlink("parent_walk_test", "/dev/parent-walk-link") ==
                   VFS_STATUS_OK);
  looked_up = NULL;
  HTEST_ASSERT(&ctx,
               vfs_lookup("/dev/parent-walk-link/../zero", &looked_up) ==
                   VFS_STATUS_OK);
  vfs_vnode_release(looked_up);

  HTEST_ASSERT(&ctx,
               vfs_link("/dev/null", "/dev/null-hard") ==
                   VFS_STATUS_INVALID_ARG);

  looked_up = NULL;
  HTEST_ASSERT(&ctx,
               vfs_lookup_at(parent_walk_dir, "../null", &looked_up) ==
                   VFS_STATUS_OK);
  vfs_vnode_release(looked_up);

  vfs_node_t* parent = NULL;
  const char* child_name = NULL;
  uint32_t child_name_len = 0;
  HTEST_ASSERT(&ctx,
               vfs_lookup_parent_at(parent_walk_dir,
                                    "../zero",
                                    &parent,
                                    &child_name,
                                    &child_name_len) == VFS_STATUS_OK);
  HTEST_ASSERT(&ctx, parent == dev_dir->vnode);
  HTEST_ASSERT(&ctx, child_name_len == sizeof("zero") - 1);
  HTEST_ASSERT(&ctx, memcmp(child_name, "zero", child_name_len) == 0);
  vfs_vnode_release(parent);

  vfs_node_t* root = NULL;
  HTEST_ASSERT(&ctx, vfs_lookup("/", &root) == VFS_STATUS_OK);
  looked_up = NULL;
  HTEST_ASSERT(&ctx, vfs_lookup("/dev/..", &looked_up) == VFS_STATUS_OK);
  HTEST_ASSERT(&ctx, looked_up == root);
  vfs_vnode_release(looked_up);
  vfs_vnode_release(root);

  vfs_node_t* direct_test = NULL;
  HTEST_ASSERT(&ctx,
               vfs_lookup("/etc/test.txt", &direct_test) == VFS_STATUS_OK);
  looked_up = NULL;
  HTEST_ASSERT(&ctx,
               vfs_lookup("/dev/../etc/test.txt", &looked_up) ==
                   VFS_STATUS_OK);
  HTEST_ASSERT(&ctx, looked_up == direct_test);
  vfs_vnode_release(looked_up);
  vfs_vnode_release(direct_test);

  vfs_node_t* etc = NULL;
  HTEST_ASSERT(&ctx, vfs_lookup("/etc", &etc) == VFS_STATUS_OK);
  parent = NULL;
  child_name = NULL;
  child_name_len = 0;
  HTEST_ASSERT(&ctx,
               vfs_lookup_parent("/dev/../etc/test.txt",
                                 &parent,
                                 &child_name,
                                 &child_name_len) == VFS_STATUS_OK);
  HTEST_ASSERT(&ctx, parent == etc);
  HTEST_ASSERT(&ctx, child_name_len == sizeof("test.txt") - 1);
  HTEST_ASSERT(&ctx, memcmp(child_name, "test.txt", child_name_len) == 0);
  vfs_vnode_release(parent);

  vfs_node_t* old_cwd = sched_pb_get_cwd(g_kernel.current_process);
  HTEST_ASSERT(&ctx, old_cwd != NULL);
  vfs_vnode_borrow(old_cwd);

  vfs_node_t* dev_cwd = NULL;
  HTEST_ASSERT(&ctx, vfs_lookup("/dev", &dev_cwd) == VFS_STATUS_OK);
  HTEST_ASSERT(&ctx, dev_cwd->mount != NULL);
  HTEST_ASSERT(&ctx, dev_cwd == dev_cwd->mount->root);
  sched_pb_set_cwd(g_kernel.current_process, dev_cwd);
  vfs_vnode_release(dev_cwd);

  HTEST_ASSERT(&ctx,
               vfs_lookup("../etc/test.txt", &direct_test) == VFS_STATUS_OK);
  HTEST_ASSERT(&ctx, direct_test != NULL);
  vfs_vnode_release(direct_test);

  parent = NULL;
  child_name = NULL;
  child_name_len = 0;
  HTEST_ASSERT(&ctx,
               vfs_lookup_parent("../etc/test.txt",
                                 &parent,
                                 &child_name,
                                 &child_name_len) == VFS_STATUS_OK);
  HTEST_ASSERT(&ctx, parent == etc);
  HTEST_ASSERT(&ctx, parent->mount == NULL);
  HTEST_ASSERT(&ctx, child_name_len == sizeof("test.txt") - 1);
  HTEST_ASSERT(&ctx, memcmp(child_name, "test.txt", child_name_len) == 0);
  vfs_vnode_release(parent);
  vfs_vnode_release(etc);

  sched_pb_set_cwd(g_kernel.current_process, old_cwd);
  vfs_vnode_release(old_cwd);

  HTEST_ASSERT(&ctx,
               vfs_unlink(dev_dir->vnode,
                          "null-link",
                          sizeof("null-link") - 1,
                          0) == VFS_STATUS_OK);
  HTEST_ASSERT(&ctx,
               vfs_unlink(dev_dir->vnode,
                          "parent-walk-link",
                          sizeof("parent-walk-link") - 1,
                          0) == VFS_STATUS_OK);
  HTEST_ASSERT(&ctx,
               vfs_rmdir(dev_dir->vnode,
                         "parent_walk_test",
                         sizeof("parent_walk_test") - 1,
                         0) == VFS_STATUS_OK);
  vfs_vnode_release(parent_walk_dir);
  HTEST_ASSERT(&ctx, vfs_close(dev_dir) == VFS_STATUS_OK);

  htest_suite_pass(&ctx);
}

static void assert_all_zero(htest_ctx_t* ctx, const char* buffer, uint64_t len) {
  for (uint64_t i = 0; i < len; ++i) {
    HTEST_ASSERT(ctx, buffer[i] == '\0');
  }
}
