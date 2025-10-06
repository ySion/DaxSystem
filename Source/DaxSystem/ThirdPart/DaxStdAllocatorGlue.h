#pragma once

#include "CoreMinimal.h"

namespace ArzDax {
    template <class T>
    struct TDaxAllocator {
        using value_type = T;
        using pointer = T*;
        using const_pointer = const T*;
        using size_type = std::size_t;
        using difference_type = std::ptrdiff_t;

        template <class U>
        struct rebind { using other = TDaxAllocator<U>; };

        TDaxAllocator() noexcept {}

        template <class U>
        TDaxAllocator(const TDaxAllocator<U>&) noexcept {}

        T* allocate(std::size_t n) {
            return static_cast<T*>(FMemory::Malloc(n * sizeof(T), alignof(T)));
        }

        void deallocate(T* p, std::size_t size) noexcept {
            FMemory::Free(p);
        }
    };

    template <class T, class U>
    bool operator==(const TDaxAllocator<T>&, const TDaxAllocator<U>&) { return true; }

    template <class T, class U>
    bool operator!=(const TDaxAllocator<T>&, const TDaxAllocator<U>&) { return false; }
}