#pragma once
#include <atomic>
#include <thread>

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

unsigned const max_hazard_pointers = 100;
struct alignas(hardware_destructive_interference_size) HazardPointer {
  std::atomic<std::thread::id> id;
  std::atomic<void *> pointer;
  char padding[hardware_destructive_interference_size -
               sizeof(std::atomic<std::thread::id>) -
               sizeof(std::atomic<void *>)];
};
extern HazardPointer hazard_pointers[max_hazard_pointers];
class HPOwner {
private:
  HazardPointer *hp;

public:
  HPOwner(const HPOwner &) = delete;
  HPOwner operator=(const HPOwner &) = delete;

  HPOwner() : hp(nullptr) {
    for (unsigned i = 0; i < max_hazard_pointers; i++) {
      std::thread::id old_id;
      if (hazard_pointers[i].id.compare_exchange_strong(
              old_id, std::this_thread::get_id())) {
        hp = &hazard_pointers[i];
        break;
      }
    }
    if (!hp) {
      throw std::runtime_error("No hazard pointers available");
    }
  };
  ~HPOwner() {
    hp->pointer.store(nullptr);
    hp->id.store(std::thread::id());
  };
  std::atomic<void *> &get_pointer() { return hp->pointer; }
};

inline std::atomic<void *> &get_hazard_pointer_for_current_thread() {
  thread_local static HPOwner hazard;
  return hazard.get_pointer();
}

inline bool outstanding_hazard_pointers_for(void *p) {
  for (unsigned i = 0; i < max_hazard_pointers; i++) {
    if (hazard_pointers[i].pointer.load() == p) {
      return true;
    }
  }
  return false;
}
} // namespace industrial
