#ifndef LOG_H
#define LOG_H

void init_logging(int level);
void lograw(int level, const char *msg);
void logfmt(int level, const char *fmt, ...);

#define loginfo(...) logfmt(0, __VA_ARGS__)
#define logdebug(...) logfmt(1, __VA_ARGS__)
#define logtrace(...) logfmt(2, __VA_ARGS__)
#define logcrazy(...) logfmt(3, __VA_ARGS__)

#endif
