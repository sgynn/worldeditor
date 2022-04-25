#pragma once

#include <base/math.h>
#include <vector>

namespace base { class Mesh; }

class WaterSystem {
	public:
	WaterSystem();
	~WaterSystem();
	base::Mesh* buildGeometry(const BoundingBox& bounds, float resolution=1, base::Mesh* mesh=0) const;

	struct SplineNode { vec3 point; vec3 direction; float a=2,b=2; };
	struct RiverNode : public SplineNode { float left=2,right=2; float speed=1; };
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

	River* traceRiver(const Ray& ray, float& t) const;
	Lake* traceLake(const Ray& ray, float& t) const;

	protected:
	std::vector<River*> m_rivers;
	std::vector<Lake*> m_lakes;
};


