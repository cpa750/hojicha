#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define HBFI_READ_CHUNK 512

typedef struct {
  char* data;
  unsigned long len;
} bf_source_t;

static void print_help(void) {
  printf("Hojicha Brainfuck Interpreter\n");
  printf("usage: hbfi [--help] file\n");
  printf("\n");
  printf("options:\n");
  printf("  --help   show this help\n");
}

static void free_source(bf_source_t* source) {
  if (source == NULL) { return; }
  free(source->data);
  source->data = NULL;
  source->len = 0;
}

static int append_bytes(bf_source_t* source,
                        const char* buf,
                        unsigned long len) {
  char* next = malloc(source->len + len + 1);
  if (next == NULL) { return -1; }

  if (source->len > 0) { memcpy(next, source->data, source->len); }
  if (len > 0) { memcpy(next + source->len, buf, len); }
  source->len += len;
  next[source->len] = '\0';

  free(source->data);
  source->data = next;
  return 0;
}

static int read_source_fd(int fd, const char* label, bf_source_t* out) {
  char buf[HBFI_READ_CHUNK];
  out->data = NULL;
  out->len = 0;

  while (1) {
    int bytes_read = read(fd, buf, sizeof(buf));
    if (bytes_read < 0) {
      printf("hbfi: cannot read %s: %d\n", label, errno);
      free_source(out);
      return 1;
    }
    if (bytes_read == 0) { break; }

    if (append_bytes(out, buf, bytes_read) < 0) {
      printf("hbfi: out of memory\n");
      free_source(out);
      return 1;
    }
  }

  if (out->data == NULL && append_bytes(out, "", 0) < 0) {
    printf("hbfi: out of memory\n");
    return 1;
  }

  return 0;
}

static int read_source_file(const char* path, bf_source_t* out) {
  int fd = open(path, O_RDONLY, 0);
  if (fd < 0) {
    printf("hbfi: cannot open %s: %d\n", path, errno);
    return 1;
  }

  int status = read_source_fd(fd, path, out);
  close(fd);
  return status;
}

static uint64_t skip_backward(bf_source_t* source, uint64_t current_cmd_idx) {
  uint64_t depth = 1;
  uint64_t next_cmd_idx = current_cmd_idx;

  while (next_cmd_idx > 0) {
    next_cmd_idx--;
    if (source->data[next_cmd_idx] == ']') {
      depth++;
    } else if (source->data[next_cmd_idx] == '[') {
      depth--;
      if (depth == 0) { return next_cmd_idx + 1; }
    }
  }

  return current_cmd_idx + 1;
}

static uint64_t skip_forward(bf_source_t* source, uint64_t current_cmd_idx) {
  uint64_t depth = 1;
  uint64_t next_cmd_idx = current_cmd_idx + 1;

  while (next_cmd_idx < source->len) {
    if (source->data[next_cmd_idx] == '[') {
      depth++;
    } else if (source->data[next_cmd_idx] == ']') {
      depth--;
      if (depth == 0) { return next_cmd_idx + 1; }
    }
    next_cmd_idx++;
  }

  return source->len;
}

static int run(bf_source_t* source) {
  uint8_t* data = (uint8_t*)calloc(1024, sizeof(uint8_t));
  if (data == NULL) { exit(1); }
  uint64_t instruction_idx = 0;
  uint64_t data_idx = 0;
  while (instruction_idx < source->len) {
    switch (source->data[instruction_idx]) {
      case '>':
        data_idx++;
        instruction_idx++;
        break;
      case '<':
        data_idx--;
        instruction_idx++;
        break;
      case '+':
        data[data_idx]++;
        instruction_idx++;
        break;
      case '-':
        data[data_idx]--;
        instruction_idx++;
        break;
      case '.':
        putchar(data[data_idx]);
        instruction_idx++;
        break;
      case ',':
        data[data_idx] = getchar();
        instruction_idx++;
        break;
      case '[':
        if (data[data_idx] != 0) {
          instruction_idx++;
        } else {
          instruction_idx = skip_forward(source, instruction_idx);
        }
        break;
      case ']':
        if (data[data_idx] == 0) {
          instruction_idx++;
        } else {
          instruction_idx = skip_backward(source, instruction_idx);
        }
        break;
      default:
        instruction_idx++;
    }
  }
  return 0;
}

int main(int argc, char** argv) {
  const char* path = NULL;

  if (argc <= 1) {
    print_help();
    return 0;
  }

  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--help") == 0) {
      print_help();
      return 0;
    }

    if (path != NULL) {
      printf("hbfi: too many file operands\n");
      return 1;
    }
    path = argv[i];
  }

  if (path == NULL) {
    print_help();
    return 0;
  }

  bf_source_t source;
  int status = read_source_file(path, &source);
  if (status != 0) { return status; }

  status = run(&source);
  free_source(&source);
  return status;
}
