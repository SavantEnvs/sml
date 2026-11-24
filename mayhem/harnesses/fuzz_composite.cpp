// Fuzz sml's hierarchical runtime: sm<Top>::process_event on a machine with a
// composite sub-state-machine that has history (H), three orthogonal regions,
// on_entry/on_exit actions, anonymous (eventless) transitions, unexpected_event
// handlers and terminate states. Input bytes choose events and guard results;
// is() and visit_current_states() are queried after every event.
#include <boost/sml.hpp>
#include <cstddef>
#include <cstdint>
#include <queue>

namespace sml = boost::sml;

namespace {
struct e1 { int v; };
struct e2 {};
struct e3 {};
struct e4 {};
struct e5 {};
struct e6 {};

struct ctx {
  const uint8_t *d;
  size_t n;
  size_t i;
  unsigned long acc;
  uint8_t next() { return i < n ? d[i++] : 0; }
  void mix(unsigned long v) { acc = acc * 31 + v; }
};

const auto s_idle = sml::state<class s_idle>;
const auto s_a = sml::state<class s_a>;
const auto s_b = sml::state<class s_b>;
const auto s_c = sml::state<class s_c>;

struct leaf {
  auto operator()() const {
    using namespace sml;
    auto g = [](ctx &c) { return (c.next() & 1) != 0; };
    auto act = [](ctx &c) { c.mix(7); };
    // clang-format off
    return make_transition_table(
      *"l1"_s + event<e2> = "l2"_s,
      "l2"_s + event<e2> [g] = "l1"_s,
      "l2"_s + event<e6> / act = X,
      "l1"_s + unexpected_event<e3> / act
    );
    // clang-format on
  }
};

struct sub {
  auto operator()() const {
    using namespace sml;
    auto g = [](ctx &c) { return (c.next() & 1) != 0; };
    auto act = [](ctx &c) { c.mix(1); };
    auto acc1 = [](ctx &c, const e1 &e) { c.mix(static_cast<unsigned long>(e.v)); };
    // clang-format off
    return make_transition_table(
      s_idle(H) + event<e1> [g] / acc1 = s_a,
      s_a + event<e2> / act = s_b,
      s_a + sml::on_entry<_> / act,
      s_a + sml::on_exit<_> / act,
      s_b + event<e1> = s_idle,
      s_b + event<e5> = state<leaf>,
      state<leaf> + event<e5> [g] = s_c,
      s_c [g] = s_a,
      s_c + event<e2> = s_idle,
      s_b + event<e3> [g] = X,
      s_b + unexpected_event<e4> / act = s_idle,
      *"sr2a"_s + event<e2> [g] = "sr2b"_s,
      "sr2b"_s + event<e4> = "sr2a"_s,
      "sr2b"_s + unexpected_event<_> / act
    );
    // clang-format on
  }
};

struct top {
  auto operator()() const {
    using namespace sml;
    auto g = [](ctx &c) { return (c.next() & 1) != 0; };
    auto act = [](ctx &c) { c.mix(3); };
    // clang-format off
    return make_transition_table(
      *"t_idle"_s + event<e1> = state<sub>,
      state<sub> + event<e4> [g] = "t_x"_s,
      state<sub> + sml::on_entry<_> / act,
      state<sub> + sml::on_exit<_> / act,
      "t_x"_s + event<e2> = state<sub>,
      "t_x"_s + event<e5> [g] = "t_y"_s,
      "t_y"_s [g] / act = "t_z"_s,
      "t_z"_s + event<e3> = "t_idle"_s,
      "t_z"_s + event<e6> = state<sub>,
      *"r2_a"_s + event<e2> = "r2_b"_s,
      "r2_b"_s + event<e2> [g] = "r2_a"_s,
      "r2_b"_s + event<e6> [g] = X,
      "r2_a"_s + unexpected_event<_> / act,
      *"r3_a"_s + event<e3> [g] / act = "r3_b"_s,
      "r3_b"_s + event<e5> = "r3_a"_s,
      "r3_b"_s + event<e4> [g] / process(e2{}) = "r3_a"_s
    );
    // clang-format on
  }
};

using sm_t = sml::sm<top, sml::process_queue<std::queue>>;
}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *d, size_t n) {
  using namespace sml;
  ctx c{d, n, 0, 0};
  sm_t sm{c};
  while (c.i < c.n) {
    const uint8_t b = c.next();
    switch (b % 6) {
      case 0: sm.process_event(e1{b}); break;
      case 1: sm.process_event(e2{}); break;
      case 2: sm.process_event(e3{}); break;
      case 3: sm.process_event(e4{}); break;
      case 4: sm.process_event(e5{}); break;
      default: sm.process_event(e6{}); break;
    }
    c.mix(sm.is(sml::state<sub>) ? 11 : 13);
    c.mix(sm.is(sml::X, sml::X, sml::X) ? 17 : 19);
    c.mix(sm.is<decltype(sml::state<sub>)>(s_idle, "sr2a"_s) ? 23 : 29);
    sm.visit_current_states([&c](auto s) { c.mix(static_cast<unsigned char>(s.c_str()[0])); });
    sm.visit_current_states<decltype(sml::state<sub>)>([&c](auto s) { c.mix(static_cast<unsigned char>(s.c_str()[0])); });
  }
  return 0;
}
