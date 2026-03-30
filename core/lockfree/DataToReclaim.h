#include "HazardPointer.h"
#include <atomic>
#include <functional>

namespace industrial {
template <typename T> void do_delete(void *p) { delete static_cast<T *>(p); }

struct DataToReclaim {
  void *data;
  std::function<void(void *)> deleter;
  DataToReclaim *next;
  template <typename T>
  DataToReclaim(T *p) : data(p), deleter(&do_delete<T>), next(nullptr) {}
  ~DataToReclaim() { deleter(data); }
};

static std::atomic<DataToReclaim *> nodes_to_reclaim;
inline void add_to_reclaim_list(DataToReclaim *node) {
  node->next = nodes_to_reclaim.load();
  while (!nodes_to_reclaim.compare_exchange_weak(node->next, node))
    ;
}
template <typename T> void reclaim_later(T *data) {
  add_to_reclaim_list(new DataToReclaim(data));
}
inline void delete_nodes_with_no_hazards() {
  DataToReclaim *current = nodes_to_reclaim.exchange(nullptr);
  while (current) {
    DataToReclaim *const next = current->next;
    if (outstanding_hazard_pointers_for(current->data)) {
      add_to_reclaim_list(current);
    } else {
      delete current;
    }
    current = next;
  }
}
} // namespace industrial