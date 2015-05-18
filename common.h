#pragma once

#include <type_traits>
#include <string>
#include <iostream>
#include <initializer_list>

#include <cstring>
#include <cerrno>
#include <ctime>
#include <csignal>

template <typename T> void check_zero(T ret, std::string msg = "") {
  static_assert(std::is_integral<T>::value,
                "Return value check only for integers.");
  if (ret) {
    std::cerr << msg << " (" << errno << "): " << strerror(errno) << std::endl;
    std::terminate();
  }
}

template <class Clock, class Duration>
void busy_until(const std::chrono::time_point<Clock, Duration> &t) {
  for (; Clock::now() < t;) {
  }
}

template <class Clock = typename std::chrono::high_resolution_clock, class Rep,
          class Period>
void busy_for(std::chrono::duration<Rep, Period> duration) {
  busy_until(Clock::now() + duration);
}

class cputime_clock {
public:
  using rep = typename std::chrono::nanoseconds::rep;
  using period = typename std::chrono::nanoseconds::period;
  using duration = typename std::chrono::nanoseconds;
  using time_point = std::chrono::time_point<cputime_clock>;
  static constexpr bool is_steady = false;

  static time_point now() {
    struct timespec cputime;

    check_zero(clock_gettime(CLOCK_THREAD_CPUTIME_ID, &cputime),
               "clock_gettime");

    rep ns = static_cast<rep>(cputime.tv_nsec) +
             static_cast<rep>(cputime.tv_sec) * 1'000'000'000;
    return time_point(duration(ns));
  }
};

struct timespec operator-(const struct timespec &lhs,
                          const struct timespec &rhs);
pid_t gettid();
void set_affinity(unsigned cpu = 0, pid_t tid = gettid());
void set_affinity(std::initializer_list<unsigned> cpus, pid_t tid = gettid());

using signal_handler_t = void (*)(int, siginfo_t *, void *);
void set_signal_handler(int signal, signal_handler_t handler);
void set_deadline_handler(signal_handler_t handler);
void wait_for_deadline(); 

