// Fuzz sml's process queue: sm<M, process_queue<std::queue>>::process_event with
// process(e) actions and actions that re-enter sm.process_event (the scenario of
// upstream issue #465, fixed in 0672b84677: process_queued_events double-pop).
// Input bytes choose the events, their payloads and every guard result.
#include <boost/sml.hpp>
#include <cstddef>
#include <cstdint>
#include <queue>

namespace sml = boost::sml;

namespace {
struct e1 { int v; };
struct e2 { int v; };
struct e3 {};
struct e4 {};

struct ctx {
  const uint8_t *d;
  size_t n;
  size_t i;
  void *sm;
  bool (*re)(void *, uint8_t);
  int depth;
  long acc;
  uint8_t next() { return i < n ? d[i++] : 0; }
};

// Re-entrancy is capped at depth 4: unbounded recursion would only find stack exhaustion.
struct M {
  auto operator()() const {
    using namespace sml;
    auto reent = [](ctx &c) {
      if (c.depth < 4 && (c.next() & 1)) {
        ++c.depth;
        c.re(c.sm, c.next());
        --c.depth;
      }
    };
    auto g = [](ctx &c) { return (c.next() & 1) != 0; };
    auto acc1 = [](ctx &c, const e1 &e) { c.acc += e.v; };
    auto acc2 = [](ctx &c, const e2 &e) { c.acc ^= e.v; };
    // clang-format off
    return make_transition_table(
      *"a"_s + event<e1> / (acc1, process(e2{1})) = "b"_s,
      "a"_s + event<e3> / process(e1{3}),
      "a"_s + event<e4> [g] / reent = "c"_s,
      "b"_s + event<e2> / (acc2, reent) = "c"_s,
      "b"_s + event<e1> [g] / process(e3{}),
      "b"_s + event<e3> / (process(e4{}), reent),
      "c"_s + event<e1> [g] / (process(e3{}), process(e2{2})) = "a"_s,
      "c"_s + event<e2> / reent = "a"_s,
      "c"_s + event<e3> / reent = "b"_s,
      "c"_s + event<e4> [g] = X,
      "c"_s + event<e4> / (process(e1{4}), reent) = "a"_s
    );
    // clang-format on
  }
};

using sm_t = sml::sm<M, sml::process_queue<std::queue>>;

bool fire(void *p, uint8_t b) {
  auto &sm = *static_cast<sm_t *>(p);
  switch (b & 3) {
    case 0: return sm.process_event(e1{b >> 2});
    case 1: return sm.process_event(e2{b >> 2});
    case 2: return sm.process_event(e3{});
    default: return sm.process_event(e4{});
  }
}
}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *d, size_t n) {
  ctx c{d, n, 0, nullptr, fire, 0, 0};
  sm_t sm{c};
  c.sm = &sm;
  while (c.i < c.n && !sm.is(sml::X)) fire(&sm, c.next());
  return 0;
}
