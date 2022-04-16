#pragma once

#include <base/math.h>
#include <vector>

namespace base { class Mesh; }

class WaterSystem {
	public:
	WaterSystem();
	~WaterSystem();
	base::Mesh* buildGeometry(const BoundingBox& bounds, float resolution=1, base::Mesh* mesh=0) const;

	struct SplineNode { vec3 point; vec3 direction; float a=1,b=1; };
	struct RiverNode : public SplineNode { float left=1,right=1; float speed=1; };
	struct River {
		std::vector<RiverNode> nodes;
	};
	struct Lake {
		std::vector<SplineNode> nodes;
		bool inside;
	};

	River* addRiver();
	Lake* addLake(bool inside);
	void destroyRiver(River*);
	void destroyLake(Lake*);
	const std::vector<River*>& rivers() { return m_rivers; }
	const std::vector<Lake*>& lakes() { return m_lakes; }

	protected:
	bool inside(River* river, const vec2& point, River* last) const;
	bool inside(Lake* lake, const vec2& point) const;
	vec3 getNearestEdge(River* river, const vec2& point) const;
	vec3 getNearestEdge(Lake* lake, const vec2& point) const;

	protected:
	std::vector<River*> m_rivers;
	std::vector<Lake*> m_lakes;
};


