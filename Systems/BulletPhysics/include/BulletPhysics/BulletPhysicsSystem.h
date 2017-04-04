// BulletPhysicsSystem.h
#pragma once

#include <memory>
#include <Core/Reflection/Reflection.h>
#include <Engine/Components/Gameplay/CCharacterController.h>
#include "build.h"

namespace sge
{
    struct SystemFrame;
    struct UpdatePipeline;

    namespace bullet_physics
    {
        struct Config;

        struct SGE_BULLET_PHYSICS_API BulletPhysicsSystem
        {
            struct Data;
            SGE_REFLECTED_TYPE;

            ////////////////////////
            ///   Constructors   ///
        public:

            BulletPhysicsSystem(const Config& config);
            ~BulletPhysicsSystem();

            ///////////////////
            ///   Methods   ///
        public:

            void register_pipeline(UpdatePipeline& pipeline);

			void initialize_subscriptions(Scene& scene);

        private:

            void phys_tick(Scene& scene, SystemFrame& frame);

            void debug_draw(Scene& scene, SystemFrame& frame);

            void initialize_world(Scene& scene);

            //////////////////
            ///   Fields   ///
        private:

            bool _initialized_world;
            std::unique_ptr<Data> _data;
			EventChannel* _node_world_transform_changed_event_channel;
			EventChannel* _new_rigid_body_event_channel;
			EventChannel* _modified_rigid_body_event_channel;
			EventChannel* _destroyed_rigid_body_event_channel;
			EventChannel* _new_box_collider_channel;
			EventChannel* _modified_box_collider_channel;
			EventChannel* _destroyed_box_collider_channel;
			EventChannel* _new_capsule_collider_channel;
			EventChannel* _modified_capsule_collider_channel;
			EventChannel* _destroyed_capsule_collider_channel;
			EventChannel* _character_controller_jump_event_channel;
			EventChannel* _character_controller_turn_event_channel;
			EventChannel* _character_controller_walk_event_channel;
			EventChannel::SubscriberId _node_world_transform_changed_sid;
			EventChannel::SubscriberId _new_rigid_body_sid;
			EventChannel::SubscriberId _modified_rigid_body_sid;
			EventChannel::SubscriberId _destroyed_rigid_body_sid;
			EventChannel::SubscriberId _new_box_collider_sid;
			EventChannel::SubscriberId _modified_box_collider_sid;
			EventChannel::SubscriberId _destroyed_box_collider_sid;
			EventChannel::SubscriberId _new_capsule_collider_sid;
			EventChannel::SubscriberId _modified_capsule_collider_sid;
			EventChannel::SubscriberId _destroyed_capsule_collider_sid;
			EventChannel::SubscriberId _character_controller_jump_sid;
			EventChannel::SubscriberId _character_controller_turn_sid;
			EventChannel::SubscriberId _character_controller_walk_sid;
        };
    }
}
