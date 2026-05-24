#include "HazardPointer.h"
#include <atomic>
#include <memory>

namespace industrial {

template <typename T> struct TreiberStack {
public:
  using value_type = T;
  TreiberStack() = default;
  virtual ~TreiberStack() {
    while (pop())
      ;
  }
  TreiberStack(const TreiberStack &) = delete;
  TreiberStack &operator=(const TreiberStack &) = delete;

  virtual void push(const T &data) {
    Node *new_node = new Node{data};
    StampedPtr old_head = head_.load(std::memory_order_acquire);
    StampedPtr new_head;
    do {
      new_node->next = old_head;
      new_head = old_head.withNext(new_node);
    } while (!head_.compare_exchange_weak(old_head, new_head,
                                          std::memory_order_release,
                                          std::memory_order_relaxed));
  }

  bool try_push(const T &data) {
    Node *new_node = new Node{data};
    StampedPtr old_head = head_.load(std::memory_order_acquire);
    new_node->next = old_head;
    StampedPtr new_head = old_head.withNext(new_node);

    if (head_.compare_exchange_weak(old_head, new_head,
                                    std::memory_order_release,
                                    std::memory_order_relaxed)) {
      return true;
    }
    // CAS 失败，释放新节点避免内存泄漏
    delete new_node;
    return false;
  }

  virtual std::shared_ptr<T> pop() {
    thread_local HazardPointerHolder hp(&domain_);
    while (true) {
      StampedPtr old_head = head_.load(std::memory_order_acquire);
      Node *node = old_head.ptr();
      if (!node)
        return nullptr;
      hp.set(node);
      StampedPtr current = head_.load(std::memory_order_acquire);
      if (current.ptr() != node)
        continue;

      StampedPtr new_head(current.ptr()->next.ptr(), current.stamp() + 1);

      if (head_.compare_exchange_strong(current, new_head,
                                        std::memory_order_release,
                                        std::memory_order_relaxed)) {

        hp.clear();

        auto res = std::make_shared<T>(std::move(node->data));

        if (++op_count_ % 32 == 0) {
          domain_.scanAndReclaim();
        }

        retireNode(node);
        return res;
      }
    }
  }
  
  std::shared_ptr<T> try_pop() {
    thread_local HazardPointerHolder hp(&domain_);

    StampedPtr old_head = head_.load(std::memory_order_acquire);
    Node* node = old_head.ptr();
    if (!node) return nullptr;

    hp.set(node);   // 保护当前节点

    // 重新验证头节点未被其他线程改变
    StampedPtr current = head_.load(std::memory_order_acquire);
    if (current.ptr() != node) {
        hp.clear();
        return nullptr;
    }

    StampedPtr new_head(current.ptr()->next.ptr(), current.stamp() + 1);
    if (head_.compare_exchange_strong(current, new_head,
                                      std::memory_order_release,
                                      std::memory_order_relaxed)) {
        hp.clear();     // 成功取出，清除保护
        auto res = std::make_shared<T>(std::move(node->data));

        // 沿用原有的周期性回收策略
        if (++op_count_ % 32 == 0) {
            domain_.scanAndReclaim();
        }
        retireNode(node);
        return res;
    }

    hp.clear();        // CAS 失败，清除保护
    return nullptr;
}
protected:
  struct Node;
  struct StampedPtr {

    StampedPtr(Node *p = nullptr, uint16_t s = 0)
        : packed(reinterpret_cast<uint64_t>(p) |
                 (static_cast<uint64_t>(s) << 48)) {}
    bool operator==(const StampedPtr &o) const { return packed == o.packed; }

    Node *ptr() const { return reinterpret_cast<Node *>(packed & PTR_MASK); }
    uint16_t stamp() const { return packed >> 48; }

    StampedPtr withNext(Node *p) const { return StampedPtr(p, stamp() + 1); }

  private:
    static constexpr uintptr_t PTR_MASK = (1ULL << 48) - 1;
    uint64_t packed;
  };

  struct Node {
    T data;
    StampedPtr next;
    explicit Node(const T &d) : data(d) {}
  };

  HazardPointerDomain domain_{64};
  alignas(hardware_destructive_interference_size) std::atomic<StampedPtr> head_{
      StampedPtr()};
  alignas(hardware_destructive_interference_size) std::atomic<size_t> op_count_{
      0};
  void retireNode(Node *node) { domain_.reclaim_later(node); };
};
} // namespace industrial
