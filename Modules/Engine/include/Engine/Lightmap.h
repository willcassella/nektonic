// Lightmap.h
#pragma once

#include <map>
#include <Resource/Misc/Color.h>
#include "Component.h"

namespace sge
{
    struct SGE_ENGINE_API SceneLightmap
    {
        SGE_REFLECTED_TYPE;

        struct LightmapElement
        {
            int32 width;
            int32 height;
            std::vector<color::RGBF32> basis_x_radiance;
            std::vector<color::RGBF32> basis_y_radiance;
            std::vector<color::RGBF32> basis_z_radiance;
            std::vector<byte> direct_mask;
        };

        void to_archive(ArchiveWriter& writer) const;

        void from_archive(ArchiveReader& reader);

        //////////////////
        ///   Fields   ///
    public:

        Vec3 light_direction;
        color::RGBF32 light_intensity;
        std::map<NodeId, LightmapElement> lightmap_elements;
    };
}
