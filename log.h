#ifndef LOG_H
#define LOG_H

void init_logging(int level);
void lograw(int level, const char *msg);
void logfmt(int level, const char *fmt, ...);

#define loginfo(fmt, ...) logfmt(0, fmt "\n", ##__VA_ARGS__)
#define logdebug(fmt, ...) logfmt(1, fmt "\n", ##__VA_ARGS__)
#define logtrace(fmt, ...) logfmt(2, fmt "\n", ##__VA_ARGS__)
#define logcrazy(fmt, ...) logfmt(3, fmt "\n", ##__VA_ARGS__)

#endif
