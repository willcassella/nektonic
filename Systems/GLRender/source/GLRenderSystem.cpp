// GLRenderSystem.cpp

#include <iostream>
#include <Core/Reflection/ReflectionBuilder.h>
#include <Resource/Archives/JsonArchive.h>
#include <Engine/Components/CTransform3D.h>
#include <Engine/Components/Display/CStaticMesh.h>
#include <Engine/Components/Display/CCamera.h>
#include <Engine/Components/Display/CLightMaskReceiver.h>
#include <Engine/Components/Display/CLightMaskObstructor.h>
#include <Engine/Tags/DebugDraw.h>
#include <Engine/Scene.h>
#include <Engine/SystemFrame.h>
#include <Engine/UpdatePipeline.h>
#include <Engine/Util/VectorUtils.h>
#include "../include/GLRender/Config.h"
#include "../private/GLRenderSystemState.h"
#include "../private/DebugLine.h"
#include "../private/Util.h"

SGE_REFLECT_TYPE(sge::gl_render::GLRenderSystem);

namespace sge
{
	namespace gl_render
	{
        static void set_render_target_params()
        {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        }

        static void upload_render_target_data(
            GLsizei width,
            GLsizei height,
            GLenum internal_format,
            GLenum upload_format,
            GLenum upload_type)
        {
            glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0, upload_format, upload_type, nullptr);
        }

		static void attach_framebuffer_layer(
			GLuint layer,
			GLenum attachment)
		{
			glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, layer, 0);
		}

        static GLuint create_viewport_program(const GLShader& v_shader, const GLShader& f_shader)
        {
            // Create the program and attach shaders
            const auto program = glCreateProgram();
            glAttachShader(program, v_shader.id());
            glAttachShader(program, f_shader.id());

            // Bind vertex attributes
            glBindAttribLocation(program, GLMaterial::POSITION_ATTRIB_LOCATION, GLMaterial::POSITION_ATTRIB_NAME);
            glBindAttribLocation(program, GLMaterial::TEXCOORD_ATTRIB_LOCATION, GLMaterial::TEXCOORD_ATTRIB_NAME);

            // Link the program and detach shaders
            glLinkProgram(program);
            glDetachShader(program, v_shader.id());
            glDetachShader(program, f_shader.id());

            // Make sure the program compiled linked
            debug_program_status(program, GLDebugOutputMode::ONLY_ERROR);

            // Set uniforms
            glUseProgram(program);
            glUniform1i(glGetUniformLocation(program, "depth_buffer"), 0);
            glUniform1i(glGetUniformLocation(program, "position_buffer"), 1);
            glUniform1i(glGetUniformLocation(program, "normal_buffer"), 2);
            glUniform1i(glGetUniformLocation(program, "albedo_buffer"), 3);
            glUniform1i(glGetUniformLocation(program, "roughness_metallic_buffer"), 4);

            // Return it
            return program;
        }

        /* These constants specify the attachment index for buffer layers. */
		static constexpr GLenum GBUFFER_DEPTH_STENCIL_ATTACHMENT = GL_DEPTH_STENCIL_ATTACHMENT;
		static constexpr GLenum GBUFFER_POSITION_ATTACHMENT = GL_COLOR_ATTACHMENT0;
		static constexpr GLenum GBUFFER_NORMAL_ATTACHMENT = GL_COLOR_ATTACHMENT1;
		static constexpr GLenum GBUFFER_ALBEDO_ATTACHMENT = GL_COLOR_ATTACHMENT2;
		static constexpr GLenum GBUFFER_ROUGHNESS_METALLIC_ATTACHMENT = GL_COLOR_ATTACHMENT3;
        static constexpr GLenum POST_BUFFER_HDR_ATTACHMENT = GL_COLOR_ATTACHMENT0;

        /* These constants define the internal format for the gbuffer layers. */
        static constexpr GLenum GBUFFER_DEPTH_STENCIL_INTERNAL_FORMAT = GL_DEPTH24_STENCIL8;
        static constexpr GLenum GBUFFER_POSITION_INTERNAL_FORMAT = GL_RGB32F;
        static constexpr GLenum GBUFFER_NORMAL_INTERNAL_FORMAT = GL_RGB32F;
        static constexpr GLenum GBUFFER_ALBEDO_INTERNAL_FORMAT = GL_RGB8;
        static constexpr GLenum GBUFFER_ROUGHNESS_METALLIC_INTERNAL_FORMAT = GL_RG16F;
        static constexpr GLenum POST_BUFFER_HDR_INTERNAL_FORMAT = GL_RGB32F;

        /* These constants are used when initializing or resizing gbuffer layers.
         * They're pretty much meaningless, but are used to ensure consistency. */
	    static constexpr GLenum GBUFFER_DEPTH_STENCIL_UPLOAD_FORMAT = GL_DEPTH_STENCIL;
        static constexpr GLenum GBUFFER_DEPTH_STENCIL_UPLOAD_TYPE = GL_UNSIGNED_INT_24_8;
        static constexpr GLenum GBUFFER_POSITION_UPLOAD_FORMAT = GL_RGB;
        static constexpr GLenum GBUFFER_POSITION_UPLOAD_TYPE = GL_FLOAT;
        static constexpr GLenum GBUFFER_NORMAL_UPLOAD_FORMAT = GL_RGB;
        static constexpr GLenum GBUFFER_NORMAL_UPLOAD_TYPE = GL_FLOAT;
        static constexpr GLenum GBUFFER_ALBEDO_UPLOAD_FORMAT = GL_RGB;
        static constexpr GLenum GBUFFER_ALBEDO_UPLOAD_TYPE = GL_UNSIGNED_BYTE;
        static constexpr GLenum GBUFFER_ROUGHNESS_METALLIC_UPLOAD_FORMAT = GL_RG;
        static constexpr GLenum GBUFFER_ROUGHNESS_METALLIC_UPLOAD_TYPE = GL_UNSIGNED_BYTE;
        static constexpr GLenum POST_BUFFER_HDR_UPLOAD_FORMAT = GL_RGB;
        static constexpr GLenum POST_BUFFER_HDR_UPLOAD_TYPE = GL_FLOAT;

		////////////////////////
		///   Constructors   ///

        GLRenderSystem::GLRenderSystem(const Config& config)
        {
            assert(config.validate() /*The given GLRenderSystem config object is not valid*/);

            // Create the internal state object
            _state = std::make_unique<GLRenderSystem::State>();
            _state->width = config.viewport_width;
            _state->height = config.viewport_height;

            // Initialize GLEW
            glewExperimental = GL_TRUE;
            glewInit();
            glGetError(); // Sometimes GLEW initialization generates an error, pop it off the stack.

            // Create the default mesh object
            {
                // Load the mesh
                StaticMesh missing_mesh;
                missing_mesh.from_file(config.missing_mesh.c_str());
                GLStaticMesh gl_missing_mesh{ missing_mesh };

                // Insert it into the resource table
                const auto vao = gl_missing_mesh.vao();
                _state->missing_mesh = vao;
                _state->static_mesh_resources[config.missing_mesh] = vao;
                _state->static_meshes.insert(std::make_pair(vao, std::move(gl_missing_mesh)));
            }

            // Create the default material resource
            {
                // Load the material
                Material missing_material;
                JsonArchive missing_material_archive;
                missing_material_archive.from_file(config.missing_material.c_str());
                missing_material_archive.deserialize_root(missing_material);
                GLMaterial gl_missing_material{ *_state, missing_material };

                // Insert it into the resource table
                const auto id = gl_missing_material.id();
                _state->missing_material = id;
                _state->material_resources[config.missing_material] = id;
                _state->materials.insert(std::make_pair(id, std::move(gl_missing_material)));
            }

            // Initialize OpenGL
            glLineWidth(1);
            glClearColor(0, 0, 0, 1);
            glClearDepth(1.f);
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_LESS);
            glEnable(GL_STENCIL_TEST);
            glEnable(GL_CULL_FACE);
            glCullFace(GL_BACK);
            glFrontFace(GL_CCW);
            glEnable(GL_FRAMEBUFFER_SRGB);

            // Get the default framebuffer
            glGetIntegerv(GL_FRAMEBUFFER_BINDING, &_state->default_framebuffer);

            // Create a framebuffer for deferred rendering (GBuffer)
            glGenFramebuffers(1, &_state->gbuffer_framebuffer);
            glBindFramebuffer(GL_FRAMEBUFFER, _state->gbuffer_framebuffer);
            glGenTextures(GBufferLayer::NUM_LAYERS, _state->gbuffer_layers.data());

            // Create gbuffer depth layer
            glBindTexture(GL_TEXTURE_2D, _state->gbuffer_layers[GBufferLayer::DEPTH_STENCIL]);
            upload_render_target_data(
                _state->width,
                _state->height,
                GBUFFER_DEPTH_STENCIL_INTERNAL_FORMAT,
                GBUFFER_DEPTH_STENCIL_UPLOAD_FORMAT,
                GBUFFER_DEPTH_STENCIL_UPLOAD_TYPE);
            set_render_target_params();
            attach_framebuffer_layer(
                _state->gbuffer_layers[GBufferLayer::DEPTH_STENCIL],
                GBUFFER_DEPTH_STENCIL_ATTACHMENT);

            // Create gbuffer position layer
            glBindTexture(GL_TEXTURE_2D, _state->gbuffer_layers[GBufferLayer::POSITION]);
            upload_render_target_data(
                _state->width,
                _state->height,
                GBUFFER_POSITION_INTERNAL_FORMAT,
                GBUFFER_POSITION_UPLOAD_FORMAT,
                GBUFFER_POSITION_UPLOAD_TYPE);
            set_render_target_params();
            attach_framebuffer_layer(
                _state->gbuffer_layers[GBufferLayer::POSITION],
                GBUFFER_POSITION_ATTACHMENT);

            // Create gbuffer normal layer
            glBindTexture(GL_TEXTURE_2D, _state->gbuffer_layers[GBufferLayer::NORMAL]);
            upload_render_target_data(
                _state->width,
                _state->height,
                GBUFFER_NORMAL_INTERNAL_FORMAT,
                GBUFFER_NORMAL_UPLOAD_FORMAT,
                GBUFFER_NORMAL_UPLOAD_TYPE);
            set_render_target_params();
            attach_framebuffer_layer(
                _state->gbuffer_layers[GBufferLayer::NORMAL],
                GBUFFER_NORMAL_ATTACHMENT);

			// Create gbuffer albedo layer
            glBindTexture(GL_TEXTURE_2D, _state->gbuffer_layers[GBufferLayer::ALBEDO]);
            upload_render_target_data(
                _state->width,
                _state->height,
                GBUFFER_ALBEDO_INTERNAL_FORMAT,
                GBUFFER_ALBEDO_UPLOAD_FORMAT,
                GBUFFER_ALBEDO_UPLOAD_TYPE);
            set_render_target_params();
            attach_framebuffer_layer(
                _state->gbuffer_layers[GBufferLayer::ALBEDO],
                GBUFFER_ALBEDO_ATTACHMENT);

            // Create gbuffer roughness/metallic layer
            glBindTexture(GL_TEXTURE_2D, _state->gbuffer_layers[GBufferLayer::ROUGHNESS_METALLIC]);
            upload_render_target_data(
                _state->width,
                _state->height,
                GBUFFER_ROUGHNESS_METALLIC_INTERNAL_FORMAT,
                GBUFFER_ROUGHNESS_METALLIC_UPLOAD_FORMAT,
                GBUFFER_ROUGHNESS_METALLIC_UPLOAD_TYPE);
            set_render_target_params();
            attach_framebuffer_layer(
                _state->gbuffer_layers[GBufferLayer::ROUGHNESS_METALLIC],
                GBUFFER_ROUGHNESS_METALLIC_ATTACHMENT);

			// Make sure the GBuffer was constructed successfully
			if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
			{
				std::cerr << "GLRenderSystem: Error creating GBuffer." << std::endl;
			}

            // Create a framebuffer for post-processing
            glGenFramebuffers(1, &_state->post_framebuffer);
            glBindFramebuffer(GL_FRAMEBUFFER, _state->post_framebuffer);
            glGenTextures(1, &_state->post_buffer_hdr);

            // Create HDR buffer
            glBindTexture(GL_TEXTURE_2D, _state->post_buffer_hdr);
            upload_render_target_data(
                _state->width,
                _state->height,
                POST_BUFFER_HDR_INTERNAL_FORMAT,
                POST_BUFFER_HDR_UPLOAD_FORMAT,
                POST_BUFFER_HDR_UPLOAD_TYPE);
            set_render_target_params();
            attach_framebuffer_layer(
                _state->post_buffer_hdr,
                POST_BUFFER_HDR_ATTACHMENT);

            // Make sure the post-processing buffer was constructed successfully
            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            {
                std::cerr << "GLRenderSystem: Error creating post-processing framebuffer." << std::endl;
            }

			constexpr float QUAD_VERTEX_DATA[] = {
				-1.f, 1.f, 0.f, 1.f,	// Top-left point
				-1.f, -1.f, 0.f, 0.f,	// Bottom-left point
				1.f, -1.f, 1.f, 0.f,	// Bottom-right point
				1.f, 1.f, 1.f, 1.f, 	// Top-right point
			};

			// Create a VAO for sprite
			glGenVertexArrays(1, &_state->sprite_vao);
			glBindVertexArray(_state->sprite_vao);

			// Create a VBO for the sprite quad and upload data
			glGenBuffers(1, &_state->sprite_vbo);
			glBindBuffer(GL_ARRAY_BUFFER, _state->sprite_vbo);
			glBufferData(GL_ARRAY_BUFFER, sizeof(QUAD_VERTEX_DATA), QUAD_VERTEX_DATA, GL_STATIC_DRAW);

			// Sprite vertex specification
			glEnableVertexAttribArray(GLMaterial::POSITION_ATTRIB_LOCATION);
			glEnableVertexAttribArray(GLMaterial::TEXCOORD_ATTRIB_LOCATION);
			glVertexAttribPointer(GLMaterial::POSITION_ATTRIB_LOCATION, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, nullptr);
			glVertexAttribPointer(GLMaterial::TEXCOORD_ATTRIB_LOCATION, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, (void*)(sizeof(float) * 2));

			// Create screen shaders
			GLShader viewport_v_shader{ GL_VERTEX_SHADER, config.viewport_vert_shader };
			GLShader scene_f_shader{ GL_FRAGMENT_SHADER, config.scene_shader };
            GLShader post_f_shader{ GL_FRAGMENT_SHADER, config.post_shader };

		    // Create the scene shader program
            _state->scene_shader_program = create_viewport_program(viewport_v_shader, scene_f_shader);
            _state->scene_program_view_uniform = glGetUniformLocation(_state->scene_shader_program, "view");

            // Create the post-processing shader program
            _state->post_shader_program = create_viewport_program(viewport_v_shader, post_f_shader);
            glUniform1i(glGetUniformLocation(_state->post_shader_program, "hdr_buffer"), 5);

            // Create a VAO for debug lines
            glGenVertexArrays(1, &_state->debug_line_vao);
            glBindVertexArray(_state->debug_line_vao);

            // Create a vbo for lines
            glGenBuffers(1, &_state->debug_line_vbo);
            glBindBuffer(GL_ARRAY_BUFFER, _state->debug_line_vbo);

            // Line vertex specification
            glEnableVertexAttribArray(DEBUG_LINE_POSITION_ATTRIB_LOCATION);
            glEnableVertexAttribArray(DEBUG_LINE_COLOR_ATTRIB_LOCATION);
            glVertexAttribPointer(
                DEBUG_LINE_POSITION_ATTRIB_LOCATION,
                3,
                GL_FLOAT,
                GL_FALSE,
                sizeof(DebugLineVert),
                nullptr);
            glVertexAttribPointer(
                DEBUG_LINE_COLOR_ATTRIB_LOCATION,
                3,
                GL_FLOAT,
                GL_FALSE,
                sizeof(DebugLineVert),
                (void*)offsetof(DebugLineVert, color_rgb));

            // Create debug line program
            GLShader debug_line_v_shader{ GL_VERTEX_SHADER, config.debug_line_vert_shader };
            GLShader debug_line_f_shader{ GL_FRAGMENT_SHADER, config.debug_line_frag_shader };
            _state->debug_line_program = glCreateProgram();
            glAttachShader(_state->debug_line_program, debug_line_v_shader.id());
            glAttachShader(_state->debug_line_program, debug_line_f_shader.id());

            // Bind vertex attributes
            glBindAttribLocation(_state->debug_line_program, DEBUG_LINE_POSITION_ATTRIB_LOCATION, DEBUG_LINE_POSITION_ATTRIB_NAME);
            glBindAttribLocation(_state->debug_line_program, DEBUG_LINE_COLOR_ATTRIB_LOCATION, DEBUG_LINE_COLOR_ATTRIB_NAME);

            // Link the program and detach shaders
            glLinkProgram(_state->debug_line_program);
            glDetachShader(_state->debug_line_program, debug_line_v_shader.id());
            glDetachShader(_state->debug_line_program, debug_line_f_shader.id());

            // Make sure the program linked sucessfully
            debug_program_status(_state->debug_line_program, GLDebugOutputMode::ONLY_ERROR);

            // Get uniforms
            _state->debug_line_view_uniform = glGetUniformLocation(_state->debug_line_program, "view");
            _state->debug_line_proj_uniform = glGetUniformLocation(_state->debug_line_program, "projection");

			const auto error = glGetError();
			if (error != GL_NO_ERROR)
			{
				std::cerr << "GLRenderSystem: An error occurred during startup - " << error << std::endl;
			}
		}

		GLRenderSystem::~GLRenderSystem()
		{
			// Todo
		}

		///////////////////
		///   Methods   ///

		void GLRenderSystem::pipeline_register(UpdatePipeline& pipeline)
		{
            const auto async_token = pipeline.new_async_token();
			const auto system_token = pipeline.register_system_fn(
                "gl_render",
                async_token,
                this,
                &GLRenderSystem::render_scene);

            pipeline.register_tag_callback_any_comp<FDebugLine>(
                system_token,
                async_token,
                TCO_NONE,
                this,
                &GLRenderSystem::cb_debug_draw_line);

            pipeline.register_tag_callback<CTransform3D, FNewComponent>(
                system_token,
                async_token,
                TCO_NONE,
                this,
                &GLRenderSystem::cb_new_transform);

            pipeline.register_tag_callback<CStaticMesh, FNewComponent>(
                system_token,
                async_token,
                TCO_NONE,
                this,
                &GLRenderSystem::cb_new_static_mesh);

            pipeline.register_tag_callback<CStaticMesh, FModifiedComponent>(
                system_token,
                async_token,
                TCO_NONE,
                this,
                &GLRenderSystem::cb_modified_static_mesh);

            pipeline.register_tag_callback<CLightMaskObstructor, FNewComponent>(
                system_token,
                async_token,
                TCO_NONE,
                this,
                &GLRenderSystem::cb_new_lightmask_obstructor);
		}

	    void GLRenderSystem::set_viewport(int width, int height)
	    {
            _state->width = width;
            _state->height = height;

            // Resize depth layer
            glBindTexture(GL_TEXTURE_2D, _state->gbuffer_layers[GBufferLayer::DEPTH_STENCIL]);
            upload_render_target_data(
                width,
                height,
                GBUFFER_DEPTH_STENCIL_INTERNAL_FORMAT,
                GBUFFER_DEPTH_STENCIL_UPLOAD_FORMAT,
                GBUFFER_DEPTH_STENCIL_UPLOAD_TYPE);

            // Resize position layer
            glBindTexture(GL_TEXTURE_2D, _state->gbuffer_layers[GBufferLayer::POSITION]);
            upload_render_target_data(
                width,
                height,
                GBUFFER_POSITION_INTERNAL_FORMAT,
                GBUFFER_POSITION_UPLOAD_FORMAT,
                GBUFFER_POSITION_UPLOAD_TYPE);

            // Resize normal layer
            glBindTexture(GL_TEXTURE_2D, _state->gbuffer_layers[GBufferLayer::NORMAL]);
            upload_render_target_data(
                width,
                height,
                GBUFFER_NORMAL_INTERNAL_FORMAT,
                GBUFFER_NORMAL_UPLOAD_FORMAT,
                GBUFFER_NORMAL_UPLOAD_TYPE);

            // Resize albedo layer
            glBindTexture(GL_TEXTURE_2D, _state->gbuffer_layers[GBufferLayer::ALBEDO]);
            upload_render_target_data(
                width,
                height,
                GBUFFER_ALBEDO_INTERNAL_FORMAT,
                GBUFFER_ALBEDO_UPLOAD_FORMAT,
                GBUFFER_ALBEDO_UPLOAD_TYPE);

            // Resize rougness/metallic layer
            glBindTexture(GL_TEXTURE_2D, _state->gbuffer_layers[GBufferLayer::ROUGHNESS_METALLIC]);
            upload_render_target_data(
                width,
                height,
                GBUFFER_ROUGHNESS_METALLIC_INTERNAL_FORMAT,
                GBUFFER_ROUGHNESS_METALLIC_UPLOAD_FORMAT,
                GBUFFER_ROUGHNESS_METALLIC_UPLOAD_TYPE);

            // Resize post buffer
            glBindTexture(GL_TEXTURE_2D, _state->post_buffer_hdr);
            upload_render_target_data(
                width,
                height,
                POST_BUFFER_HDR_INTERNAL_FORMAT,
                POST_BUFFER_HDR_UPLOAD_FORMAT,
                POST_BUFFER_HDR_UPLOAD_TYPE);
	    }

	    void GLRenderSystem::render_scene(SystemFrame& frame, float current_time, float dt)
		{
            // Initialize the render scene data structure, if we haven't already
            if (!_state->initialized_render_scene)
            {
                render_scene_init(*_state, frame);
                _state->initialized_render_scene = true;
            }

            /*---------------------------*/
            /*---   GBUFFER FILLING   ---*/

			constexpr GLenum GBUFFER_DRAW_BUFFERS[] = {
				GBUFFER_POSITION_ATTACHMENT,
				GBUFFER_NORMAL_ATTACHMENT,
				GBUFFER_ALBEDO_ATTACHMENT,
				GBUFFER_ROUGHNESS_METALLIC_ATTACHMENT };
			constexpr GLsizei NUM_GBUFFER_DRAW_BUFFERS = sizeof(GBUFFER_DRAW_BUFFERS) / sizeof(GLenum);

			// Bind the GBuffer and its sub-buffers for drawing
			glBindFramebuffer(GL_FRAMEBUFFER, _state->gbuffer_framebuffer);
			glDrawBuffers(NUM_GBUFFER_DRAW_BUFFERS, GBUFFER_DRAW_BUFFERS);

			// Clear the GBuffer
            glEnable(GL_STENCIL_TEST);
			glEnable(GL_DEPTH_TEST);
            glEnable(GL_CULL_FACE);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
            glDepthFunc(GL_LEQUAL);

			// Create matrices
			bool hasCamera = false;
			Mat4 view;
			Mat4 proj;

			// Get the first camera in the scene
			frame.process_entities([&](
				ProcessingFrame& /*pframe*/,
				const CTransform3D& transform,
				const CPerspectiveCamera& camera)
			{
				view = transform.get_world_matrix().inverse();
				proj = camera.get_projection_matrix((float)this->_state->width / this->_state->height);
				hasCamera = true;
				return ProcessControl::BREAK;
			});

			// If no camera was found, return
            if (!hasCamera)
            {
                return;
            }

            // Render the scene
            render_scene_update_matrices(*_state, frame);
            render_scene_render_normal(*_state, view, proj);
            render_scene_render_lightmasks(*_state, view, proj);

            // Draw debug lines
            glBindVertexArray(_state->debug_line_vao);
            glUseProgram(_state->debug_line_program);
            glBindBuffer(GL_ARRAY_BUFFER, _state->debug_line_vbo);
            glBufferData(
                GL_ARRAY_BUFFER,
                _state->frame_debug_lines.size() * sizeof(DebugLineVert),
                _state->frame_debug_lines.data(),
                GL_DYNAMIC_DRAW);
		    glUniformMatrix4fv(_state->debug_line_view_uniform, 1, GL_FALSE, view.vec());
		    glUniformMatrix4fv(_state->debug_line_proj_uniform, 1, GL_FALSE, proj.vec());
		    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(_state->frame_debug_lines.size()));
		    _state->frame_debug_lines.clear();

            constexpr GLenum POST_BUFFER_DRAW_BUFFERS[] = {
                POST_BUFFER_HDR_ATTACHMENT,
            };
            constexpr GLsizei NUM_POST_BUFFER_DRAW_BUFFERS = sizeof(POST_BUFFER_DRAW_BUFFERS) / sizeof(GLenum);

            /*-----------------------*/
            /*---   PBR SHADING   ---*/

            glDisable(GL_CULL_FACE);
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_STENCIL_TEST);

            // Bind the screen quad for rasterization (in use for remainder of rendering)
            glBindVertexArray(_state->sprite_vao);

            // Bind the GBuffer's sub-buffers as textures for reading (in use for remainder of rendering)
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, _state->gbuffer_layers[GBufferLayer::DEPTH_STENCIL]);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, _state->gbuffer_layers[GBufferLayer::POSITION]);
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, _state->gbuffer_layers[GBufferLayer::NORMAL]);
            glActiveTexture(GL_TEXTURE3);
            glBindTexture(GL_TEXTURE_2D, _state->gbuffer_layers[GBufferLayer::ALBEDO]);
            glActiveTexture(GL_TEXTURE4);
            glBindTexture(GL_TEXTURE_2D, _state->gbuffer_layers[GBufferLayer::ROUGHNESS_METALLIC]);

            // Bind the post-processing framebuffer for drawing
            glBindFramebuffer(GL_FRAMEBUFFER, _state->post_framebuffer);
            glDrawBuffers(NUM_POST_BUFFER_DRAW_BUFFERS, POST_BUFFER_DRAW_BUFFERS);

            // Bind the scene shading program
            glUseProgram(_state->scene_shader_program);

            // Upload view matrix
            glUniformMatrix4fv(_state->scene_program_view_uniform, 1, GL_FALSE, view.vec());

            // Draw the screen quad
            glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

            /*---------------------------*/
            /*---   POST-PROCESSING   ---*/

            // Bind the post-buffer for reading
            glActiveTexture(GL_TEXTURE5);
            glBindTexture(GL_TEXTURE_2D, _state->post_buffer_hdr);

			// Bind the default framebuffer for drawing
			glBindFramebuffer(GL_FRAMEBUFFER, _state->default_framebuffer);

            // Bind the post-processing shader program
            glUseProgram(_state->post_shader_program);

			// Draw the screen quad
			glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
		}

	    void GLRenderSystem::cb_debug_draw_line(
            SystemFrame& /*frame*/,
            const EntityId* ent_range,
            const TagIndex_t* tag_indices,
            const TagCount_t* counts,
            std::size_t ent_range_len,
            const FDebugLine* lines)
	    {
            std::size_t num_lines = 0;

            // Count up the total number of lines
            for (std::size_t i = 0; i < ent_range_len; ++i)
            {
                num_lines += counts[i];
            }

            // Reserve space for line verts
            _state->frame_debug_lines.reserve(_state->frame_debug_lines.size() + num_lines * 2);

            // For each line
            for (std::size_t i = 0; i < num_lines; ++i)
            {
                // Create start vert
                DebugLineVert start_vert;
                start_vert.world_position = lines[i].world_start;
                start_vert.color_rgb = Vec3{
                    static_cast<Scalar>(lines[i].color.red()) / 255,
                    static_cast<Scalar>(lines[i].color.green()) / 255,
                    static_cast<Scalar>(lines[i].color.blue()) / 255 };

                // Create end vert
                DebugLineVert end_vert;
                end_vert.world_position = lines[i].world_end;
                end_vert.color_rgb = start_vert.color_rgb;

                // Add to the buffer
                _state->frame_debug_lines.push_back(start_vert);
                _state->frame_debug_lines.push_back(end_vert);
            }
	    }

	    void GLRenderSystem::cb_new_transform(
            SystemFrame& /*frame*/,
            const EntityId* ent_range,
            std::size_t range_len)
	    {
            const auto num_dups = insert_ord_entities(_state->render_scene.ord_render_entities, ent_range, range_len);
            _state->render_scene.ord_render_entities_matrices.insert(_state->render_scene.ord_render_entities_matrices.end(), range_len, Mat4{});
            assert(num_dups == 0);
	    }

	    void GLRenderSystem::cb_new_static_mesh(
            SystemFrame& frame,
            const EntityId* ent_range,
            std::size_t range_len)
	    {
            // Allocate space
            auto& render_scene = _state->render_scene;
            const auto old_len = render_scene.ord_mesh_entities.size();
            render_scene.ord_mesh_entities.insert(render_scene.ord_mesh_entities.end(), range_len, NULL_ENTITY);
            render_scene.ord_mesh_entity_meshes.insert(render_scene.ord_mesh_entity_meshes.end(), range_len, 0);
            render_scene.ord_mesh_entity_materials.insert(render_scene.ord_mesh_entity_materials.end(), range_len, 0);

            // Insert new data
		    rev_multi_insert(render_scene.ord_mesh_entities.data(), old_len, old_len + range_len, &ent_range, &range_len, 1,
                [&render_scene](std::size_t new_index, std::size_t old_index) // Swap function
		    {
                render_scene.ord_mesh_entities[new_index] = render_scene.ord_mesh_entities[old_index];
                render_scene.ord_mesh_entity_meshes[new_index] = render_scene.ord_mesh_entity_meshes[old_index];
                render_scene.ord_mesh_entity_materials[new_index] = render_scene.ord_mesh_entity_materials[old_index];
		    },
                [&render_scene](EntityId new_entity, std::size_t index) // Insert function
		    {
                render_scene.ord_mesh_entities[index] = new_entity;
                render_scene.ord_mesh_entity_meshes[index] = 0;
                render_scene.ord_mesh_entity_materials[index] = 0;
            });

            // Get meshes and materials for the new entities
            frame.process_entities(zip(ord_ents_range(ent_range, range_len), ord_ents_range(render_scene.ord_mesh_entities)),
                [&state = *_state, &render_scene = _state->render_scene] (
                    ProcessingFrame& pframe,
                    const CStaticMesh& static_mesh)
            {
                const auto mesh_index = pframe.user_iterator(1);
                render_scene.ord_mesh_entity_meshes[mesh_index] = state.get_static_mesh_resource(static_mesh.mesh());
                render_scene.ord_mesh_entity_materials[mesh_index] = state.get_material_resource(static_mesh.material());
            });
	    }

	    void GLRenderSystem::cb_modified_static_mesh(
            SystemFrame& frame,
            const EntityId* ent_range,
            std::size_t range_len)
	    {
            frame.process_entities(zip(ord_ents_range(ent_range, range_len), ord_ents_range(_state->render_scene.ord_mesh_entities)),
                [&state = *_state, &render_scene = _state->render_scene] (
                    ProcessingFrame& pframe,
                    const CStaticMesh& mesh)
            {
                const auto mesh_index = pframe.user_iterator(1);
                render_scene.ord_mesh_entity_meshes[mesh_index] = state.get_static_mesh_resource(mesh.mesh());
                render_scene.ord_mesh_entity_materials[mesh_index] = state.get_material_resource(mesh.material());
            });
	    }

        void GLRenderSystem::cb_new_lightmask_obstructor(
            SystemFrame& frame,
            const EntityId* ent_range,
            std::size_t range_len)
        {
            insert_ord_entities(_state->render_scene.ord_lightmask_obstructors, ent_range, range_len);
        }
	}
}
