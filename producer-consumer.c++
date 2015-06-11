#include <vector>
#include <thread>
#include <chrono>
#include <memory>
#include <iterator>
#include <algorithm>
#include <pthread.h>
#include <utility>
#include <atomic>
#include <random>

#include <signal.h>

#include "atlas.h"
#include "common.h"

static std::atomic_bool running__{true};

static void sig_term(int, siginfo_t *, void *) { running__ = false; }

struct client_state {
  mutable std::atomic<size_t> samples{0};
  mutable std::atomic_bool in_next{false};
  std::atomic_bool initialized{false};
  std::thread::id tid;


  bool is_blocked() const { return samples == 0 && in_next; }
};

namespace continuous {
/* producer and consumer continuously submitting and processing work */
static void producer(const std::vector<std::unique_ptr<std::thread>> &consumers,
                     const size_t samples,
                     const std::pair<size_t, size_t> producer_info,
                     const client_state *const queue,
                     std::atomic_bool &running) {
  using namespace std::chrono;

  const size_t producer_id = producer_info.first;
  const size_t num_producers = producer_info.second;
  const size_t num_consumers = consumers.size();
  size_t consumer_id;

  auto cpus = std::thread::hardware_concurrency();
  if (cpus > 3) {
    set_affinity({1, 2});
  } else {
    set_affinity({0});
  }

  std::for_each(queue, queue + num_consumers, [](const auto &client) {
    while (!client.initialized)
      std::this_thread::yield();
  });
  /* Note: 4-core execution time + deadline: 1s */

  for (size_t sample = 0; running; ++sample) {
    consumer_id = 0;
    for (const auto &consumer : consumers) {
      if (queue[consumer_id].samples < samples) {
        uint64_t id = num_producers * num_consumers * sample +
                      num_consumers * producer_id + consumer_id;
        ++queue[consumer_id].samples;
        check_zero(atlas::np::submit(consumer->get_id(), id, 5000ms, 5000ms));
      }
      ++consumer_id;
    }
  }
}

}

namespace miss {
/* producer and consumer, where consumer sometimes misses deadlines */
static void producer(std::thread::id consumer, std::atomic_bool &running,
                     const client_state &client) {
  using namespace std::chrono;

  uint64_t id = 0;
  std::mt19937_64 generator;
  std::uniform_int_distribution<bool> blocking;

  auto cpus = std::thread::hardware_concurrency();
  if (cpus > 3) {
    set_affinity({1, 2});
  } else {
    set_affinity({0});
  }

  while (!client.initialized)
    std::this_thread::yield();

  std::this_thread::sleep_for(1s);

  for (; running;) {
    if (blocking(generator)) {
      while (!client.is_blocked())
        ;
      std::this_thread::sleep_for(1ms);
      ++client.samples;
      atlas::np::submit(consumer, id++, 90ms, 100ms);
    } else {
      while (client.samples >= 10)
        ;
      ++client.samples;
      atlas::np::submit(consumer, id++, 90ms, 100ms);
    }
  }
}

}

static void producer(const std::vector<std::unique_ptr<std::thread>> &consumers,
                     const client_state *const queue, const size_t samples,
                     const std::pair<size_t, size_t> producer_info) {
  using namespace std::chrono;

  const size_t producer_id = producer_info.first;
  const size_t num_producers = producer_info.second;
  const size_t num_consumers = consumers.size();
  size_t consumer_id;

  auto cpus = std::thread::hardware_concurrency();
  if (cpus > 3) {
    set_affinity({1, 2});
  } else {
    set_affinity({0});
  }

  std::for_each(queue, queue + num_consumers, [](const auto &client) {
    while (!client.initialized)
      std::this_thread::yield();
  });

  /* Note: 4-core execution time + deadline: 1ms */
  consumer_id = 0;
  for (const auto &consumer : consumers) {
    uint64_t id = num_consumers * producer_id + consumer_id;
    check_zero(atlas::np::submit(consumer->get_id(), id, 5000ms, 5000ms));
    ++consumer_id;
  }

  std::this_thread::sleep_for(1s);

  for (size_t sample = 1; sample < samples; ++sample) {
    consumer_id = 0;
    for (const auto &consumer : consumers) {
      uint64_t id = num_producers * num_consumers * sample +
                    num_consumers * producer_id + consumer_id;
      /*timeval.tv_usec += 1;*/
      check_zero(atlas::np::submit(consumer->get_id(), id, 5000ms, 5000ms));
      ++consumer_id;
    }
  }
}

static void consumer(client_state &state, std::atomic_bool &running,
                     const bool enable_deadline_misses = false) {
  std::mt19937_64 generator;
  std::uniform_int_distribution<bool> miss;
  unsigned nr_cpus = std::thread::hardware_concurrency();
  set_affinity(nr_cpus - 1);

  state.tid = std::this_thread::get_id();
  state.initialized = true;

  for (; running || state.samples;) {
    state.in_next = true;
    check_zero(atlas::next());
    --state.samples;
    state.in_next = false;

    if (enable_deadline_misses && miss(generator)) {
      wait_for_deadline();
    } else {
      for (size_t i = 0; i < 1000 * 1000; ++i)
        __asm__ __volatile__("nop");
    }
  }
}

int main() {
  bool continuous = true;
  bool miss = false;
  size_t num_consumers = 3;
  size_t num_producers = 10;
  size_t samples = 1000;

  auto queue = std::make_unique<client_state[]>(num_consumers);
  for (size_t consumer = 0; consumer < num_consumers; ++consumer) {
    queue[consumer].samples = 0;
    queue[consumer].in_next = false;
    queue[consumer].initialized = false;
  }

  std::vector<std::unique_ptr<std::thread>> producers;
  std::vector<std::unique_ptr<std::thread>> consumers;

  set_signal_handler(SIGTERM, sig_term);

  if (continuous) {
    for (size_t consumer = 0; consumer < num_consumers; ++consumer) {
      consumers.emplace_back(
          std::make_unique<std::thread>([&state = queue[consumer]]() {
            ::consumer(state, running__);
          }));
    }

    for (size_t producer = 0; producer < num_producers; ++producer) {
      producers.push_back(std::make_unique<std::thread>([
        &consumers,
        samples,
        producer_info = std::make_pair(producer, num_producers),
        &queue
      ]() {
        continuous::producer(consumers, samples, producer_info, queue.get(),
                             running__);
      }));
    }
  } else if (miss) {
    consumers.emplace_back(std::make_unique<std::thread>([&state = queue[0]]() {
      ::consumer(state, running__, true);
    }));

    producers.push_back(std::make_unique<std::thread>(
        [ consumer = consumers.back()->get_id(), &state = queue[0] ]() {
          miss::producer(consumer, running__, state);
        }));
  } else {
    for (size_t consumer = 0; consumer < num_consumers; ++consumer) {
      consumers.emplace_back(
          std::make_unique<std::thread>([&samples = queue[consumer]]() {
            ::consumer(samples, running__);
          }));
    }

    for (size_t producer = 0; producer < num_producers; ++producer) {
      producers.push_back(std::make_unique<std::thread>([
        &consumers,
        clients = queue.get(),
        samples,
        producer_info = std::make_pair(producer, num_producers)
      ]() { ::producer(consumers, clients, samples, producer_info); }));
    }
  }

  for (auto &&producer : producers) {
    producer->join();
  }

  for (auto &&consumer : consumers) {
    consumer->join();
  }
}
