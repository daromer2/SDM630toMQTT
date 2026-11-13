// Small logger: writes to the debug serial port and keeps a ring buffer for the web UI.
#pragma once
#include <Arduino.h>

void logBegin(Print *out);
void logf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

// Calls `fn` with the log content in chronological order (max two chunks).
void logForEach(void (*fn)(const char *data, size_t len, void *ctx), void *ctx);
