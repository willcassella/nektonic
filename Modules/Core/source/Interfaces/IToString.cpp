// IToString.cpp

#include "../../include/Core/Interfaces/IToString.h"
#include "../../include/Core/Reflection/ReflectionBuilder.h"

SGE_REFLECT_INTERFACE(sge::IToString)
.implemented_for<bool>()
.implemented_for<int8>()
.implemented_for<uint8>()
.implemented_for<int16>()
.implemented_for<uint16>()
.implemented_for<int32>()
.implemented_for<uint32>()
.implemented_for<int64>()
.implemented_for<uint64>()
.implemented_for<float>()
.implemented_for<double>();
