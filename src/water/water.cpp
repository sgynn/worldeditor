#include "water.h"
#include <base/mesh.h>
#include <base/hardwarebuffer.h>
#include <base/collision.h>
#include <base/debuggeometry.h>
#include <cstring>
#include <cstdio>

using namespace base;

WaterSystem::WaterSystem() {
}

WaterSystem::~WaterSystem() {
	for(River* r: m_rivers) delete r;
	for(Lake* l: m_lakes) delete l;
}

WaterSystem::River* WaterSystem::addRiver() {
	River* river = new River;
	m_rivers.push_back(river);
	return river;
}

WaterSystem::Lake* WaterSystem::addLake(bool inside) {
	Lake* lake = new Lake;
	lake->inside = inside;
	m_lakes.push_back(lake);
	return lake;
}

void WaterSystem::destroyRiver(River* r) {
	for(size_t i=0; i<m_rivers.size(); ++i) {
		if(m_rivers[i] == r) {
			m_rivers[i] = m_rivers.back();
			m_rivers.pop_back();
			delete r;
		}
	}
}


void WaterSystem::destroyLake(Lake* l) {
	for(size_t i=0; i<m_lakes.size(); ++i) {
		if(m_lakes[i] == l) {
			m_lakes[i] = m_lakes.back();
			m_lakes.pop_back();
			delete l;
		}
	}
}

// ============================================================== //

template<class V>
V bezierPoint(const V* points, float t) {
	static const int factorial[] = { 1,1,2,6,24,120 };
	V result;
	for(int k=0; k<4; ++k) {
		float kt = t>0||k>0? powf(t,k): 1;
		float rt = t<1||k<3? powf(1-t,3-k): 1;
		float ni = factorial[3] / (factorial[k] * factorial[3-k]);
		float bias = ni * kt * rt;
		result += points[k] * bias;
	}
	return result;
}

class Bezier {
	public:
	Bezier(const vec3& a, const vec3& ad, const vec3& b, const vec3& bd, int steps=8) : segments(steps) {
		data[0] = a.xz();
		data[1] = a.xz() + ad.xz();
		data[2] = b.xz() + bd.xz();
		data[3] = b.xz();
	}
	Bezier(vec2&& a, vec2&& ad, vec2&& b, vec2&& bd, int steps=8) : segments(steps) {
		data[0] = a;
		data[1] = a + ad;
		data[2] = b + bd;
		data[3] = b;
	}
	// Returns interpolant. Max is squared
	float closestPoint(const vec2& target, float& max) const {
		const float step = 1.f/segments;
		vec2 last = data[0];
		float t = -1;
		for(int i=1; i<=segments; ++i) {
			vec2 p = bezierPoint(data, i*step);
			vec2 d = p - last;
			float st = d.dot(target-last) / d.dot(d);
			st = st>0? st<1? st: 1: 0;
			float dist = target.distance2(last+d*st);
			if(dist < max) {
				t = i + st*step;
				max = dist;
			}
			last = p;
		}
		return t;
	}
	bool intersects(const BoundingBox2D& box) const {
		const float step = 1.f/segments;
		float t;
		int hits = 0;
		vec3 last = data[0].xzy();
		vec3 ctr = box.centre().xzy();
		vec3 extent = box.size().xzy(1) * 0.5;
		for(int i=1; i<=segments; ++i) {
			vec3 p = bezierPoint(data, i*step).xzy();
			if(base::intersectRayAABB(last, p-last, ctr, extent, t) && t>=0 && t<=1) return true;
			last = p;
		}
		return hits;
	}
	// Returns number of intersections
	int intersections(const vec2& a, const vec2& b) const {
		const float step = 1.f/segments;
		float s, t;
		int hits = 0;
		vec2 last = data[0];
		for(int i=1; i<=segments; ++i) {
			vec2 p = bezierPoint(data, i*step);
			if(base::intersectLines(last, p, a, b, s, t) && s>=0&&s<1 && t>0 && t<1) ++hits;
			last = p;
		}
		return hits;
	}
	BoundingBox2D bounds() const {
		BoundingBox2D box(data[0]);
		box.include(data[1]);
		box.include(data[2]);
		box.include(data[3]);
		return box;
	}
	private:
	vec2 data[4];
	int segments;
};


// ============================================================== //


template<class F> static void testBeziers(const WaterSystem::River* river, F&& func) {
	if(!river || river->nodes.size() < 2) return;
	const vec3 up(0,1,0);
	for(size_t i=1; i<river->nodes.size(); ++i) {
		const WaterSystem::RiverNode& a = river->nodes[i-1];
		const WaterSystem::RiverNode& b = river->nodes[i];
		vec3 na = a.direction.cross(up);
		vec3 nb = b.direction.cross(up);
		Bezier left (a.point - na*a.left,  a.direction*a.b, b.point - nb*b.left,  -b.direction*b.a);
		if(!func(left)) return;
		Bezier right(a.point + na*a.right, a.direction*a.b, b.point + nb*b.right, -b.direction*b.a);
		if(!func(right)) return;
	}
	// Caps
	const WaterSystem::RiverNode& first = river->nodes[0];
	vec2 na = first.direction.cross(up).xz();
	Bezier startCap(first.point.xz()-na*first.left, vec2(), first.point.xz()+na*first.right, vec2(), 1);
	if(!func(startCap)) return;

	const WaterSystem::RiverNode& last = river->nodes.back();
	vec2 nb = last.direction.cross(up).xz();
	Bezier endCap(last.point.xz()-nb*last.left, vec2(), last.point.xz()+nb*last.right, vec2(), 1);
	if(!func(endCap)) return;
}
template<class F> static void testBeziers(const WaterSystem::Lake* lake, F&& func) {
	if(!lake || lake->nodes.size()<2) return;
	for(size_t i=lake->nodes.size()-1, j=0; j<lake->nodes.size(); i=j++) {
		const auto& n = lake->nodes;
		Bezier bezier(n[i].point, n[i].direction*n[i].b, n[j].point, -n[j].direction*n[j].a);
		if(!func(bezier)) return;
	}
}

template<class W>
static bool inside(const W* water, const vec2& point) {
	int hits = 0;
	const vec2 somewhere(1e8f, 2e6f);
	testBeziers(water, [&](const Bezier& b) { hits += b.intersections(point, somewhere); return true; });
	return hits&1;
}

template<class W>
static bool testBox(const W* water, const BoundingBox2D& box) {
	bool hit = false;
	testBeziers(water, [&](const Bezier& b) { hit=b.intersects(box); return !hit; });
	return hit;
}
template<class W>
static BoundingBox2D getBounds(const W* water) {
	BoundingBox2D box;
	box.setInvalid();
	testBeziers(water, [&box](const Bezier& b) { box.include(b.bounds()); return true; });
	return box;
}

template<class W>
static float getClosestEdge(const W* water, const vec2& point, float& limit) {
	float result = -1;
	testBeziers(water, [&](const Bezier& bezier) {
		float t = bezier.closestPoint(point, limit);
		if(t>=0) result = t; // FIXME: result needs bezier index
		return true;
	});
	return result;
}

static float getClosestCore(const WaterSystem::River* river, const vec2& point, float& limit) {
	if(!river || river->nodes.size() < 2) return -1;
	float result = -1;
	for(size_t i=1; i<river->nodes.size(); ++i) {
		const WaterSystem::RiverNode& a = river->nodes[i-1];
		const WaterSystem::RiverNode& b = river->nodes[i];
		Bezier bezier(a.point,  a.direction*a.b, b.point,  -b.direction*b.a);
		float t = bezier.closestPoint(point, limit);
		if(t>=0) result = t + i;
	}
	return result;
}

template<class T> inline void rasterise(const T* water, std::vector<bool>& cells, int stride, int w, int h, float resolution) {
	BoundingBox2D bounds = getBounds(water);
	Point a(floor(bounds.min.x/resolution), floor(bounds.min.y/resolution));
	Point b(ceil(bounds.max.x/resolution), ceil(bounds.max.y/resolution));
	if(a.x<0) a.x = 0;
	if(a.y<0) a.y = 0;
	if(b.x>=w) b.x = w - 1;
	if(b.y>=h) b.y = h - 1;
	BoundingBox2D cell;
	for(int x=a.x; x<=b.x; ++x) {
		cell.min.x = x*resolution;
		cell.max.x = cell.min.x + resolution;
		int state=0;
		for(int y=a.y; y<=b.y; ++y) {
			if(cells[x+y*stride]) continue;
			cell.min.y = y*resolution;
			cell.max.y = cell.min.y + resolution;
			if(testBox(water, cell)) state = 1;
			else if(state==1) {
				if(inside(water, cell.centre())) state = 2;
				else state = 0;
			}
			cells[x+y*stride] = state > 0;
		}
	}
}



base::Mesh* WaterSystem::buildGeometry(const BoundingBox& box, float resolution, base::Mesh* mesh) const {
	if(m_lakes.empty() && m_rivers.empty()) return 0;

	// Test all points - this can easily be multithreaded
	vec2 start = box.min.xz();
	int countX = (int)floor(box.size().x / resolution) + 1;
	int countY = (int)floor(box.size().z / resolution) + 1;

	// Second version - rasterisation
	std::vector<bool> cells;
	cells.resize(countX * countY, false);
	BoundingBox2D cell(0,0,resolution,resolution);

	// Do each component separately
	for(River* river: m_rivers) rasterise(river, cells, countX, countX-1, countY-1, resolution);
	for(Lake* lake: m_lakes) rasterise(lake, cells, countX, countX-1, countY-1, resolution);

	auto requiredVertex = [&cells, &countX](int x, int y) {
		int k = x+y*countX;
		return cells[k] || (x&&cells[k-1]) || (y&&cells[k-countX]) || (x&&y&&cells[k-countX-1]);
	};


	// Count vertices
	int vertexCount = 0;
	int indexCount = 0;
	for(int x=0; x<countX; ++x) {
		for(int y=0;y<countY; ++y) {
			if(requiredVertex(x,y)) vertexCount += 1;
			if(cells[x+y*countX]) indexCount += 6;
		}
	}
	float* vertices = new float[vertexCount * 10]; // pos3, normal3, velocity2, wave1, edge1

	printf("Water mesh: %d vertices\n", vertexCount);

	// Build mesh
	vec2 pos;
	vec3 control[4];
	Lake* lastLake = 0;
	int currentIndex = 0;
	uint16* indexMap = new uint16[countX*countY];
	memset(indexMap, 0xff, countX*countY*2);
	for(int x=0; x<countX; ++x) {
		pos.x = start.x + x * resolution;
		for(int y=0; y<countY; ++y) {
			if(requiredVertex(x, y)) {
				pos.y = start.y + y * resolution;
				float* vx = vertices + currentIndex*10;
				float height = 10; // FIXME values
				float waves = 0;
				float distanceToEdge = 0;
				vec3 normal(0,1,0);
				vec2 velocity(0,1);

				if(lastLake && !inside(lastLake, pos)) lastLake = 0;
				if(!lastLake) for(Lake* l: m_lakes) if(inside(l, pos)) { lastLake = l; break; };
				if(lastLake) height = lastLake->nodes[0].point.y;
				else {
					float t = 0;
					float limit = 100;
					River* river = 0;
					for(River* r: m_rivers) {
						float rt = getClosestCore(r, pos, limit);
						if(rt>=0) { t=rt; river=r; };
					}
					for(Lake* lake : m_lakes) {
						float lt = getClosestEdge(lake, pos, limit);
						if(lt>=0) { river=0; lastLake=lake; height=lake->nodes[0].point.y; };
					}
					if(river) {
						const RiverNode& a = river->nodes[(int)t];
						const RiverNode& b = river->nodes[(int)t+1];
						control[0] = control[1] = a.point;
						control[2] = control[3] = b.point;
						control[1] += a.direction*a.b;
						control[2] -= b.direction*b.a;
						t -= floor(t);
						height = bezierPoint(control, t).y;
					}
				}

				vx[0] = pos.x;
				vx[1] = height;
				vx[2] = pos.y;
				vx[3] = normal.x;
				vx[4] = normal.y;
				vx[5] = normal.z;
				vx[6] = velocity.x;
				vx[7] = velocity.y;
				vx[8] = waves;
				vx[9] = distanceToEdge;
				indexMap[x+y*countX] = currentIndex;
				++currentIndex;
			}
		}
	}

	// Index buffer
	uint16* indices = new uint16[indexCount];
	uint16* ix = indices;
	for(int x=0; x<countX-1; ++x) {
		for(int y=0; y<countY-1; ++y) {
			int k = x + y * countX;
			if(cells[k]) {
				ix[1] = indexMap[k];
				ix[0] = indexMap[k+1];
				ix[2] = indexMap[k+countX];
				ix += 3;
				
				ix[1] = indexMap[k+1];
				ix[0] = indexMap[k+1+countX];
				ix[2] = indexMap[k+countX];
				ix += 3;
			}
		}
	}

	// Output!
	if(!mesh) {
		mesh = new Mesh();
		HardwareVertexBuffer* vbuf = new HardwareVertexBuffer();
		HardwareIndexBuffer* ibuf = new HardwareIndexBuffer();
		vbuf->attributes.add(VA_VERTEX, VA_FLOAT3);
		vbuf->attributes.add(VA_NORMAL, VA_FLOAT3);
		vbuf->attributes.add(VA_CUSTOM, VA_FLOAT2, "velocity");
		vbuf->attributes.add(VA_CUSTOM, VA_FLOAT2, "info");
		vbuf->createBuffer();
		ibuf->createBuffer();
		mesh->setVertexBuffer(vbuf);
		mesh->setIndexBuffer(ibuf);
	}

	mesh->getVertexBuffer()->setData(vertices, vertexCount, 10*4, true);
	mesh->getIndexBuffer()->setData(indices, indexCount);
	return mesh;
}


