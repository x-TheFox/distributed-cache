#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include <cstddef>
#include <memory>
#include <mutex>
#include <vector>

template <typename T>
class PoolAllocator {
public:
    PoolAllocator(size_t poolSize);
    ~PoolAllocator();

    T* allocate();
    void deallocate(T* ptr);

private:
    std::vector<T*> pool;
    std::mutex mtx;
    size_t poolSize;
    size_t currentIndex;
};

template <typename T>
PoolAllocator<T>::PoolAllocator(size_t size) : poolSize(size), currentIndex(0) {
    pool.reserve(poolSize);
    for (size_t i = 0; i < poolSize; ++i) {
        pool.push_back(static_cast<T*>(::operator new(sizeof(T))));
    }
}

template <typename T>
PoolAllocator<T>::~PoolAllocator() {
    for (auto ptr : pool) {
        ::operator delete(ptr);
    }
}

template <typename T>
T* PoolAllocator<T>::allocate() {
    std::lock_guard<std::mutex> lock(mtx);
    if (currentIndex < poolSize) {
        return pool[currentIndex++];
    }
    return nullptr; // or throw an exception
}

template <typename T>
void PoolAllocator<T>::deallocate(T* ptr) {
    std::lock_guard<std::mutex> lock(mtx);
    if (currentIndex > 0) {
        pool[--currentIndex] = ptr;
    }
}

#endif // ALLOCATOR_H