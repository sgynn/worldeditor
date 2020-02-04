#include "foliage.h"
#include "scene/mesh.h"
#include "model/mesh.h"
#include "model/hardwarebuffer.h"
#include <cstdio>

using base::bmodel::Mesh;
using scene::DrawableMesh;
using scene::Material;
using namespace base;

class RNG {
	unsigned m_seed;
	public:
	RNG(unsigned seed=0) : m_seed(seed) {}
	unsigned rand() { m_seed = m_seed * 1103515245 + 12345; return m_seed&0x7fffffff; }
	float    randf() { return (float)rand() / 0x7fffffff; }
	float    randf(const Rangef& r) { return r.min + randf() * r.size(); }
};

// ===================================================================================================== //

FoliageMap::FoliageMap(int w, int h, unsigned char* data, int stride) : m_data(0), m_ref(0) {
	setData(w,h,data,stride);
}
FoliageMap::~FoliageMap() {
	delete [] m_data;
}
void FoliageMap::setData(int w, int h, unsigned char* data, int stride) {
	delete [] m_data;
	int length = w * h;
	m_data = new unsigned char[length];
	for(int i=0; i<length; ++i) m_data[i] = data[i*stride];
	m_width = w;
	m_height = h;
}
float FoliageMap::getValue(float x, float y) const {
	x *= m_width - 1;
	y *= m_height - 1;
	x = x<0? 0: x>=m_width? m_width-1.0001: x;
	y = y<0? 0: y>=m_height? m_height-1.0001: y;
	// Values
	int ix = floor(x);
	int iy = floor(y);
	unsigned char* k = m_data + ix + iy * m_width;
	float a = k[0] * (ix-x+1) + k[m_width] * (x-ix);
	float b = k[1] * (ix-x+1) + k[m_width+1] * (x-ix);
	return (a * (iy-y+1) + b * (y-iy)) / 255.f;
}

// ===================================================================================================== //

FoliageLayer::FoliageLayer(float cs, float r) : m_parent(0), m_material(0), m_chunkSize(cs), m_range(r), m_density(1), m_scaleRange(1), m_densityMap(0) {
}
FoliageLayer::~FoliageLayer() {
	deleteMap(m_densityMap);
	clear();
}

void FoliageLayer::setViewRange(float r) { m_range = r; }
void FoliageLayer::setDensity(float d) { m_density = d; }
void FoliageLayer::setHeightRange(float min, float max) { m_heightRange.set(min, max); }
void FoliageLayer::setSlopeRange(float min, float max)  { m_slopeRange.set(min, max); }
void FoliageLayer::setScaleRange(float min, float max)  { m_scaleRange.set(min, max); }
void FoliageLayer::setMaterial(Material* m) {
	m_material = m;
	for(auto& c: m_chunks) {
		if(c.second->mesh) c.second->mesh->setMaterial(m);
	}
}
void FoliageLayer::setMapBounds(const BoundingBox2D& b) { m_mapBounds = b; }
void FoliageLayer::setDensityMap(FoliageMap* map) {
	deleteMap(m_densityMap);
	m_densityMap = referenceMap(map);
	m_densityMap = map;
}

void FoliageLayer::clear() {
	for(auto& i:m_chunks) deleteChunk(i.second);
	m_chunks.clear();
}

void FoliageLayer::regenerate() {
	for(auto& i:m_chunks) {
		if(i.second->state != EMPTY) {
			detach(i.second->mesh);
			destroyChunk(*i.second);
			i.second->mesh = 0;
			i.second->state = EMPTY;
			m_parent->queueChunk(this, i.first, i.second);
		}
	}
}

FoliageMap* FoliageLayer::referenceMap(FoliageMap* map) {
	if(map) ++map->m_ref;
	return map;
}
void FoliageLayer::deleteMap(FoliageMap*& map) {
	if(map && --map->m_ref<=0) delete map;
	map = 0;
}

// ----------------------------------------------------------------------------------------------------- //

inline float FoliageLayer::getMapValue(const FoliageMap* map, const vec3& point) const {
	return m_parent->getMapValue(map, m_mapBounds, point);
}

int FoliageLayer::generatePoints(const Index& index, int count, vec3* corners, const vec3& up, PointList& out) const {
	bool testHeight = m_heightRange.isValid();
	bool testSlope = m_slopeRange.isValid();

	RNG rng(index.x * 54321 + index.y + 7126);
	vec3 t0, t1, pos;
	GenPoint point;
	float height;
	size_t start = out.size();
	out.reserve(start + count);
	for(int i=0; i<count; ++i) {
		float rx = rng.randf();
		float ry = rng.randf();
		t0 = lerp(corners[0], corners[1], rx);
		t1 = lerp(corners[2], corners[3], rx);
		pos = lerp(t0, t1, ry);

		m_parent->resolvePosition(pos, point.position, height);
		if(testHeight && !m_heightRange.contains(height)) continue;
		m_parent->resolveNormal(pos, point.normal);
		if(testSlope && !m_slopeRange.contains(1 - point.normal.dot(up))) continue;

		if(m_densityMap) {
			float d = getMapValue(m_densityMap, point.position);
			if(rng.randf() > d) continue;
		}

		out.push_back(point);
	}
	return out.size() - start;
}

int FoliageLayer::generatePoints(const Index& index, PointList& points, vec3& up) const {
	vec3 corners[4];
	float amount = m_density * m_chunkSize * m_chunkSize;
	m_parent->getCorners(index, m_chunkSize, corners, up);
	return generatePoints(index, amount, corners, up, points);
}

// ----------------------------------------------------------------------------------------------------- //

void FoliageLayer::update(const vec3& context) {
	IndexList active;
	for(auto& ci: m_chunks) ci.second->active = false;
	m_parent->getActive(context, m_chunkSize, m_range, active);
	// Activate any new chunks
	for(Index& index: active) {
		auto c = m_chunks.find(index);
		if(c == m_chunks.end()) {
			Chunk* chunk = new Chunk();
			chunk->state = EMPTY;
			chunk->mesh = 0;
			c = m_chunks.insert( std::make_pair(index, chunk) ).first;
			m_parent->queueChunk(this, index, chunk);
		}
		c->second->active = true;
		// Completed
		if(c->second->state == GENERATED) {
			c->second->state = COMPLETE;
			DrawableMesh* d = c->second->mesh;
			if(d) {
				d->getMesh()->getVertexBuffer()->createBuffer();
				d->getMesh()->getIndexBuffer()->createBuffer();
				if(d->getInstanceBuffer()) d->getInstanceBuffer()->createBuffer();
				attach(c->second->mesh);
			}
		}
	}
	// delete inactive chunks
	for(auto i=m_chunks.begin(); i!=m_chunks.end();) {
		if(!i->second->active) {
			if(i->second->mesh) detach(i->second->mesh);
			deleteChunk(i->second);
			i = m_chunks.erase(i);
		}
		else ++i;
	}
}

bool FoliageLayer::deleteChunk(Chunk* chunk) {
	if(m_parent->cancelChunk(chunk)) {
		destroyChunk(*chunk);
		delete chunk->mesh;
		delete chunk;
		return true;
	}
	return false;
}

// ===================================================================================================== //


FoliageInstanceLayer::FoliageInstanceLayer(float cs, float r) : FoliageLayer(cs,r), m_mesh(0), m_alignMode(VERTICAL) {}
void FoliageInstanceLayer::setMesh(Mesh* mesh) { m_mesh = mesh; }
void FoliageInstanceLayer::setAlignment(OrientaionMode m, const Rangef& r) { m_alignMode = m; m_alignRange = r; }
DrawableMesh* FoliageInstanceLayer::generateGeometry(const Index& index) const {
	if(!m_mesh || !m_material) return 0;
	vec3 up;
	PointList points;
	generatePoints(index, points, up);
	if(points.empty()) return 0;
	static const vec3 unitY(0,1,0);
	Quaternion q, a, rot = Quaternion::arc(unitY, up );

	RNG rng(0);
	float* data = new float[ points.size() * 8 ]; // Instance buffer data
	float* vx = data;
	for(GenPoint& point: points) {
		float angle = rng.randf() * PI;
		a.w = cos(angle);
		a.y = sin(angle);

		switch(m_alignMode) {
		case VERTICAL: q = rot; break;
		case NORMAL:   q = Quaternion::arc( unitY, point.normal); break;
		case ABSOLUTE: q.fromAxis( up.cross(point.normal), rng.randf(m_alignRange) ); break;
		case RELATIVE: q = slerp(rot, Quaternion::arc( unitY, point.normal ), rng.randf(m_alignRange)); break;
		}

		memcpy(vx+0, point.position, sizeof(vec3));
		memcpy(vx+4, a * q, sizeof(Quaternion));
		vx[3] = rng.randf(m_scaleRange);
	}

	// Create instance buffer
	HardwareVertexBuffer* buffer = new HardwareVertexBuffer();
	buffer->setData(data, points.size(), 32, true);
	buffer->setAttribute(base::VA_CUSTOM, base::VA_FLOAT4, 0, "loc", 1);
	buffer->setAttribute(base::VA_CUSTOM, base::VA_FLOAT4, 0, "rot", 1);

	DrawableMesh* d = new DrawableMesh(m_mesh, m_material);
	d->setInstanceBuffer(buffer);
	d->setInstanceCount(points.size());
	return d;
}


// ===================================================================================================== //

GrassLayer::GrassLayer(float cs, float r): FoliageLayer(cs,r), m_size(1,1), m_tiles(1), m_scaleMap(0) {}
GrassLayer::~GrassLayer() { deleteMap(m_scaleMap); }
void GrassLayer::setSpriteSize(float w, float h, int tile) {
	m_size.set(w,h);
	m_tiles = tile<1? 1: tile;
}
void GrassLayer::setScaleMap(FoliageMap* map, float min, float max) {
	deleteMap(m_scaleMap);
	m_scaleMap = referenceMap(map);
	m_scaleMapRange.set(min, max);
}
DrawableMesh* GrassLayer::generateGeometry(const Index& index) const {
	if(!m_material) return 0;
	vec3 up;
	PointList points;
	generatePoints(index, points, up);
	if(points.empty()) return 0;
	Quaternion rot = Quaternion::arc( vec3(0,1,0), up );
	float hw = m_size.x / 2;
	float lean = 0.4;
	up *= m_size.y;

	RNG rng(0);
	vec3 direction;
	float* vdata = new float[ points.size() * 4 * 8 ];
	unsigned short* idata = new unsigned short[ points.size() * 6 ];
	float* vx = vdata;
	unsigned short* ix = idata;
	unsigned short k = 0;
	for(GenPoint& point: points) {
		// Random direction
		float angle = rng.randf() * TWOPI;
		float s = rng.randf(m_scaleRange);
		if(m_scaleMap) {
			float ms = getMapValue(m_scaleMap, point.position);
			s *= m_scaleMapRange.min + ms * m_scaleMapRange.size();
		}

		direction.set(sin(angle)*hw*s, 0, cos(angle)*hw*s);
		direction = rot * direction;
		vec3 top = up * s + vec3(direction.z*lean,0,-direction.x*lean);
		// Positiion
		memcpy(vx+0,  point.position - direction,        sizeof(vec3));
		memcpy(vx+8,  point.position - direction + top, sizeof(vec3));
		memcpy(vx+16, point.position + direction + top, sizeof(vec3));
		memcpy(vx+24, point.position + direction,        sizeof(vec3));
		// Normal
		memcpy(vx+3,  point.normal, sizeof(vec3));
		memcpy(vx+11, point.normal, sizeof(vec3));
		memcpy(vx+19, point.normal, sizeof(vec3));
		memcpy(vx+27, point.normal, sizeof(vec3));
		// UVs
		vx[6] = vx[14] = vx[15] = vx[23] = 0;
		vx[7] = vx[22] = vx[30] = vx[31] = 1;
		// Index
		ix[0] = ix[3] = k;
		ix[1] = k+1;
		ix[2] = ix[4] = k+2;
		ix[5] = k+3;
		// Next
		k += 4;
		ix += 6;
		vx += 4*8;
	}

	// Build buffer objects
	HardwareVertexBuffer* vbuffer = new HardwareVertexBuffer();
	vbuffer->setData(vdata, points.size()*4, 32, true);
	vbuffer->setAttribute(VA_VERTEX, VA_FLOAT3, 0);
	vbuffer->setAttribute(VA_NORMAL, VA_FLOAT3, 12);
	vbuffer->setAttribute(VA_TEXCOORD, VA_FLOAT2, 24);

	HardwareIndexBuffer* ibuffer = new HardwareIndexBuffer();
	ibuffer->setData(idata, points.size()*6, true);

	// Drawable
	Mesh* mesh = new Mesh();
	mesh->setPolygonMode(base::bmodel::TRIANGLES);
	mesh->setVertexBuffer(vbuffer);
	mesh->setIndexBuffer(ibuffer);
	return new DrawableMesh(mesh, m_material);
}

void GrassLayer::destroyChunk(const Chunk& chunk) const {
	if(chunk.mesh) {
		delete chunk.mesh->getMesh();
		chunk.mesh->setMesh(0);
	}
}


// ===================================================================================================== //


FoliageSystem::FoliageSystem(int threads) : m_threads(0), m_threadCount(threads), m_running(true) {
	if(threads) m_threads = new Thread[threads];
	for(int i=0; i<threads; ++i) m_threads[i].begin(this, &FoliageSystem::threadFunc, i);
}
FoliageSystem::~FoliageSystem() {
	m_running = false;
	for(int i=0; i<m_threadCount; ++i) m_threads[i].join();
	for(FoliageLayer* layer: m_layers) {
		delete layer;
	}
	delete [] m_threads;
}
void FoliageSystem::addLayer(FoliageLayer* l) {
	l->m_parent = this;
	m_layers.push_back(l);
	addChild(l);
}

void FoliageSystem::removeLayer(FoliageLayer* l) {
	removeChild(l);
	for(size_t i=0; i<m_layers.size(); ++i) {
		if(m_layers[i] == l) {
			m_layers[i] = m_layers.back();
			m_layers.pop_back();
			break;
		}
	}
}

void FoliageSystem::update(const vec3& context) {
	// Single thread version
	if(!m_threads && !m_queue.empty()) {
		m_queue.back().chunk->mesh =  m_queue.back().layer->generateGeometry(m_queue.back().index);
		m_queue.back().chunk->state = FoliageLayer::GENERATED;
		m_queue.pop_back();
	}

	for(FoliageLayer* layer : m_layers) layer->update(context);
}


// ----------------------------------------------------------------------------------------------------- //

void FoliageSystem::queueChunk(FoliageLayer* layer, const Index& index, FoliageLayer::Chunk* chunk) {
	MutexLock scopedLock(m_mutex);
	m_queue.push_back( GenChunk { layer, index, chunk } );
}
bool FoliageSystem::cancelChunk(FoliageLayer::Chunk* chunk) {
	if(chunk->state > FoliageLayer::GENERATING) return true;
	if(chunk->state == FoliageLayer::GENERATING) return false;
	MutexLock scopedLock(m_mutex);
	for(size_t i=0; i<m_queue.size(); ++i) {
		if(m_queue[i].chunk == chunk) {
			m_queue[i] = m_queue.back();
			m_queue.pop_back();
			return true;
		}
	}
	return true;
}
void FoliageSystem::threadFunc(int index) {
	GenChunk current{0,Index(),0};
	while(m_running) {
		m_mutex.lock();
		if(m_queue.empty()) current.chunk = 0;
		else {
			current = m_queue.back();
			m_queue.pop_back();
			current.chunk->state = FoliageLayer::GENERATING;
		}
		m_mutex.unlock();

		// Generate this chunk
		if(current.chunk) {
			current.chunk->mesh = current.layer->generateGeometry(current.index);
			current.chunk->state = FoliageLayer::GENERATED;
		}
		else Thread::sleep(1);
	}
}

// ----------------------- Default interface --------------------------- //


int FoliageSystem::getActive(const vec3& p, float cs, float range, IndexList& out) const {
	Point a( floor((p.x-range)/cs), floor((p.z-range)/cs) );
	Point b( ceil((p.x+range)/cs), ceil((p.z+range)/cs) );
	for(Point r=a; r.x<b.x; ++r.x) for(r.y=a.y; r.y<b.y; ++r.y) out.push_back(r);
	return out.size();
}

void FoliageSystem::getCorners(const Index& ix, float cs, vec3* out, vec3& up) const {
	up.set(0,1,0);
	out[0].set(ix.x*cs,    0, ix.y*cs);
	out[1].set(ix.x*cs+cs, 0, ix.y*cs);
	out[2].set(ix.x*cs,    0, ix.y*cs+cs);
	out[3].set(ix.x*cs+cs, 0, ix.y*cs+cs);
}

float FoliageSystem::getMapValue(const FoliageMap* map, const BoundingBox2D& bounds, const vec3& pos) const {
	vec2 p = (pos.xz() - bounds.min) / bounds.size();
	return map->getValue(p.x, p.y);
}

