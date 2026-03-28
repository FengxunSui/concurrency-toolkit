#ifndef INDUSTRIAL_MCSSpinLock_HPP
#define INDUSTRIAL_MCSSpinLock_HPP

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <new>
#include <thread>

// platform check
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) ||             \
    defined(_M_IX86)
#define MCSSpinLock_X86
#include <xmmintrin.h>
#elif defined(__aarch64__) || defined(__arm__)
#define MCSSpinLock_ARM
#endif

namespace industrial {
#ifdef __cpp_lib_hardware_interference_size
using std::hardware_destructive_interference_size;
#else
#if defined(__x86_64__) || defined(_M_X64)
constexpr size_t hardware_destructive_interference_size = 128;
#else
constexpr size_t hardware_destructive_interference_size = 64;
#endif
#endif

struct alignas(hardware_destructive_interference_size) MCSNode {
  std::atomic<bool> locked{false};
  std::atomic<MCSNode *> next{nullptr};
  char padding[hardware_destructive_interference_size - sizeof(locked) -
               sizeof(next)];
};

class MCSSpinLock {
public:
  MCSSpinLock() : tail_{nullptr} {}

  MCSSpinLock(const MCSSpinLock &) = delete;
  MCSSpinLock &operator=(const MCSSpinLock &) = delete;
  MCSSpinLock(MCSSpinLock &&) = delete;
  MCSSpinLock &operator=(MCSSpinLock &&) = delete;

  void lock() noexcept {
    MCSNode* node = myNode_.get();
    node->locked.store(true, std::memory_order_relaxed);
    node->next.store(nullptr, std::memory_order_relaxed);
    MCSNode *prev = tail_.exchange(node, std::memory_order_acq_rel);
    if (prev) {
      prev->next.store(node, std::memory_order_release);
      while (node->locked.load(std::memory_order_acquire)) {
        cpu_relax();
      }
    }
  }

  void unlock() noexcept {
    MCSNode* node = myNode_.get(); 
    MCSNode *next = node->next.load(std::memory_order_acquire);
    if (!next) {
      MCSNode *snapshot = node;
      if (tail_.compare_exchange_strong(snapshot, nullptr,
                                        std::memory_order_release,
                                        std::memory_order_relaxed))
        return;
      while ((next = node->next.load(std::memory_order_acquire)) == nullptr) {
        cpu_relax();
      }
    }
    next->locked.store(false, std::memory_order_release);
  }

private:
  static inline void cpu_relax() noexcept {
#ifdef MCSSpinLock_X86
    ::_mm_pause();
#elif defined(MCSSpinLock_ARM)
    asm volatile("yield" ::: "memory");
#else
    asm volatile("" ::: "memory");
#endif
  }
  class MyNode {
  public:
    // 延迟初始化，第一次使用时分配
    MCSNode *get() {
      MCSNode *node = ptr_.load(std::memory_order_acquire);
      if (node == nullptr) {
        node = new MCSNode(); // 首次使用，堆分配
        MCSNode *expected = nullptr;
        if (!ptr_.compare_exchange_strong(expected, node,
                                          std::memory_order_release,
                                          std::memory_order_relaxed)) {
          delete node; // 竞争失败，用别人创建的
          node = ptr_.load(std::memory_order_acquire);
        }
      }
      return node;
    }

    ~MyNode() { delete ptr_.load(std::memory_order_relaxed); }

  private:
    std::atomic<MCSNode *> ptr_{nullptr};
  };

  // thread_local 存储MyNode（管理MCSNode生命周期）
  static thread_local MyNode myNode_;

  alignas(hardware_destructive_interference_size) std::atomic<MCSNode *> tail_{
      nullptr};
};

inline thread_local MCSSpinLock::MyNode MCSSpinLock::myNode_;

class MCSLockGuard {
public:
    explicit MCSLockGuard(MCSSpinLock& lock) : lock_(lock) { lock_.lock(); }
    ~MCSLockGuard() { lock_.unlock(); }
    MCSLockGuard(const MCSLockGuard&) = delete;
    MCSLockGuard& operator=(const MCSLockGuard&) = delete;
private:
    MCSSpinLock& lock_;
};

} // namespace industrial

#endif // INDUSTRIAL_MCSSpinLock_HPP