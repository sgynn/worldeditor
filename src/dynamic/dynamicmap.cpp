#include "dynamicmap.h"
#include "dynamicmaterial.h"
#include "streaming/landscape.h"
#include "model/hardwarebuffer.h"
#include "scene/renderer.h"
#include "scene/scene.h"
#include "scene/shader.h"
#include "scene/drawable.h"
#include <base/collision.h>
#include <base/opengl.h>
#include <base/camera.h>

#include "scene/debuggeometry.h"

#ifdef WIN32
extern PFNGLBINDVERTEXARRAYPROC glBindVertexArray;
extern PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays;
#endif


class DynamicHeightmapDrawable : public scene::Drawable {
	Landscape* m_land;
	struct PatchTag {
		base::HardwareVertexBuffer* vertexBuffer;
		base::HardwareIndexBuffer* indexBuffer;
		uint binding;
	};
	public:
	DynamicHeightmapDrawable(Landscape* land) : m_land(land) {
		m_land->setPatchCallbacks( ::bind(this, &DynamicHeightmapDrawable::patchCreated),
		                           ::bind(this, &DynamicHeightmapDrawable::patchDetroyed),
								   ::bind(this, &DynamicHeightmapDrawable::patchUpdated));
	}
	void draw(scene::RenderState& r) {
		base::Camera cam = *r.getCamera();
		cam.setPosition( cam.getPosition() - vec3(&getTransform()[12]));
		cam.updateFrustum();
		m_land->update( &cam );
		m_land->cull( &cam );
		r.setMaterial( m_material );
		for(const PatchGeometry* g: m_land->getGeometry()) {
			PatchTag* tag = (PatchTag*)g->tag;
			if(tag) {
				glBindVertexArray(tag->binding);
				tag->indexBuffer->bind(); // Because it is not part of the buffer for some bizarre reason
				glDrawElements(GL_TRIANGLE_STRIP, g->indexCount, tag->indexBuffer->getDataType(), 0);
			}
		}
	}
	void patchCreated(PatchGeometry* patch) {
		PatchTag* tag = new PatchTag;
		patch->tag = tag;
		tag->vertexBuffer = new base::HardwareVertexBuffer();
		tag->vertexBuffer->attributes.add(base::VA_VERTEX, base::VA_FLOAT3);
		tag->vertexBuffer->attributes.add(base::VA_NORMAL, base::VA_FLOAT3);
		tag->vertexBuffer->attributes.add(base::VA_TANGENT, base::VA_FLOAT4); // actually noormal2,height2 but need something to pad out the stride
		tag->vertexBuffer->setData(patch->vertices, patch->vertexCount, 10*sizeof(float));
		tag->vertexBuffer->createBuffer();
		tag->vertexBuffer->addReference();

		tag->indexBuffer = new base::HardwareIndexBuffer();
		tag->indexBuffer->setData(patch->indices, patch->indexCount);
		tag->indexBuffer->createBuffer();
		tag->indexBuffer->addReference();
		
		m_binding = 0;
		addBuffer(tag->vertexBuffer);
		addBuffer(tag->indexBuffer);
		tag->binding = m_binding;
		m_binding = 0;
	}
	void patchUpdated(PatchGeometry* patch) {
		PatchTag* tag = (PatchTag*)patch->tag;
		if(tag) {
			tag->vertexBuffer->setData(patch->vertices, patch->vertexCount, 10*sizeof(float));
			tag->indexBuffer->setData(patch->indices, patch->indexCount);
		}
	}
	void patchDetroyed(PatchGeometry* patch) {
		PatchTag* tag = (PatchTag*)patch->tag;
		if(tag) {
			glDeleteVertexArrays(1, &tag->binding);
			tag->indexBuffer->dropReference();
			tag->vertexBuffer->dropReference();
			patch->tag = nullptr;
			delete tag;
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
	// Instanciate dynamic material
	delete m_material;
	m_material = dyn->getMaterial()->clone();
	// Apply maps
	for(MaterialLayer* layer : *dyn) {
		if(layer->mapIndex) {
			EditableMap* map = layer->mapIndex<maps.size()? maps[layer->mapIndex]: 0;

			char mapName[32];
			sprintf(mapName, "map%u", layer->mapIndex);
			m_material->getPass(0)->setTexture( mapName, map? map->getTexture(): 0 );
			
			if(layer->mapData&0x100) {
				sprintf(mapName, "map%dW", layer->mapIndex);
				m_material->getPass(0)->setTexture( mapName, map? map->getTexture(1): 0 );
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

size_t DynamicHeightmap::getDataSize() const {
	return m_width * m_height;
}
void DynamicHeightmap::getData(float* out) const {
	memcpy(out, m_heightData, getDataSize() * sizeof(float));
}
void DynamicHeightmap::setData(const float* data) {
	memcpy(m_heightData, data, getDataSize() * sizeof(float));
	m_land->updateGeometry( BoundingBox(0, 0, 0, m_width*m_resolution, 0,  m_height*m_resolution), true);
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



