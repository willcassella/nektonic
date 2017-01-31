// EmptyComponentContainer.h
#pragma once

#include <algorithm>
#include "../Component.h"
#include "../Util/VectorUtils.h"

namespace sge
{
    template <typename ComponentT>
    class EmptyComponentContainer final : public ComponentContainer
    {
        void reset() override
        {
            _instance_set.clear();
        }

        void to_archive(ArchiveWriter& writer) const override
        {
            writer.typed_array(_instance_set.data(), _instance_set.size());
        }

        void from_archive(ArchiveReader& reader) override
        {
            reset();
            std::size_t size = 0;
            if (!reader.array_size(size))
            {
                return;
            }

            // Fill the instance set with the components from the archive
            _instance_set.assign(size, 0);
            reader.typed_array(_instance_set.data(), _instance_set.size());

            // Sort them
            std::sort(_instance_set.begin(), _instance_set.end());

            // Remove duplicates
            size = compact_ord_entities(_instance_set.data(), _instance_set.size());
            _instance_set.erase(_instance_set.begin() + size, _instance_set.end());
        }

        InstanceIterator get_start_iterator() const override
        {
            return _instance_set.data();
        }

        InstanceIterator get_end_iterator() const override
        {
            return _instance_set.data() + _instance_set.size();
        }

        void create_instances(const EntityId* ordered_entities, std::size_t num) override
        {
            insert_ord_entities(_instance_set, ordered_entities, num);
        }

        void remove_instances(const EntityId* ordered_entities, std::size_t num) override
        {
            remove_ord_entities(_instance_set, ordered_entities, num);
        }

        void reset_interface(InstanceIterator /*instance*/, ComponentInterface* interf) override
        {
            static_cast<ComponentT*>(interf)->reset();
        }

        //////////////////
        ///   Fields   ///
    private:

        std::vector<EntityId> _instance_set;
    };
}
