#include <fs/vfs.h>
#include <fs/vfs_test.h>
#include <kernel/g_kernel.h>
#include <multitask/scheduler.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <utils/test.h>

typedef struct mock_node mock_node_t;
struct mock_node {
  vfs_node_t vnode;
  const char* name;
  uint64_t size;
  bool present;
};

typedef struct mock_vfs_state mock_vfs_state_t;
struct mock_vfs_state {
  bool mounted;
  vfs_mount_t mount;

  mock_node_t root;
  mock_node_t existing_file;
  mock_node_t created_file;
  mock_node_t removable_file;
  mock_node_t existing_dir;
  mock_node_t created_dir;

  uint64_t lookup_calls;
  uint64_t open_calls;
  uint64_t create_file_calls;
  uint64_t create_dir_calls;
  uint64_t unlink_calls;
  uint64_t rmdir_calls;
  uint64_t stat_calls;
  uint64_t free_calls;
  uint64_t read_calls;
  uint64_t write_calls;
  uint64_t seek_calls;
  uint64_t readdir_calls;
  uint64_t close_calls;

  vfs_node_t* last_lookup_dir;
  vfs_node_t* last_open_vnode;
  vfs_node_t* last_stat_vnode;
  vfs_node_t* last_freed_vnode;
  void* last_read_buffer;
  void* last_write_buffer;
  uint64_t last_read_len;
  uint64_t last_write_len;
  uint64_t last_seek_offset;
  vfs_seek_whence_t last_seek_whence;
  uint32_t last_open_flags;
  uint32_t last_unlink_flags;
  uint32_t last_rmdir_flags;
  uint32_t last_name_len;
  char last_name[32];
};

static vfs_status_t mock_lookup(vfs_node_t* dir,
                                const char* name,
                                uint32_t name_len,
                                vfs_node_t** out);
static vfs_status_t mock_open(vfs_node_t* vnode,
                              uint32_t flags,
                              vfs_file_t** out);
static vfs_status_t mock_create_file(vfs_node_t* dir,
                                     const char* name,
                                     uint32_t name_len,
                                     vfs_node_t** out);
static vfs_status_t mock_create_dir(vfs_node_t* dir,
                                    const char* name,
                                    uint32_t name_len,
                                    vfs_node_t** out);
static vfs_status_t mock_unlink(vfs_node_t* dir,
                                const char* name,
                                uint32_t name_len,
                                uint32_t flags);
static vfs_status_t mock_rmdir(vfs_node_t* dir,
                               const char* name,
                               uint32_t name_len,
                               uint32_t flags);
static void mock_free(vfs_node_t* vnode);
static vfs_status_t mock_stat(vfs_node_t* vnode, vfs_stat_t** out);

static vfs_status_t mock_read(vfs_file_t* file,
                              void* buffer,
                              uint64_t len,
                              uint64_t* bytes_read_out);
static vfs_status_t mock_write(vfs_file_t* file,
                               void* buffer,
                               uint64_t len,
                               uint64_t* bytes_written_out);
static vfs_status_t mock_readdir(vfs_file_t* dir, vfs_dirent_t** out);
static vfs_status_t mock_seek(vfs_file_t* file,
                              int64_t offset,
                              vfs_seek_whence_t whence,
                              uint64_t* new_pos);
static vfs_status_t mock_close(vfs_file_t* file);

static mock_vfs_state_t* mock_state(void);
static mock_node_t* mock_node_from_vnode(vfs_node_t* vnode);
static void mock_record_name(mock_vfs_state_t* state,
                             const char* name,
                             uint32_t name_len);
static mock_node_t* mock_find_child(mock_vfs_state_t* state,
                                    const char* name,
                                    uint32_t name_len);
static void mock_node_init(mock_node_t* node,
                           const vfs_node_ops_t* ops,
                           vfs_node_type_t type,
                           const char* name,
                           uint64_t size,
                           bool present);
static void mock_reset(mock_vfs_state_t* state);
static void ensure_mock_mount(htest_ctx_t* ctx);
static void cleanup_mock_mount(htest_ctx_t* ctx);
static bool find_fd_for_file(vfs_file_t* file, uint64_t* fd_out);
static void free_dirent(vfs_dirent_t* dirent);

static const vfs_node_ops_t mock_dir_ops = {
    .lookup = mock_lookup,
    .open = mock_open,
    .create_file = mock_create_file,
    .create_dir = mock_create_dir,
    .unlink = mock_unlink,
    .rmdir = mock_rmdir,
    .free = mock_free,
    .stat = mock_stat,
};

static const vfs_node_ops_t mock_file_ops = {
    .open = mock_open,
    .free = mock_free,
    .stat = mock_stat,
};

static const vfs_file_ops_t mock_dir_file_ops = {
    .readdir = mock_readdir,
    .seek = mock_seek,
    .close = mock_close,
};

static const vfs_file_ops_t mock_file_file_ops = {
    .read = mock_read,
    .write = mock_write,
    .seek = mock_seek,
    .close = mock_close,
};

static mock_vfs_state_t mock = {0};

void vfs_test(void) {
  htest_ctx_t ctx = {0};
  htest_suite_begin(&ctx, "vfs");
  ensure_mock_mount(&ctx);

  htest_case_begin(&ctx, "lookup and fd lifecycle");
  mock_reset(&mock);

  vfs_node_t* looked_up = NULL;
  HTEST_ASSERT(
      &ctx,
      vfs_lookup("/etc/vfs_mock/existing.txt", &looked_up) == VFS_STATUS_OK);
  HTEST_ASSERT(&ctx, looked_up == &mock.existing_file.vnode);
  vfs_vnode_release(looked_up);

  vfs_file_t* file = NULL;
  HTEST_ASSERT(&ctx,
               vfs_open("/etc/vfs_mock/existing.txt",
                        VFS_OPEN_READ | VFS_OPEN_WRITE,
                        &file) == VFS_STATUS_OK);
  HTEST_ASSERT(&ctx, mock.lookup_calls > 0);
  HTEST_ASSERT(&ctx, mock.last_lookup_dir == &mock.root.vnode);
  HTEST_ASSERT(&ctx, mock.open_calls == 1);
  HTEST_ASSERT(&ctx, mock.last_open_vnode == &mock.existing_file.vnode);
  HTEST_ASSERT(&ctx, mock.last_open_flags == (VFS_OPEN_READ | VFS_OPEN_WRITE));

  vfs_stat_t* stat = NULL;
  HTEST_ASSERT(&ctx, vfs_fstat(file, &stat) == VFS_STATUS_OK);
  HTEST_ASSERT(&ctx, mock.stat_calls == 1);
  HTEST_ASSERT(&ctx, stat->type == VFS_NODE_FILE);
  HTEST_ASSERT(&ctx, stat->size == 128);
  free(stat);

  uint64_t fd = 0;
  HTEST_ASSERT(&ctx, find_fd_for_file(file, &fd));
  vfs_file_t* resolved = NULL;
  HTEST_ASSERT(&ctx, vfs_resolve_fd(fd, &resolved) == VFS_STATUS_OK);
  HTEST_ASSERT(&ctx, resolved == file);
  HTEST_ASSERT(&ctx, vfs_close(file) == VFS_STATUS_OK);
  HTEST_ASSERT(&ctx, mock.close_calls == 1);
  HTEST_ASSERT(&ctx, sched_pb_fd_get(g_kernel.current_process, fd) == NULL);

  htest_case_begin(&ctx, "create with open");
  mock_reset(&mock);

  vfs_node_t* parent = NULL;
  const char* name = NULL;
  uint32_t name_len = 0;
  HTEST_ASSERT(&ctx,
               vfs_lookup_parent(
                   "/etc/vfs_mock/created.txt", &parent, &name, &name_len) ==
                   VFS_STATUS_OK);
  HTEST_ASSERT(&ctx, parent == &mock.root.vnode);
  HTEST_ASSERT(&ctx, name_len == strlen("created.txt"));
  HTEST_ASSERT(&ctx, memcmp(name, "created.txt", strlen("created.txt")) == 0);
  vfs_vnode_release(parent);

  file = NULL;
  HTEST_ASSERT(&ctx,
               vfs_open("/etc/vfs_mock/created.txt",
                        VFS_OPEN_READ | VFS_OPEN_WRITE | VFS_OPEN_CREATE,
                        &file) == VFS_STATUS_OK);
  HTEST_ASSERT(&ctx, mock.create_file_calls == 1);
  HTEST_ASSERT(&ctx, mock.open_calls == 1);
  HTEST_ASSERT(&ctx, mock.last_name_len == strlen("created.txt"));
  HTEST_ASSERT(&ctx, strcmp(mock.last_name, "created.txt") == 0);
  HTEST_ASSERT(&ctx, mock.last_open_flags == (VFS_OPEN_READ | VFS_OPEN_WRITE));
  HTEST_ASSERT(&ctx, vfs_close(file) == VFS_STATUS_OK);

  htest_case_begin(&ctx, "read/write/seek");
  mock_reset(&mock);

  file = NULL;
  HTEST_ASSERT(&ctx,
               vfs_open("/etc/vfs_mock/created.txt",
                        VFS_OPEN_READ | VFS_OPEN_WRITE | VFS_OPEN_CREATE,
                        &file) == VFS_STATUS_OK);

  char write_buf[] = "payload";
  uint64_t bytes_written = 0;
  HTEST_ASSERT(
      &ctx,
      vfs_write(file, write_buf, sizeof(write_buf) - 1, &bytes_written) ==
          VFS_STATUS_OK);
  HTEST_ASSERT(&ctx, mock.write_calls == 1);
  HTEST_ASSERT(&ctx, mock.last_write_buffer == write_buf);
  HTEST_ASSERT(&ctx, mock.last_write_len == sizeof(write_buf) - 1);
  HTEST_ASSERT(&ctx, bytes_written == sizeof(write_buf) - 1);

  char read_buf[8] = {0};
  uint64_t bytes_read = 0;
  HTEST_ASSERT(
      &ctx,
      vfs_read(file, read_buf, sizeof(read_buf), &bytes_read) == VFS_STATUS_OK);
  HTEST_ASSERT(&ctx, mock.read_calls == 1);
  HTEST_ASSERT(&ctx, mock.last_read_buffer == read_buf);
  HTEST_ASSERT(&ctx, mock.last_read_len == sizeof(read_buf));
  HTEST_ASSERT(&ctx, bytes_read == sizeof(read_buf));

  uint64_t new_pos = 0;
  HTEST_ASSERT(&ctx,
               vfs_seek(file, 12, VFS_SEEK_CUR, &new_pos) == VFS_STATUS_OK);
  HTEST_ASSERT(&ctx, mock.seek_calls == 1);
  HTEST_ASSERT(&ctx, mock.last_seek_offset == 12);
  HTEST_ASSERT(&ctx, mock.last_seek_whence == VFS_SEEK_CUR);
  HTEST_ASSERT(&ctx, new_pos == 12);
  HTEST_ASSERT(&ctx, vfs_close(file) == VFS_STATUS_OK);

  htest_case_begin(&ctx, "invalid open guards");
  mock_reset(&mock);

  HTEST_ASSERT(&ctx,
               vfs_open("/etc/vfs_mock/bad/",
                        VFS_OPEN_CREATE | VFS_OPEN_DIRECTORY,
                        &file) == VFS_STATUS_INVALID_ARG);
  HTEST_ASSERT(&ctx, mock.lookup_calls == 0);
  HTEST_ASSERT(&ctx, mock.create_file_calls == 0);
  HTEST_ASSERT(&ctx, mock.open_calls == 0);

  htest_case_begin(&ctx, "access mode guards");
  mock_reset(&mock);

  file = NULL;
  HTEST_ASSERT(&ctx,
               vfs_open("/etc/vfs_mock/existing.txt", VFS_OPEN_READ, &file) ==
                   VFS_STATUS_OK);
  HTEST_ASSERT(&ctx,
               vfs_write(file, "x", 1, &bytes_written) == VFS_STATUS_BAD_FD);
  HTEST_ASSERT(&ctx, mock.write_calls == 0);
  HTEST_ASSERT(&ctx, bytes_written == 0);
  HTEST_ASSERT(&ctx, vfs_close(file) == VFS_STATUS_OK);

  file = NULL;
  HTEST_ASSERT(&ctx,
               vfs_open("/etc/vfs_mock/existing.txt", VFS_OPEN_WRITE, &file) ==
                   VFS_STATUS_OK);
  HTEST_ASSERT(&ctx,
               vfs_read(file, read_buf, sizeof(read_buf), &bytes_read) ==
                   VFS_STATUS_BAD_FD);
  HTEST_ASSERT(&ctx, mock.read_calls == 0);
  HTEST_ASSERT(&ctx, bytes_read == 0);
  HTEST_ASSERT(&ctx, vfs_readdir(file, NULL) == VFS_STATUS_NOTDIR);
  HTEST_ASSERT(&ctx, mock.readdir_calls == 0);
  HTEST_ASSERT(&ctx, vfs_close(file) == VFS_STATUS_OK);

  htest_case_begin(&ctx, "stat/readdir");
  mock_reset(&mock);

  stat = NULL;
  HTEST_ASSERT(&ctx,
               vfs_stat("/etc/vfs_mock/existing.txt", &stat) == VFS_STATUS_OK);
  HTEST_ASSERT(&ctx, mock.stat_calls == 1);
  HTEST_ASSERT(&ctx, mock.last_stat_vnode == &mock.existing_file.vnode);
  HTEST_ASSERT(&ctx, stat->type == VFS_NODE_FILE);
  free(stat);

  file = NULL;
  HTEST_ASSERT(
      &ctx,
      vfs_open("/etc/vfs_mock/", VFS_OPEN_DIRECTORY, &file) == VFS_STATUS_OK);
  vfs_dirent_t* dirent = NULL;
  HTEST_ASSERT(&ctx, vfs_readdir(file, &dirent) == VFS_STATUS_OK);
  HTEST_ASSERT(&ctx, mock.readdir_calls == 1);
  HTEST_ASSERT(&ctx, strcmp(dirent->name, "existing.txt") == 0);
  free_dirent(dirent);
  dirent = (vfs_dirent_t*)1;
  HTEST_ASSERT(&ctx, vfs_readdir(file, &dirent) == VFS_STATUS_OK);
  HTEST_ASSERT(&ctx, dirent == NULL);
  HTEST_ASSERT(&ctx, mock.readdir_calls == 2);
  HTEST_ASSERT(&ctx, vfs_close(file) == VFS_STATUS_OK);

  htest_case_begin(&ctx, "mkdir/rmdir");
  mock_reset(&mock);

  vfs_node_t* made_dir = NULL;
  HTEST_ASSERT(
      &ctx,
      vfs_mkdir(&mock.root.vnode, "made_dir/", 9, &made_dir) == VFS_STATUS_OK);
  HTEST_ASSERT(&ctx, mock.create_dir_calls == 1);
  HTEST_ASSERT(&ctx, strcmp(mock.last_name, "made_dir") == 0);
  HTEST_ASSERT(&ctx, mock.last_name_len == strlen("made_dir"));
  HTEST_ASSERT(&ctx, made_dir == &mock.created_dir.vnode);
  vfs_vnode_release(made_dir);

  HTEST_ASSERT(&ctx,
               vfs_rmdir(&mock.root.vnode, "made_dir", 8, 9) == VFS_STATUS_OK);
  HTEST_ASSERT(&ctx, mock.rmdir_calls == 1);
  HTEST_ASSERT(&ctx, mock.last_rmdir_flags == 9);
  HTEST_ASSERT(&ctx, mock.free_calls == 1);
  HTEST_ASSERT(&ctx, mock.last_freed_vnode == &mock.created_dir.vnode);

  htest_case_begin(&ctx, "remove type guards");
  mock_reset(&mock);

  HTEST_ASSERT(
      &ctx,
      vfs_unlink(&mock.root.vnode, "existing_dir", 12, 0) == VFS_STATUS_ISDIR);
  HTEST_ASSERT(&ctx, mock.unlink_calls == 0);
  HTEST_ASSERT(
      &ctx,
      vfs_rmdir(&mock.root.vnode, "existing.txt", 12, 7) == VFS_STATUS_NOTDIR);
  HTEST_ASSERT(&ctx, mock.rmdir_calls == 0);

  htest_case_begin(&ctx, "unlink delegation");
  mock_reset(&mock);

  HTEST_ASSERT(
      &ctx,
      vfs_unlink(&mock.root.vnode, "removable.txt", 13, 5) == VFS_STATUS_OK);
  HTEST_ASSERT(&ctx, mock.unlink_calls == 1);
  HTEST_ASSERT(&ctx, mock.last_unlink_flags == 5);
  HTEST_ASSERT(&ctx, mock.free_calls == 1);
  HTEST_ASSERT(&ctx, mock.last_freed_vnode == &mock.removable_file.vnode);

  htest_case_begin(&ctx, "unmount cleanup");
  cleanup_mock_mount(&ctx);

  htest_suite_pass(&ctx);
}

static mock_vfs_state_t* mock_state(void) { return &mock; }

static mock_node_t* mock_node_from_vnode(vfs_node_t* vnode) {
  if (vnode == &mock.root.vnode) { return &mock.root; }
  return (mock_node_t*)vnode->fs_data;
}

static vfs_status_t mock_lookup(vfs_node_t* dir,
                                const char* name,
                                uint32_t name_len,
                                vfs_node_t** out) {
  mock_vfs_state_t* state = mock_state();
  state->lookup_calls++;
  state->last_lookup_dir = dir;
  mock_record_name(state, name, name_len);

  mock_node_t* child = mock_find_child(state, name, name_len);
  if (child == NULL) {
    if (out != NULL) { *out = NULL; }
    return VFS_STATUS_NOENT;
  }

  vfs_vnode_borrow(&child->vnode);
  if (out != NULL) { *out = &child->vnode; }
  return VFS_STATUS_OK;
}

static vfs_status_t mock_open(vfs_node_t* vnode,
                              uint32_t flags,
                              vfs_file_t** out) {
  if (out == NULL) { return VFS_STATUS_INVALID_ARG; }

  mock_vfs_state_t* state = mock_state();
  vfs_file_t* file = (vfs_file_t*)malloc(sizeof(vfs_file_t));
  if (file == NULL) { return VFS_STATUS_NOMEM; }

  file->vnode = vnode;
  file->flags = flags;
  file->offset = 0;
  file->fs_data = state;
  file->ops =
      vnode->type == VFS_NODE_DIR ? &mock_dir_file_ops : &mock_file_file_ops;

  state->open_calls++;
  state->last_open_vnode = vnode;
  state->last_open_flags = flags;
  *out = file;
  return VFS_STATUS_OK;
}

static vfs_status_t mock_create_file(vfs_node_t* dir,
                                     const char* name,
                                     uint32_t name_len,
                                     vfs_node_t** out) {
  mock_vfs_state_t* state = mock_state();
  state->create_file_calls++;
  mock_record_name(state, name, name_len);

  if (name_len != strlen("created.txt") ||
      memcmp(name, "created.txt", name_len) != 0) {
    if (out != NULL) { *out = NULL; }
    return VFS_STATUS_NOENT;
  }

  state->created_file.present = true;
  state->created_file.vnode.link_count = 1;
  vfs_vnode_borrow(&state->created_file.vnode);
  if (out != NULL) { *out = &state->created_file.vnode; }
  return VFS_STATUS_OK;
}

static vfs_status_t mock_create_dir(vfs_node_t* dir,
                                    const char* name,
                                    uint32_t name_len,
                                    vfs_node_t** out) {
  mock_vfs_state_t* state = mock_state();
  state->create_dir_calls++;
  mock_record_name(state, name, name_len);

  state->created_dir.present = true;
  state->created_dir.vnode.link_count = 1;
  vfs_vnode_borrow(&state->created_dir.vnode);
  if (out != NULL) { *out = &state->created_dir.vnode; }
  return VFS_STATUS_OK;
}

static vfs_status_t mock_unlink(vfs_node_t* dir,
                                const char* name,
                                uint32_t name_len,
                                uint32_t flags) {
  mock_vfs_state_t* state = mock_state();
  state->unlink_calls++;
  state->last_unlink_flags = flags;
  mock_record_name(state, name, name_len);

  mock_node_t* child = mock_find_child(state, name, name_len);
  if (child == NULL || child->vnode.type == VFS_NODE_DIR) {
    return VFS_STATUS_NOENT;
  }

  child->present = false;
  return VFS_STATUS_OK;
}

static vfs_status_t mock_rmdir(vfs_node_t* dir,
                               const char* name,
                               uint32_t name_len,
                               uint32_t flags) {
  mock_vfs_state_t* state = mock_state();
  state->rmdir_calls++;
  state->last_rmdir_flags = flags;
  mock_record_name(state, name, name_len);

  mock_node_t* child = mock_find_child(state, name, name_len);
  if (child == NULL || child->vnode.type != VFS_NODE_DIR) {
    return VFS_STATUS_NOENT;
  }

  child->present = false;
  return VFS_STATUS_OK;
}

static void mock_free(vfs_node_t* vnode) {
  mock_vfs_state_t* state = mock_state();
  state->free_calls++;
  state->last_freed_vnode = vnode;
}

static vfs_status_t mock_stat(vfs_node_t* vnode, vfs_stat_t** out) {
  if (out == NULL) { return VFS_STATUS_INVALID_ARG; }

  mock_vfs_state_t* state = mock_state();
  mock_node_t* node = mock_node_from_vnode(vnode);
  vfs_stat_t* stat = (vfs_stat_t*)malloc(sizeof(vfs_stat_t));
  if (stat == NULL) { return VFS_STATUS_NOMEM; }

  stat->type = vnode->type;
  stat->size = node->size;
  state->stat_calls++;
  state->last_stat_vnode = vnode;
  *out = stat;
  return VFS_STATUS_OK;
}

static vfs_status_t mock_read(vfs_file_t* file,
                              void* buffer,
                              uint64_t len,
                              uint64_t* bytes_read_out) {
  mock_vfs_state_t* state = (mock_vfs_state_t*)file->fs_data;
  state->read_calls++;
  state->last_read_buffer = buffer;
  state->last_read_len = len;
  memset(buffer, 'R', len);
  if (bytes_read_out != NULL) { *bytes_read_out = len; }
  return VFS_STATUS_OK;
}

static vfs_status_t mock_write(vfs_file_t* file,
                               void* buffer,
                               uint64_t len,
                               uint64_t* bytes_written_out) {
  mock_vfs_state_t* state = (mock_vfs_state_t*)file->fs_data;
  state->write_calls++;
  state->last_write_buffer = buffer;
  state->last_write_len = len;
  if (bytes_written_out != NULL) { *bytes_written_out = len; }
  return VFS_STATUS_OK;
}

static vfs_status_t mock_readdir(vfs_file_t* dir, vfs_dirent_t** out) {
  if (out == NULL) { return VFS_STATUS_INVALID_ARG; }

  mock_vfs_state_t* state = (mock_vfs_state_t*)dir->fs_data;
  state->readdir_calls++;
  if (dir->offset != 0) {
    *out = NULL;
    return VFS_STATUS_OK;
  }

  vfs_dirent_t* dirent = (vfs_dirent_t*)malloc(sizeof(vfs_dirent_t));
  if (dirent == NULL) { return VFS_STATUS_NOMEM; }
  dirent->name = vfs_clone_name("existing.txt", strlen("existing.txt"), false);
  dirent->inode_no = 1;
  dir->offset = 1;
  *out = dirent;
  return VFS_STATUS_OK;
}

static vfs_status_t mock_seek(vfs_file_t* file,
                              int64_t offset,
                              vfs_seek_whence_t whence,
                              uint64_t* new_pos) {
  mock_vfs_state_t* state = (mock_vfs_state_t*)file->fs_data;
  state->seek_calls++;
  state->last_seek_offset = offset;
  state->last_seek_whence = whence;
  file->offset = offset;
  if (new_pos != NULL) { *new_pos = file->offset; }
  return VFS_STATUS_OK;
}

static vfs_status_t mock_close(vfs_file_t* file) {
  mock_vfs_state_t* state = (mock_vfs_state_t*)file->fs_data;
  state->close_calls++;
  free(file);
  return VFS_STATUS_OK;
}

static void mock_record_name(mock_vfs_state_t* state,
                             const char* name,
                             uint32_t name_len) {
  uint32_t copy_len = name_len;
  if (copy_len >= sizeof(state->last_name)) {
    copy_len = sizeof(state->last_name) - 1;
  }

  memcpy(state->last_name, name, copy_len);
  state->last_name[copy_len] = '\0';
  state->last_name_len = name_len;
}

static mock_node_t* mock_find_child(mock_vfs_state_t* state,
                                    const char* name,
                                    uint32_t name_len) {
  mock_node_t* children[] = {
      &state->existing_file,
      &state->created_file,
      &state->removable_file,
      &state->existing_dir,
      &state->created_dir,
  };

  for (uint64_t i = 0; i < sizeof(children) / sizeof(children[0]); ++i) {
    mock_node_t* child = children[i];
    if (!child->present) { continue; }
    if (strlen(child->name) != name_len) { continue; }
    if (memcmp(child->name, name, name_len) == 0) { return child; }
  }
  return NULL;
}

static void mock_node_init(mock_node_t* node,
                           const vfs_node_ops_t* ops,
                           vfs_node_type_t type,
                           const char* name,
                           uint64_t size,
                           bool present) {
  node->name = name;
  node->size = size;
  node->present = present;
  node->vnode.ops = ops;
  node->vnode.type = type;
  node->vnode.refcount = 0;
  node->vnode.link_count = present ? 1 : 0;
  node->vnode.mount = NULL;
  node->vnode.fs_data = node;
}

static void mock_reset(mock_vfs_state_t* state) {
  state->lookup_calls = 0;
  state->open_calls = 0;
  state->create_file_calls = 0;
  state->create_dir_calls = 0;
  state->unlink_calls = 0;
  state->rmdir_calls = 0;
  state->stat_calls = 0;
  state->free_calls = 0;
  state->read_calls = 0;
  state->write_calls = 0;
  state->seek_calls = 0;
  state->readdir_calls = 0;
  state->close_calls = 0;
  state->last_lookup_dir = NULL;
  state->last_open_vnode = NULL;
  state->last_stat_vnode = NULL;
  state->last_freed_vnode = NULL;
  state->last_read_buffer = NULL;
  state->last_write_buffer = NULL;
  state->last_read_len = 0;
  state->last_write_len = 0;
  state->last_seek_offset = 0;
  state->last_seek_whence = VFS_SEEK_SET;
  state->last_open_flags = 0;
  state->last_unlink_flags = 0;
  state->last_rmdir_flags = 0;
  state->last_name_len = 0;
  state->last_name[0] = '\0';

  mock_node_init(&state->existing_file,
                 &mock_file_ops,
                 VFS_NODE_FILE,
                 "existing.txt",
                 128,
                 true);
  mock_node_init(&state->created_file,
                 &mock_file_ops,
                 VFS_NODE_FILE,
                 "created.txt",
                 64,
                 false);
  mock_node_init(&state->removable_file,
                 &mock_file_ops,
                 VFS_NODE_FILE,
                 "removable.txt",
                 32,
                 true);
  mock_node_init(&state->existing_dir,
                 &mock_dir_ops,
                 VFS_NODE_DIR,
                 "existing_dir",
                 0,
                 true);
  mock_node_init(
      &state->created_dir, &mock_dir_ops, VFS_NODE_DIR, "made_dir", 0, false);

  state->root.vnode.refcount = state->mounted ? 1 : 0;
  state->root.vnode.link_count = 1;
}

static void ensure_mock_mount(htest_ctx_t* ctx) {
  if (mock.mounted) {
    mock_reset(&mock);
    return;
  }

  mock_node_init(&mock.root, &mock_dir_ops, VFS_NODE_DIR, "/", 0, true);
  mock.root.vnode.fs_data = &mock;
  mock.mount.root = &mock.root.vnode;
  mock.mount.point = NULL;
  mock.mount.parent = NULL;
  mock.mount.fs_data = &mock;
  mock_reset(&mock);
  mock.root.vnode.fs_data = &mock;

  vfs_file_t* etc = NULL;
  HTEST_ASSERT(ctx,
               vfs_open("/etc/", VFS_OPEN_DIRECTORY, &etc) == VFS_STATUS_OK);

  vfs_node_t* mountpoint = NULL;
  HTEST_ASSERT(
      ctx, vfs_mkdir(etc->vnode, "vfs_mock/", 9, &mountpoint) == VFS_STATUS_OK);
  HTEST_ASSERT(ctx, vfs_mount(mountpoint, &mock.mount, NULL) == VFS_STATUS_OK);
  vfs_vnode_release(mountpoint);
  HTEST_ASSERT(ctx, vfs_close(etc) == VFS_STATUS_OK);

  mock.mounted = true;
  mock_reset(&mock);
  mock.root.vnode.fs_data = &mock;
}

static void cleanup_mock_mount(htest_ctx_t* ctx) {
  if (!mock.mounted) { return; }

  HTEST_ASSERT(ctx, vfs_unmount(&mock.mount) == VFS_STATUS_OK);
  mock.mounted = false;

  vfs_node_t* unmounted_root = NULL;
  HTEST_ASSERT(ctx,
               vfs_lookup("/etc/vfs_mock/", &unmounted_root) == VFS_STATUS_OK);
  HTEST_ASSERT(ctx, unmounted_root->mount == NULL);
  vfs_vnode_release(unmounted_root);

  HTEST_ASSERT(ctx,
               vfs_lookup("/etc/vfs_mock/existing.txt", &unmounted_root) ==
                   VFS_STATUS_NOENT);

  vfs_file_t* etc = NULL;
  HTEST_ASSERT(ctx,
               vfs_open("/etc/", VFS_OPEN_DIRECTORY, &etc) == VFS_STATUS_OK);
  HTEST_ASSERT(ctx, vfs_rmdir(etc->vnode, "vfs_mock", 8, 0) == VFS_STATUS_OK);
  HTEST_ASSERT(ctx, vfs_close(etc) == VFS_STATUS_OK);

  HTEST_ASSERT(
      ctx, vfs_lookup("/etc/vfs_mock/", &unmounted_root) == VFS_STATUS_NOENT);
}

static bool find_fd_for_file(vfs_file_t* file, uint64_t* fd_out) {
  for (uint64_t i = 0; i < MAX_FDS; ++i) {
    if (sched_pb_fd_get(g_kernel.current_process, i) == file) {
      if (fd_out != NULL) { *fd_out = i; }
      return true;
    }
  }
  return false;
}

static void free_dirent(vfs_dirent_t* dirent) {
  if (dirent == NULL) { return; }
  free((void*)dirent->name);
  free(dirent);
}
