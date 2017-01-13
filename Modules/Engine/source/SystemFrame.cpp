// SystemFrame.cpp

#include <Core/Memory/Functions.h>
#include <Core/Reflection/ReflectionBuilder.h>
#include "../include/Engine/SystemFrame.h"
#include "../include/Engine/ProcessingFrame.h"
#include "../include/Engine/SceneData.h"

SGE_REFLECT_TYPE(sge::SystemFrame);
SGE_REFLECT_TYPE(sge::SystemFrameMut);

namespace sge
{
	SystemFrame::SystemFrame(const Scene& scene, const SceneData& scene_data)
		: _scene(&scene),
		_scene_data(&scene_data)
	{
	}

	const Scene& SystemFrame::get_scene() const
	{
		return *_scene;
	}

	void SystemFrame::sync()
	{
		// TODO: Synchronize threads
		flush_changes();
	}

	EntityId SystemFrame::get_entity_parent(EntityId entity) const
	{
		if (entity == NULL_ENTITY || entity == WORLD_ENTITY)
		{
			return NULL_ENTITY;
		}

		auto iter = _scene_data->entity_parents.find(entity);
		return iter != _scene_data->entity_parents.end() ? iter->second : NULL_ENTITY;
	}

	std::string SystemFrame::get_entity_name(EntityId entity) const
	{
		if (entity == NULL_ENTITY)
		{
			return "null";
		}

		if (entity == WORLD_ENTITY)
		{
			return "World";
		}

		// Search for the name
		auto iter = _scene_data->entity_names.find(entity);
		return iter != _scene_data->entity_names.end() ? iter->second : "";
	}

	void SystemFrame::process_entities(const TypeInfo* const types[], std::size_t num_types, FunctionView<ProcessFn> process_fn) const
	{
		SystemFrame::impl_process_entities(*this, *_scene_data, types, num_types, process_fn);
	}

	void SystemFrame::process_single(EntityId entity, const TypeInfo* const types[], std::size_t num_types, FunctionView<ProcessFn> process_fn) const
	{
		SystemFrame::impl_process_single(*this, *_scene_data, entity, types, num_types, process_fn);
	}

	template <typename SelfT, typename SceneDataT, typename ProcessFnT>
	void SystemFrame::impl_process_entities(
		SelfT& self,
		SceneDataT& scene_data,
		const TypeInfo* const types[],
		std::size_t num_types,
		ProcessFnT& process_fn)
	{
		if (num_types == 0)
		{
			return;
		}

		auto primary_type = scene_data.components.find(types[0]);
		if (primary_type == scene_data.components.end())
		{
			return;
		}

		// Create a processing frame
		ProcessingFrame pframe;

		// Create the array of component interfaces
		ComponentInterface** interface_array = SGE_STACK_ALLOC(ComponentInterface*, num_types);

		// Fill the interface array
		for (std::size_t i = 0; i < num_types; ++i)
		{
			interface_array[i] = reinterpret_cast<ComponentInterface*>(SGE_STACK_ALLOC(byte, types[i]->size()));
		}

		// Iterate through all entities that the primary component type appears on
		for (EntityId entity : primary_type->second.instances)
		{
			bool satisfied = true;
			for (std::size_t i = 1; i < num_types; ++i)
			{
				// If this type does not exist in the scene
				auto iter = scene_data.components.find(types[i]);
				if (iter == scene_data.components.end())
				{
					satisfied = false;
					break;
				}

				// Try to create the component interface
				if (!iter->second.container->create_interface(pframe, entity, interface_array[i]))
				{
					satisfied = false;
					break;
				}
			}

			// If all further requirements were satisfied
			if (satisfied)
			{
				// Create the primary component interface
				primary_type->second.container->create_interface(pframe, entity, interface_array[0]);

				// Call the processing function
				auto control = process_fn(pframe, entity, interface_array);

				// Handle the control result
				if (control == ProcessControl::BREAK)
				{
					break;
				}
			}
		}
	}

	template <typename SelfT, typename SceneDataT, typename ProcessFnT>
	void SystemFrame::impl_process_single(
		SelfT& self,
		SceneDataT& scene_data,
		EntityId entity,
		const TypeInfo* const types[],
		std::size_t num_types,
		ProcessFnT& process_fn)
	{
		if (num_types == 0)
		{
			return;
		}

		// Create a processing frame
		ProcessingFrame pframe;

		// Create the array of component interfaces
		ComponentInterface** interface_array = SGE_STACK_ALLOC(ComponentInterface*, num_types);

		// Fill the interface array
		for (std::size_t i = 0; i < num_types; ++i)
		{
			// Make sure this type is supported by the scene
			auto iter = scene_data.components.find(types[i]);
			if (iter == scene_data.components.end())
			{
				return;
			}

			// Allocate space for the component interface object
			interface_array[i] = reinterpret_cast<ComponentInterface*>(SGE_STACK_ALLOC(byte, types[i]->size()));

			// Try to get the component interface from the entity
			if (!iter->second.container->create_interface(pframe, entity, interface_array[i]))
			{
				return;
			}
		}

		// Call the processing function
		process_fn(pframe, entity, interface_array);
	}

	void SystemFrame::flush_changes()
	{
	}

	SystemFrameMut::SystemFrameMut(const Scene& scene, SceneData& scene_data)
		: SystemFrame(scene, scene_data),
		_scene_data_mut(&scene_data)
	{
	}

	EntityId SystemFrameMut::create_entity() const
	{
		auto id = _scene_data_mut->next_entity_id++;
		_scene_data_mut->entity_parents[id] = WORLD_ENTITY;

		return id;
	}

	void SystemFrameMut::destroy_entity(EntityId entity)
	{
		// Get the size before we mark the entity as destroyed
		const auto old_size = _destroyed_entities.size();
		_destroyed_entities.insert(entity);

		// If adding the entity didn't change the size, the entity was already destroyed
		if (old_size == _destroyed_entities.size())
		{
			return;
		}

		// Destroy all components of this entity
		for (auto& component_type : _scene_data_mut->components)
		{
			// If this component type has an instance on this entity
			auto iter = component_type.second.instances.find(entity);
			if (iter != component_type.second.instances.end())
			{
				_destroyed_components.insert(ComponentId{ entity, *component_type.first });
			}
		}

		// Destroy all children of this entity
		for (auto parent_entry : _scene_data_mut->entity_parents)
		{
			if (parent_entry.second == entity)
			{
				destroy_entity(parent_entry.first);
			}
		}
	}

	void SystemFrameMut::set_entity_parent(EntityId entity, EntityId parent) const
	{
		if (entity == NULL_ENTITY || entity == WORLD_ENTITY || parent == NULL_ENTITY)
		{
			return;
		}

		// Make sure the target entity actually exists
		auto iter = _scene_data_mut->entity_parents.find(entity);
		if (iter == _scene_data_mut->entity_parents.end())
		{
			return;
		}

		// Make sure the parent entity actually exists
		auto parentIter = _scene_data_mut->entity_parents.find(parent);
		if (parentIter == _scene_data_mut->entity_parents.end())
		{
			return;
		}

		// TODO: Check for entity parent cycle
		iter->second = parent;
	}

	void SystemFrameMut::set_entity_name(EntityId entity, std::string name) const
	{
		// Make sure the entity is a user entity, and the name is valid
		if (entity == NULL_ENTITY || entity == WORLD_ENTITY || name.empty())
		{
			return;
		}

		// Make sure the entity is a valid user entity
		if (_scene_data_mut->entity_parents.find(entity) == _scene_data_mut->entity_parents.end())
		{
			return;
		}

		// Insert the name
		_scene_data_mut->entity_names[entity] = std::move(name);
	}

	void SystemFrameMut::remove_entity_name(EntityId entity) const
	{
		auto iter = _scene_data_mut->entity_names.find(entity);
		if (iter == _scene_data_mut->entity_names.end())
		{
			return;
		}

		_scene_data_mut->entity_names.erase(iter);
	}

	ComponentId SystemFrameMut::new_component(EntityId entity, const TypeInfo& type, std::function<ComponentInitFn> init_fn)
	{
		// Make sure the entity is a valid user entity
		if (entity == NULL_ENTITY || entity == WORLD_ENTITY || _scene_data_mut->entity_parents.find(entity) == _scene_data_mut->entity_parents.end())
		{
			return ComponentId::null();
		}

		// Create an Id for the component
		ComponentId id{ entity, type };

		// Insert it into the table
		_new_components.insert(std::make_pair(id, std::move(init_fn)));

		return id;
	}

	void SystemFrameMut::destroy_component(ComponentId component)
	{
		_destroyed_components.insert(component);
	}

	void SystemFrameMut::destroy_component(EntityId entity, const TypeInfo& type)
	{
		destroy_component(ComponentId{ entity, type });
	}

	void SystemFrameMut::process_entities_mut(const TypeInfo* const types[], std::size_t num_types, FunctionView<ProcessMutFn> process_fn)
	{
		SystemFrame::impl_process_entities(*this, *_scene_data_mut, types, num_types, process_fn);
	}

	void SystemFrameMut::process_single_mut(EntityId entity, const TypeInfo* const types[], std::size_t num_types, FunctionView<ProcessMutFn> process_fn)
	{
		SystemFrame::impl_process_single(*this, *_scene_data_mut, entity, types, num_types, process_fn);
	}

	void SystemFrameMut::flush_changes()
	{
		SystemFrame::flush_changes();

		// For each component destroyed
		for (auto component : _destroyed_components)
		{
			// Find the container
			auto type_iter = _scene_data_mut->components.find(component.type());
			if (type_iter == _scene_data_mut->components.end())
			{
				continue;
			}

			// Find the entity
			auto entity_iter = type_iter->second.instances.find(component.entity());
			if (entity_iter == type_iter->second.instances.end())
			{
				continue;
			}

			// Destroy the component
			type_iter->second.container->remove_component(*entity_iter);
			type_iter->second.instances.erase(entity_iter);
		}
		_destroyed_components.clear();

		// For each entity destroyed
		for (auto entity : _destroyed_entities)
		{
			// Find the entity
			auto entity_iter = _scene_data_mut->entity_parents.find(entity);
			if (entity_iter == _scene_data_mut->entity_parents.end())
			{
				continue;
			}

			// Remove it
			_scene_data_mut->entity_parents.erase(entity_iter);

			// Remove the name, if it exists
			auto name_iter = _scene_data_mut->entity_names.find(entity);
			if (name_iter == _scene_data_mut->entity_names.end())
			{
				continue;
			}

			// Remove the name
			_scene_data_mut->entity_names.erase(name_iter);
		}
		_destroyed_entities.clear();

		// For each new component
		for (auto& component : _new_components)
		{
			// Search for the container
			auto type_iter = _scene_data_mut->components.find(component.first.type());
			if (type_iter == _scene_data_mut->components.end())
			{
				continue;
			}

			// Create the component
			type_iter->second.container->create_component(component.first.entity());
			type_iter->second.instances.insert(component.first.entity());

			// If the user supplied an init function
			if (component.second)
			{
				// TODO
			}
		}
		_new_components.clear();
	}
}