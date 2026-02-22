#ifndef INDUSTRIAL_SPINLOCK_HPP
#define INDUSTRIAL_SPINLOCK_HPP

#include <new>
#include <atomic>
#include <thread>
#include <chrono>
#include <cstdint>
#include <mutex>

// platform check
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    #define SPINLOCK_X86
    #include <xmmintrin.h>
#elif defined(__aarch64__) || defined(__arm__)
    #define SPINLOCK_ARM
#endif

namespace industrial{

#ifdef __cpp_lib_hardware_interference_size
    using std::hardware_constructive_interference_size;
#else
    constexpr size_t hardware_constructive_interference_size = 64;
#endif

class alignas(hardware_constructive_interference_size) SpinLock
{
public:
    SpinLock() : locked_(false) { }
    
    SpinLock(const SpinLock&) = delete;
    SpinLock& operator=(const SpinLock&) = delete;
    SpinLock(SpinLock&&) = delete;
    SpinLock& operator=(SpinLock&&) = delete;

    void lock() noexcept{
        if (!locked_.exchange(true, std::memory_order_acquire))
            return;
        lock_contented();
    }

    bool try_lock() noexcept{
        if (locked_.load(std::memory_order_relaxed))
            return false;
        return !locked_.exchange(true, std::memory_order_acquire);
    }

    void unlock() noexcept {
        locked_.store(false, std::memory_order_release);
    }

private:
    void lock_contented() noexcept{ 
        uint32_t spin_count = 0;
        constexpr uint32_t SHORT_SPIN = 512;
        constexpr uint32_t MEDIUM_SPIN = 4096;
        while (true){
            while (locked_.load(std::memory_order_relaxed)){
                if (spin_count<SHORT_SPIN){
                    cpu_relax();
                    ++spin_count;
                }else if (spin_count<MEDIUM_SPIN){
                    uint32_t backoff = (spin_count - SHORT_SPIN) >> 5;
                    uint32_t iterations = 1u << backoff? backoff < 8: 8;
                    for (uint32_t i = 0; i < iterations; ++i) {
                        cpu_relax();
                    }
                    ++spin_count;
                }else {
                    std::this_thread::yield();
                    spin_count = SHORT_SPIN-256;
                }
            }
            if (!locked_.exchange(true, std::memory_order_acquire))
                return;
        }
    }

    static inline void cpu_relax() noexcept{
        #ifdef SPINLOCK_X86
            ::_mm_pause();
        #elif defined(SPINLOCK_ARM)
            __asm__ volatile("yield" ::: "memory");
        #else 
            std::this_thread::yield();
        #endif
    }
    alignas(hardware_constructive_interference_size) std::atomic<bool> locked_;
    char padding_[hardware_constructive_interference_size - sizeof(std::atomic<bool>)];
};

using LGuard = std::lock_guard<SpinLock>;
} //industrial


#endif // INDUSTRIAL_SPINLOCK_HPP