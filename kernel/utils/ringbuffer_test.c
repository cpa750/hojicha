#include <utils/ringbuffer.h>
#include <utils/ringbuffer_test.h>
#include <utils/test.h>

#define RINGBUFFER_SIZE      3
#define MOCK_LOCK_MAX_EVENTS 8

typedef struct mock_lock mock_lock_t;
struct mock_lock {
  uint64_t read_lock_count;
  uint64_t read_unlock_count;
  uint64_t write_lock_count;
  uint64_t write_unlock_count;
  uint64_t event_count;
  char events[MOCK_LOCK_MAX_EVENTS];
  bool locked;
  bool relocked;
  bool unlocked_before_lock;
};

static void mock_lock_record(mock_lock_t* lock, char event) {
  if (lock->event_count < MOCK_LOCK_MAX_EVENTS) {
    lock->events[lock->event_count++] = event;
  }
}

static void mock_read_lock(void* lock) {
  mock_lock_t* mock = (mock_lock_t*)lock;
  mock_lock_record(mock, 'R');
  mock->read_lock_count++;
  if (mock->locked) { mock->relocked = true; }
  mock->locked = true;
}

static void mock_read_unlock(void* lock) {
  mock_lock_t* mock = (mock_lock_t*)lock;
  mock_lock_record(mock, 'r');
  mock->read_unlock_count++;
  if (!mock->locked) { mock->unlocked_before_lock = true; }
  mock->locked = false;
}

static void mock_write_lock(void* lock) {
  mock_lock_t* mock = (mock_lock_t*)lock;
  mock_lock_record(mock, 'W');
  mock->write_lock_count++;
  if (mock->locked) { mock->relocked = true; }
  mock->locked = true;
}

static void mock_write_unlock(void* lock) {
  mock_lock_t* mock = (mock_lock_t*)lock;
  mock_lock_record(mock, 'w');
  mock->write_unlock_count++;
  if (!mock->locked) { mock->unlocked_before_lock = true; }
  mock->locked = false;
}

void ringbuffer_test(void) {
  htest_ctx_t ctx = {0};
  htest_suite_begin(&ctx, "ringbuffer");

  htest_case_begin(&ctx, "simple write + read");
  {
    ringbuffer_t* rb = NULL;
    char a;
    ringbuffer_new(RINGBUFFER_SIZE, &rb, NULL, NULL, NULL, NULL, NULL);
    HTEST_ASSERT(&ctx, rb != NULL);
    ringbuffer_write(rb, 'a');

    HTEST_ASSERT(&ctx, ringbuffer_read(rb, &a) == true);
    HTEST_ASSERT(&ctx, a == 'a');
    ringbuffer_free(rb);
  }

  htest_case_begin(&ctx, "starved read");
  {
    ringbuffer_t* rb = NULL;
    char a;
    ringbuffer_new(RINGBUFFER_SIZE, &rb, NULL, NULL, NULL, NULL, NULL);
    HTEST_ASSERT(&ctx, rb != NULL);
    ringbuffer_write(rb, 'a');
    ringbuffer_read(rb, &a);
    char b = 'c';
    HTEST_ASSERT(&ctx, ringbuffer_read(rb, &b) == false);
    HTEST_ASSERT(&ctx, b == 'c');

    ringbuffer_write(rb, 'b');
    HTEST_ASSERT(&ctx, ringbuffer_read(rb, &b) == true);
    HTEST_ASSERT(&ctx, b == 'b');

    ringbuffer_free(rb);
  }

  htest_case_begin(&ctx, "overflow");
  {
    ringbuffer_t* rb = NULL;
    char a;
    char b;
    char c;
    char d;

    ringbuffer_new(RINGBUFFER_SIZE, &rb, NULL, NULL, NULL, NULL, NULL);
    HTEST_ASSERT(&ctx, rb != NULL);
    ringbuffer_write(rb, 'a');
    ringbuffer_write(rb, 'b');
    ringbuffer_write(rb, 'c');
    ringbuffer_read(rb, &a);
    ringbuffer_read(rb, &b);

    HTEST_ASSERT(&ctx, ringbuffer_read(rb, &c) == true);
    HTEST_ASSERT(&ctx, c == 'c');

    ringbuffer_write(rb, 'd');
    HTEST_ASSERT(&ctx, ringbuffer_read(rb, &d) == true);
    HTEST_ASSERT(&ctx, d == 'd');

    ringbuffer_free(rb);
  }

  htest_case_begin(&ctx, "overwrite");
  {
    ringbuffer_t* rb = NULL;
    char d;

    ringbuffer_new(RINGBUFFER_SIZE, &rb, NULL, NULL, NULL, NULL, NULL);
    HTEST_ASSERT(&ctx, rb != NULL);
    ringbuffer_write(rb, 'a');
    ringbuffer_write(rb, 'b');
    ringbuffer_write(rb, 'c');
    ringbuffer_write(rb, 'd');

    HTEST_ASSERT(&ctx, ringbuffer_read(rb, &d) == true);  // First read
    HTEST_ASSERT(&ctx, d == 'd');  // overflow replaces unread chars

    ringbuffer_free(rb);
  }

  htest_case_begin(&ctx, "read and write lock callbacks");
  {
    ringbuffer_t* rb = NULL;
    mock_lock_t lock = {0};
    char a;

    ringbuffer_new(RINGBUFFER_SIZE,
                   &rb,
                   &lock,
                   mock_read_lock,
                   mock_read_unlock,
                   mock_write_lock,
                   mock_write_unlock);
    HTEST_ASSERT(&ctx, rb != NULL);

    ringbuffer_write(rb, 'a');
    HTEST_ASSERT(&ctx, lock.read_lock_count == 0);
    HTEST_ASSERT(&ctx, lock.read_unlock_count == 0);
    HTEST_ASSERT(&ctx, lock.write_lock_count == 1);
    HTEST_ASSERT(&ctx, lock.write_unlock_count == 1);

    HTEST_ASSERT(&ctx, ringbuffer_read(rb, &a) == true);
    HTEST_ASSERT(&ctx, a == 'a');
    HTEST_ASSERT(&ctx, lock.read_lock_count == 1);
    HTEST_ASSERT(&ctx, lock.read_unlock_count == 1);
    HTEST_ASSERT(&ctx, lock.write_lock_count == 1);
    HTEST_ASSERT(&ctx, lock.write_unlock_count == 1);
    HTEST_ASSERT(&ctx, ringbuffer_read(rb, &a) == false);
    HTEST_ASSERT(&ctx, lock.read_lock_count == 2);
    HTEST_ASSERT(&ctx, lock.read_unlock_count == 2);
    HTEST_ASSERT(&ctx, lock.write_lock_count == 1);
    HTEST_ASSERT(&ctx, lock.write_unlock_count == 1);
    HTEST_ASSERT(&ctx, lock.event_count == 6);
    HTEST_ASSERT(&ctx, lock.events[0] == 'W');
    HTEST_ASSERT(&ctx, lock.events[1] == 'w');
    HTEST_ASSERT(&ctx, lock.events[2] == 'R');
    HTEST_ASSERT(&ctx, lock.events[3] == 'r');
    HTEST_ASSERT(&ctx, lock.events[4] == 'R');
    HTEST_ASSERT(&ctx, lock.events[5] == 'r');
    HTEST_ASSERT(&ctx, lock.locked == false);
    HTEST_ASSERT(&ctx, lock.relocked == false);
    HTEST_ASSERT(&ctx, lock.unlocked_before_lock == false);

    ringbuffer_free(rb);
  }

  htest_suite_pass(&ctx);
}
