#include "ada/idna/to_ascii.h"

#include <cstdlib>
#include <iostream>
#include <memory>
#if ICU_AVAILABLE
#include <unicode/uidna.h>
#include <unicode/utf8.h>
#include <unicode/utypes.h>
#endif

#include "idna.h"
#include "performancecounters/event_counter.h"
event_collector collector;
size_t N = 1000;

#include <benchmark/benchmark.h>

std::string
    inputs
        [] =
            {
                "-x.xn--zca",
                "xn--zca.xn--zca",
                "xn--mgba3gch31f060k",
                "xn--1ch",
                "x-.\xc3\x9f",
                "me\xc3\x9f\x61\x67\x65\x66\x61\x63\x74\x6f\x72\x79\x2e\x63"
                "\x61",  // https://lemire.me/blog/2023/01/23/international-domain-names-where-does-https-mesagefactory-ca-lead-you/
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
    state.counters["cycles/domain"] =
        aggregate.best.cycles() / std::size(inputs);
    state.counters["instructions/domain"] =
        aggregate.best.instructions() / std::size(inputs);
    state.counters["instructions/cycle"] =
        aggregate.best.instructions() / aggregate.best.cycles();
    state.counters["instructions/byte"] =
        aggregate.best.instructions() / inputs_total_byte;
    state.counters["instructions/ns"] =
        aggregate.best.instructions() / aggregate.best.elapsed_ns();
    state.counters["GHz"] =
        aggregate.best.cycles() / aggregate.best.elapsed_ns();
    state.counters["ns/domain"] =
        aggregate.best.elapsed_ns() / std::size(inputs);
    state.counters["cycle/byte"] = aggregate.best.cycles() / inputs_total_byte;
  }
  state.counters["time/byte"] = benchmark::Counter(
      inputs_total_byte, benchmark::Counter::kIsIterationInvariantRate |
                             benchmark::Counter::kInvert);
  state.counters["time/domain"] = benchmark::Counter(
      double(std::size(inputs)), benchmark::Counter::kIsIterationInvariantRate |
                                     benchmark::Counter::kInvert);
  state.counters["speed"] = benchmark::Counter(
      inputs_total_byte, benchmark::Counter::kIsIterationInvariantRate);
  state.counters["url/s"] = benchmark::Counter(
      double(std::size(inputs)), benchmark::Counter::kIsIterationInvariantRate);
}

BENCHMARK(Ada);

#if ICU_AVAILABLE

// returns empty string on error
std::string icu_to_array(std::string_view input) {
  static std::string error = "";

  std::string out(255, 0);
  constexpr bool be_strict = false;

  UErrorCode status = U_ZERO_ERROR;
  uint32_t options =
      UIDNA_CHECK_BIDI | UIDNA_CHECK_CONTEXTJ | UIDNA_NONTRANSITIONAL_TO_ASCII;

  if (be_strict) {
    options |= UIDNA_USE_STD3_RULES;
  }

  UIDNA* uidna = uidna_openUTS46(options, &status);
  if (U_FAILURE(status)) {
    return error;
  }

  UIDNAInfo info = UIDNA_INFO_INITIALIZER;
  // RFC 1035 section 2.3.4.
  // The domain  name must be at most 255 octets.
  // It cannot contain a label longer than 63 octets.
  // Thus we should never need more than 255 octets, if we
  // do the domain name is in error.
  int32_t length =
      uidna_nameToASCII_UTF8(uidna, input.data(), int32_t(input.length()),
                             out.data(), 255, &info, &status);

  if (status == U_BUFFER_OVERFLOW_ERROR) {
    status = U_ZERO_ERROR;
    out.resize(length);
    // When be_strict is true, this should not be allowed!
    length =
        uidna_nameToASCII_UTF8(uidna, input.data(), int32_t(input.length()),
                               out.data(), length, &info, &status);
  }

  // A label contains hyphen-minus ('-') in the third and fourth positions.
  info.errors &= ~UIDNA_ERROR_HYPHEN_3_4;
  // A label starts with a hyphen-minus ('-').
  info.errors &= ~UIDNA_ERROR_LEADING_HYPHEN;
  // A label ends with a hyphen-minus ('-').
  info.errors &= ~UIDNA_ERROR_TRAILING_HYPHEN;

  if (!be_strict) {  // This seems to violate RFC 1035 section 2.3.4.
    // A non-final domain name label (or the whole domain name) is empty.
    info.errors &= ~UIDNA_ERROR_EMPTY_LABEL;
    // A domain name label is longer than 63 bytes.
    info.errors &= ~UIDNA_ERROR_LABEL_TOO_LONG;
    // A domain name is longer than 255 bytes in its storage form.
    info.errors &= ~UIDNA_ERROR_DOMAIN_NAME_TOO_LONG;
  }

  uidna_close(uidna);

  if (U_FAILURE(status) || info.errors != 0 || length == 0) {
    return error;
  }
  out.resize(length);  // we possibly want to call :shrink_to_fit otherwise we
                       // use 255 bytes.
  return out;
}

static void Icu(benchmark::State& state) {
  for (auto _ : state) {
    for (std::string& url_string : inputs) {
      benchmark::DoNotOptimize(icu_to_array(url_string));
    }
  }

  if (collector.has_events()) {
    event_aggregate aggregate{};
    for (size_t i = 0; i < N; i++) {
      std::atomic_thread_fence(std::memory_order_acquire);
      collector.start();
      for (std::string& url_string : inputs) {
        benchmark::DoNotOptimize(icu_to_array(url_string));
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

BENCHMARK(Icu);

bool verify() {
  bool is_ok = true;
  for (std::string& url_string : inputs) {
    std::string icu_answer = icu_to_array(url_string);
    std::string ada_answer = ada::idna::to_ascii(url_string);
    if (icu_answer != ada_answer) {
      std::cerr << " ada/icu mismatch " << ada_answer << " vs. " << icu_answer
                << std::endl;
      is_ok = false;
    }
  }
  if (!is_ok) {
    std::cout << "\n\n\nWarning: errors found.\n\n\n\n";
  } else {
    std::cout << "ICU and ada/idna agree on all test inputs.\n";
  }
  return is_ok;
}
#endif

int main(int argc, char** argv) {
#if ICU_AVAILABLE
  verify();
#endif
#if (__APPLE__ && __aarch64__) || defined(__linux__)
  if (!collector.has_events()) {
    benchmark::AddCustomContext("performance counters",
                                "No privileged access (sudo may help).");
  }
#else
  if (!collector.has_events()) {
    benchmark::AddCustomContext("performance counters", "Unsupported system.");
  }
#endif
  benchmark::AddCustomContext("input bytes",
                              std::to_string(size_t(inputs_total_byte)));
  benchmark::AddCustomContext("number of domains",
                              std::to_string(std::size(inputs)));
  benchmark::AddCustomContext(
      "bytes/domains", std::to_string(inputs_total_byte / std::size(inputs)));
  if (collector.has_events()) {
    benchmark::AddCustomContext("performance counters", "Enabled");
  }
  benchmark::Initialize(&argc, argv);
  benchmark::RunSpecifiedBenchmarks();
  benchmark::Shutdown();
}
