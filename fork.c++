#include <iostream>
#include <chrono>
#include <thread>

#include <unistd.h>

#include "atlas.h"
#include "common.h"

int main() {
  using namespace std::chrono;
  check_zero(atlas::submit(gettid(), 0, 1s, 5s));

  /* After next() we're scheduled by ATLAS */
  check_zero(atlas::next());

  fork();

  std::cout << "Scheduled by " << sched_getscheduler(0) << std::endl;
}
