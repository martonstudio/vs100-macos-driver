#ifndef VS100_LOG_H
#define VS100_LOG_H
#include <os/log.h>
#include <stdio.h>
#include <stdarg.h>

static inline os_log_t vs100_logger(void) {
    static os_log_t l;
    if (!l) l = os_log_create("com.vs100.driver", "usb");
    return l;
}
static inline void vs100_logf(int isError, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
static inline void vs100_logf(int isError, const char *fmt, ...) {
    char buf[512];
    va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof buf, fmt, ap); va_end(ap);
    os_log_with_type(vs100_logger(), isError ? OS_LOG_TYPE_ERROR : OS_LOG_TYPE_DEFAULT, "%{public}s", buf);
#ifdef VS100_LOG_STDERR
    fprintf(stderr, "[vs100] %s\n", buf);
#endif
}
#define LOG(...)  vs100_logf(0, __VA_ARGS__)
#define LOGE(...) vs100_logf(1, __VA_ARGS__)
#endif
