#include "DataToReclaim.h"
#include <atomic>
#include <memory>

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

template <typename T> struct TreiberStack {
public:
  using value_type = T;

  TreiberStack(const TreiberStack &) = delete;
  TreiberStack &operator=(const TreiberStack &) = delete;

private:
  struct Node {
    T data;
    Node *next;
    Node(const T &data_) : data(data_) {}
  };
  std::atomic<Node *> head;
  void push(const T &data) {
    const Node *new_node = new Node(data);
    new_node->next = head.load();
    while (!head.compare_exchange_weak(new_node->next, new_node))
      ;
  }
  std::shared_ptr<T> pop() {
    std::atomic<void *> &hp = get_hazard_pointer_for_current_thread();
    Node *old_head = head.load();
    Node *temp;
    do {
      do {
        temp = old_head;
        hp.store(old_head);
        old_head = head.load();
      } while (old_head != temp);
    } while (old_head &&
             !head.compare_exchange_strong(old_head, old_head->next));

    hp.store(nullptr);
    std::shared_ptr<T> res;
    if (old_head) {
      res.swap(old_head->data);
      if (outstanding_hazard_pointers_for(old_head)) {
        reclaim_later(old_head);
      } else {
        delete old_head;
      }
      delete_nodes_with_no_hazards();
    }
    return res;
  }
};

} // namespace industrial
