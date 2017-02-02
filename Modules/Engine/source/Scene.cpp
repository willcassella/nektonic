// Scene.cpp

#include <Core/Reflection/TypeDB.h>
#include <Core/Reflection/ReflectionBuilder.h>
#include <Core/Util/StringUtils.h>
#include "../include/Engine/Scene.h"
#include "../include/Engine/SystemFrame.h"
#include "../include/Engine/ProcessingFrame.h"
#include "../include/Engine/UpdatePipeline.h"

SGE_REFLECT_TYPE(sge::Scene)
.implements<IToArchive>()
.implements<IFromArchive>();

namespace sge
{
	////////////////////////
	///   Constructors   ///

	Scene::Scene(TypeDB& typedb)
		: _type_db(&typedb)
	{
		_current_time = 0;
		_scene_data.next_entity_id = 2; // '1' is reserved for WORLD_ENTITY
	}

	Scene::~Scene()
	{
	}

	///////////////////
	///   Methods   ///

	void Scene::reset_scene()
	{
		_current_time = 0;
		_scene_data.next_entity_id = 2;
		_scene_data.entity_parents = {};
		_scene_data.entity_names = {};

		for (auto& component_type : _scene_data.components)
		{
			component_type.second->reset();
		}
	}

	const SceneData& Scene::get_raw_scene_data() const
	{
		return _scene_data;
	}

	void Scene::to_archive(ArchiveWriter& writer) const
	{
		// Serialize next entity id
		writer.object_member("next_entity_id", _scene_data.next_entity_id);

		// Serialize all entities
		writer.push_object_member("entities");
		for (auto entity : _scene_data.entity_parents)
		{
			writer.push_object_member(to_string(entity.first).c_str());

			// Write the entity id and parent id
			writer.object_member("parent", entity.second);

			// See if the entity has a name
			auto nameIter = _scene_data.entity_names.find(entity.first);
			if (nameIter != _scene_data.entity_names.end())
			{
				writer.object_member("name", nameIter->second);
			}

			writer.pop();
		}
		writer.pop();

		// Serialize all components
		writer.push_object_member("components");
		for (const auto& componentType : _scene_data.components)
		{
			// Add the component type name as a field
			writer.push_object_member(componentType.first->name().c_str());
            componentType.second->to_archive(writer);
            writer.pop();
		}
		writer.pop();
	}

	void Scene::from_archive(ArchiveReader& reader)
	{
		reset_scene();

		// Deserialize entities
		reader.object_member("next_entity_id", _scene_data.next_entity_id);
		reader.pull_object_member("entities");

		// Get the number of entities
		std::size_t numEntities = 0;
		reader.object_size(numEntities);
		_scene_data.entity_parents.reserve(numEntities);

		// Load all entities
		reader.enumerate_object_members([&reader, &data=_scene_data](const char* id)
		{
			// Get the entities ID and Parent
			EntityId entity = NULL_ENTITY, parent = WORLD_ENTITY;
			entity = std::strtoull(id, nullptr, 10);
			reader.object_member("parent", parent);

			// Make sure the entity fields are valid
			if (entity == NULL_ENTITY || entity == WORLD_ENTITY || parent == NULL_ENTITY)
			{
				return;
			}

			// Add the entity to the world
			data.entity_parents.insert(std::make_pair(entity, parent));

			// Get the entity's name
			std::string name;
			if (reader.object_member("name", name))
			{
				data.entity_names.insert(std::make_pair(entity, std::move(name)));
			}
		});
		reader.pop();

		// Deserialize all components
		reader.pull_object_member("components");
		reader.enumerate_object_members([&](const char* name)
		{
			// Try to get the component type
			auto type = get_component_type(name);
			if (!type)
			{
				return;
			}

			// Clear the entities
			auto storageIter = _scene_data.components.find(type);

			// Deserialize the storage object
			storageIter->second->from_archive(reader);
		});
		reader.pop();

	}

	TypeDB& Scene::get_type_db()
	{
		return *_type_db;
	}

	const TypeDB& Scene::get_type_db() const
	{
		return *_type_db;
	}

	const TypeInfo* Scene::get_component_type(const char* typeName) const
	{
		auto type = _type_db->find_type(typeName);
		if (type && _scene_data.components.find(type) != _scene_data.components.end())
		{
			return type;
		}

		return nullptr;
	}

	void Scene::register_component_type(const TypeInfo& type, std::unique_ptr<ComponentContainer> container)
	{
		// Insert the type
		_scene_data.components.insert(std::make_pair(&type, std::move(container)));
		_type_db->new_type(type);
	}

	void Scene::update(UpdatePipeline& pipeline, float dt)
	{
		// Update the time
		_current_time += dt;

		// For each step in the pipeline
		for (const auto& pipeline_step : pipeline.get_pipeline())
		{
            // Run the first system TODO: ALLOW MULITPLE SYSTEMS
            SystemFrame frame{ UpdatePipeline::NO_SYSTEM, *this, _scene_data };
            pipeline_step[0](frame, _current_time, dt);

            auto tag_frames = apply_changes(pipeline, &frame, 1);
            while (!tag_frames.empty())
            {
                tag_frames = apply_changes(pipeline, tag_frames.data(), tag_frames.size());
            }
		}
	}

    std::vector<SystemFrame> Scene::apply_changes(
        UpdatePipeline& pipeline,
        SystemFrame* frames,
        std::size_t num_frames)
    {
        std::vector<SystemFrame> result;

        // For each frame
        for (std::size_t i = 0; i < num_frames; ++i)
        {
            // For each type of tag in the frame
            for (const auto& tag_type : frames[i]._tags)
            {
                auto iter = pipeline._tag_callbacks.find(tag_type.first);
                if (iter == pipeline._tag_callbacks.end())
                {
                    continue;
                }

                // TODO: MAKE THIS ASYNCHRONOUS (AND NOT SHITTY)
                // For each tag callback set
                for (auto& tag_cb_set : iter->second)
                {
                    // For each tag callback
                    for (auto& tag_cb : tag_cb_set.second)
                    {
                        // Make sure this tag callback is compatible with this frame
                        if (tag_cb.system != UpdatePipeline::NO_SYSTEM && tag_cb.system == frames[i].system_token())
                        {
                            continue;
                        }

                        // For each tag set
                        for (const auto& tag_set : tag_type.second)
                        {
                            // Make sure this callback is compatible with the set
                            if (tag_cb.component_type != nullptr && tag_cb.component_type != &tag_set.component_type())
                            {
                                continue;
                            }

                            // Create a system frame for the callback
                            SystemFrame tag_frame{ tag_cb.system, *this, _scene_data };

                            // Call the tag callback
                            tag_cb.callback(
                                tag_frame,
                                *tag_type.first,
                                tag_set.component_type(),
                                tag_set.get_buffer(),
                                tag_set.get_ord_entities(),
                                tag_set.num_tags());

                            // If the frame has changes, add it to the list
                            if (tag_frame._has_tags)
                            {
                                result.push_back(std::move(tag_frame));
                            }
                        }
                    }
                }
            }
        }

        return result;
    }
}
