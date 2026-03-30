#include "DataToReclaim.h"
#include <atomic>
#include <memory>

namespace industrial {

template <typename T> struct TreiberStack {
public:
  using value_type = T;

  TreiberStack(const TreiberStack &) = delete;
  TreiberStack &operator=(const TreiberStack &) = delete;

private:
  template <typename PtrT> struct StampedPtr {
    uint64_t packed;

    static constexpr uint64_t PTR_MASK = (1ULL << 48) - 1;

    StampedPtr(PtrT *p = nullptr, uint16_t s = 0)
        : packed(reinterpret_cast<uint64_t>(p) |
                 (static_cast<uint64_t>(s) << 48)) {}
    bool operator==(const StampedPtr &o) const { return packed == o.packed; }

    PtrT *ptr() const { return reinterpret_cast<PtrT *>(packed & PTR_MASK); }
    uint16_t stamp() const { return packed >> 48; }

    StampedPtr withNext(PtrT *p) const { return StampedPtr(p, stamp() + 1); }
  };

  struct Node {
    T data;
    StampedPtr<Node> next;
    Node(const T &data_) : data(data_) {}
  };

  using node_ptr = StampedPtr<Node>;
  std::atomic<node_ptr> head;
  void push(const T &data) {
    node_ptr old_head = head.load();
    Node *new_node = new Node{data, old_head};
    node_ptr new_head;
    do {
      new_node->next = old_head;
      new_head = old_head.withNext(new_node);
    } while (!head.compare_exchange_weak(old_head, new_head));
  }
  std::shared_ptr<T> pop() {
    std::atomic<void *> &hp = get_hazard_pointer_for_current_thread();
    node_ptr old_head = head.load();
    node_ptr temp;
    node_ptr new_head;
    do {
      do {
        temp = old_head;
        hp.store(old_head.ptr());
        old_head = head.load();
      } while (old_head != temp);
      new_head = StampedPtr(old_head->next.ptr(), old_head.stamp() + 1);
    } while (old_head && !head.compare_exchange_strong(old_head, new_head));

    hp.store(nullptr);
    std::shared_ptr<T> res;
    Node *node = old_head.ptr();
    if (node) {
      res = std::make_shared<T>(std::move(node->data));
      if (outstanding_hazard_pointers_for(node)) {
        reclaim_later(node);
      } else {
        delete node;
      }
      delete_nodes_with_no_hazards();
    }
    return res;
  }
};

} // namespace industrial
