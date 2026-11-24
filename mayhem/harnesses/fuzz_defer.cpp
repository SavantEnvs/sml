// Fuzz sml's defer queue together with the process queue:
// sm<M, process_queue<std::queue>, defer_queue<std::deque>>::process_event with
// defer and process(e) actions (process_defer_events, process_event_no_defer,
// process_queued_events). Input bytes choose events, payloads and guard results.
// Actions here do NOT re-enter process_event (that is fuzz_queue's job).
#include <boost/sml.hpp>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <queue>

namespace sml = boost::sml;

namespace {
struct e1 { int v; };
struct e2 { int v; };
struct e3 {};
struct e4 {};
struct e5 {};

struct ctx {
  const uint8_t *d;
  size_t n;
  size_t i;
  long acc;
  uint8_t next() { return i < n ? d[i++] : 0; }
};

struct M {
  auto operator()() const {
    using namespace sml;
    auto g = [](ctx &c) { return (c.next() & 1) != 0; };
    auto acc1 = [](ctx &c, const e1 &e) { c.acc += e.v; };
    auto acc2 = [](ctx &c, const e2 &e) { c.acc ^= e.v; };
    // clang-format off
    return make_transition_table(
      *"a"_s + event<e1> / (acc1, process(e2{1})) = "b"_s,
      "a"_s + event<e3> / defer,
      "a"_s + event<e5> [g] / defer,
      "a"_s + event<e4> [g] = "c"_s,
      "a"_s + event<e2> [g] / process(e5{}),
      "b"_s + event<e2> / acc2 = "c"_s,
      "b"_s + event<e1> / defer,
      "b"_s + event<e5> [g] = "a"_s,
      "b"_s + event<e3> / (process(e4{}), process(e5{})),
      "b"_s + event<e4> [g] / defer,
      "c"_s + event<e1> [g] / (process(e3{}), process(e2{2})) = "a"_s,
      "c"_s + event<e2> [g] / defer,
      "c"_s + event<e2> = "a"_s,
      "c"_s + event<e3> = "b"_s,
      "c"_s + event<e5> / defer,
      "c"_s + event<e4> [g] = X
    );
    // clang-format on
  }
};

using sm_t = sml::sm<M, sml::process_queue<std::queue>, sml::defer_queue<std::deque>>;
}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *d, size_t n) {
  ctx c{d, n, 0, 0};
  sm_t sm{c};
  while (c.i < c.n && !sm.is(sml::X)) {
    const uint8_t b = c.next();
    switch (b % 5) {
      case 0: sm.process_event(e1{b}); break;
      case 1: sm.process_event(e2{b}); break;
      case 2: sm.process_event(e3{}); break;
      case 3: sm.process_event(e4{}); break;
      default: sm.process_event(e5{}); break;
    }
  }
  return 0;
}
