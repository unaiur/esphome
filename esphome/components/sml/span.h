#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace esphome {
namespace sml {

template<typename T> class Span {
 public:
  Span() : data_(nullptr), size_(0) {}
  Span(T *data, size_t size) : data_(data), size_(size) {}
  Span(const Span<T> &) = default;

  Span &operator=(const Span<T> &) = default;
  size_t size() const { return size_; }

  const T &operator[](size_t index) const { return data_[index]; }
  const T *begin() const { return data_; }
  const T *end() const { return data_ + size_; }

  T &operator[](size_t index) { return data_[index]; }
  T *begin() { return data_; }
  T *end() { return data_ + size_; }

 private:
  T *data_;
  size_t size_;
};

}  // namespace sml
}  // namespace esphome
