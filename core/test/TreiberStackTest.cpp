// test/test_treiber_stack.cpp
#include "lockfree/TreiberStack.h"
#include <atomic>
#include <catch2/catch_all.hpp>
#include <chrono>
#include <iostream>
#include <random>
#include <thread>
#include <vector>


using namespace industrial;
using namespace std::chrono;

// 测试辅助：带超时的 pop（防止测试死锁）
template <typename T>
std::shared_ptr<T> try_pop_for(TreiberStack<T> &stack,
                               milliseconds timeout = 5s) {
  auto start = steady_clock::now();
  while (steady_clock::now() - start < timeout) {
    auto val = stack.pop();
    if (val)
      return val;
    std::this_thread::yield();
  }
  return nullptr;
}

TEST_CASE("TreiberStack: 单线程基本操作", "[stack][single_thread]") {
  TreiberStack<int> stack;

  SECTION("空栈返回 nullptr") { REQUIRE(stack.pop() == nullptr); }

  SECTION("顺序 push/pop") {
    for (int i = 0; i < 100; ++i) {
      stack.push(i);
    }

    for (int i = 99; i >= 0; --i) {
      auto val = stack.pop();
      REQUIRE(val != nullptr);
      REQUIRE(*val == i);
    }

    REQUIRE(stack.pop() == nullptr);
  }

  SECTION("交替 push/pop（ABA 模拟）") {
    stack.push(1); // A
    stack.push(2); // B

    auto b = stack.pop();
    REQUIRE(*b == 2);

    auto a = stack.pop();
    REQUIRE(*a == 1);

    stack.push(1); // A 再次入栈

    auto a2 = stack.pop();
    REQUIRE(*a2 == 1);
    REQUIRE(stack.pop() == nullptr);
  }
}

TEST_CASE("TreiberStack: 单生产者单消费者 (SPSC)",
          "[stack][multi_thread][spsc]") {
  TreiberStack<int> stack;
  const int iterations = 10'000;
  std::atomic<int> consumed{0};

  std::thread producer([&]() {
    for (int i = 0; i < iterations; ++i) {
      stack.push(i);
    }
  });

  std::thread consumer([&]() {
    int count = 0;
    while (count < iterations) {
      auto val = try_pop_for(stack);
      if (val) {
        REQUIRE(*val >= 0);
        REQUIRE(*val < iterations);
        ++count;
      }
    }
    consumed = count;
  });

  producer.join();
  consumer.join();

  REQUIRE(consumed == iterations);
  REQUIRE(stack.pop() == nullptr);
}

TEST_CASE("TreiberStack: 多生产者多消费者压力测试 (MPMC)",
          "[stack][multi_thread][mpmc][stress]") {
  TreiberStack<int> stack;
  const int num_producers = 4;
  const int num_consumers = 4;
  const int items_per_producer = 1000;

  std::atomic<int> total_consumed{0};
  std::atomic<int> total_pushed{0};
  std::atomic<bool> done{false};

  std::vector<std::thread> producers;
  std::vector<std::thread> consumers;

  // 启动生产者
  for (int t = 0; t < num_producers; ++t) {
    producers.emplace_back([&, t]() {
      for (int i = 0; i < items_per_producer; ++i) {
        stack.push(t * items_per_producer + i);
      }
      total_pushed += items_per_producer;
    });
  }

  // 启动消费者
  for (int t = 0; t < num_consumers; ++t) {
    consumers.emplace_back([&]() {
      int local_count = 0;
      while (true) {
        auto val = stack.pop(); // 直接 pop，不绕弯
        if (val) {
          ++local_count;
        } else if (done.load() && !val) {
          // 生产者完成且栈空
          break;
        }
        if (!val)
          std::this_thread::yield();
      }
      total_consumed += local_count;
    });
  }

  for (auto &t : producers)
    t.join();
  done = true;
  for (auto &t : consumers)
    t.join();

  REQUIRE(total_consumed == num_producers * items_per_producer);
  REQUIRE(stack.pop() == nullptr);
}

TEST_CASE("TreiberStack: 复杂数据类型支持", "[stack][complex_type]") {
  struct TestStruct {
    std::string str;
    int value;
    TestStruct(const std::string &s, int v) : str(s), value(v) {}
  };

  TreiberStack<TestStruct> stack;

  std::thread t1([&]() {
    for (int i = 0; i < 100; ++i) {
      stack.push(TestStruct("producer1", i));
    }
  });

  std::thread t2([&]() {
    for (int i = 0; i < 100; ++i) {
      stack.push(TestStruct("producer2", i));
    }
  });

  std::thread t3([&]() {
    int count = 0;
    while (count < 200) {
      auto val = try_pop_for(stack);
      if (val) {
        REQUIRE(!val->str.empty());
        ++count;
      }
    }
  });

  t1.join();
  t2.join();
  t3.join();

  // 清空剩余
  while (stack.pop())
    ;
}

TEST_CASE("TreiberStack: 内存安全与生命周期", "[stack][memory][stress]") {
  // 重复创建销毁栈，检测 use-after-free 和内存泄漏
  for (int round = 0; round < 50; ++round) {
    TreiberStack<int> stack;

    std::thread t1([&]() {
      for (int i = 0; i < 500; ++i)
        stack.push(i);
    });

    std::thread t2([&]() {
      int count = 0;
      while (count < 250) {
        if (try_pop_for(stack))
          ++count;
      }
    });

    std::thread t3([&]() {
      int count = 0;
      while (count < 250) {
        if (try_pop_for(stack))
          ++count;
      }
    });

    t1.join();
    t2.join();
    t3.join();

    // 栈析构时自动清理剩余节点
  }
}

TEST_CASE("TreiberStack: Hazard Pointer 槽位压力测试",
          "[stack][hazard_pointer][stress]") {
  TreiberStack<int> stack;
  const int num_threads = 20; // 假设 HP 槽位 >= 20

  std::vector<std::thread> threads;
  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back([&]() {
      for (int j = 0; j < 100; ++j) {
        stack.push(j);
        auto val = stack.pop();
        // 短暂持有增加 HP 竞争
        if (val)
          std::this_thread::sleep_for(microseconds(10));
      }
    });
  }

  for (auto &t : threads)
    t.join();

  // 确保没有内存泄漏：清空栈
  while (stack.pop())
    ;
}

TEST_CASE("TreiberStack: 性能基准", "[stack][benchmark][!mayfail]") {
  TreiberStack<int> stack;
  const int iterations = 100'000;

  auto start = steady_clock::now();

  std::thread t1([&]() {
    for (int i = 0; i < iterations; ++i)
      stack.push(i);
  });

  std::thread t2([&]() {
    int count = 0;
    while (count < iterations) {
      if (stack.pop())
        ++count;
    }
  });

  t1.join();
  t2.join();

  auto end = steady_clock::now();
  auto duration = duration_cast<milliseconds>(end - start).count();

  std::cout << "\n[Perf] " << iterations << " ops in " << duration << "ms"
            << " (" << (iterations * 1000.0 / duration) << " ops/sec)";

  // 性能断言：应该在合理时间内完成（10秒）
  REQUIRE(duration < 10000);
}

// 可选：使用 Catch2 的 BENCHMARK 宏（如果启用）
#ifdef CATCH_CONFIG_ENABLE_BENCHMARKING
TEST_CASE("TreiberStack: 微基准", "[!benchmark]") {
  TreiberStack<int> stack;

  BENCHMARK("Single-threaded push") {
    stack.push(42);
    return stack.pop();
  };

  BENCHMARK_ADVANCED("Multi-threaded contended")
  (Catch::Benchmark::Chronometer meter) {
    std::vector<std::thread> threads;
    meter.measure([&](int i) {
      if (i == 0) {
        // 设置
        for (int j = 0; j < 100; ++j)
          stack.push(j);
      }
      // 测量
      auto val = stack.pop();
      if (val)
        stack.push(*val);
    });
  };
}
#endif