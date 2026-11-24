// Force-included (-include) into every upstream test TU by mayhem/build.sh. Prints a marker
// when the test program reaches normal process exit, so mayhem/test.sh can tell a test binary
// that actually ran its upstream `expect(...)` checks from one that was turned into a no-op.
#pragma once
#include <cstdio>
namespace sml_mayhem_marker {
struct marker {
  ~marker() { std::printf("SML_TEST_RAN\n"); std::fflush(stdout); }
};
static marker instance;
}  // namespace sml_mayhem_marker
