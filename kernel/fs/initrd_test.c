#include <fs/vfs.h>
#include <fs/initrd_test.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <utils/test.h>

static void assert_dir_contains(htest_ctx_t* ctx,
                                vfs_file_t* dir,
                                const char* name);
static void assert_dir_missing(htest_ctx_t* ctx,
                               vfs_file_t* dir,
                               const char* name);
static void assert_read_eq(htest_ctx_t* ctx,
                           vfs_file_t* file,
                           const char* expected);
static void free_dirent(vfs_dirent_t* dirent);
static bool dir_contains(htest_ctx_t* ctx, vfs_file_t* dir, const char* name);

void initrd_test(void) {
  htest_ctx_t ctx = {0};
  htest_suite_begin(&ctx, "initrd");

  htest_case_begin(&ctx, "create and read");
  vfs_file_t* etc = NULL;
  HTEST_ASSERT(
      &ctx, vfs_open("/etc/", VFS_OPEN_DIRECTORY, &etc, NULL) == VFS_STATUS_OK);

  HTEST_ASSERT(&ctx, vfs_mkdir(etc->vnode, "test_mkdir/", 11, NULL) ==
                         VFS_STATUS_OK);

  vfs_file_t* testcreate = NULL;
  HTEST_ASSERT(&ctx,
               vfs_open("/etc/test_open_create.txt",
                        VFS_OPEN_READ | VFS_OPEN_WRITE | VFS_OPEN_CREATE,
                        &testcreate,
                        NULL) == VFS_STATUS_OK);
  char write_test[] = "open create write test";
  HTEST_ASSERT(&ctx,
               vfs_write(testcreate, write_test, 22, NULL) == VFS_STATUS_OK);
  HTEST_ASSERT(&ctx, vfs_seek(testcreate, 0, VFS_SEEK_SET, NULL) == VFS_STATUS_OK);
  assert_read_eq(&ctx, testcreate, write_test);

  vfs_file_t* mkdir_test = NULL;
  HTEST_ASSERT(&ctx,
               vfs_open("/etc/test_mkdir/mkdir_test.txt",
                        VFS_OPEN_READ | VFS_OPEN_WRITE | VFS_OPEN_CREATE,
                        &mkdir_test,
                        NULL) == VFS_STATUS_OK);
  char mkdir_write_test[] = "mkdir write test";
  HTEST_ASSERT(&ctx,
               vfs_write(mkdir_test, mkdir_write_test, 16, NULL) ==
                   VFS_STATUS_OK);
  HTEST_ASSERT(&ctx, vfs_seek(mkdir_test, 0, VFS_SEEK_SET, NULL) == VFS_STATUS_OK);
  assert_read_eq(&ctx, mkdir_test, mkdir_write_test);

  assert_dir_contains(&ctx, etc, "test_mkdir/");
  assert_dir_contains(&ctx, etc, "test_open_create.txt");

  htest_case_begin(&ctx, "unlink semantics");
  vfs_file_t* unlink_test = NULL;
  HTEST_ASSERT(&ctx,
               vfs_open("/etc/test_mkdir/unlink_test.txt",
                        VFS_OPEN_READ | VFS_OPEN_WRITE | VFS_OPEN_CREATE,
                        &unlink_test,
                        NULL) == VFS_STATUS_OK);
  char unlink_write_test[] = "unlink write test";
  HTEST_ASSERT(&ctx,
               vfs_write(unlink_test, unlink_write_test, 17, NULL) ==
                   VFS_STATUS_OK);

  vfs_file_t* testmkdir = NULL;
  HTEST_ASSERT(&ctx,
               vfs_open("/etc/test_mkdir/", VFS_OPEN_DIRECTORY, &testmkdir,
                        NULL) ==
                   VFS_STATUS_OK);
  HTEST_ASSERT(&ctx, vfs_unlink(testmkdir->vnode, "unlink_test.txt", 15, 0) ==
                   VFS_STATUS_OK);
  HTEST_ASSERT(&ctx, vfs_seek(unlink_test, 0, VFS_SEEK_SET, NULL) == VFS_STATUS_OK);
  assert_read_eq(&ctx, unlink_test, unlink_write_test);

  vfs_file_t* unlink_reopen = NULL;
  HTEST_ASSERT(&ctx,
               vfs_open("/etc/test_mkdir/unlink_test.txt", VFS_OPEN_READ,
                        &unlink_reopen,
                        NULL) == VFS_STATUS_NOENT);
  HTEST_ASSERT(&ctx, unlink_reopen == NULL);

  HTEST_ASSERT(&ctx,
               vfs_unlink(etc->vnode, "test_mkdir", 10, 0) == VFS_STATUS_ISDIR);
  HTEST_ASSERT(&ctx,
               vfs_unlink(testmkdir->vnode, "missing.txt", 11, 0) ==
                   VFS_STATUS_NOENT);
  assert_dir_contains(&ctx, testmkdir, "mkdir_test.txt");
  assert_dir_missing(&ctx, testmkdir, "unlink_test.txt");

  htest_case_begin(&ctx, "seek and write");
  vfs_file_t* test = NULL;
  HTEST_ASSERT(&ctx,
               vfs_open("/etc/test.txt",
                        VFS_OPEN_READ | VFS_OPEN_WRITE,
                        &test,
                        NULL) ==
                   VFS_STATUS_OK);
  char test_buf[64] = {0};
  uint64_t bytes_read = 0;
  HTEST_ASSERT(&ctx, vfs_read(test, test_buf, 50, &bytes_read) == VFS_STATUS_OK);
  HTEST_ASSERT(&ctx, bytes_read > 0);
  HTEST_ASSERT(&ctx,
               vfs_seek(test, -500, VFS_SEEK_CUR, NULL) ==
                   VFS_STATUS_OK);
  HTEST_ASSERT(&ctx, vfs_read(test, test_buf, 50, &bytes_read) == VFS_STATUS_OK);
  HTEST_ASSERT(&ctx, bytes_read > 0);
  HTEST_ASSERT(&ctx,
               vfs_seek(test, -15, VFS_SEEK_END, NULL) ==
                   VFS_STATUS_OK);
  HTEST_ASSERT(&ctx, vfs_read(test, test_buf, 15, &bytes_read) == VFS_STATUS_OK);
  HTEST_ASSERT(&ctx, bytes_read > 0);

  char append_test[] = " append test";
  HTEST_ASSERT(&ctx,
               vfs_write(test, append_test, 12, NULL) == VFS_STATUS_OK);
  HTEST_ASSERT(&ctx,
               vfs_seek(test, -12, VFS_SEEK_END, NULL) ==
                   VFS_STATUS_OK);
  assert_read_eq(&ctx, test, append_test);

  htest_case_begin(&ctx, "read eof semantics");
  HTEST_ASSERT(&ctx, vfs_seek(test, 0, VFS_SEEK_END, NULL) == VFS_STATUS_OK);
  memset(test_buf, 0xAB, sizeof(test_buf));
  bytes_read = 1234;
  HTEST_ASSERT(&ctx, vfs_read(test, test_buf, sizeof(test_buf), &bytes_read) ==
                         VFS_STATUS_OK);
  HTEST_ASSERT(&ctx, bytes_read == 0);

  htest_case_begin(&ctx, "cleanup");
  HTEST_ASSERT(&ctx, vfs_close(unlink_test) == VFS_STATUS_OK);
  unlink_test = NULL;
  HTEST_ASSERT(&ctx, vfs_close(mkdir_test) == VFS_STATUS_OK);
  mkdir_test = NULL;
  HTEST_ASSERT(&ctx,
               vfs_unlink(testmkdir->vnode, "mkdir_test.txt", 14, 0) ==
                   VFS_STATUS_OK);
  assert_dir_missing(&ctx, testmkdir, "mkdir_test.txt");
  HTEST_ASSERT(&ctx, vfs_close(testmkdir) == VFS_STATUS_OK);
  testmkdir = NULL;

  HTEST_ASSERT(&ctx, vfs_rmdir(etc->vnode, "test_mkdir", 10, 0) == VFS_STATUS_OK);

  vfs_file_t* removed_dir = NULL;
  HTEST_ASSERT(&ctx,
               vfs_open("/etc/test_mkdir/",
                        VFS_OPEN_DIRECTORY,
                        &removed_dir,
                        NULL) == VFS_STATUS_NOENT);
  HTEST_ASSERT(&ctx, removed_dir == NULL);
  assert_dir_missing(&ctx, etc, "test_mkdir/");

  HTEST_ASSERT(&ctx, vfs_close(test) == VFS_STATUS_OK);
  HTEST_ASSERT(&ctx, vfs_close(testcreate) == VFS_STATUS_OK);
  HTEST_ASSERT(&ctx, vfs_close(etc) == VFS_STATUS_OK);

  htest_suite_pass(&ctx);
}

static void assert_dir_contains(htest_ctx_t* ctx,
                                vfs_file_t* dir,
                                const char* name) {
  HTEST_ASSERT(ctx, dir_contains(ctx, dir, name));
}

static void assert_dir_missing(htest_ctx_t* ctx,
                               vfs_file_t* dir,
                               const char* name) {
  HTEST_ASSERT(ctx, !dir_contains(ctx, dir, name));
}

static void assert_read_eq(htest_ctx_t* ctx,
                           vfs_file_t* file,
                           const char* expected) {
  uint64_t expected_len = strlen(expected);
  char buffer[128] = {0};
  uint64_t bytes_read = 0;

  HTEST_ASSERT(ctx, expected_len < sizeof(buffer));
  HTEST_ASSERT(ctx,
               vfs_read(file, buffer, expected_len, &bytes_read) ==
                   VFS_STATUS_OK);
  HTEST_ASSERT(ctx, bytes_read == expected_len);
  HTEST_ASSERT(ctx, memcmp(buffer, expected, expected_len) == 0);
}

static void free_dirent(vfs_dirent_t* dirent) {
  if (dirent == NULL) { return; }
  free((void*)dirent->name);
  free(dirent);
}

static bool dir_contains(htest_ctx_t* ctx, vfs_file_t* dir, const char* name) {
  uint64_t pos = 0;
  HTEST_ASSERT(ctx, vfs_seek(dir, 0, VFS_SEEK_SET, &pos) == VFS_STATUS_OK);

  while (true) {
    vfs_dirent_t* dirent = NULL;
    vfs_status_t status = vfs_readdir(dir, &dirent);
    HTEST_ASSERT(ctx, status == VFS_STATUS_OK);
    if (dirent == NULL) { return false; }
    bool match = strcmp(dirent->name, name) == 0;
    free_dirent(dirent);
    if (match) { return true; }
  }
}
