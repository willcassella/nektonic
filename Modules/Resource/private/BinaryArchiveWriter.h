// BinaryArchiveWriter.h
#pragma once

#include <cassert>
#include <cstring>
#include <vector>
#include <stack>
#include <Core/IO/ArchiveWriter.h>
#include "BinaryArchiveNode.h"

namespace sge
{
    class BinaryArchiveWriter final : public ArchiveWriter
    {
        struct Cursor
        {
            BinaryArchiveNode node_type;
            std::size_t offset;
        };

        ////////////////////////
        ///   Constructors   ///
    public:

        BinaryArchiveWriter(std::vector<byte>& buffer)
            : buffer(&buffer)
        {
            current_node.node_type = BAN_NULL;
            current_node.offset = 0;
            buffer.push_back(BAN_NULL);
        }

        ///////////////////
        ///   Methods   ///
    public:

        void pop() override
        {
            // If this node is a generic array or object, we need to set the span
            if (current_node.node_type == BAN_ARRAY_GENERIC || current_node.node_type == BAN_OBJECT)
            {
                const auto span = static_cast<BinaryArchiveSize_t>(buffer->size() - current_node.offset);
                *reinterpret_cast<BinaryArchiveSize_t*>(buffer->data() + current_node.offset + 1 + sizeof(BinaryArchiveSize_t)) = span;
            }

            if (node_stack.empty())
            {
                delete this;
                return;
            }

            // Get the parent from the stack
            current_node = node_stack.top();
            node_stack.pop();
        }

        void null() override
        {
            node_set_type(BAN_NULL);
        }

        void boolean(bool value) override
        {
            node_set_type(BAN_BOOLEAN);
            buffer_append<BinaryArchiveBool_t>(value ? BAN_TRUE : BAN_FALSE);
        }

        void number(int8 value) override
        {
            node_set_type(BAN_INT8);
            buffer_append(value);
        }

        void number(uint8 value) override
        {
            node_set_type(BAN_UINT8);
            buffer_append(value);
        }

        void number(int16 value) override
        {
            node_set_type(BAN_INT16);
            buffer_append(value);
        }

        void number(uint16 value) override
        {
            node_set_type(BAN_UINT16);
            buffer_append(value);
        }

        void number(int32 value) override
        {
            node_set_type(BAN_INT32);
            buffer_append(value);
        }

        void number(uint32 value) override
        {
            node_set_type(BAN_UINT32);
            buffer_append(value);
        }

        void number(int64 value) override
        {
            node_set_type(BAN_INT64);
            buffer_append(value);
        }

        void number(uint64 value) override
        {
            node_set_type(BAN_UINT64);
            buffer_append(value);
        }

        void number(float value) override
        {
            node_set_type(BAN_FLOAT);
            buffer_append(value);
        }

        void number(double value) override
        {
            node_set_type(BAN_DOUBLE);
            buffer_append(value);
        }

        void string(const char* str, std::size_t len) override
        {
            node_set_type(BAN_STRING);
            buffer_append(static_cast<BinaryArchiveSize_t>(len));
            buffer->insert(buffer->end(), reinterpret_cast<const byte*>(str), reinterpret_cast<const byte*>(str + len));
        }

        void typed_array(const bool* arr, std::size_t size) override
        {
            assert(
                current_node.node_type == BAN_NULL
            /*You may not change a node's type, or set it twice. Generic arrays are considered a different type than typed arrays.*/);

            // Reserve space for size, and data (on top of current size)
            buffer->reserve(buffer->size() + sizeof(BinaryArchiveSize_t) + sizeof(BinaryArchiveBool_t) * size);

            // Set the node's type
            (*buffer)[current_node.offset] = BAN_ARRAY_BOOLEAN;
            current_node.node_type = BAN_ARRAY_BOOLEAN;

            // Push the size into the buffer
            buffer_append(static_cast<BinaryArchiveSize_t>(size));

            // Push the array into the buffer
            for (std::size_t i = 0; i < size; ++i)
            {
                buffer_append<BinaryArchiveBool_t>(arr[i] ? BinaryArchiveBool_t{ 1 } : BinaryArchiveBool_t{ 0 });
            }
        }

        void typed_array(const int8* arr, std::size_t size) override
        {
            impl_typed_array(BAN_ARRAY_INT8, arr, size);
        }

        void typed_array(const uint8* arr, std::size_t size) override
        {
            impl_typed_array(BAN_ARRAY_UINT8, arr, size);
        }

        void typed_array(const int16* arr, std::size_t size) override
        {
            impl_typed_array(BAN_ARRAY_INT16, arr, size);
        }

        void typed_array(const uint16* arr, std::size_t size) override
        {
            impl_typed_array(BAN_ARRAY_UINT16, arr, size);
        }

        void typed_array(const int32* arr, std::size_t size) override
        {
            impl_typed_array(BAN_ARRAY_INT32, arr, size);
        }

        void typed_array(const uint32* arr, std::size_t size) override
        {
            impl_typed_array(BAN_ARRAY_UINT32, arr, size);
        }

        void typed_array(const int64* arr, std::size_t size) override
        {
            impl_typed_array(BAN_ARRAY_INT64, arr, size);
        }

        void typed_array(const uint64* arr, std::size_t size) override
        {
            impl_typed_array(BAN_ARRAY_UINT64, arr, size);
        }

        void typed_array(const float* arr, std::size_t size) override
        {
            impl_typed_array(BAN_ARRAY_FLOAT, arr, size);
        }

        void typed_array(const double* arr, std::size_t size) override
        {
            impl_typed_array(BAN_ARRAY_DOUBLE, arr, size);
        }

        void push_array_element() override
        {
            assert(
                current_node.node_type == BAN_NULL || current_node.node_type == BAN_ARRAY_GENERIC
                /*You may not change a node's type. Generic arrays are considered a different type than typed arrays.*/);

            // Set the node as a generic array if it isn't already
            if (current_node.node_type == BAN_NULL)
            {
                (*buffer)[current_node.offset] = BAN_ARRAY_GENERIC;
                current_node.node_type = BAN_ARRAY_GENERIC;
                buffer_append(BinaryArchiveSize_t{ 0 });
                buffer_append(BinaryArchiveSize_t{ 0 });
            }

            // Increment the element count
            *reinterpret_cast<BinaryArchiveSize_t*>(buffer->data() + current_node.offset + 1) += 1;

            // Push the current node onto the node stack as a parent
            node_stack.push(current_node);

            // Set up the new node
            current_node.node_type = BAN_NULL;
            current_node.offset = buffer->size();
            buffer->push_back(BAN_NULL);
        }

        void push_object_member(const char* name) override
        {
            assert(current_node.node_type == BAN_NULL || current_node.node_type == BAN_OBJECT /*You may not change a node's type.*/);

            // Set the node as an object if it isn't already
            if (current_node.node_type == BAN_NULL)
            {
                (*buffer)[current_node.offset] = BAN_OBJECT;
                current_node.node_type = BAN_OBJECT;
                buffer_append(BinaryArchiveSize_t{ 0 });
                buffer_append(BinaryArchiveSize_t{ 0 });
            }

            // Increment the member count
            *reinterpret_cast<BinaryArchiveSize_t*>(buffer->data() + current_node.offset + 1) += 1;

            // Push the current node onto the stack as a parent
            node_stack.push(current_node);

            // Push the new node's name
            buffer->insert(buffer->end(), reinterpret_cast<const byte*>(name), reinterpret_cast<const byte*>(name + std::strlen(name)));
            buffer->push_back(0);

            // Set up the new node
            current_node.node_type = BAN_NULL;
            current_node.offset = buffer->size();
            buffer->push_back(BAN_NULL);
        }

        template <typename T>
        void impl_typed_array(BinaryArchiveNode array_type, const T* arr, std::size_t size)
        {
            assert(
                current_node.node_type == BAN_NULL
                /*You may not change a node's type, or set it twice. Generic arrays are considered a different type than typed arrays.*/);

            // Reserve space for size, and data (on top of current size)
            buffer->reserve(buffer->size() + sizeof(BinaryArchiveSize_t) + sizeof(T) * size);

            // Set the node's type
            (*buffer)[current_node.offset] = array_type;
            current_node.node_type = array_type;

            // Push the size into the buffer
            buffer_append(static_cast<BinaryArchiveSize_t>(size));

            // Push the array into the buffer
            buffer->insert(buffer->end(), reinterpret_cast<const byte*>(arr), reinterpret_cast<const byte*>(arr + size));
        }

        template <typename T>
        void buffer_append(const T& value)
        {
            const byte* addr = reinterpret_cast<const byte*>(&value);
            buffer->insert(buffer->end(), addr, addr + sizeof(T));
        }

        void node_set_type(BinaryArchiveNode type)
        {
            assert(
                current_node.node_type == BAN_NULL
                /*Archive nodes may not may not have their value set twice.*/);

            current_node.node_type = type;
            (*buffer)[current_node.offset] = type;
        }

        //////////////////
        ///   Fields   ///
    public:

        Cursor current_node;
        std::stack<Cursor> node_stack;
        std::vector<byte>* buffer;
    };
}
