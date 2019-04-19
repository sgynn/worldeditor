#include "streamer.h"
#include "landscape.h"
#include "tiff.h"
#include "scene/scene.h"
#include "base/camera.h"
#include "texturestream.h"
#include "dynamicmaterial.h"

#include "scene/shader.h"
#include <base/opengl.h>
#include <cstdio>


Streamer::Streamer(float hs) : m_heightScale(hs), m_land(0), m_drawable(0), m_material(0) {
	setBufferSize(256, 256);
	m_encode = m_decode = 0;
}

Streamer::~Streamer() {
	closeStream();
}

void Streamer::streamOpened() {
	// Setup landscape from stream data
	int p = 0;
	int size = m_stream->width() & ~1;
	int scale = (1 << m_stream->bpp()) - 1;
	m_decode = m_heightScale / scale;
	m_encode = scale / m_heightScale;
	while((1<<p) < size) ++p;
	m_offset = vec3(-size/2, 0, -size/2);
	s_streamer = this;
	m_land = new Landscape(size, m_offset);
	m_land->setLimits(0, p-3);
	m_land->setPatchCallbacks(Streamer::patchCreated, Streamer::patchDestroyed);
	m_land->setHeightFunction(Streamer::heightFunc);
	m_drawable = new StreamerDrawable(m_land);
}

void Streamer::closeStream() {
	BufferedStream::closeStream();
	if(m_land) delete m_land;
	if(s_streamer==this) s_streamer=0;
	if(m_drawable) delete m_drawable;
	m_land = 0;
}

void Streamer::setLod(float value) {
	if(m_land) m_land->setThreshold(value);
}

void Streamer::setMaterial(const DynamicMaterial* m) {
	float size = m_stream->width() & ~1;
	m_material = m->getStream();
	m_material->setCoordinates(vec2(size,size), m_offset.xz());
	m->setCoordinates(vec2(size, size), m_offset.xz());
	// Change materials in existing patches
	GL_CHECK_ERROR;
	m_swapMaterialFlag = true;
	int r = m_land->visitAllPatches(Streamer::updatePatchMaterial);
	printf("%d patches visited\n", r);
	m_swapMaterialFlag = false;
	GL_CHECK_ERROR;
	
}

void Streamer::addTexture(const char* name, TextureStream* tex) {
	m_material->addStream(name, tex);
}
void Streamer::addTexture(const char* name, const Texture& tex) {
	m_material->setTexture(name, tex);
}

// ========================================================================================= //

struct PatchTag {
	Material* material;
};


Streamer* Streamer::s_streamer = 0;
float Streamer::heightFunc(const vec3& p) {
	int x = p.x - s_streamer->m_offset.x;
	int y = p.z - s_streamer->m_offset.z;
	if(x<0 || y<0 || x>=s_streamer->width() || y>=s_streamer->height()) return 0;
	uint16 pixel;
	s_streamer->getPixel(x, y, &pixel);
	return pixel * s_streamer->m_decode;
}
void Streamer::patchCreated(PatchGeometry* g) {
	PatchTag* tag = new PatchTag;
	tag->material = 0;
	g->tag = tag;
}
void Streamer::patchDestroyed(PatchGeometry* g) {
	PatchTag* tag = static_cast<PatchTag*>(g->tag);
	s_streamer->m_material->dropMaterial( tag->material );
	delete tag;
	g->tag = 0;
}


vec3 lodCameraPosition;
void Streamer::updatePatchMaterial(PatchGeometry* g) {
	PatchTag* tag = static_cast<PatchTag*>(g->tag);
	// Switch material lod
	vec3 cp = g->bounds->clamp( lodCameraPosition );
	float d = lodCameraPosition.distance2(cp);
	Material* global = s_streamer&&s_streamer->m_material? s_streamer->m_material->getGlobal(): 0;

	if(g->bounds->size().x > -s_streamer->m_offset.x * 0.5) d = 1e20f;

	if(d > 1000 * 1000 || s_streamer->m_swapMaterialFlag) {
		// May need to drop reference
		if(tag->material != global) s_streamer->m_material->dropMaterial( tag->material );
		tag->material = global;
	}
	else if(tag->material == 0 || tag->material == global) {
		vec2 offset = s_streamer->m_offset.xz();
		vec2 size = offset * -2.0;
		int div = s_streamer->m_material->getDivisions();
		vec2 index = (g->bounds->centre() - s_streamer->m_offset).xz() * div / size;
		Material* m = s_streamer->m_material->getMaterial( (int)index.x, (int)index.y);
		tag->material = m;
	}
}


// ========================================================================================= //



void Streamer::addToScene(Scene* r)      { if(m_drawable) r->add(m_drawable); }
void Streamer::removeFromScene(Scene* r) { if(m_drawable) r->remove(m_drawable); }

StreamerDrawable::StreamerDrawable(Landscape* land) : m_land(land) {}
void StreamerDrawable::draw( RenderInfo& r) {
	// Update terrain lod stuff - Note: only needs to be called one per frame
	lodCameraPosition = r.getCamera()->getPosition();
	m_land->update( r.getCamera() );
	m_land->visitAllPatches(Streamer::updatePatchMaterial);

	// View frustum culling
	m_land->cull( r.getCamera() );

	scene::Shader::current().enableAttributeArray(0);
	scene::Shader::current().enableAttributeArray(1);

	const int stride = 10 * sizeof(float);
	r.state(0); //VERTEX_ARRAY | NORMAL_ARRAY);
	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	for(uint i=0; i<m_land->getGeometry().size(); ++i) {
		const PatchGeometry* g = m_land->getGeometry()[i];
		const PatchTag* tag = static_cast<const PatchTag*>(g->tag);

		r.material( tag->material );
		scene::Shader::current().setAttributePointer(0, 3, GL_FLOAT, stride, scene::SA_FLOAT, g->vertices);
		scene::Shader::current().setAttributePointer(1, 3, GL_FLOAT, stride, scene::SA_FLOAT, g->vertices+3);
		glDrawElements(GL_TRIANGLE_STRIP, g->indexCount, GL_UNSIGNED_SHORT, g->indices);
	}
	//glPolygonMode(GL_FRONT, GL_FILL);
//
	scene::Shader::current().disableAttributeArray(0);
	scene::Shader::current().disableAttributeArray(1);
}

// ========================================================================================= //

static ubyte* s_buffer = 0;
static Rect  s_rect;

inline uint16 clamp16(float v) { return v<0? 0: v>65535? 65535: v; }

int StreamingHeightmapEditor::getHeights(const Rect& r, float* array) const {
	// Read + Convert pixels
	int count = r.width * r.height;
	uint16* data = new uint16[count];
	m_map->getPixels(r, data);
	for(int i=0; i<count; ++i) array[i] = data[i] * m_map->m_decode;
	delete [] data;
	return 1;
}
int StreamingHeightmapEditor::setHeights(const Rect& r, const float* array) {
	// Convert + Write pixels
	int count = r.width * r.height;
	uint16* data = new uint16[count];
	for(int i=0; i<count; ++i) data[i] = clamp16(array[i] * m_map->m_encode);
	m_map->setPixels(r, data);
	delete [] data;

	// Update any active geometries
	BoundingBox box(r.left(), 0, r.top(), r.right(), 0, r.bottom());
	box += m_map->m_offset;
	m_map->m_land->updateGeometry( box, true );
	s_buffer = m_map->m_buffer;
	s_rect = m_map->m_bufferRect;
	return 1;
}


// ========================================================================================= //


float Streamer::height(float x, float z, vec3& normal) const {
	if(!m_land) return 0;
	normal = vec3(0,1,0); // ToDo: get normal
	return m_land->getHeight(x, z, normal);
}
float Streamer::height(float x, float z) const {
	if(!m_land) return 0;
	return m_land->getHeight(x, z);
}
int Streamer::ray(const vec3& start, const vec3& direction, float& out) const {
	if(!m_land) return 0;
	vec3 point, normal;
	int r = m_land->intersect(start, start+direction*1e6f, point, normal);
	if(r) out = direction.dot(point-start) / direction.dot(direction);
	return r;
}










