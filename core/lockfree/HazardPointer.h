#pragma once
#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <thread>
#include <vector>

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

class HazardPointerDomain;
class HazardPointerHolder;
struct alignas(hardware_destructive_interference_size) HazardPointerSlot {
  std::atomic<void *> ptr{nullptr};
  std::atomic<std::thread::id> owner;

  bool tryAcquire() {
    std::thread::id null_id;
    return owner.compare_exchange_strong(null_id, std::this_thread::get_id(),
                                         std::memory_order_relaxed);
  }

  void release() {
    ptr.store(nullptr, std::memory_order_release);
    owner.store(std::thread::id(), std::memory_order_release);
  }
};

// struct alignas(hardware_destructive_interference_size) HazardPointer {
//   std::atomic<std::thread::id> id;
//   std::atomic<void *> pointer;
//   char padding[hardware_destructive_interference_size -
//                sizeof(std::atomic<std::thread::id>) -
//                sizeof(std::atomic<void *>)];
// };
// extern HazardPointer hazard_pointers[max_hazard_pointers];
class HazardPointerDomain {
public:
  static constexpr size_t DEFAULT_SLOT_COUNT = 100;
  explicit HazardPointerDomain(size_t slot_count = DEFAULT_SLOT_COUNT);
  ~HazardPointerDomain();
  HazardPointerDomain(const HazardPointerDomain &) = delete;
  HazardPointerDomain operator=(const HazardPointerDomain &) = delete;

  HazardPointerSlot *acquireSlot();
  void releaseSlot(HazardPointerSlot *slot);
  std::vector<void *> getProtectedPointers() const;
  void scanAndReclaim();
  void setThreshold(size_t threshold) { reclaim_threshold_ = threshold; }
  template <typename T> void reclaim_later(T *node) {
    addToRetireList(
        new RetiredNode(node, [](void *p) { delete static_cast<T *>(p); }));
  }

private:
  struct RetiredNode {
    void *data;
    std::function<void(void *)> deleter;
    RetiredNode *next;
    RetiredNode(void *p, std::function<void(void *)> func)
        : data(p), deleter(func), next(nullptr) {}
  };
  void addToRetireList(RetiredNode *node);
  std::vector<std::unique_ptr<HazardPointerSlot>> slots_;
  std::atomic<size_t> slot_index_{0};
  std::atomic<RetiredNode *> retired_list_{nullptr};
  size_t reclaim_threshold_ = 100;
  std::vector<RetiredNode *> reclaim_buffer_;
};

class HazardPointerHolder {
public:
  explicit HazardPointerHolder(HazardPointerDomain *domain = nullptr);
  ~HazardPointerHolder();
  HazardPointerHolder(const HazardPointerHolder &) = delete;
  HazardPointerHolder &operator=(const HazardPointerHolder &) = delete;
  HazardPointerHolder(HazardPointerHolder &&other) noexcept;
  HazardPointerHolder &operator=(HazardPointerHolder &&other) noexcept;

  void *protect(const std::atomic<void *> &src);

  template <typename T> T *protect(const std::atomic<T *> &src) {
    return static_cast<T *>(
        protect(reinterpret_cast<const std::atomic<void *> &>(src)));
  }

  void set(void *ptr);
  void clear();

  void *get() const;

private:
  HazardPointerDomain *domain_;
  HazardPointerSlot *slot_;
};

HazardPointerDomain &defaultHazardPointerDomain();

inline HazardPointerHolder makeHazardPointer() {
  return HazardPointerHolder(&defaultHazardPointerDomain());
}

} // namespace industrial
