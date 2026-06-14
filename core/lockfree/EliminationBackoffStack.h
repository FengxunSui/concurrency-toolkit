#include "LockFreeExchanger.h"
#include "TreiberStack.h"
#include <thread>
namespace industrial {

class RangePolicy {
public:
  RangePolicy(size_t initialRange, size_t upper)
      : upper_(upper), range_(initialRange), totalAttempts_(0),
        successAttempts_(0) {}

  // 获取当前应使用的消除数组范围
  size_t getRange() const {
    // 返回当前范围值，确保至少为1
    return range_.load(std::memory_order_relaxed);
  }

  // 记录一次成功的交换（push/pop 配对成功）
  void recordEliminationSuccess() {
    // 增加成功计数（用于未来的可选优化）
    successAttempts_.fetch_add(1, std::memory_order_relaxed);

    // 动态调整：成功 -> 增加范围
    increaseRange();
  }

  // 记录一次交换超时（在数组中没有找到配对的线程）
  void recordEliminationTimeout() {
    // 动态调整：超时 -> 减小范围
    decreaseRange();
  }

private:
  void increaseRange() {
    size_t oldVal = range_.load(std::memory_order_relaxed);
    size_t newVal = (oldVal < upper_) ? oldVal + 1 : upper_; // 每次成功 +1
    range_.compare_exchange_weak(oldVal, newVal, std::memory_order_relaxed);
    // 可以加一个最大范围限制，比如 capacity
  }

  void decreaseRange() {
    size_t oldVal = range_.load(std::memory_order_relaxed);
    // 确保范围不会低于最小值1
    size_t newVal = (oldVal > 1) ? oldVal - 1 : 1;
    range_.compare_exchange_weak(oldVal, newVal, std::memory_order_relaxed);
  }

  size_t upper_;
  std::atomic<size_t> range_;           // 当前动态范围值
  std::atomic<size_t> totalAttempts_;   // 总尝试次数
  std::atomic<size_t> successAttempts_; // 成功交换次数
};

template <typename T> struct EliminationBackoffStack : public TreiberStack<T> {
public:
  using value_type = T;
  using enum TryPopStatus;
  using Node = typename TreiberStack<T>::Node;
  EliminationBackoffStack() : eliminationVector(capacity) {};
  ~EliminationBackoffStack() = default; 

  void push(const T &data) override {
    while (true) {
      if (this->try_push(data)) {
        return;
      } else {
        try {
          Node *new_node = new Node{data};
          void *other_value = eliminationVector.visit(
              static_cast<void *>(new_node), localPolicy.getRange());
          if (other_value == nullptr) {
            localPolicy.recordEliminationSuccess();
            return;
          }
        } catch (const std::runtime_error &e) {
          localPolicy.recordEliminationTimeout();
        }
      }
    }
  }

  std::shared_ptr<T> pop() override {
    int empty_cycles = 0;
    TryPopResult<T> result;
    std::shared_ptr<T> res;
    TryPopStatus curr_state;
    while (true) {
      result = this->try_pop();
      curr_state = result.status;
      switch (curr_state) {
      case kEmpty:
        if (++empty_cycles > MAX_EMPTY_RETRIES) {
          return nullptr; // 放弃等待
        }
      case kRetry:
        try {
          void *other_value =
              eliminationVector.visit(nullptr, localPolicy.getRange());
          if (other_value != nullptr) {
            Node *node = static_cast<Node *>(other_value);
            res = std::make_shared<T>(node->data);
            delete node;
            return res;
          }
        } catch (const std::exception &e) {
          localPolicy.recordEliminationTimeout();
        }
        break;
      case kSuccess:
        localPolicy.recordEliminationSuccess();
        return result.value;
        break;
      default:
        break;
      }
    }
  }

private:
  static inline int MAX_EMPTY_RETRIES = 8;
  static inline size_t capacity = 32;
  EliminationVector eliminationVector;
  static inline thread_local RangePolicy localPolicy{5, capacity};
};
}; // namespace industrial