// GLStaticMesh.h
#pragma once

#include <array>
#include <Core/Math/Vec3.h>
#include <Core/Math/IVec3.h>
#include <Core/Math/IVec2.h>
#include "glew.h"

namespace sge
{
	namespace gl_render
	{
        namespace gl_static_mesh
        {
            enum BufferIndex
            {
                POSITION_BUFFER_INDEX,
                NORMAL_BUFFER_INDEX,
                TANGENT_BUFFER_INDEX,
                BITANGENT_BUFFER_INDEX,
                BITANGENT_SIGN_BUFFER_INDEX,
                MATERIAL_UV_BUFFER_INDEX,
                LIGHTMAP_UV_BUFFER_INDEX,
                NUM_VERTEX_BUFFERS
            };

            struct MeshSlice
            {
                GLuint material = 0;
                uint32 start_element_index = 0;
                uint32 num_element_indices = 0;
            };

            struct StaticMesh
            {
                GLuint vao = 0;
                GLuint ebo = 0;
                GLint num_total_elements = 0;
                std::array<GLuint, NUM_VERTEX_BUFFERS> vertex_buffers;
                std::vector<MeshSlice> material_slices;
            };

            /**
             * \brief Uploads vertex data for the given static mesh.
             * \param num_vertices The number of vertices in the mesh.
             * \param vertex_positions Array of vertex positions.
             * \param vertex_normals Array of vertex normals.
             * \param vertex_tangents Array of vertex tangents.
             * \param vertex_bitangent_signs Array of bitangent signs.
             * \param material_uv_coords Array of material UV coordinates.
             * \param lightmap_uv_coords Array of lightmap UV coordaintes.
             */
            void upload_static_mesh_vertex_data(
                GLuint vao,
                const GLuint vertex_buffers[],
                std::size_t num_vertices,
                const Vec3* vertex_positions,
                const HalfVec3* vertex_normals,
                const HalfVec3* vertex_tangents,
                const int8* vertex_bitangent_signs,
                const UHalfVec2* material_uv_coords,
                const UHalfVec2* lightmap_uv_coords);

            /**
             * \brief Uploads the given element buffer.
             * \param num_elements The number of elements to upload.
             * \param elements The elements to upload.
             * \param vao The VAO of the mesh to associate the element buffer with.
             * \param ebo The buffer to store the elements in.
             */
            void upload_static_mesh_elements(
                GLuint vao,
                GLuint ebo,
                std::size_t num_elements,
                const uint32* elements);
        }
	}
}
