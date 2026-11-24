// Leak detection is off for every fuzz/standalone binary (leaks are not the bug class fuzzed here);
// ASan's memory-error checks and UBSan stay fully active.
extern "C" int __lsan_is_turned_off() { return 1; }
