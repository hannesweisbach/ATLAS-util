#pragma once

#include <chrono>
#include <thread>
#include <pthread.h>
#include <algorithm>
#include <sstream>
#include <stdexcept>

#include <cerrno>

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

namespace {
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
}

template <class Rep1, class Period1, class Rep2, class Period2>
decltype(auto) submit(pid_t tid, uint64_t id,
                      std::chrono::duration<Rep1, Period1> exec_time,
                      std::chrono::duration<Rep2, Period2> deadline) {
  struct timeval tv_exectime = to_timeval(exec_time);
  struct timeval tv_deadline =
      to_timeval(std::chrono::high_resolution_clock::now() + deadline);

  return syscall(SYS_atlas_submit, tid, id, &tv_exectime, &tv_deadline);
}

template <class Rep, class Period, class Clock, class Duration>
decltype(auto) submit(pid_t tid, uint64_t id,
                      std::chrono::duration<Rep, Period> exec_time,
                      std::chrono::time_point<Clock, Duration> deadline) {
  struct timeval tv_exectime = to_timeval(exec_time);
  struct timeval tv_deadline = to_timeval(deadline);

  return syscall(SYS_atlas_submit, tid, id, &tv_exectime, &tv_deadline);
}

namespace np {

static inline pid_t from(const pthread_t tid) {
  /* on linux x86_64 glibc has the struct pthread.tid member at (byte-) offset
   * 720. */
  const size_t offset = 90;
  pid_t result;
  uint64_t *tmp = reinterpret_cast<uint64_t *>(tid) + offset;
  pid_t *src = reinterpret_cast<pid_t *>(tmp);
  std::copy(src, src + 1, &result);
  if (!result) {
    std::ostringstream os;
    os << "Thread " << tid << " has either not started or already quit.";
    throw std::runtime_error(os.str());
  }
  return result;
}

static inline pid_t from(const std::thread::id tid) {
  /* std::thread::id has a pthread_t as first member in libc++ */
  return from(*reinterpret_cast<const pthread_t *>(&tid));
}

template <class Rep1, class Period1, class Rep2, class Period2>
decltype(auto) submit(const std::thread::id tid, uint64_t id,
                      std::chrono::duration<Rep1, Period1> exec_time,
                      std::chrono::duration<Rep2, Period2> deadline) {
  return atlas::submit(from(tid), id, exec_time, deadline);
}

template <class Rep1, class Period1, class Rep2, class Period2>
decltype(auto) submit(const pthread_t tid, uint64_t id,
                      std::chrono::duration<Rep1, Period1> exec_time,
                      std::chrono::duration<Rep2, Period2> deadline) {
  return atlas::submit(from(tid), id, exec_time, deadline);
}

template <class Rep, class Period, class Clock, class Duration>
decltype(auto) submit(const std::thread::id tid, uint64_t id,
                      std::chrono::duration<Rep, Period> exec_time,
                      std::chrono::time_point<Clock, Duration> deadline) {
  return atlas::submit(from(tid), id, exec_time, deadline);
}

template <class Rep, class Period, class Clock, class Duration>
decltype(auto) submit(const pthread_t tid, uint64_t id,
                      std::chrono::duration<Rep, Period> exec_time,
                      std::chrono::time_point<Clock, Duration> deadline) {
  return atlas::submit(from(tid), id, exec_time, deadline);
}
}

static inline decltype(auto) next(void) {
  long ret;
  for (ret = syscall(SYS_atlas_next); ret && errno == EINTR;
       ret = syscall(SYS_atlas_next)) {
  }
  return ret;
}

}
