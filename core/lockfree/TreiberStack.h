#include "HazardPointer.h"
#include <atomic>
#include <memory>

namespace industrial {

template <typename T> struct TreiberStack {
public:
  using value_type = T;
  TreiberStack() = default;
  ~TreiberStack() {
    while (pop())
      ;
  }
  TreiberStack(const TreiberStack &) = delete;
  TreiberStack &operator=(const TreiberStack &) = delete;

  void push(const T &data) {
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
  std::shared_ptr<T> pop() {
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

      StampedPtr new_head(current->next.ptr(), current.stamp() + 1);

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

private:
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
  void retireNode(Node *node) {

    if (!domain_.getProtectedPointers().empty()) {
      domain_.reclaim_later(node);
    } else {
      delete node;
    }
  }
};

} // namespace industrial
