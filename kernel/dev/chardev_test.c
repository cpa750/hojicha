#include <dev/chardev_test.h>

#include <fs/vfs.h>
#include <stdint.h>
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

  htest_suite_pass(&ctx);
}

static void assert_all_zero(htest_ctx_t* ctx, const char* buffer, uint64_t len) {
  for (uint64_t i = 0; i < len; ++i) {
    HTEST_ASSERT(ctx, buffer[i] == '\0');
  }
}
