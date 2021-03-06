// FieldInfo.h
#pragma once

#include <string>
#include "Any.h"

namespace sge
{
    struct TypeInfo;

    using FieldFlags_t = uint32;
    enum FieldFlags : FieldFlags_t
    {
        /**
         * \brief This field has no special flags.
         */
        FF_NONE = 0,

        /**
         * \brief This field should not be serialized by the 'to_archive' and 'from_archive' functions.
         * \note This only applies to types using the default ToArchive and FromArchive implementations.
         */
        FF_TRANSIENT = (1 << 0),

        /**
         * \brief This field should not be written to.
         */
        FF_READONLY = (1 << 1) | FF_TRANSIENT
    };

    struct FieldInfo
    {
        struct Data
        {
            ////////////////////////
            ///   Constructors   ///
        public:

            Data()
                : flags(FF_NONE),
                index(0),
                type(nullptr),
                offset(0)
            {
            }

            //////////////////
            ///   Fields   ///
        public:

            FieldFlags_t flags;
            uint32 index;
            const TypeInfo* type;
            std::size_t offset;
        };

        ////////////////////////
        ///   Constructors   ///
    public:

        FieldInfo(Data data)
            : _data(std::move(data))
        {
        }

        ///////////////////
        ///   Methods   ///
    public:

        /**
         * \brief Returns the flags assigned to this field.
         */
        FieldFlags_t flags() const
        {
            return _data.flags;
        }

        /**
         * \brief Returns the registration index of this field.
         */
        uint32 index() const
        {
            return _data.index;
        }

        /**
         * \brief Returns the type of this field.
         */
        const TypeInfo& type() const
        {
            return *_data.type;
        }

        /**
         * \brief Returns the offset of this field from the base object pointer.
         */
        std::size_t offset() const
        {
            return _data.offset;
        }

        /**
         * \brief Accesses this field.
         * \param self The instance to access the field on.
         * \return A pointer the field.
         */
        Any<> get(const void* self) const
        {
            return{ *_data.type, static_cast<const char*>(self) + _data.offset };
        }

        /**
         * \brief Accesses this field.
         * \param self The instance to access the field on.
         * \return A pointer to the field.
         */
        AnyMut<> get(void* self) const
        {
            return{ *_data.type, static_cast<char*>(self) + _data.offset };
        }

        //////////////////
        ///   Fields   ///
    private:

        Data _data;
    };
}
