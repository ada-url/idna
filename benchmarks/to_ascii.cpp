#include "ada/idna/to_ascii.h"

#include <cstdlib>
#include <iostream>
#include <memory>

#include "idna.h"
#include "performancecounters/event_counter.h"
event_collector collector;
size_t N = 1000;

#include <benchmark/benchmark.h>

std::string inputs[] = {
    "-x.xn--zca",
    "xn--zca.xn--zca",
    "xn--mgba3gch31f060k",
    "xn--1ch",
};

double inputs_total_byte = []() -> double {
  size_t bytes{0};
  for (std::string& url_string : inputs) {
    bytes += url_string.size();
  }
  return double(bytes);
}();

static void Ada(benchmark::State& state) {
  for (auto _ : state) {
    for (std::string& url_string : inputs) {
      benchmark::DoNotOptimize(ada::idna::to_ascii(url_string));
    }
  }

  if (collector.has_events()) {
    event_aggregate aggregate{};
    for (size_t i = 0; i < N; i++) {
      std::atomic_thread_fence(std::memory_order_acquire);
      collector.start();
      for (std::string& url_string : inputs) {
        benchmark::DoNotOptimize(ada::idna::to_ascii(url_string));
      }
      std::atomic_thread_fence(std::memory_order_release);
      event_count allocate_count = collector.end();
      aggregate << allocate_count;
    }
    state.counters["cycles/url"] = aggregate.best.cycles() / std::size(inputs);
    state.counters["instructions/url"] =
        aggregate.best.instructions() / std::size(inputs);
    state.counters["instructions/cycle"] =
        aggregate.best.instructions() / aggregate.best.cycles();
    state.counters["instructions/byte"] =
        aggregate.best.instructions() / inputs_total_byte;
    state.counters["instructions/ns"] =
        aggregate.best.instructions() / aggregate.best.elapsed_ns();
    state.counters["GHz"] =
        aggregate.best.cycles() / aggregate.best.elapsed_ns();
    state.counters["ns/url"] = aggregate.best.elapsed_ns() / std::size(inputs);
    state.counters["cycle/byte"] = aggregate.best.cycles() / inputs_total_byte;
  }
  state.counters["time/byte"] = benchmark::Counter(
      inputs_total_byte, benchmark::Counter::kIsIterationInvariantRate |
                             benchmark::Counter::kInvert);
  state.counters["time/url"] = benchmark::Counter(
      double(std::size(inputs)), benchmark::Counter::kIsIterationInvariantRate |
                                     benchmark::Counter::kInvert);
  state.counters["speed"] = benchmark::Counter(
      inputs_total_byte, benchmark::Counter::kIsIterationInvariantRate);
  state.counters["url/s"] = benchmark::Counter(
      double(std::size(inputs)), benchmark::Counter::kIsIterationInvariantRate);
}

BENCHMARK(Ada);

BENCHMARK_MAIN();