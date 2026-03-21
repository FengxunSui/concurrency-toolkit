#include <atomic>
#include <memory>
#include <cassert>

namespace industrial
{

#ifdef __cpp_lib_hardware_interference_size
  using std::hardware_destructive_interference_size;
#else
  #if defined(__x86_64__) || defined(_M_X64)
    constexpr size_t hardware_destructive_interference_size = 128;
  #else
    constexpr size_t hardware_destructive_interference_size = 64;
  #endif
#endif

template <typename T>
struct SPSCQueue{
public:
  using value_type = T;

  SPSCQueue(const SPSCQueue &) = delete;
  SPSCQueue &operator=(const SPSCQueue &) = delete;

  explicit SPSCQueue(uint32_t size)
      : size_(size),
        data_(static_cast<T *>(std::malloc(sizeof(T) * size))),
        writeIndex_(0),
        readIndex_(0)
  {
    assert(size >= 2);
    if (!data_)
      throw std::bad_alloc();
  }

  ~SPSCQueue()
  {
    if (!std::is_trivially_destructible_v<T>)
    {
      size_t readIndex = readIndex_;
      size_t writeIndex = writeIndex_;
      while (readIndex != writeIndex)
      {
        data_[readIndex].~T();
        if (++readIndex == size_)
        {
          readIndex = 0;
        }
      }
    }
    std::free(data_);
  }

  void push(T &&value)
  {
    const auto currWrite = writeIndex_.load(std::memory_order_relaxed);
    auto nextWrite = currWrite + 1;
    if (nextWrite == size_)
      nextWrite = 0;
    if (nextWrite == readIndex_.load(std::memory_order_acquire))
      return;
    new (&data_[currWrite]) T(std::move(value));
    writeIndex_.store(nextWrite, std::memory_order_release);
  }

  void pop(T &result)
  {
    const auto currRead = readIndex_.load(std::memory_order_relaxed);
    assert(currRead != writeIndex_.load(std::memory_order_acquire));
    auto nextRead = currRead + 1;
    if (nextRead == size_)
      nextRead = 0;
    result = std::move(data_[currRead]);
    data_[currRead].~T();
    readIndex_.store(nextRead, std::memory_order_release);
  }

  bool try_pop(T &result)
  {
    const auto currRead = readIndex_.load(std::memory_order_relaxed);
    if (currRead == writeIndex_.load(std::memory_order_acquire))
      return false;
    auto nextRead = currRead + 1;
    if (nextRead == size_)
      nextRead = 0;
    result = std::move(data_[currRead]);
    data_[currRead].~T();
    readIndex_.store(nextRead, std::memory_order_release);
    return true;
  }

  bool isFull() const{
    auto nextIndex = writeIndex_.load(std::memory_order_relaxed) + 1;
    if (nextIndex == size_){
      nextIndex = 0;
    }
    if (nextIndex != readIndex_.load(std::memory_order_acquire)){
      return false;
    }
    return true;
  }

  bool empty() const{
  return readIndex_.load(std::memory_order_acquire) ==
      writeIndex_.load(std::memory_order_acquire);
  }

  size_t capacity() const { return size_ - 1; }

private:
  using AtomicIndex = std::atomic<unsigned int>;
  char pad0_[hardware_destructive_interference_size];
  const uint32_t size_;
  T *const data_;
  AtomicIndex writeIndex_;
  AtomicIndex readIndex_;
  char pad1_[hardware_destructive_interference_size - sizeof(AtomicIndex)];
};

} // namespace industrial
