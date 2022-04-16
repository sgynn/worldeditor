#include "water.h"
#include <base/drawablemesh.h>
#include <base/mesh.h>
#include <base/collision.h>
#include <cstring>

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

// ============================================================== //

bool WaterSystem::inside(River* river, const vec2& point, River* last) const {
	// intersect line from point with end caps and offset segments.
	// be careful of inverted bits from an inner curve being too wide
	// perhaps use separate splines for edges
	// Need last river to get best one if point is inside multiple rivers
	return false;
}

bool WaterSystem::inside(Lake* lake, const vec2& point) const {
	if(!lake || lake->nodes.size()<2) return false;
	int hits = 0;
	vec3 points[4];
	vec2 somewhere(1e8f, 2e6f);
	float s,t;
	for(size_t i=lake->nodes.size()-1, j=0; j<lake->nodes.size(); i=j++) {
		points[0] = lake->nodes[i].point;
		points[1] = lake->nodes[i].point + lake->nodes[i].direction * lake->nodes[i].b;
		points[2] = lake->nodes[j].point - lake->nodes[j].direction * lake->nodes[j].a;
		points[3] = lake->nodes[j].point;
		vec2 a = points[0].xz();
		for(int k=1; k<=8; ++k) {
			vec2 b = bezierPoint(points, k/8.f).xz();
			if(base::intersectLines(a, b, point, somewhere, s, t)) {
				if(s>0&&s<1 && t>0 && t<1) ++hits;
			}
			a = b;
		}
	}
	return hits&1;
}

vec3 WaterSystem::getNearestEdge(River* river, const vec2& point) const {
	return vec3();
}

vec3 WaterSystem::getNearestEdge(Lake* lake, const vec2& point) const {
	if(!lake || lake->nodes.empty()) return vec3();
	vec3 result;
	float distance = 1e8f;
	vec3 points[3];
	for(size_t i=lake->nodes.size()-1, j=0; j<lake->nodes.size(); i=j++) {
		points[0] = lake->nodes[i].point;
		points[1] = lake->nodes[i].point + lake->nodes[i].direction * lake->nodes[i].b;
		points[2] = lake->nodes[j].point - lake->nodes[j].direction * lake->nodes[j].a;
		points[3] = lake->nodes[j].point;
		vec3 a = points[0];
		for(int k=1; k<=8; ++k) {
			vec3 b = bezierPoint(points, k/8.f);
			vec3 c = base::closestPointOnLine(point.xzy(0), a, b);
			float d = c.xz().distance2(point);
			if(d<distance) result=c, distance=d;
			a = b;
		}
	}
	return result;
}

base::Drawable* WaterSystem::buildGeometry(const BoundingBox& box, float resolution) const {
	if(m_lakes.empty() && m_rivers.empty()) return 0;

	// Test all points - this can easily be multithreaded
	vec2 start = box.min.xz();
	int countX = (int)floor(box.size().x / resolution);
	int countY = (int)floor(box.size().z / resolution);
	static const uint16 none = 0xffff;

	// Work out all of this on back threads then just copy into vertex buffer on main thread
	struct Info {
		uint8 flags; 	// river, lake, adjacent
		uint16 index;	// River or Lake index
		float height;	// water height for this point
		vec3 normal;	// water normal
		vec2 velocity;	// Water velocity
		vec3 edge;		// closest edge point
	};

	vec2 point;
	uint16 index = none;
	uint16* info = new uint16[countX * countY];
	memset(info, 0xff, countX * countY * 2);
	for(int x=0; x<countX; ++x) {
		point.x = start.x + x * resolution;
		for(int y=0; y<countY; ++y) {
			point.y = start.y + y * resolution;
			int k = x + y * countX;
			River* last = 0;
			for(size_t i=0; i<m_rivers.size(); ++i) {
				if(inside(m_rivers[i], point, last)) {
					last = m_rivers[i];
					info[k] = i;
				}
			}
			if(index!=none) {
				// Quite likely to be the same as previous test
				if(x>0 && info[k-1]&0x8000 && inside(m_lakes[info[k-1]&0x1fff], point)) {
					info[k] = info[k-1];
				}
				else for(size_t i=0; i<m_lakes.size(); ++i) {
					if(inside(m_lakes[i], point)) {
						info[k] = i | 0x8000;
						break;
					}
				}
			}

			// Border
			if(x>0&&info[k-1]==none && info[k]!=none) info[k-1] = info[k] | 0x4000;
			if(y>0&&info[k-countX]==none && info[k]!=none) info[k-countX] = info[k] | 0x4000;
			if(info[k]!=none && x<countX-1) info[k+1] = info[k] | 0x2000;
			if(info[k]!=none && y<countX-countX) info[k+countX] = info[k] | 0x2000;
		}
	}

	// Count vertices
	int ss = countX * countY;
	int vertexCount = 0;
	for(int i=0; i<ss; ++i) if(info[i] != none) ++vertexCount;
	float* data = new float[vertexCount * 10]; // pos3, normal3, velocity2, wave1, edge1

	
	// Build mesh
	int currentIndex = 0;
	uint* indexMap = new uint[countX*countY];
	memset(indexMap, 0xff, countX*countY*4);
	for(int x=0; x<countX; ++x) {
		float posX = start.x + x * resolution;
		for(int y=0; y<countY; ++y) {
			if(info[x+y*countX] != none) {
				indexMap[x+y*countX] = currentIndex;
				float* vx = data + currentIndex*10;
				float height = 0; // FIXME values
				float waves = 0;
				float distanceToEdge = 0;
				vec3 normal(0,1,0);
				vec2 velocity(0,1);

				vx[0] = posX;
				vx[1] = height;
				vx[2] = start.y + y * resolution;
				vx[3] = normal.x;
				vx[4] = normal.y;
				vx[5] = normal.z;
				vx[6] = velocity.x;
				vx[7] = velocity.y;
				vx[8] = waves;
				vx[9] = distanceToEdge;
				++currentIndex;
			}
		}
	}

	// count indices
	int indexCount = 0;
	for(int x=0; x<countX-1; ++x) {
		for(int y=0; y<countY-1; ++y) {
			int k = x + y * countX;
			if(~(info[k]|info[k+1]|info[k+countX])&0x2000) indexCount += 3;
			if(~(info[k+1+countX]|info[k+1]|info[k+countX])&0x2000) indexCount += 3;
		}
	}

	// Index buffer
	uint* indices = new uint[indexCount];
	uint* ix = indices;
	for(int x=0; x<countX-1; ++x) {
		for(int y=0; y<countY-1; ++y) {
			int k = x + y * countX;
			if(~(info[k]|info[k+1]|info[k+countX])&0x2000) {
				ix[0] = indexMap[k];
				ix[1] = indexMap[k+1];
				ix[2] = indexMap[k+countX];
				ix += 3;
			}
			if(~(info[k+1+countX]|info[k+1]|info[k+countX])&0x2000) {
				ix[1] = indexMap[k+1];
				ix[0] = indexMap[k+1+countX];
				ix[2] = indexMap[k+countX];
				ix += 3;
			}
		}
	}

	// Output!
	return 0;

}


