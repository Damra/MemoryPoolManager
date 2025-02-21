#include <iostream>
#include <memory>
#include <vector>
#include <type_traits>
#include <stdexcept> // For std::bad_alloc
#include <functional> // For std::function

using namespace std;

class MemoryPool {
private:
    vector<void*> blocks;
    size_t blockSize;
    size_t capacity;

public:
    MemoryPool(size_t blockSize, size_t capacity)
        : blockSize(blockSize), capacity(capacity) {
        blocks.reserve(capacity);
        for (size_t i = 0; i < capacity; ++i) {
            blocks.push_back(malloc(blockSize)); // Allocate a block of memory
        }
    }

    ~MemoryPool() {
        for (void* block : blocks) {
            free(block); // Free all allocated blocks
        }
    }

    void* allocate() {
        if (blocks.empty()) {
            throw bad_alloc(); // If no blocks left, throw bad_alloc
        }
        void* block = blocks.back(); // Get last allocated block
        blocks.pop_back(); // Remove from available blocks
        return block; // Return memory block
    }

    void deallocate(void* block) {
        blocks.push_back(block); // Return the block to the pool
    }

    bool hasAvailableMemory() const {
        return !blocks.empty();
    }
};

template <typename T>
class PoolAllocator {
private:
    MemoryPool& pool;

public:
    using value_type = T;

    PoolAllocator(MemoryPool& pool) : pool(pool) {}

    template <typename U>
    PoolAllocator(const PoolAllocator<U>& other) : pool(other.pool) {}

    T* allocate(size_t n) {
        if (n == 0) {
            return nullptr; // Return null pointer for zero elements
        }

        // Check if there's enough memory
        if (!pool.hasAvailableMemory()) {
            throw bad_alloc(); // No memory available, throw exception
        }

        void* memory = pool.allocate();
        return static_cast<T*>(memory); // Allocate memory from the pool
    }

    void deallocate(T* p, size_t n) {
        pool.deallocate(p); // Deallocate memory to the pool
    }

    template <typename U>
    bool operator==(const PoolAllocator<U>& other) const {
        return &pool == &other.pool;
    }

    template <typename U>
    bool operator!=(const PoolAllocator<U>& other) const {
        return !(*this == other);
    }
};

// Compile-time factorial calculation (constexpr)
constexpr int factorial(int n) {
    return (n <= 1) ? 1 : n * factorial(n - 1);
}

// Variadic templates with perfect forwarding
template <typename T, typename... Args>
unique_ptr<T, function<void(T*)>> make_unique_pool(MemoryPool& pool, Args&&... args) {
    // Define the deleter as a lambda
    auto deleter = [&pool](T* ptr) {
        ptr->~T(); // Call the destructor explicitly
        pool.deallocate(ptr); // Deallocate the memory back to the pool
    };

    // Allocate memory from the pool
    void* memory = pool.allocate();

    // Use placement new to construct the object in the allocated memory
    T* ptr = new (memory) T(forward<Args>(args)...);

    // Return a unique_ptr, ensuring the deleter is properly passed
    return unique_ptr<T, function<void(T*)>>(ptr, deleter);
}

// RAII class: automatically manages resources
class PoolManager {
public:
    MemoryPool pool;

    PoolManager(size_t blockSize, size_t capacity)
        : pool(blockSize, capacity) {
    }

    template <typename T, typename... Args>
    unique_ptr<T, function<void(T*)>> create(Args&&... args) {
        return make_unique_pool<T>(pool, forward<Args>(args)...);
    }
};

int main() {
    try {
        PoolManager poolManager(sizeof(int), 10);

        // Compile-time factorial calculation
        constexpr int fact5 = factorial(5);
        cout << "Factorial of 5 (compile-time): " << fact5 << endl;

        // Create objects
        auto ptr1 = poolManager.create<int>(42);
        auto ptr2 = poolManager.create<int>(100);

        cout << "ptr1: " << *ptr1 << endl;
        cout << "ptr2: " << *ptr2 << endl;

        // Create vector with custom allocator
        auto vec = poolManager.create<vector<int, PoolAllocator<int>>>(PoolAllocator<int>(poolManager.pool));
        vec->push_back(1);
        vec->push_back(2);
        vec->push_back(3);

        cout << "Vector elements: ";
        for (int num : *vec) {
            cout << num << " ";
        }
        cout << endl;

    } catch (const bad_alloc& e) {
        cout << "Memory allocation failed: " << e.what() << endl;
    }

    return 0;
}
