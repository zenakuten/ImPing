#pragma once

#include <array>
#include <cstddef>
#include <optional>

namespace imping {

template <typename T, size_t Capacity>
class RingBuffer {
public:
    void push(const T& value) {
        data_[write_index_] = value;
        write_index_ = (write_index_ + 1) % Capacity;
        if (size_ < Capacity) {
            ++size_;
        }
    }

    [[nodiscard]] std::optional<T> at(size_t index) const {
        if (index >= size_) {
            return std::nullopt;
        }
        size_t actual_index = (write_index_ + Capacity - size_ + index) % Capacity;
        return data_[actual_index];
    }

    [[nodiscard]] std::optional<T> back() const {
        if (size_ == 0) {
            return std::nullopt;
        }
        size_t index = (write_index_ + Capacity - 1) % Capacity;
        return data_[index];
    }

    [[nodiscard]] size_t size() const { return size_; }
    [[nodiscard]] bool empty() const { return size_ == 0; }
    [[nodiscard]] static constexpr size_t capacity() { return Capacity; }

    void clear() {
        size_ = 0;
        write_index_ = 0;
    }

    // Direct access to underlying data (no optional)
    [[nodiscard]] const T& ref_at(size_t index) const {
        size_t actual_index = (write_index_ + Capacity - size_ + index) % Capacity;
        return data_[actual_index];
    }

    // Iterator support for range-based for loops
    class Iterator {
    public:
        Iterator(const RingBuffer* buffer, size_t index)
            : buffer_(buffer), index_(index) {}

        const T& operator*() const {
            return buffer_->ref_at(index_);
        }

        Iterator& operator++() {
            ++index_;
            return *this;
        }

        bool operator!=(const Iterator& other) const {
            return index_ != other.index_;
        }

    private:
        const RingBuffer* buffer_;
        size_t index_;
    };

    [[nodiscard]] Iterator begin() const { return Iterator(this, 0); }
    [[nodiscard]] Iterator end() const { return Iterator(this, size_); }

private:
    std::array<T, Capacity> data_{};
    size_t write_index_ = 0;
    size_t size_ = 0;
};

} // namespace imping
