#ifndef INDUSTRIAL_MCSSpinLock_HPP
#define INDUSTRIAL_MCSSpinLock_HPP

#include <new>
#include <atomic>
#include <thread>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>

// platform check
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    #define MCSSpinLock_X86
    #include <xmmintrin.h>
#elif defined(__aarch64__) || defined(__arm__)
    #define MCSSpinLock_ARM
#endif

namespace industrial{
#ifdef __cpp_lib_hardware_interference_size
using std::hardware_constructive_interference_size;
#else
    constexpr size_t hardware_constructive_interference_size = 64;
#endif

struct alignas(hardware_constructive_interference_size) MCSNode{
    alignas(hardware_constructive_interference_size) std::atomic<uint32_t> locked{true}; 
    alignas(hardware_constructive_interference_size) std::atomic<MCSNode*> next{nullptr};
};


class MCSSpinLock{
public:
    MCSSpinLock() : tail_{nullptr}{}

    MCSSpinLock(const MCSSpinLock&) = delete;
    MCSSpinLock& operator=(const MCSSpinLock&) = delete;
    MCSSpinLock(MCSSpinLock&&) = delete;
    MCSSpinLock& operator=(MCSSpinLock&&) = delete;

    void lock(MCSNode* node) noexcept{
        node->locked.store(true, std::memory_order_relaxed);
        node->next.store(nullptr, std::memory_order_relaxed);
        MCSNode* prev = tail_.exchange(node, std::memory_order_acq_rel);
        if (prev != nullptr){
            prev->next.store(node, std::memory_order_release);
            while (node->locked.load(std::memory_order_acquire)){
                cpu_relax();
            }
        }
    }

    void unlock(MCSNode* node) noexcept{
        MCSNode* next = node->next.load(std::memory_order_acquire);
        if (next == nullptr){ 
            MCSNode* snapshot = node;
            if(tail_.compare_exchange_strong(snapshot, nullptr, std::memory_order_release, std::memory_order_relaxed))
                return;
            while((next = node->next.load(std::memory_order_acquire)) == nullptr){
                cpu_relax();
            }
        }
        next->locked.store(false, std::memory_order_release);
    }

private:
    

    static inline void cpu_relax() noexcept{
        #ifdef MCSSpinLock_X86
            ::_mm_pause();
        #elif defined(MCSSpinLock_ARM)
            asm volatile("yield"::: "memory");
        #else 
            asm volatile("" ::: "memory");
        #endif
    }
    alignas(hardware_constructive_interference_size) std::atomic<MCSNode*> tail_{nullptr}; 
};

class MCSLockGuard {
public:
    explicit MCSLockGuard(MCSSpinLock& lock) 
        : lock_(lock), node_(std::make_unique<MCSNode>()) { 
        lock_.lock(node_.get());
    }
    
    // 或者：栈分配 + 侵入式链表（需要小心）
    
    ~MCSLockGuard() {
        if (node_) {
            lock_.unlock(node_.get());
        }
    }
    
    MCSLockGuard(MCSLockGuard&&) = default;
    MCSLockGuard& operator=(MCSLockGuard&&) = default;

private:
    MCSSpinLock& lock_;
    std::unique_ptr<MCSNode> node_;
};

}//industrial

#endif // INDUSTRIAL_MCSSpinLock_HPP