// Matrix3.h
#pragma once

#include "TVector3.h"

namespace sge
{
    template <typename T>
    struct TMatrix3
    {
        using Column_t = TVector3<T>;

        ////////////////////////
        ///   Constructors   ///
    public:

        TMatrix3()
            : columns{ Column_t{ 1, 0, 0 }, Column_t{ 0, 1, 0 }, Column_t{ 0, 0, 1 } }
        {
        }
        TMatrix3(Column_t column_1, Column_t column_2, Column_t column_3)
            : columns{ column_1, column_2, column_3 }
        {
        }

        //////////////////
        ///   Fields   ///
    public:

        Column_t columns[3];
    };
}
