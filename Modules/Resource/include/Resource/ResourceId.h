// ResourceId.h
#pragma once

#include <Core/Reflection/ReflectionBuilder.h>
#include <Core/Containers/FixedString.h>
#include "build.h"

namespace sge
{
    template <typename T>
    struct SGE_RESOURCE_API ResourceId
    {
        SGE_REFLECTED_TYPE;

        using PackageName = FixedString<8>;
        using ResourceName = FixedString<32>;

        ////////////////////////
        ///   Constructors   ///
    public:

        ResourceId()
        {
        }

        ///////////////////
        ///   Methods   ///
    public:

        /*void uri(std::string uri)
        {
            _uri = std::move(uri);
        }

        const std::string& uri() const
        {
            return _uri;
        }

        bool is_null() const
        {
            return _uri.empty();
        }
*/
        //////////////////
        ///   Fields   ///
    private:

        PackageName _package;
        ResourceName _identifier;
    };
}
