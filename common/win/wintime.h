#ifndef INC__WIN_WINTIME
#define INC__WIN_WINTIME

#include <winsock2.h>
#include <windows.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(_POSIX_TIMERS) || (_POSIX_TIMERS <= 0)
// NOTE: The availability of clock_gettime() is indicated that _POSIX_TIMERS
// is defined and is greater than 0.

#ifndef CLOCK_REALTIME
#define CLOCK_REALTIME 1
#endif

int SRTCompat_clock_gettime(int X, struct timespec *ts);
static inline int clock_gettime(int X, struct timespec *ts)
{
   return SRTCompat_clock_gettime(X, ts);
}

#if defined(_MSC_VER) || defined(_MSC_EXTENSIONS)
    #define DELTA_EPOCH_IN_MICROSECS  11644473600000000Ui64
#else
    #define DELTA_EPOCH_IN_MICROSECS  11644473600000000ULL
#endif
#endif

#ifndef _TIMEZONE_DEFINED /* also in sys/time.h */
#define _TIMEZONE_DEFINED
struct timezone 
{
    int tz_minuteswest; /* minutes W of Greenwich */
    int tz_dsttime;     /* type of dst correction */
};

#endif

void SRTCompat_timeradd(
      struct timeval *a, struct timeval *b, struct timeval *result);
static inline void timeradd(
      struct timeval *a, struct timeval *b, struct timeval *result)
{
   SRTCompat_timeradd(a, b, result);
}

int SRTCompat_gettimeofday(struct timeval* tp, struct timezone* tz);
static inline int gettimeofday(struct timeval* tp, struct timezone* tz)
{
   return SRTCompat_gettimeofday(tp, tz);
}

#ifdef __cplusplus
}
#endif

#endif // INC__WIN_WINTIME
