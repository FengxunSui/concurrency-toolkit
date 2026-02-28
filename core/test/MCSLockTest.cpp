#include "spinlock/MCSSpinLock.h"
#include <catch2/catch_all.hpp>
#include <iostream>
#include <vector>
#include <thread>

TEST_CASE("MCSLockGuard RAII", "[guard]"){
    industrial::MCSSpinLock lock;

    // ========== test SpinLock + std::lock_guard ==========
    {
        industrial::MCSLockGuard guard2(lock);
        std::cout << "MCSSpinLockGuard alias works!\n";
    }
    
    uint64_t counter = 0;
    constexpr int NUM_THREADS = 16;
    constexpr int OPS = 1000000;
    
    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back([&] {
            for (int j = 0; j < OPS; ++j) {
                industrial::MCSLockGuard guard(lock);
                ++counter;
            }
        });
    }
    
    for (auto& t : threads) t.join();
    
    std::cout << "Final counter: " << counter 
              << " (expected: " << NUM_THREADS * OPS << ")\n"<< std::endl;
    
    std::cout << ((counter == NUM_THREADS * OPS) ? 0 : 1)<< std::endl;
    return;
}