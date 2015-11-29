#include "streamer.h"
#include "landscape.h"
#include "tiff.h"
#include "render/render.h"
#include "texturestream.h"

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

	/*
	// Ok, lets try adding a streamed texture overlay ...
	m_overlay = new MaterialStream(&m_material);
	TextureStream* ts = new TextureStream();
	ts->openStream("maps/kenshi_colour.tif");
	if(ts->channels() > 0) {
		m_overlay->addStream("colour", ts);
		ts->initialise(2048, false);
		m_overlay->setCoordinates(vec2(size,size), m_offset.xz());
		m_overlay->initialise();
		m_drawable->m_overlay = m_overlay;
	}
	*/
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

void Streamer::setMaterial(Material* m) {
	float size = m_stream->width() & ~1;
	if(m_material) delete m_material;
	m_material = new MaterialStream(m);
	m_material->setCoordinates(vec2(size,size), m_offset.xz());
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
	Material* global = s_streamer->m_material->getGlobal();

	if(g->bounds->size().x > -s_streamer->m_offset.x * 0.5) d = 1e20f;

	if(d > 1000 * 1000) {
		// May need to drop reference
		s_streamer->m_material->dropMaterial( tag->material );
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



void Streamer::addToScene(Render* r)      { if(m_drawable) r->add(m_drawable); }
void Streamer::removeFromScene(Render* r) { if(m_drawable) r->remove(m_drawable); }

StreamerDrawable::StreamerDrawable(Landscape* land) : m_land(land) {}
void StreamerDrawable::draw( RenderInfo& r) {

	// Update terrain lod stuff - Note: only needs to be called one per frame
	vec3 cp = lodCameraPosition = r.getCamera()->getPosition();
	m_land->update( r.getCamera() );
	m_land->visitAllPatches(Streamer::updatePatchMaterial);

	// View frustum culling
	m_land->cull( r.getCamera() );

	const int stride = 10 * sizeof(float);
	r.state(VERTEX_ARRAY | NORMAL_ARRAY);
//	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	for(uint i=0; i<m_land->getGeometry().size(); ++i) {
		const PatchGeometry* g = m_land->getGeometry()[i];
		const PatchTag* tag = static_cast<const PatchTag*>(g->tag);

		r.material( tag->material );
		base::Shader::current().Uniform3f("cameraPos", cp.x, cp.y, cp.z);

		glVertexPointer(3, GL_FLOAT, stride, g->vertices);
		glNormalPointer(GL_FLOAT, stride, g->vertices+3);
		glDrawElements(GL_TRIANGLE_STRIP, g->indexCount, GL_UNSIGNED_SHORT, g->indices);
	}
//	glPolygonMode(GL_FRONT, GL_FILL);
}

// ========================================================================================= //

static ubyte* s_buffer = 0;
static Rect  s_rect;

inline uint16 clamp16(float v) { return v<0? 0: v>65535? 65535: v; }

int StreamingHeightmapEditor::getHeights(const Rect& r, float* array) const {
	// Read + Convert pixels
	uint16* data = new uint16[r.width*r.height];
	m_map->getPixels(r, data);
	for(int i=0; i<r.width*r.height; ++i) array[i] = data[i] * m_map->m_decode;
	return 1;
}
int StreamingHeightmapEditor::setHeights(const Rect& r, const float* array) {
	// Convert + Write pixels
	uint16* data = new uint16[r.width*r.height];
	for(int i=0; i<r.width*r.height; ++i) data[i] = clamp16(array[i] * m_map->m_encode);
	m_map->setPixels(r, data);

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









