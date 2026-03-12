#include "lockfree/SPSCQueue.h"  // 注意路径
#include <catch2/catch_all.hpp>
#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <random>

using namespace industrial;

TEST_CASE("SPSCQueue Basic Operations", "[spsc][basic]") {
    SPSCQueue<int> queue(4);
    
    SECTION("capacity and empty check") {
        REQUIRE(queue.capacity() == 3);
        REQUIRE(queue.empty());
        REQUIRE_FALSE(queue.isFull());
    }
    
    SECTION("push and pop") {
        queue.push(42);
        REQUIRE_FALSE(queue.empty());
        
        int val = 0;
        REQUIRE(queue.try_pop(val));
        REQUIRE(val == 42);
        REQUIRE(queue.empty());
    }
    
    SECTION("FIFO order") {
        queue.push(1);
        queue.push(2);
        queue.push(3);
        
        int val;
        REQUIRE(queue.try_pop(val)); REQUIRE(val == 1);
        REQUIRE(queue.try_pop(val)); REQUIRE(val == 2);
        REQUIRE(queue.try_pop(val)); REQUIRE(val == 3);
        REQUIRE_FALSE(queue.try_pop(val));
    }
}

TEST_CASE("SPSCQueue Multi-Threaded", "[spsc][mt][.]") {
    constexpr int OPS = 1'000'000;
    SPSCQueue<int> queue(65536);
    
    std::atomic<long long> sum_produced{0};
    std::atomic<long long> sum_consumed{0};
    std::atomic<int> produced{0};
    std::atomic<int> consumed{0};
    std::atomic<bool> go{false};
    
    // 忙等 push（避免数据丢失）
    auto blocking_push = [&](int val) {
        while (true) {
            // 注意：这里需要访问私有成员，有两种方案：
            
            // 方案1：在 SPSCQueue.h 中添加 friend struct SPSCTestAccess;
            // 方案2：使用 public 的 push 并配合重试逻辑（推荐简单测试用）
            
            // 简单方案：直接用 try_pop 的风格测试，不测试满队列情况
            // 或者扩大队列避免满
            
            queue.push(val);  // 如果队列够大，不会丢数据
            return;
        }
    };
    
    std::thread producer([&]() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(1, 100);
        
        while (!go.load()) std::this_thread::yield();
        
        for (int i = 0; i < OPS; ++i) {
            int val = dis(gen);
            // 队列够大时直接 push，不会满
            queue.push(val);
            sum_produced += val;
            produced++;
        }
    });
    
    std::thread consumer([&]() {
        while (!go.load()) std::this_thread::yield();
        
        int val;
        while (consumed < OPS) {
            if (queue.try_pop(val)) {
                sum_consumed += val;
                consumed++;
            } else {
                std::this_thread::yield();
            }
        }
    });
    
    auto start = std::chrono::high_resolution_clock::now();
    go = true;
    
    producer.join();
    consumer.join();
    
    auto end = std::chrono::high_resolution_clock::now();
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    
    std::cout << "\n[SPSCQueue MT] " << OPS << " items, " 
              << us << " us, " << (OPS * 2.0 / us) << " M ops/sec\n"
              << "  Produced: " << produced << ", Consumed: " << consumed << "\n"
              << "  Checksum: " << (sum_produced == sum_consumed ? "PASS" : "FAIL") << std::endl;
    
    REQUIRE(produced == OPS);
    REQUIRE(consumed == OPS);
    REQUIRE(sum_produced == sum_consumed);
    
    std::cout << ((produced == OPS && consumed == OPS && sum_produced == sum_consumed) ? 0 : 1) << std::endl;
}