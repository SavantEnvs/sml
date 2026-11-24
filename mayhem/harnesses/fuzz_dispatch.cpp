// Fuzz sml's runtime-id dispatch: sml::utility::make_dispatch_table maps a runtime
// event id (from the input) to a typed event, and the same transition table is
// instantiated under every dispatch policy (jump_table, branch_stm, switch_stm,
// fold_expr). All four machines get the same event sequence; they must agree on
// the handled result and the current state after every event (abort otherwise).
#include <boost/sml.hpp>
#include <boost/sml/utility/dispatch_table.hpp>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <utility>

namespace sml = boost::sml;

namespace {
struct runtime_event {
  int id;
  int data;
};
struct ev1 {
  static constexpr auto id = 1;
  explicit ev1(const runtime_event &e) : data(e.data) {}
  int data;
};
struct ev2 {
  static constexpr auto id = 2;
};
struct ev3 {
  static constexpr auto id = 3;
  ev3(const runtime_event &e, int i) : data(e.data + i) {}
  int data;
};
struct ev4 : sml::utility::id<4, 5, 7> {
  explicit ev4(const runtime_event &e) : data(e.data) {}
  int data;
};
struct ev8 {
  static constexpr auto id = 8;
};

constexpr int kMin = 1;
constexpr int kMax = 9;  // id 6 and 9 map to no event

const auto s_idle = sml::state<class s_idle>;
const auto s1 = sml::state<class s1>;
const auto s2 = sml::state<class s2>;
const auto s3 = sml::state<class s3>;

struct table {
  auto operator()() const {
    using namespace sml;
    auto g1 = [](const ev1 &e) { return (e.data & 1) != 0; };
    auto g3 = [](const ev3 &e) { return (e.data & 2) != 0; };
    auto g4 = [](const ev4 &e) { return (e.data & 4) != 0; };
    // clang-format off
    return make_transition_table(
      *s_idle + event<ev1> [g1] = s1,
      s_idle + event<ev1> = s2,
      s1 + event<ev2> = s2,
      s1 + event<ev3> [g3] = s3,
      s2 + event<ev3> = s1,
      s2 + event<ev4> [g4] = s3,
      s2 + event<ev4> = s_idle,
      s3 + event<ev8> = s_idle,
      s3 + event<ev1> [g1] = X,
      s3 + event<ev2> = s1
    );
    // clang-format on
  }
};

template <class Policy>
struct runner {
  sml::sm<table, sml::dispatch<Policy>> sm{};
  decltype(sml::utility::make_dispatch_table<runtime_event, kMin, kMax>(
      std::declval<sml::sm<table, sml::dispatch<Policy>> &>())) dispatch =
      sml::utility::make_dispatch_table<runtime_event, kMin, kMax>(sm);
  int state() const {
    return sm.is(s_idle) ? 0 : sm.is(s1) ? 1 : sm.is(s2) ? 2 : sm.is(s3) ? 3 : sm.is(sml::X) ? 4 : -1;
  }
};
}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *d, size_t n) {
  runner<sml::back::policies::jump_table> a;
  runner<sml::back::policies::branch_stm> b;
  runner<sml::back::policies::switch_stm> c;
  runner<sml::back::policies::fold_expr> f;
  for (size_t i = 0; i + 1 < n; i += 2) {
    // make_dispatch_table indexes table[id - kMin] unchecked: ids must be in [kMin, kMax].
    const runtime_event e{kMin + d[i] % (kMax - kMin + 1), static_cast<int>(d[i + 1])};
    const bool ra = a.dispatch(e, e.id);
    const bool rb = b.dispatch(e, e.id);
    const bool rc = c.dispatch(e, e.id);
    const bool rf = f.dispatch(e, e.id);
    const int sa = a.state();
    if (rb != ra || rc != ra || rf != ra || b.state() != sa || c.state() != sa || f.state() != sa || sa < 0) {
      std::abort();  // dispatch policies disagree on the same event sequence
    }
  }
  return 0;
}
