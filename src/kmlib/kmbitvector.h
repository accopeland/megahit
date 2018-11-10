//
// Created by vout on 28/2/2018.
//

#ifndef KMLIB_ATOMIC_BIT_VECTOR_H
#define KMLIB_ATOMIC_BIT_VECTOR_H

#include <memory>
#include <algorithm>
#include <vector>
#include <atomic>

namespace kmlib {
/*!
 * @brief Atomic bit vector: a class that represent a vector of "bits".
 * @details Update of each bit is threads safe via set and get.
 * It can also be used as a vector of bit locks via try_lock, lock and unlock
 */
template<typename WordType = unsigned long>
class AtomicBitVector {
 public:
  using word_type = WordType;
  using size_type = typename std::vector<word_type>::size_type;
 public:
  /*!
   * @brief Constructor
   * @param size the size (number of bits) of the bit vector
   */
  explicit AtomicBitVector(size_type size = 0)
      : size_(size),
        data_array_((size + kBitsPerWord - 1) / kBitsPerWord) {
  }
  /*!
   * @brief Construct a bit vector from iterators of words
   * @tparam WordIterator iterator to access words
   * @param first the iterator pointing to the first word
   * @param last the iterator pointing to the last word
   */
  template<typename WordIterator>
  explicit AtomicBitVector(WordIterator first, WordIterator last)
      : size_((last - first) * kBitsPerWord), data_array_(first, last) {}
  /*!
   * @brief the move constructor
   */
  AtomicBitVector(AtomicBitVector &&rhs)
      : size_(rhs.size_), data_array_(std::move(rhs.data_array_)) {}
  /*!
   * @brief the move operator
   */
  AtomicBitVector &operator=(AtomicBitVector &&rhs) {
    size_ = rhs.size_;
    data_array_ = std::move(rhs.data_array_);
    return *this;
  }
  ~AtomicBitVector() = default;

  /*!
   * @return the size of the bit vector
   */
  size_type size() const {
    return size_;
  }

  /*!
   * @brief set the i-th bit to 1
   * @param i the index of the bit to be set to 1
   */
  void set(size_type i) {
    word_type mask = word_type(1) << (i % kBitsPerWord);
    data_array_[i / kBitsPerWord].v.fetch_or(mask, std::memory_order_release);
  }

  /*!
   * @brief set the i-th bit to 0
   * @param i the index of the bit to be set to 0
   */
  void unset(size_type i) {
    word_type mask = ~(word_type(1) << (i % kBitsPerWord));
    data_array_[i / kBitsPerWord].v.fetch_and(mask, std::memory_order_release);
  }

  /*!
   * @param i the index of the bit
   * @return value of the i-th bit
   */
  bool at(size_type i) const {
    return !!(data_array_[i / kBitsPerWord].v.load(std::memory_order_acquire)
        & (word_type(1) << i % kBitsPerWord));
  }

  /*!
   * @param i the index of the bit
   * @return whether the i-th bit has been locked successfully
   */
  bool try_lock(size_type i) {
    auto p = data_array_.begin() + i / kBitsPerWord;
    word_type old_value = p->v.load(std::memory_order_acquire);
    while (!((old_value >> i % kBitsPerWord) & 1)) {
      word_type new_value = old_value | (word_type(1) << (i % kBitsPerWord));
      if (p->v.compare_exchange_weak(old_value,
                                     new_value,
                                     std::memory_order_release)) {
        return true;
      }
    }
    return false;
  }

  /*!
   * @brief lock the i-th bit
   * @param i the bit to lock
   */
  void lock(size_type i) {
    while (!try_lock(i)) {
      continue;
    }
  }

  /*!
   * @brief unlock the i-th bit
   * @param i the index of the bits
   */
  void unlock(size_type i) {
    unset(i);
  }

  /*!
   * @brief reset the size of the bit vector and clear all bits
   * @param size the new size of the bit vector
   */
  void reset(size_type size) {
    data_array_ = std::move(array_type(0)); // clear memory
    size_ = size;
    data_array_ = std::move(array_type((size + kBitsPerWord - 1) / kBitsPerWord, 0));
  }

  /*!
   * @brief swap with another bit vector
   * @param rhs the target to swap
   */
  void swap(AtomicBitVector &rhs) {
    std::swap(size_, rhs.size_);
    std::swap(data_array_, rhs.data_array_);
  }

 private:
  /*!
   * @brief a wrapper for std::Atomic. std::Atomic do not support copy and move
   * constructor, so this wrapper is used to make suitable to std::vector
   * @tparam T the underlying type of the atomic struct
   */
  template<typename T>
  struct AtomicWrapper {
    std::atomic<T> v;
    AtomicWrapper(T a = T()) : v(a) {}
    AtomicWrapper(const AtomicWrapper &rhs) : v(rhs.v.load()) {}
    AtomicWrapper &operator=(const AtomicWrapper &rhs) {
      v.store(rhs.v.load());
      return *this;
    }
  };

  using array_type = std::vector<AtomicWrapper<word_type>>;
  static const unsigned kBitsPerByte = 8;
  static const unsigned kBitsPerWord = sizeof(word_type) * kBitsPerByte;
  size_type size_;
  array_type data_array_;
  static_assert(sizeof(AtomicWrapper<word_type>) == sizeof(word_type), "");
};

} // namespace kmlib

using AtomicBitVector = kmlib::AtomicBitVector<uint64_t>;

#endif //KMLIB_ATOMIC_BIT_VECTOR_H
