// CBoxCollider.h
#pragma once

#include <Core/Math/Vec3.h>
#include "../../Component.h"

namespace sge
{
	class SGE_ENGINE_API CBoxCollider final : public TComponentInterface<CBoxCollider>
	{
	public:

		SGE_REFLECTED_TYPE;
		struct Data;

		////////////////////
		////   Methods   ///
	public:

        static void register_type(Scene& scene);

        void reset(Data& data);

		float width() const;

        void width(float value);

        float height() const;

        void height(float value);

        float depth() const;

        void depth(float value);

        Vec3 shape() const;

        void shape(const Vec3& shape);

		//////////////////
		///   Fields   ///
	private:

		Data* _data = nullptr;
	};
}
