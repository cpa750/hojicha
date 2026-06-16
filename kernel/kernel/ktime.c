#include <kernel/ktime.h>
#include <limine.h>
#include <stdint.h>

#include "drivers/pit.h"
#include "kernel/g_kernel.h"

__attribute__((
    used,
    section(
        ".limine_requests"))) static volatile struct limine_date_at_boot_request
    time_request = {.id = LIMINE_DATE_AT_BOOT_REQUEST, .revision = 0};

static int64_t unix_boot_timestamp = 0;

void ktime_initialize() {
  unix_boot_timestamp = time_request.response->timestamp;
}

uint64_t uptime() {
  return pit_get_ns_elapsed_since_init(g_kernel.pit) / NS_DIVISOR;
}

int64_t unix_time() {
  return unix_boot_timestamp +
         (pit_get_ns_elapsed_since_init(g_kernel.pit) / NS_DIVISOR);
}

