#include <drivers/tty.h>
#include <drivers/tty_control.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
  TTY_CONTROL_STATE_TEXT,
  TTY_CONTROL_STATE_ESCAPE,
  TTY_CONTROL_STATE_CSI,
} tty_control_state_t;

typedef struct {
  tty_control_state_t state;
  tty_control_sequence_t sequence;
  uint64_t current_param;
  bool has_current_param;
  bool last_was_separator;
} tty_control_parser_t;

static tty_control_parser_t parser;

static bool tty_control_is_alpha(char c);
static bool tty_control_is_digit(char c);
static void tty_control_csi_begin(void);
static void tty_control_param_push(void);
static uint64_t tty_control_param_or_default(tty_control_sequence_t* sequence,
                                             uint64_t idx,
                                             uint64_t default_value);
static void tty_control_dispatch(tty_control_sequence_t* sequence);
static void tty_control_handle_h(tty_control_sequence_t* sequence);
static void tty_control_handle_j(tty_control_sequence_t* sequence);

void tty_control_reset(void) {
  parser.state = TTY_CONTROL_STATE_TEXT;
  parser.sequence.command = '\0';
  parser.sequence.param_count = 0;
  parser.current_param = 0;
  parser.has_current_param = false;
  parser.last_was_separator = false;
}

void tty_control_write(const char* data, size_t len) {
  for (size_t i = 0; i < len; ++i) {
    char c = data[i];

    switch (parser.state) {
      case TTY_CONTROL_STATE_TEXT:
        if (c == '\033') {
          parser.state = TTY_CONTROL_STATE_ESCAPE;
        } else {
          terminal_putchar(c);
        }
        break;

      case TTY_CONTROL_STATE_ESCAPE:
        if (c == '[') {
          tty_control_csi_begin();
        } else if (c != '\033') {
          tty_control_reset();
        }
        break;

      case TTY_CONTROL_STATE_CSI:
        if (tty_control_is_digit(c)) {
          parser.current_param =
              parser.current_param * 10 + (uint64_t)(c - '0');
          parser.has_current_param = true;
          parser.last_was_separator = false;
        } else if (c == ';') {
          tty_control_param_push();
          parser.last_was_separator = true;
        } else if (tty_control_is_alpha(c)) {
          if (parser.has_current_param || parser.last_was_separator) {
            tty_control_param_push();
          }
          parser.sequence.command = c;
          tty_control_dispatch(&parser.sequence);
          tty_control_reset();
        }
        break;
    }
  }
}

static bool tty_control_is_alpha(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

static bool tty_control_is_digit(char c) { return c >= '0' && c <= '9'; }

static void tty_control_csi_begin(void) {
  parser.state = TTY_CONTROL_STATE_CSI;
  parser.sequence.command = '\0';
  parser.sequence.param_count = 0;
  parser.current_param = 0;
  parser.has_current_param = false;
  parser.last_was_separator = false;
}

static void tty_control_param_push(void) {
  if (parser.sequence.param_count < TTY_CONTROL_MAX_PARAMS) {
    parser.sequence.params[parser.sequence.param_count++] =
        parser.has_current_param ? parser.current_param
                                 : TTY_CONTROL_PARAM_NONE;
  }
  parser.current_param = 0;
  parser.has_current_param = false;
}

static uint64_t tty_control_param_or_default(tty_control_sequence_t* sequence,
                                             uint64_t idx,
                                             uint64_t default_value) {
  if (idx >= sequence->param_count ||
      sequence->params[idx] == TTY_CONTROL_PARAM_NONE) {
    return default_value;
  }
  return sequence->params[idx];
}

static void tty_control_dispatch(tty_control_sequence_t* sequence) {
  switch (sequence->command) {
    case 'H':
    case 'f':
      tty_control_handle_h(sequence);
      break;
    case 'J':
      tty_control_handle_j(sequence);
      break;
    default:
      break;
  }
}

static void tty_control_handle_h(tty_control_sequence_t* sequence) {
  uint64_t row = tty_control_param_or_default(sequence, 0, 1);
  uint64_t col = tty_control_param_or_default(sequence, 1, 1);
  uint64_t terminal_height = terminal_get_height();
  uint64_t terminal_width = terminal_get_width();

  if (terminal_height == 0 || terminal_width == 0) { return; }

  if (row == 0) { row = 1; }
  if (col == 0) { col = 1; }
  if (row > terminal_height) { row = terminal_height; }
  if (col > terminal_width) { col = terminal_width; }

  terminal_set_cursor_pos((uint16_t)(row - 1), (uint16_t)(col - 1));
}

static void tty_control_handle_j(tty_control_sequence_t* sequence) {
  uint64_t mode = tty_control_param_or_default(sequence, 0, 0);

  switch (mode) {
    case TTY_CLEAR_TO_END:
    case TTY_CLEAR_TO_BEGINNING:
    case TTY_CLEAR_ALL:
      terminal_clear_display((tty_clear_mode_t)mode);
      break;
    default:
      break;
  }
}
