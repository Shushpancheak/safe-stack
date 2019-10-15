#include <gtest/gtest.h>
#include <iostream>
#include "shush-stack.hpp"

using namespace shush::stack;

TEST(DYNAMIC, warmup) {
  try {
    SafeStack<int> stack_0;
    SafeStack<int> stack_1;

    stack_0.Push(1);
    stack_1.Push(4);

    ASSERT_EQ(stack_0.GetCurSize(), stack_1.GetCurSize());

    int a = stack_0.Pop();
    int b = stack_1.Pop();

    ASSERT_EQ(a, 1);
    ASSERT_EQ(b, 4);

    ASSERT_EQ(stack_0.GetCurSize(), 0);
    ASSERT_EQ(stack_1.GetCurSize(), 0);
  } catch (shush::dump::Dump& dump) {
    shush::dump::HandleFinalDump(dump);
  }
}

TEST(DYNAMIC, stress_1) {
  try {
    SafeStack<uint64_t> stack;
    for (size_t i = 0; i < 1000; ++i) {
      stack.Push(i);
      ASSERT_EQ(stack.GetCurSize(), i + 1);
      size_t a = stack.Pop();
      ASSERT_EQ(a, i);
      stack.Push(i);
    }
  } catch (shush::dump::Dump& dump) {
    shush::dump::HandleFinalDump(dump);
  }
}

TEST(DYNAMIC, stress_2) {
  SafeStack<uint8_t> stack_8;
  SafeStack<uint16_t> stack_16;
  SafeStack<uint32_t> stack_32;
  SafeStack<uint64_t> stack_64;

  for (size_t i = 0; i < 200; ++i) {
    stack_8.Push(i);
    stack_16.Push(i);
    stack_32.Push(i);
    stack_64.Push(i);
  }

  for (size_t i = 0; i < 200; ++i) {
    size_t a = stack_8.Pop();
    size_t b = stack_16.Pop();
    ASSERT_EQ(a, b);
    b = stack_32.Pop();
    ASSERT_EQ(a, b);
    b = stack_64.Pop();
    ASSERT_EQ(a, b);
  }
}

struct MyClass {
  uint64_t foo;
  uint64_t bar;
  uint32_t boo;
  uint64_t far;
};

std::string to_string(const MyClass mc) {
  return "{" + std::to_string(mc.foo) + ", "
             + std::to_string(mc.bar) + ", "
             + std::to_string(mc.boo) + ", "
             + std::to_string(mc.far) + "}";
}

TEST(DYNAMIC, my_class) {
  SafeStack<MyClass> stack;

  for (size_t i = 0; i < 100; ++i) {
    stack.Push({i, i + 1, static_cast<uint32_t>(i + 2), i + 3});
    ASSERT_EQ(stack.GetCurSize(), i + 1);
    auto a = stack.Pop();
    ASSERT_EQ(a.foo, i);
    ASSERT_EQ(a.bar, i + 1);
    ASSERT_EQ(a.boo, static_cast<uint32_t>(i + 2));
    ASSERT_EQ(a.far, i + 3);
    stack.Push({i, i + 1, static_cast<uint32_t>(i + 2), i + 3});
  }
}

TEST(STATIC, stress_1) {
  SafeStackStatic<uint64_t, 1000> stack;
  for (size_t i = 0; i < 1000; ++i) {
    stack.Push(i);
    ASSERT_EQ(stack.GetCurSize(), i + 1);
    size_t a = stack.Pop();
    ASSERT_EQ(a, i);
    stack.Push(i);
  }
}

TEST(DYNAMIC, intrusion) {
  try {
    SafeStack<int> stack;
    uint64_t* canary_pos = *reinterpret_cast<uint64_t**>(&stack);

    *canary_pos = 0;
    stack.Push(0);
  } catch(shush::dump::Dump& dump) {
    shush::dump::HandleFinalDump(dump);
  }
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}