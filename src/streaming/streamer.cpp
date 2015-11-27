#include "streamer.h"
#include "landscape.h"
#include "tiff.h"
#include "render/render.h"
#include "texturestream.h"

#include <base/opengl.h>
#include <cstdio>


Streamer::Streamer(float hs) : m_heightScale(hs), m_land(0), m_drawable(0), m_overlay(0) {
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
	m_land->setHeightFunction(Streamer::heightFunc);
	m_land->setMaterialCallbacks(Streamer::materialFunc, Streamer::dropMaterialFunc);
	m_drawable = new StreamerDrawable(m_land);
	m_drawable->setMaterial(m_material);


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
	if(s_streamer==this) s_streamer=0;
	if(m_land) delete m_land;
	if(m_drawable) delete m_drawable;
	m_land = 0;
}

void Streamer::setLod(float value) {
	if(m_land) m_land->setThreshold(value);
}

// ========================================================================================= //


Streamer* Streamer::s_streamer = 0;
float Streamer::heightFunc(const vec3& p) {
	int x = p.x - s_streamer->m_offset.x;
	int y = p.z - s_streamer->m_offset.z;
	if(x<0 || y<0 || x>=s_streamer->width() || y>=s_streamer->height()) return 0;
	uint16 pixel;
	s_streamer->getPixel(x, y, &pixel);
	return pixel * s_streamer->m_decode;
}
Material* Streamer::materialFunc(const BoundingBox& box) {
	if(!s_streamer->m_overlay) return s_streamer->m_material;
	vec2 offset = s_streamer->m_offset.xz();
	vec2 size = offset * -2.0;
	int div = s_streamer->m_overlay->getDivisions();
	vec2 a = floor((box.min.xz() - offset) * div / size);
	vec2 b = floor((box.max.xz() - offset) * div / size - 0.0001);
	if(a==b) return s_streamer->m_overlay->getMaterial( (int)a.x, (int)a.y);
	else return s_streamer->m_overlay->getGlobal();
}
void Streamer::dropMaterialFunc(Material* m) {
	if(!s_streamer->m_overlay) return;
	s_streamer->m_overlay->dropMaterial( m );
}


// ========================================================================================= //

static ubyte* s_buffer = 0;
static Rect  s_rect;


void Streamer::setMaterial(Material* m) {
	m_material = m;
	if(m_drawable) m_drawable->setMaterial(m_material);
}

void Streamer::addToScene(Render* r)      { if(m_drawable) r->add(m_drawable); }
void Streamer::removeFromScene(Render* r) { if(m_drawable) r->remove(m_drawable); }
StreamerDrawable::StreamerDrawable(Landscape* land) : m_land(land) {}
void StreamerDrawable::draw( RenderInfo& r) {
	m_land->update( r.getCamera() );	// Only needs to be called once per frame (shadows, reflection)
	const vec3& cp = r.getCamera()->getPosition();

	updateStreamedMaterials( cp, 512 );

//	m_land->cull( r.getCamera() );
	r.material(m_material);
	r.state(VERTEX_ARRAY | NORMAL_ARRAY);
	const int stride = 10 * sizeof(float);
//	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	for(uint i=0; i<m_land->getGeometry().size(); ++i) {
		const PatchGeometry* g = m_land->getGeometry()[i];
		r.material( g->material? g->material: m_material );
		base::Shader::current().Uniform3f("cameraPos", cp.x, cp.y, cp.z);

		glVertexPointer(3, GL_FLOAT, stride, g->vertices);
		glNormalPointer(GL_FLOAT, stride, g->vertices+3);
		glDrawElements(GL_TRIANGLE_STRIP, g->size, GL_UNSIGNED_SHORT, g->indices);
	}
//	glPolygonMode(GL_FRONT, GL_FILL);

	// Debug: Draw the buffer
	/*
	if(s_buffer) {
		r.material( 0 );
		int w = s_rect.width;
		vec3 off = vec3(-8192,0,-8192) + vec3(s_rect.x, 0, s_rect.y);
		float m_decode = 544.0 / 65535.0;
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		glEnable(GL_POLYGON_OFFSET_LINE);
		glPolygonOffset(-1,-1);
		glBegin(GL_QUADS);
		for(int x=0; x<s_rect.width-1; ++x) for(int y=0; y<s_rect.height-1; ++y) {
			uint16* h = ((uint16*)s_buffer) + x+y*w;
			glVertex3f(x+off.x, *h*m_decode, y+off.z);
			glVertex3f(x+off.x, *(h+w)*m_decode, y+off.z+1);
			glVertex3f(x+off.x+1, *(h+1+w)*m_decode, y+off.z+1);
			glVertex3f(x+off.x+1, *(h+1)*m_decode, y+off.z);
		}
		glEnd();
		glPolygonMode(GL_FRONT, GL_FILL);
	}*/
}

void StreamerDrawable::updateStreamedMaterials(const vec3& v, float threshold) {
/*
	// Select streamed materials based on distance
	float t = threshold * threshold;
	for(uint i=0; i<m_land->getGeometry().size(); ++i) {
		const PatchGeometry* g = m_land->getGeometry()[i];
		// HACK - get aabb
		BoundingBox b( g->vertices );
		b.include( g->vertices + (g->size-1) * 10);
		// Get closest point on aabb
		vec3 cp = v;
		for(int i=0; i<3; ++i) {
			if(cp[i]<b.min[i]) cp[i] = b.min[i];
			else if(cp[i]>b.max[i]) cp[i] = b.max[i];
		}
		// Manage materials
		if(cp.distance2(v) < t) {
			Material* m = Streamer::materialFunc(b);
			m_overlay->dropMaterial( g->material );
			g->material = m;
		} else {
			if(g->material != m_overlay->getGlobal()) m_overlay->dropMaterial( g->material );
			g->material = m_overlay->getGlobal();
		}
	}
	*/
}

// ========================================================================================= //

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










