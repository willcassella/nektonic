// CLightMaskReceiver.cpp

#include <Core/Reflection/ReflectionBuilder.h>
#include "../../../include/Engine/Components/Display/CLightMaskReceiver.h"
#include "../../../include/Engine/Util/EmptyComponentContainer.h"
#include "../../../include/Engine/Scene.h"

SGE_REFLECT_TYPE(sge::CLightMaskReceiver);

namespace sge
{
    CLightMaskReceiver::CLightMaskReceiver(ProcessingFrame& pframe, EntityId entity)
        : TComponentInterface<sge::CLightMaskReceiver>(pframe, entity)
    {
    }

    void CLightMaskReceiver::register_type(Scene& scene)
    {
        scene.register_component_type(type_info, std::make_unique<EmptyComponentContainer<CLightMaskReceiver>>());
    }
}