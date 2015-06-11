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

#include <boost/program_options.hpp>

#include "atlas.h"
#include "common.h"

static std::atomic_bool running__{true};

static void sig_term(int, siginfo_t *, void *) { running__ = false; }

struct client_state {
  size_t id;
  std::thread::id tid;
  mutable std::atomic<size_t> samples{0};
  mutable std::atomic_bool in_next{false};
  std::atomic_bool initialized{false};

  bool is_blocked() const { return samples == 0 && in_next; }
};

namespace continuous {
/* producer and consumer continuously submitting and processing work */
static void producer(const std::vector<client_state> &consumers,
                     const size_t samples,
                     const std::pair<size_t, size_t> producer_info,
                     std::atomic_bool &running) {
  using namespace std::chrono;

  const size_t producer_id = producer_info.first;
  const size_t num_producers = producer_info.second;
  const size_t num_consumers = consumers.size();

  auto cpus = std::thread::hardware_concurrency();
  if (cpus > 3) {
    set_affinity({1, 2});
  } else {
    set_affinity({0});
  }

  for (const auto &consumer : consumers) {
    while (!consumer.initialized)
      std::this_thread::yield();
  }

  /* Note: 4-core execution time + deadline: 1s */

  for (size_t sample = 0; running; ++sample) {
    for (const auto &consumer : consumers) {
      if (consumer.samples < samples) {
        uint64_t id = num_producers * num_consumers * sample +
                      num_consumers * producer_id + consumer.id;
        ++consumer.samples;
        check_zero(atlas::np::submit(consumer.tid, id, 5000ms, 5000ms));
      }
    }
  }
}
}

namespace miss {
/* producer and consumer, where consumer sometimes misses deadlines */
static void producer(const client_state &client, std::atomic_bool &running) {
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
      atlas::np::submit(client.tid, id++, 90ms, 100ms);
    } else {
      while (client.samples >= 10)
        ;
      ++client.samples;
      atlas::np::submit(client.tid, id++, 90ms, 100ms);
    }
  }
}

}

static void producer(const std::vector<client_state> &consumers,
                     const size_t samples,
                     const std::pair<size_t, size_t> producer_info) {
  using namespace std::chrono;

  const size_t producer_id = producer_info.first;
  const size_t num_producers = producer_info.second;
  const size_t num_consumers = consumers.size();

  auto cpus = std::thread::hardware_concurrency();
  if (cpus > 3) {
    set_affinity({1, 2});
  } else {
    set_affinity({0});
  }

  for (const auto &consumer : consumers) {
    while (!consumer.initialized)
      std::this_thread::yield();
  }

  /* Note: 4-core execution time + deadline: 1ms */
  for (const auto &consumer : consumers) {
    uint64_t id = num_consumers * producer_id + consumer.id;
    check_zero(atlas::np::submit(consumer.tid, id, 5000ms, 5000ms));
  }

  std::this_thread::sleep_for(1s);

  for (size_t sample = 1; sample < samples; ++sample) {
    for (const auto &consumer : consumers) {
      uint64_t id = num_producers * num_consumers * sample +
                    num_consumers * producer_id + consumer.id;
      /*timeval.tv_usec += 1;*/
      check_zero(atlas::np::submit(consumer.tid, id, 5000ms, 5000ms));
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

int main(int argc, char *argv[]) {
  bool continuous;
  bool miss;
  size_t num_consumers;
  size_t num_producers;
  size_t samples;

  namespace po = boost::program_options;
  po::options_description desc("Producer-consumer test suite.");
  auto o = desc.add_options();
  o("help", "produce help message");
  o("miss", po::value(&miss)->default_value(false)->implicit_value(true),
    "Consumer miss occasionally a deadline.");
  o("continuous",
    po::value(&continuous)->default_value(false)->implicit_value(true),
    "Run the test suite continously.");
  o("num-producers", po::value(&num_producers)->default_value(1),
    "The number of producer threads");
  o("num-consumers", po::value(&num_consumers)->default_value(1),
    "The number of consumer threads (default: 1)");
  o("jobs", po::value(&samples)->default_value(100),
    "Number of jobs per producer. In the continuous case, the maximum number"
    "of unfinished jobs per consumer.");

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help")) {
    std::cout << desc << std::endl;
    return EXIT_FAILURE;
  }

  set_signal_handler(SIGTERM, sig_term);

  std::vector<std::unique_ptr<std::thread>> producers;
  std::vector<std::unique_ptr<std::thread>> consumers;

  size_t consumer_id = 0;
  std::vector<client_state> consumer_states(num_consumers);
  for (auto &&state : consumer_states) {
    consumers.emplace_back(std::make_unique<std::thread>([&state = state]() {
      ::consumer(state, running__);
    }));
    state.tid = consumers.back()->get_id();
    state.initialized = true;
    state.id = consumer_id++;
  }

  if (continuous) {
    for (size_t producer = 0; producer < num_producers; ++producer) {
      producers.push_back(std::make_unique<std::thread>([
        &consumer_states,
        samples,
        producer_info = std::make_pair(producer, num_producers)
      ]() {
        continuous::producer(consumer_states, samples, producer_info,
                             running__);
      }));
    }
  } else if (miss) {
    producers.push_back(
        std::make_unique<std::thread>([&state = consumer_states[0]]() {
          miss::producer(state, running__);
        }));
  } else {
    for (size_t producer = 0; producer < num_producers; ++producer) {
      producers.push_back(std::make_unique<std::thread>([
        &consumers,
        &consumer_states,
        samples,
        producer_info = std::make_pair(producer, num_producers)
      ]() { ::producer(consumer_states, samples, producer_info); }));
    }
  }

  for (auto &&producer : producers) {
    producer->join();
  }

  for (auto &&consumer : consumers) {
    consumer->join();
  }
}
