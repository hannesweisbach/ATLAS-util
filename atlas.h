#pragma once

#include <chrono>

#include <sys/types.h>

#if defined(__x86_64__)
#define SYS_atlas_next 323
#define SYS_atlas_submit 324
#elif defined(__i386__)
#define SYS_atlas_next 359
#define SYS_atlas_submit 360
#else
#error Architecture not supported.
#endif

namespace atlas {

enum class deadline {
  absolute = 0,
  relative = 1,
};

static inline long submit(pid_t pid, uint64_t id, struct timeval *exectime,
                          struct timeval *deadline, enum deadline reference) {
  return syscall(SYS_atlas_submit, pid, id, exectime, deadline, reference);
}

template <class Rep, class Period>
struct timeval to_timeval(const std::chrono::duration<Rep, Period> &duration) {
  using namespace std::chrono;
  auto secs = duration_cast<seconds>(duration);
  auto usecs = duration_cast<microseconds>(duration - secs);
  return {
      static_cast<time_t>(secs.count()),
      static_cast<suseconds_t>(usecs.count()),
  };
}

template <class Clock, class Duration>
struct timeval to_timeval(const std::chrono::time_point<Clock, Duration> &t) {
  using namespace std::chrono;
  auto secs = time_point_cast<seconds>(t);
  auto usecs = duration_cast<microseconds>(t - secs);

  return {
      static_cast<time_t>(secs.time_since_epoch().count()),
      static_cast<suseconds_t>(usecs.count()),
  };
}

template <class Rep1, class Period1, class Rep2, class Period2>
decltype(auto) submit(pid_t tid, uint64_t id,
                      std::chrono::duration<Rep1, Period1> exec_time,
                      std::chrono::duration<Rep2, Period2> deadline) {
  struct timeval tv_exectime = to_timeval(exec_time);
  struct timeval tv_deadline = to_timeval(deadline);

  return submit(tid, id, &tv_exectime, &tv_deadline, deadline::relative);
}

template <class Rep, class Period, class Clock, class Duration>
decltype(auto) submit(pid_t tid, uint64_t id,
                      std::chrono::duration<Rep, Period> exec_time,
                      std::chrono::time_point<Clock, Duration> deadline) {
  struct timeval tv_exectime = to_timeval(exec_time);
  struct timeval tv_deadline = to_timeval(deadline);

  return submit(tid, id, &tv_exectime, &tv_deadline, deadline::absolute);
}

static inline decltype(auto) next(void) { return syscall(SYS_atlas_next); }
}
