// StrideIter.h
#pragma once

namespace sge {
    template <typename T>
    struct StrideIter {
        T* current;
        size_t stride;

        StrideIter(
        ) : current(nullptr), stride(0) {
        }

        StrideIter(
            T* const start,
            size_t const stride = sizeof(T)
        ) : current(start), stride(stride) {
        }

        StrideIter next(
        ) const {
            auto result = *this;
            result.move_next();

            return result;
        }

        void move_next(
        ) {
            current = (T*)((char*)current + stride);
        }
    };
}
