#include <atomic>
#include <chrono>
#include <optional>
#include <random>
using namespace std::chrono;

struct LockFreeExchanger {
public:
  enum class State : int { EMPTY, WAITING, BUSY };
  using enum State;

  std::optional<void *> exchange(void *my_item, milliseconds timeout) {
    auto deadline = steady_clock::now() + timeout;
    State curr_state;
    StatePtr rt_ptr = slot.load();
    StatePtr my_ptr;
    while (true) {
      if (steady_clock::now() > deadline)
        throw std::runtime_error("timed out");
      curr_state = rt_ptr.state();
      switch (curr_state) {
      case EMPTY:
        my_ptr = StatePtr(my_item, WAITING);
        if (slot.compare_exchange_strong(rt_ptr, my_ptr)) {
          while (steady_clock::now() < deadline) {
            rt_ptr = slot.load();
            if (rt_ptr.state() == BUSY) {
              slot.store(StatePtr());
              return rt_ptr.ptr();
            }
          }
          if (slot.compare_exchange_strong(my_ptr, StatePtr())) {
            throw std::runtime_error("timed out");
          } else {
            slot.store(StatePtr());
            return my_ptr.ptr();
          }
        }
        break;
      case WAITING:
        my_ptr = StatePtr(my_item, BUSY);
        if (slot.compare_exchange_strong(rt_ptr, my_ptr))
          return rt_ptr.ptr();
        break;
      case BUSY:
        break;
      default:
        break;
      }
    }
  };

private:
  struct StatePtr {
    uintptr_t data;

    static constexpr uintptr_t STATE_MASK = 0x3; // 2 bits for state
    static constexpr uintptr_t PTR_MASK = ~STATE_MASK;

    StatePtr(void *p = nullptr, State s = EMPTY) {
      data = (reinterpret_cast<uintptr_t>(p) & PTR_MASK) |
             static_cast<uintptr_t>(s);
    }
    void *ptr() const { return reinterpret_cast<void *>(data & PTR_MASK); }
    State state() const { return static_cast<State>(data & STATE_MASK); }
  };
  std::atomic<StatePtr> slot;
};

struct EliminationVector {

private:
  static inline milliseconds duration = 50ms;
};