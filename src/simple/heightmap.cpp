#include "heightmap.h"
#include "dynamicmaterial.h"
#include "scene/scene.h"
#include "scene/mesh.h"
#include "model/mesh.h"
#include <base/collision.h>


SimpleHeightmap::SimpleHeightmap() : m_width(0), m_height(0), m_drawable(0), m_mesh(0) {
}
SimpleHeightmap::~SimpleHeightmap() {
	if(m_drawable) delete m_drawable;
	if(m_mesh) delete m_mesh;
}

void SimpleHeightmap::setup(int w, int h, float r) {
	m_width = w;
	m_height = h;
	m_resolution = r;
}

void SimpleHeightmap::create(int w, int h, float res, const ubyte* data, int stride, float scale, float offset) {
	setup(w,h,res);
	float* vx = new float[w*h*6];
	for(int x=0; x<w; ++x) for(int y=0; y<h; ++y) {
		float* v = vx + (x + y*w) * 6;
		v[0] = x * res;
		v[1] = data[x+y*w] * scale + offset;
		v[2] = y * res;
	}
	createMesh(vx);
}

void SimpleHeightmap::create(int w, int h, float res, const float* data) {
	setup(w,h,res);
	float* vx = new float[w*h*6];
	for(int x=0; x<w; ++x) for(int y=0; y<h; ++y) {
		float* v = vx + (x + y*w) * 6;
		v[0] = x * res;
		v[1] = data[x+y*w];
		v[2] = y * res;
	}
	createMesh(vx);
}

void SimpleHeightmap::create(int w, int h, float res, const float height) {
	setup(w,h,res);
	float* vx = new float[w*h*6];
	for(int x=0; x<w; ++x) for(int y=0; y<h; ++y) {
		float* v = vx + (x + y*w) * 6;
		v[0] = x * res;
		v[1] = height;
		v[2] = y * res;
	}
	createMesh(vx);
}

void SimpleHeightmap::calculateNormals(const Rect& r) {
	int w = m_width, h = m_height;
	const vec3* ap[6];
	int normalOffset = m_mesh->getVertexBuffer()->getAttribute(base::VA_NORMAL)->offset / sizeof(float);
	for(int x=r.x; x<r.x+r.width; ++x) for(int y=r.y; y<r.y+r.height; ++y) {
		int k = x + y * w;
		const vec3& p = m_mesh->getVertex(k);
		ap[0] = y?         &m_mesh->getVertex(k-w):   0;
		ap[1] = y&&x<w-1?  &m_mesh->getVertex(k-w+1): 0;
		ap[2] = x<w-1?     &m_mesh->getVertex(k+1):   0;
		ap[3] = y<h-1?     &m_mesh->getVertex(k+w):   0;
		ap[4] = x&&y<h-1?  &m_mesh->getVertex(k+w-1): 0;
		ap[5] = x?         &m_mesh->getVertex(k-1):   0;

		float* vp = m_mesh->getVertexBuffer()->getVertex(k);
		vec3& normal = *reinterpret_cast<vec3*>(vp + normalOffset);
		normal.set(0,0,0);
		for(int i=0, j=5; i<6; j=i, ++i)
			if(ap[i] && ap[j]) normal += (*ap[i] - p).cross( *ap[j] - p );
		normal.normalise();
	}
}

template<typename T> int createIndices(int w, int h, T* ix) {
	int i = 0;
	for(int y=1; y<h; ++y) {
		for(int x=0; x<w; ++x) {
			ix[i]     = x + y * w - w;
			ix[i + 1] = x + y * w;
			i += 2;
		}
		ix[i] = ix[i-1];
		ix[i+1] = y*w;
		i += 2;
	}
	return i;
}
void SimpleHeightmap::createMesh(float* vx) {
	int w = m_width, h = m_height;
	// Create index array


	// Create mesh
	if(!m_mesh) m_mesh = new base::bmodel::Mesh();
	m_mesh->setPolygonMode(base::bmodel::TRIANGLE_STRIP);
	base::HardwareVertexBuffer* vbuf = new base::HardwareVertexBuffer();
	vbuf->setData(vx, w*h, 6*sizeof(float), true);
	vbuf->setAttribute(base::VA_VERTEX, base::VA_FLOAT3, 0);
	vbuf->setAttribute(base::VA_NORMAL, base::VA_FLOAT3, 3*sizeof(float));
	m_mesh->setVertexBuffer(vbuf);

	// Creae index buffer
	base::HardwareIndexBuffer* ibuf;
	int i = m_width * (m_height-1) * 2 + m_height * 2;
	if(i<65536) {
		uint16* ix = new uint16[i];
		i = createIndices(m_width, m_height, ix);
		ibuf = new base::HardwareIndexBuffer(16);
		ibuf->setData(ix, i, true);
	}
	else {
		uint* ix = new uint[i];
		i = createIndices(m_width, m_height, ix);
		ibuf = new base::HardwareIndexBuffer(32);
		ibuf->setData(ix, i, true);
	}
	m_mesh->setIndexBuffer(ibuf);

	// Normals
	calculateNormals( Rect(0,0,w,h) );

	// Drawable
	if(m_drawable) asm("int $3"); // already exists
	m_drawable = new DrawableMesh(m_mesh);
}

void SimpleHeightmap::setMaterial(scene::Material* m) {
	if(!m_drawable) return;
	m_drawable->setMaterial(m);
}
void SimpleHeightmap::addToScene(Scene* r) {
	if(!m_drawable) return;
	r->add(m_drawable);
}
void SimpleHeightmap::removeFromScene(Scene* r) {
	if(!m_drawable) return;
	r->remove(m_drawable);
}

// =================================== //

float SimpleHeightmap::getHeight(int x, int y) const {
	if(!m_drawable) return 0;
	if(x < 0 || y < 0 || x >= m_width || y >= m_height) return 0;
	return m_mesh->getVertex(x + y * m_width).y;
}

const vec3& SimpleHeightmap::getNormal(int x, int y) const {
	static const vec3 up(0,1,0);
	if(!m_drawable) return up;
	if(x < 0 || y < 0 || x >= m_width || y >= m_height) return up;
	return m_mesh->getNormal(x + y * m_width);
}

float SimpleHeightmap::height(float x, float y) const {
	float fx = x / m_resolution;
	float fy = y / m_resolution;
	int ix = (int) floor(fx); fx -= ix;
	int iy = (int) floor(fy); fy -= iy;
	int side = fx + fy < 1? 0: 1;
	float v = side? 1-fy: fx;
	float w = side? 1-fx: fy;
	float u = 1 - v - w;
	return u * getHeight(ix+side, iy+side) + v * getHeight(ix+1, iy) + w * getHeight(ix, iy+1);
}

float SimpleHeightmap::height(float x, float y, vec3& n) const {
	float fx = x / m_resolution;
	float fy = y / m_resolution;
	int ix = (int) floor(fx); fx -= ix;
	int iy = (int) floor(fy); fy -= iy;
	// Barycentric coords
	int side = fx + fy < 1? 0: 1;
	float v = side? 1-fy: fx;
	float w = side? 1-fx: fy;
	float u = 1 - v - w;
	
	n = u * getNormal(ix+side, iy+side) + v * getNormal(ix+1, iy) + w * getNormal(ix, iy+1);
	return u * getHeight(ix+side, iy+side) + v * getHeight(ix+1, iy) + w * getHeight(ix, iy+1);
}

int SimpleHeightmap::ray(const vec3& s, const vec3& d, vec3& out) const {
	float t;
	int r = ray(s, d, t);
	if(r) out = s + d * t;
	return r;
}

int SimpleHeightmap::ray(const vec3& s, const vec3& d, float& out) const {
	// Bresenham thingy, or just pure brute force ...
	
	// just test all polygons for now
	bool hit = false;
	float t, best = 1e8f;
	const vec3 end = s + d;
	const int w = m_width;
	for(int x=1; x<m_width; ++x) for(int y=1; y<m_height; ++y) {
		const vec3& v11 = m_mesh->getVertex(x+y*w);
		const vec3& v10 = m_mesh->getVertex(x-1+y*w);
		const vec3& v01 = m_mesh->getVertex(x+y*w-w);
		const vec3& v00 = m_mesh->getVertex(x-1+y*w-w);
		// Intersect triangles
		if(base::intersectLineTriangle(s,end, v00,v10,v01, t) && t<best && t>0) hit=true, best = out = t;
		if(base::intersectLineTriangle(s,end, v11,v01,v10, t) && t<best && t>0) hit=true, best = out = t;
	}
	return hit;
}

// =================================================================================================== //


int SimpleHeightmapEditor::setHeights(const Rect& r, const float* data) {
	int x0 = r.x<0? 0: r.x;
	int x1 = r.right()<m_map->m_width? r.right(): m_map->m_width;
	int y0 = r.y<0? 0: r.y;
	int y1 = r.bottom()<m_map->m_height? r.bottom(): m_map->m_height;
	
	int w = m_map->m_width;
	int offset = m_map->m_mesh->getVertexBuffer()->getAttribute(base::VA_VERTEX)->offset / sizeof(float);
	int stride = m_map->m_mesh->getVertexBuffer()->getStride() / sizeof(float);
	float* vp  = m_map->m_mesh->getVertexBuffer()->getVertex(0) + offset + 1;
	for(int x=x0; x<x1; ++x) for(int y=y0; y<y1; ++y) {
		float h = data[ (x-r.x) + (y-r.y)*r.width ];
		vp[ (x+y*w) * stride ] = h;
	}
	// TODO: Edge modes: zero, edge, continue, wrap
	return 1;
}
int SimpleHeightmapEditor::getHeights(const Rect& r, float* data) const {
	int x0 = r.x<0? 0: r.x;
	int x1 = r.right()<m_map->m_width? r.right(): m_map->m_width;
	int y0 = r.y<0? 0: r.y;
	int y1 = r.bottom()<m_map->m_height? r.bottom(): m_map->m_height;
	memset(data, 0, r.width*r.height*sizeof(float));
	
	int w = m_map->m_width;
	int offset = m_map->m_mesh->getVertexBuffer()->getAttribute(base::VA_VERTEX)->offset / sizeof(float);
	int stride = m_map->m_mesh->getVertexBuffer()->getStride() / sizeof(float);
	float* vp  = m_map->m_mesh->getVertexBuffer()->getVertex(0) + offset + 1;
	for(int x=x0; x<x1; ++x) for(int y=y0; y<y1; ++y) {
		float h = vp[ (x+y*w) * stride ];
		data[ (x-r.x) + (y-r.y)*r.width ] = h;
	}
	// Update normals
	Rect clipped(x0, y0, x1-x0, y1-y0);
	m_map->calculateNormals(clipped);
	return 1;
}

void SimpleHeightmapEditor::setMaterial(const DynamicMaterial* m) {
	m_map->setMaterial( m->getMaterial() );
	m->setCoordinates( vec2(m_map->m_width*m_map->m_resolution, m_map->m_height*m_map->m_resolution), vec2(0,0) );
}



