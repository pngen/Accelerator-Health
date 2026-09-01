// Copyright 2026 Summon Software Labs
// Licensed under the Apache License, Version 2.0 (the "License"); see LICENSE.
#pragma once
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace testfw {
struct TestCase { const char* name; std::function<void()> fn; };
inline std::vector<TestCase>& registry() { static std::vector<TestCase> r; return r; }
struct Registrar {
  Registrar(const char* n, std::function<void()> f) { registry().push_back({n, std::move(f)}); }
};
inline int& failureCount() { static int n = 0; return n; }
inline void reportFailure(const char* file, int line, const std::string& msg) {
  ++failureCount();
  std::printf("  FAIL %s:%d  %s\n", file, line, msg.c_str());
}
inline int runAll(const char* suite) {
  int total = 0, failed = 0;
  for (auto& t : registry()) {
    ++total;
    const int before = failureCount();
    std::printf("[ RUN ] %s.%s\n", suite, t.name);
    t.fn();
    if (failureCount() > before) { ++failed; std::printf("[FAIL ] %s.%s\n", suite, t.name); }
    else { std::printf("[ OK  ] %s.%s\n", suite, t.name); }
  }
  std::printf("==== %s: %d/%d passed, %d failed ====\n", suite, total - failed, total, failed);
  return failed;
}
}  // namespace testfw

#define AH_CONCAT2(a,b) a##b
#define AH_CONCAT(a,b) AH_CONCAT2(a,b)

#define AH_TEST(name)                                          \
  static void AH_CONCAT(test_fn_, name)();                     \
  static ::testfw::Registrar AH_CONCAT(reg_, name)(#name,      \
      &AH_CONCAT(test_fn_, name));                             \
  static void AH_CONCAT(test_fn_, name)()

#define CHECK(cond)                                            \
  do { if (!(cond)) ::testfw::reportFailure(__FILE__, __LINE__, #cond); } while (0)

#define CHECK_EQ(a, b)                                         \
  do { if (!((a) == (b))) ::testfw::reportFailure(__FILE__, __LINE__,    \
      std::string(#a " == " #b)); } while (0)

#define CHECK_MSG(cond, msg)                                   \
  do { if (!(cond)) ::testfw::reportFailure(__FILE__, __LINE__, msg); } while (0)
