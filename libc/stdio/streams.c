#include <internal/__stdio.h>

FILE __hojicha_stdin = {.fd = 0, .flags = 0};
FILE __hojicha_stdout = {.fd = 1, .flags = 0};
FILE __hojicha_stderr = {.fd = 2, .flags = 0};
