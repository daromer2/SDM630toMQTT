#include "log.h"
#include <stdarg.h>

static char ring[3072];
static size_t head = 0;
static bool wrapped = false;
static Print *out = nullptr;

void logBegin(Print *p) { out = p; }

static void put(const char *s) {
  while (*s) {
    ring[head++] = *s++;
    if (head >= sizeof(ring)) {
      head = 0;
      wrapped = true;
    }
  }
}

void logf(const char *fmt, ...) {
  char buf[256];
  unsigned long s = millis() / 1000;
  int n = snprintf(buf, sizeof(buf), "[%lu] ", s);
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf + n, sizeof(buf) - n, fmt, ap);
  va_end(ap);
  put(buf);
  put("\n");
  if (out) out->println(buf);
}

void logForEach(void (*fn)(const char *, size_t, void *), void *ctx) {
  if (wrapped && head < sizeof(ring)) fn(ring + head, sizeof(ring) - head, ctx);
  if (head) fn(ring, head, ctx);
}
