// VectorUtils.cpp

#include <algorithm>
#include <Core/Memory/Functions.h>
#include "../../include/Engine/Util/VectorUtils.h"

namespace sge
{
    static std::size_t iter_binary_search(
        const EntityId* const ord_ents,
        const std::size_t len,
        const std::size_t i,
        const EntityId target)
    {
        const auto upper = std::upper_bound(ord_ents + i, ord_ents + len, target);
        const auto lower = std::lower_bound(ord_ents + i, upper, target);
        return lower - ord_ents;
    }

    std::size_t insert_ord_entities(
        std::vector<EntityId>& target_ord_instances,
        const EntityId* ord_new_instances,
        std::size_t num_new_instances)
    {
        target_ord_instances.insert(target_ord_instances.end(), num_new_instances, NULL_ENTITY);
        auto target_back_iter = target_ord_instances.rbegin();
        std::size_t i = num_new_instances;
        std::size_t num_dups = 0;

        auto target_front_iter = target_ord_instances.rbegin() + num_new_instances;
        for (const auto target_rend = target_ord_instances.rend(); i > 0 && target_front_iter != target_rend; ++target_back_iter)
        {
            EntityId new_instance = ord_new_instances[i - 1];

            // If the new instance is greater
            if (*target_front_iter < new_instance)
            {
                // Put it in
                *target_back_iter = new_instance;
                --i;
                continue;
            }

            // If they're equal
            if (*target_front_iter == new_instance)
            {
                // Increment the dup count
                ++num_dups;
            }

            // Put the current one in
            *target_back_iter = *target_front_iter;
            ++target_front_iter;
        }

        for (; i > 0; --i, ++target_back_iter)
        {
            *target_back_iter = ord_new_instances[i - 1];
        }

        return num_dups;
    }

    void remove_ord_entities(
        std::vector<EntityId>& vec,
        const EntityId* ordered_entities,
        std::size_t num)
    {
        auto back_iter = vec.begin();
        auto front_iter = vec.begin();

        for (std::size_t i = 0; i < num; ++front_iter)
        {
            if (*front_iter == ordered_entities[i])
            {
                ++i;
                continue;
            }

            *back_iter = *front_iter;
            ++back_iter;
        }

        for (; front_iter != vec.end(); ++front_iter, ++back_iter)
        {
            *back_iter = *front_iter;
        }

        vec.erase(back_iter, vec.end());
    }

    std::size_t compact_ord_entities(
        EntityId* ordered_entities,
        std::size_t num)
    {
        EntityId* front = ordered_entities;
        EntityId* back = ordered_entities;
        EntityId last_entity = NULL_ENTITY;

        while (front != ordered_entities + num)
        {
            // If the one we're looking at is equal to the one we just looked at (or it's the null entity)
            if (*front == last_entity)
            {
                ++front;
                continue;
            }

            // Move forward
            last_entity = *front;
            *back = *front;
            ++front;
            ++back;
        }

        return back - ordered_entities;
    }

    bool is_ordered(
        const EntityId* ordered_entities,
        std::size_t num,
        std::size_t& out_num_dups)
    {
        out_num_dups = 0;
        EntityId last_entity = WORLD_ENTITY;

        for (std::size_t i = 0; i < num; ++i)
        {
            const auto entity = ordered_entities[i];

            // If the current one is less than the last
            if (entity < last_entity)
            {
                return false;
            }

            if (entity == last_entity)
            {
                ++out_num_dups;
            }

            last_entity = entity;
        }

        return true;
    }

    EntityId ord_entity_union(
        const EntityId** SGE_RESTRICT iters,
        const EntityId* const* SGE_RESTRICT end_iters,
        std::size_t num_iters)
    {
        EntityId target = NULL_ENTITY;

        while (true)
        {
            for (std::size_t i = 0; i < num_iters; ++i)
            {
                auto upper = std::upper_bound(iters[i], end_iters[i], target);
                iters[i] = std::lower_bound(iters[i], upper, target);
            }

            bool found = true;
            for (std::size_t i = 0; i < num_iters; ++i)
            {
                // If this iterator reached its end
                if (iters[i] == end_iters[i])
                {
                    return NULL_ENTITY;
                }

                // If this iterator is equal to the target
                if (*iters[i] == target)
                {
                    continue;
                }

                // This iterator isn't at the target, keep searching
                target = std::max(target, *iters[i]);

                // If this is the first iterator, the others may fail at the same value and we can consider it a match
                found = i == 0;
            }

            if (found)
            {
                return target;
            }
        }
    }

    EntityId ord_entities_match(
        const EntityId* const* ord_entity_arrays,
        const std::size_t* lens,
        std::size_t* iters,
        std::size_t required_len,
        std::size_t inv_required_len,
        std::size_t num_arrays,
        EntityId target)
    {
        while (true)
        {
            for (std::size_t i = 0; i < num_arrays; ++i)
            {
                const auto upper = std::upper_bound(ord_entity_arrays[i] + iters[i], ord_entity_arrays[i] + lens[i], target);
                const auto lower = std::lower_bound(ord_entity_arrays[i] + iters[i], upper, target);
                iters[i] = lower - ord_entity_arrays[i];
            }

            // Make sure all of the required iterators landed on the same valiue
            bool found = true;
            for (std::size_t i = 0; i < required_len; ++i)
            {
                // If this iterator reached its end
                if (iters[i] == lens[i])
                {
                    return NULL_ENTITY;
                }

                const auto iter_entity = ord_entity_arrays[i][iters[i]];

                // If this iterator is equal to the target
                if (iter_entity == target)
                {
                    continue;
                }

                // This iterator isn't at the target, keep searching
                target = std::max(target, iter_entity);

                // If this is the first iterator, the others may fail at the same value and we can consider it a match
                found = i == 0;
            }

            if (!found)
            {
                continue;
            }

            // Make sure none of the inverse required iterators landed on the same value
            for (std::size_t i = 0; i < inv_required_len; ++i)
            {
                // If this iterator reached its end
                if (iters[i + required_len] == lens[i + required_len])
                {
                    continue;
                }

                const auto iter_entity = ord_entity_arrays[i + required_len][iters[i + required_len]];

                // If this iterator is equal to the target
                if (iter_entity == target)
                {
                    found = false;
                }
            }

            // If none of the inverse required iterators landed on the target
            if (found)
            {
                return target;
            }

            // Increment the required iterators
            for (std::size_t i = 0; i < required_len; ++i)
            {
                ++iters[i];
            }
        }
    }

    std::size_t rev_count_dups(
        const EntityId* const* ord_ent_arrays,
        const std::size_t* lens,
        std::size_t num_arrays)
    {
        // Create iterators
        auto* const iters = SGE_STACK_ALLOC(std::size_t, num_arrays);
        std::memcpy(iters, lens, sizeof(std::size_t) * num_arrays);

        // Search for matches
        std::size_t num_dups = 0;

        while (true)
        {
            EntityId max = NULL_ENTITY;
            std::size_t max_index = 0;
            std::size_t remaining_iters = num_arrays;

            // For each iterator
            for (std::size_t i = 0; i < num_arrays; ++i)
            {
                // If this iterator has reached it's end
                if (iters[i] == 0)
                {
                    --remaining_iters;
                    continue;
                }

                const EntityId ent = ord_ent_arrays[i][iters[i] - 1];

                // If this iterator is equal to the largest entity
                if (ent == max)
                {
                    num_dups += 1;
                    --iters[i];
                }
                // If this iterator is greater than the largest entity
                else if (ent > max)
                {
                    max = ent;
                    max_index = i;
                }
            }

            // Decrement the larget iterator
            --iters[max_index];

            if (remaining_iters <= 1)
            {
                return num_dups;
            }
        }
    }
}
