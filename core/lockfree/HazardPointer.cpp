#include "HazardPointer.h"
#include <algorithm>

namespace industrial {

HazardPointerDomain::HazardPointerDomain(size_t slot_count) {
  slots_.reserve(slot_count);
  for (size_t i = 0; i < slot_count; ++i) {
    slots_.push_back(std::make_unique<HazardPointerSlot>());
  }
}

HazardPointerDomain::~HazardPointerDomain() {
  // 强制回收所有节点
  scanAndReclaim();
  // 再次扫描确保清空
  scanAndReclaim();
}

HazardPointerSlot *HazardPointerDomain::acquireSlot() {
  // 先尝试无锁获取
  size_t start = slot_index_.fetch_add(1, std::memory_order_relaxed);
  for (size_t i = 0; i < slots_.size(); ++i) {
    size_t idx = (start + i) % slots_.size();
    if (slots_[idx]->tryAcquire()) {
      return slots_[idx].get();
    }
  }
  return nullptr; // 槽位耗尽
}

void HazardPointerDomain::releaseSlot(HazardPointerSlot *slot) {
  if (slot) {
    slot->release();
  }
}

std::vector<void *> HazardPointerDomain::getProtectedPointers() const {
  std::vector<void *> protected_ptrs;
  protected_ptrs.reserve(slots_.size());

  for (const auto &slot : slots_) {
    void *p = slot->ptr.load(std::memory_order_acquire);
    if (p != nullptr) {
      protected_ptrs.push_back(p);
    }
  }
  return protected_ptrs;
}

void HazardPointerDomain::scanAndReclaim() {
  // 1. 获取所有受保护的指针（排序后二分查找，O(N log N)）
  auto protected_ptrs = getProtectedPointers();
  std::sort(protected_ptrs.begin(), protected_ptrs.end());

  // 2. 遍历待回收列表，分离可回收节点
  RetiredNode *current = retired_list_.exchange(nullptr);
  RetiredNode *to_reclaim = nullptr;
  RetiredNode *to_reclaim_tail = nullptr;
  RetiredNode *to_keep = nullptr;

  while (current) {
    RetiredNode *next = current->next;

    bool is_protected = std::binary_search(protected_ptrs.begin(),
                                           protected_ptrs.end(), current->data);

    if (is_protected) {
      // 仍然被保护，放回待回收列表
      current->next = to_keep;
      to_keep = current;
    } else {
      // 可以安全回收
      current->next = to_reclaim;
      to_reclaim = current;
      if (!to_reclaim_tail)
        to_reclaim_tail = current;
    }
    current = next;
  }

  // 3. 恢复仍然受保护的节点到全局列表
  if (to_keep) {
    addToRetireList(to_keep);
  }

  // 4. 批量删除可回收节点（Folly 风格：批量删除减少缓存抖动）
  while (to_reclaim) {
    RetiredNode *node = to_reclaim;
    to_reclaim = node->next;
    node->deleter(node->data);
    delete node; // 或者使用对象池
  }
}

template <typename T> void HazardPointerDomain::reclaim_later(T *node) {
  addToRetireList(
      new RetiredNode(node, [](void *p) { delete static_cast<T *>(p); }));
}

void HazardPointerDomain::addToRetireList(RetiredNode *node) {
  RetiredNode *old_head = retired_list_.load();
  RetiredNode *tail;
  do {
    // 找到 to_keep 链表的尾部
    tail = node;
    while (tail->next)
      tail = tail->next;
    tail->next = old_head;
  } while (!retired_list_.compare_exchange_weak(old_head, node));
}

// Holder 实现
HazardPointerHolder::HazardPointerHolder(HazardPointerDomain *domain)
    : domain_(domain ? domain : &defaultHazardPointerDomain()),
      slot_(domain_->acquireSlot()) {
  if (!slot_) {
    throw std::runtime_error("No hazard pointer slot available");
  }
}

HazardPointerHolder::~HazardPointerHolder() {
  if (slot_) {
    domain_->releaseSlot(slot_);
  }
}

HazardPointerHolder::HazardPointerHolder(HazardPointerHolder &&other) noexcept
    : domain_(other.domain_), slot_(other.slot_) {
  other.slot_ = nullptr;
}

HazardPointerHolder &
HazardPointerHolder::operator=(HazardPointerHolder &&other) noexcept {
  if (this != &other) {
    if (slot_)
      domain_->releaseSlot(slot_);
    domain_ = other.domain_;
    slot_ = other.slot_;
    other.slot_ = nullptr;
  }
  return *this;
}

void *HazardPointerHolder::protect(const std::atomic<void *> &src) {
  void *ptr;
  do {
    ptr = src.load(std::memory_order_acquire);
    slot_->ptr.store(ptr, std::memory_order_release);
    // 内存屏障确保 store 对其他线程可见
    std::atomic_thread_fence(std::memory_order_seq_cst);
  } while (src.load(std::memory_order_acquire) != ptr);
  return ptr;
}

void HazardPointerHolder::set(void *ptr) {
  slot_->ptr.store(ptr, std::memory_order_release);
  std::atomic_thread_fence(std::memory_order_seq_cst);
}

void HazardPointerHolder::clear() {
  slot_->ptr.store(nullptr, std::memory_order_release);
}

void *HazardPointerHolder::get() const {
  return slot_->ptr.load(std::memory_order_acquire);
}

// 全局域
HazardPointerDomain &defaultHazardPointerDomain() {
  static HazardPointerDomain domain;
  return domain;
}

} // namespace industrial