#include "dynamicmap.h"
#include "dynamicmaterial.h"
#include "streaming/landscape.h"
#include "scene/renderer.h"
#include "scene/scene.h"
#include "scene/shader.h"
#include "scene/drawable.h"
#include <base/collision.h>
#include <base/opengl.h>
#include <base/camera.h>

class DynamicHeightmapDrawable : public scene::Drawable {
	Landscape* m_land;
	public:
	DynamicHeightmapDrawable(Landscape* land) : m_land(land) {}
	void draw(scene::RenderState& r) {
		base::Camera cam = *r.getCamera();
		cam.setPosition( cam.getPosition() - vec3(&getTransform()[12]));
		cam.updateFrustum();
		m_land->update( &cam );
		m_land->cull( &cam );

		const int stride = 10 * sizeof(float);
		r.setMaterial( m_material );
		r.setAttributeArrays(scene::VERTEX_ARRAY | scene::NORMAL_ARRAY);
		for(const PatchGeometry* g: m_land->getGeometry()) {
			scene::Shader::current().setAttributePointer(0, 3, GL_FLOAT, stride, scene::SA_FLOAT, g->vertices);
			scene::Shader::current().setAttributePointer(1, 3, GL_FLOAT, stride, scene::SA_FLOAT, g->vertices+3);
			glDrawElements(GL_TRIANGLE_STRIP, g->indexCount, GL_UNSIGNED_SHORT, g->indices);
		}
	}
};

// =================================== //

DynamicHeightmap::DynamicHeightmap() : m_width(0), m_height(0), m_resolution(0), m_heightData(0), m_land(0), m_material(0) {
}
DynamicHeightmap::~DynamicHeightmap() {
	delete m_land;
	delete [] m_heightData;
	for(scene::Drawable* d: m_drawables) delete d;
	delete m_material;
}

void DynamicHeightmap::setup(int w, int h, float r) {
	m_width = w;
	m_height = h;
	m_resolution = r;
	delete [] m_heightData;
	int p = 0;
	while((1<<p)<w) ++p;
	m_heightData = new float[w*h];
	m_land = new Landscape(w&~1);
	m_land->setLimits(0, p-3);
}

void DynamicHeightmap::create(int w, int h, float res, const ubyte* data, int stride, float scale, float offset) {
	setup(w,h,res);
	int size = w * h;
	for(int i=0; i<size; ++i) m_heightData[i] = data[i] * scale + offset;
	m_land->setHeightFunction( bind(this, &DynamicHeightmap::heightFunc) );
}

void DynamicHeightmap::create(int w, int h, float res, const float* data) {
	setup(w,h,res);
	int size = w * h;
	for(int i=0; i<size; ++i) m_heightData[i] = data[i];
	m_land->setHeightFunction( bind(this, &DynamicHeightmap::heightFunc) );
}

void DynamicHeightmap::create(int w, int h, float res, const float height) {
	setup(w,h,res);
	int size = w * h;
	for(int i=0; i<size; ++i) m_heightData[i] = height;
	m_land->setHeightFunction( bind(this, &DynamicHeightmap::heightFunc) );
}

void DynamicHeightmap::setMaterial(DynamicMaterial* dyn, const MapList& maps) {
	dyn->setCoordinates( vec2(m_width*m_resolution, m_height*m_resolution), vec2(0,0) );

	// Instanciate dynamic material
	delete m_material;
	m_material = dyn->getMaterial()->clone();
	// Apply maps
	for(MaterialLayer* layer : *dyn) {
		if(layer->mapIndex && layer->mapIndex<maps.size()) {
			EditableMap* map = maps[layer->mapIndex];
			if(!map) continue;

			char mapName[32];
			sprintf(mapName, "map%u", layer->mapIndex);
			m_material->getPass(0)->setTexture( mapName, map->getTexture() );
			
			if(layer->mapData&0x100) {
				sprintf(mapName, "map%dW", layer->mapIndex);
				m_material->getPass(0)->setTexture( mapName, map->getTexture(1) );
			}
		}
	}
	setMaterial( m_material );
	m_material->getPass(0)->compile();
}
void DynamicHeightmap::setMaterial(scene::Material* m) {
	for(scene::Drawable* d: m_drawables) d->setMaterial(m);
}

void DynamicHeightmap::setDetail(float value) {
	if(m_land) m_land->setThreshold(value);
}

scene::Drawable* DynamicHeightmap::createDrawable() {
	if(!m_land) return 0;
	DynamicHeightmapDrawable* d = new DynamicHeightmapDrawable(m_land);
	m_drawables.push_back(d);
	d->setMaterial(m_material);
	return d;
}

// =================================== //

float DynamicHeightmap::getHeight(const vec3& p) const {
	return height(p.x, p.z);
}

float DynamicHeightmap::getHeight(int x, int y) const {
	if(!m_heightData) return 0;
	if(x<0) x=0;
	else if(x>=m_width) x=m_width-1;
	if(y<0) y=0;
	else if(y>=m_height) y=m_height-1;
	return m_heightData[x + y * m_width];
}

vec3 DynamicHeightmap::getNormal(int x, int y) const {
	static const vec3 up(0,1,0);
	if(!m_heightData) return up;
	if(x < 0 || y < 0 || x >= m_width || y >= m_height) return up;
	// Get normal from renderer
	vec3 normal;
	m_land->getHeight(x, y, normal);
	return normal;
}

float DynamicHeightmap::height(float x, float y) const {
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

float DynamicHeightmap::height(float x, float y, vec3& n) const {
	float fx = x / m_resolution;
	float fy = y / m_resolution;
	int ix = (int) floor(fx); fx -= ix;
	int iy = (int) floor(fy); fy -= iy;
	// Barycentric coords
	int side = fx + fy < 1? 0: 1;
	float v = side? 1-fy: fx;
	float w = side? 1-fx: fy;
	float u = 1 - v - w;
	
	m_land->getHeight(x, y, n);
	return u * getHeight(ix+side, iy+side) + v * getHeight(ix+1, iy) + w * getHeight(ix, iy+1);
}

float DynamicHeightmap::heightFunc(const vec3& p) {
	return height(p.x, p.z);
}

int DynamicHeightmap::castRay(const vec3& start, const vec3& direction, float& out) const {
	if(!m_land) return 0;
	vec3 point, normal;
	int r = m_land->intersect(start, start+direction*1e6f, point, normal);
	if(r) out = direction.dot(point-start) / direction.dot(direction);
	return r;
}


// =================================================================================================== //


void DynamicHeightmapEditor::getValue(int x, int y, float* values) const {
	values[0] = m_map->m_heightData[x + y * m_map->m_width];
}

void DynamicHeightmapEditor::setValue(int x, int y, const float* values) {
	m_map->m_heightData[x + y * m_map->m_width] = values[0];
}

void DynamicHeightmapEditor::apply(const Rect& r) {
	if(r.width>0 && r.height>0) {
		const float res = m_map->m_resolution;
		m_map->m_land->updateGeometry( BoundingBox(r.left()*res, 0, r.top()*res, r.right()*res, 0, r.bottom()*res), true );
	}
}



