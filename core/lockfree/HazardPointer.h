#pragma once
#include <atomic>
#include <thread>

unsigned const max_hazard_pointers = 100;
struct HazardPointer {
  std::atomic<std::thread::id> id;
  std::atomic<void *> pointer;
};
extern HazardPointer hazard_pointers[max_hazard_pointers];
class hp_owner {
private:
  HazardPointer *hp;

public:
  hp_owner(const hp_owner &) = delete;
  hp_owner operator=(const hp_owner &) = delete;

  hp_owner() : hp(nullptr) {
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
  ~hp_owner() {
    hp->pointer.store(nullptr);
    hp->id.store(std::thread::id());
  };
  std::atomic<void *> &get_pointer() { return hp->pointer; }
};

inline std::atomic<void *> &get_hazrd_pointer_for_current_thread() {
  thread_local static hp_owner hazard;
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
