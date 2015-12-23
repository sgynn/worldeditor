#include "heightmap.h"
#include "render/render.h"
#include "dynamicmaterial.h"


SimpleHeightmap::SimpleHeightmap() : m_width(0), m_height(0), m_drawable(0) {
}
SimpleHeightmap::~SimpleHeightmap() {
	if(m_drawable) delete m_drawable;
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
	createDrawable(vx);
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
	createDrawable(vx);
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
	createDrawable(vx);
}

void SimpleHeightmap::calculateNormals(const Rect& r) {
	int w = m_width, h = m_height;
	const vec3* ap[6];
	for(int x=r.x; x<r.x+r.width; ++x) for(int y=r.y; y<r.y+r.height; ++y) {
		int k = x + y * w;
		const vec3& p = m_drawable->getVertex(k);
		ap[0] = y?         &m_drawable->getVertex(k-w):   0;
		ap[1] = y&&x<w-1?  &m_drawable->getVertex(k-w+1): 0;
		ap[2] = x<w-1?     &m_drawable->getVertex(k+1):   0;
		ap[3] = y<h-1?     &m_drawable->getVertex(k+w):   0;
		ap[4] = x&&y<h-1?  &m_drawable->getVertex(k+w-1): 0;
		ap[5] = x?         &m_drawable->getVertex(k-1):   0;

		vec3 normal;
		for(int i=0, j=5; i<6; j=i, ++i)
			if(ap[i] && ap[j]) normal += (*ap[i] - p).cross( *ap[j] - p );
		normal.normalise();
		float* np = m_drawable->getData() + k * 6;
		memcpy(np, normal, sizeof(vec3));
	}
}

void SimpleHeightmap::createDrawable(float* vx) {
	int w = m_width, h = m_height;
	// Create index array
	int i = 0;
	uint16* ix = new uint16[w * h * 2 + 2 * h];
	for(int y=0; y<h; ++y) {
		for(int x=0; x<w; ++x) {
			ix[i] = x + y * w;
			ix[i + 1] = x + y * w + w;
			i += 2;
		}
		ix[i] = ix[i+1] = y * w + w;
		i += 2;
	}


	// Create drawable
	if(m_drawable) delete m_drawable;
	m_drawable = new DMesh;
	m_drawable->setVertices(w * h, vx, base::model::VERTEX3);
	m_drawable->setIndices(i, ix);
	calculateNormals( Rect(0,0,w,h) );
}

void SimpleHeightmap::setMaterial(Material* m) {
	if(!m_drawable) return;
	m_drawable->setMaterial(m);
}
void SimpleHeightmap::addToScene(Render* r) {
	if(!m_drawable) return;
	r->add(m_drawable);
}
void SimpleHeightmap::removeFromScene(Render* r) {
	if(!m_drawable) return;
	r->remove(m_drawable);
}

// =================================== //

float SimpleHeightmap::getHeight(int x, int y) const {
	if(!m_drawable) return 0;
	if(x < 0 || y >0 || x >= m_width || y >= m_height) return 0;
	return m_drawable->getVertex(x + y * m_width).y;
}

const vec3& SimpleHeightmap::getNormal(int x, int y) const {
	static const vec3 up(0,1,0);
	if(!m_drawable) return up;
	if(x < 0 || y >0 || x >= m_width || y >= m_height) return up;
	return m_drawable->getNormal(x + y * m_width);
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
	return 0;
}

// =================================================================================================== //


int SimpleHeightmapEditor::setHeights(const Rect& r, const float* data) {
	return 0;
}
int SimpleHeightmapEditor::getHeights(const Rect& r, float* data) const {
	return 0;
}

void SimpleHeightmapEditor::setMaterial(const DynamicMaterial* m) {
	m_map->setMaterial( m->getMaterial() );
}



