// CStaticMeshCollider.h
#pragma once

#include "../../Component.h"

namespace sge
{
    struct SGE_ENGINE_API CStaticMeshCollider
    {
        struct SharedData;
        SGE_REFLECTED_TYPE;

        ////////////////////////
        ///   Constructors   ///
    public:

        CStaticMeshCollider(NodeId node, SharedData& shared_data);

        ///////////////////
        ///   Methods   ///
    public:

        static void register_type(Scene& scene);

        void to_archive(ArchiveWriter& writer) const;

        void from_archive(ArchiveReader& reader);

        NodeId node() const;

        const std::string& mesh() const;

        bool lightmask_receiver() const;

        void lightmask_receiver(bool value);

        void mesh(std::string path);

        //////////////////
        ///   Fields   ///
    private:

        NodeId _node;
        std::string _mesh;
        bool _lightmask_receiver = false;
        SharedData* _shared_data;
    };
}
