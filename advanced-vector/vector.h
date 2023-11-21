#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>
#include <stdexcept>
#include <algorithm>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
            : buffer_(Allocate(capacity))
            , capacity_(capacity) {
    }

    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory& rhs) = delete;

    RawMemory(RawMemory&& other) noexcept
            : buffer_(other.buffer_)
            , capacity_(other.capacity_) {
        other.buffer_ = nullptr;
        other.capacity_ = 0;
    }
    RawMemory& operator=(RawMemory&& rhs) noexcept {
        if (this != &rhs) {
            Deallocate(buffer_);
            buffer_ = std::move(rhs.buffer_);
            capacity_ = rhs.capacity_;
            rhs.buffer_ = nullptr;
            rhs.capacity_ = 0;
        }
        return *this;
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    T* operator+(size_t offset) noexcept {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector {
public:
    Vector() noexcept = default;

    explicit Vector(size_t size)
            : data_(size)
            , size_(size)
    {
        std::uninitialized_value_construct_n(begin(), size_);
    }

    Vector(const Vector& other)
            : data_(other.size_)
            , size_(other.size_)
    {
        std::uninitialized_copy_n(other.begin(), other.size_, begin());
    }

    Vector(Vector&& other) noexcept
            : data_(std::move(other.data_))
            , size_(other.size_)
    {
        other.size_ = 0;
    }

    ~Vector() {
        std::destroy_n(begin(), size_);
    }

    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

    Vector& operator=(const Vector& rhs) {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                Vector temp(rhs);
                Swap(temp);
            } else {
                std::copy_n(rhs.begin(), std::min(size_, rhs.size_), this->begin());
                if (size_ > rhs.size_) {
                    std::destroy_n(begin() + rhs.size_, (size_ - rhs.size_));
                } else if (size_ < rhs.size_) {
                    Reserve(rhs.size_);
                    std::uninitialized_copy_n(rhs.begin() + size_, rhs.size_ - size_, begin() + size_);
                }
                size_ = rhs.size_;
            }
        }
        return *this;
    }
    Vector& operator=(Vector&& rhs) noexcept {
        if (this != &rhs) {
            data_ = std::move(rhs.data_);
            size_ = rhs.size_;
            rhs.size_ = 0;
        }
        return *this;
    }

    using iterator = T*;
    using const_iterator = const T*;

    iterator begin() noexcept {
        return data_.GetAddress();
    }
    iterator end() noexcept {
        return begin() + size_;
    }
    const_iterator begin() const noexcept {
        return data_.GetAddress();
    }
    const_iterator end() const noexcept {
        return begin() + size_;
    }

    const_iterator cbegin() const noexcept {
        return begin();
    }

    const_iterator cend() const noexcept {
        return end();
    }

    void Swap(Vector& other) noexcept {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }
        RawMemory<T> new_data(new_capacity);
        SafetyMoveOrCopy(begin(), new_data.GetAddress(), size_);
        std::destroy_n(begin(), size_);
        data_.Swap(new_data);
    }

    void Resize(size_t new_size) {
        if (size_ > new_size) {
            std::destroy_n(begin() + new_size, size_ - new_size);
        } else if (size_ < new_size) {
            Reserve(new_size);
            std::uninitialized_value_construct_n(begin() + size_, new_size - size_);
        }
        size_ = new_size;
    }

    void PushBack(const T& value) {
        EmplaceBack(value);
    }

    void PushBack(T&& value) {
        EmplaceBack(std::move(value));
    }

    void PopBack() noexcept {
        if (size_ != 0) {
            std::destroy_n(begin() + (size_ - 1), 1);
            --size_;
        }
    }

    template <typename... Args>
    T& EmplaceBack(Args&&... args){
        return *Emplace(end(), std::forward<Args>(args)...);
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {
        const size_t index = pos - cbegin();
        assert(index <= size_);
        if (size_ == data_.Capacity()) {
            EmplaceWithRelocate(index, std::forward<Args>(args)...);
        } else {
            EmplaceWithoutRelocate(index, std::forward<Args>(args)...);
        }
        ++size_;
        return begin() + index;
    }

    iterator Insert(const_iterator pos, const T& value) {
        return Emplace(pos, value);
    }

    iterator Insert(const_iterator pos, T&& value) {
        return Emplace(pos, std::move(value));
    }

    iterator Erase(const_iterator pos) noexcept(std::is_nothrow_move_assignable_v<T>) {
        const size_t index = pos - cbegin();
        std::move(begin() + index + 1, end(), begin() + index);
        std::destroy_n(begin() + size_, 1);
        --size_;
        return begin() + index;
    }

private:
    RawMemory<T> data_;
    size_t size_ = 0;

    void SafetyMoveOrCopy(T* from, T* to, const size_t size) {
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(from, size, to);
        } else {
            std::uninitialized_copy_n(from, size, to);
        }
    }

    template <typename... Args>
    void EmplaceWithRelocate(const size_t index, Args&&... args) {
        RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
        new (new_data.GetAddress() + index) T(std::forward<Args>(args)...);
        try {
            SafetyMoveOrCopy(begin(), new_data.GetAddress(), index);
        } catch(...) {
            std::destroy_n(new_data.GetAddress() + index, 1);
            throw;
        }
        try {
            SafetyMoveOrCopy(begin() + index, new_data.GetAddress() + index + 1, size_ - index);
        } catch (...) {
            std::destroy_n(new_data.GetAddress(), index + 1);
            throw;
        }
        std::destroy_n(begin(), size_);
        data_.Swap(new_data);
    }

    template <typename... Args>
    void EmplaceWithoutRelocate(const size_t index, Args&&... args) {
        if (index != size_) {
            T temp(std::forward<Args>(args)...);
            new(begin() + size_) T(std::forward<T>(data_[size_ - 1]));
            try {
                std::move_backward(begin() + index, end() - 1, end());
            } catch (...) {
                std::destroy_n(begin() + size_, 1);
                throw;
            }
            data_[index] = std::forward<T>(temp);
        } else {
            new(begin() + index) T(std::forward<Args>(args)...);
        }
    }
};