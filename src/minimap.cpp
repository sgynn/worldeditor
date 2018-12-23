#include "minimap.h"
#include "terraineditor/editor.h"
#include <cstring>

using base::Texture;

MiniMap::MiniMap(int size) : m_map(0) {
	m_texture = Texture::create(size, size, Texture::R8);
	m_data = new unsigned char[size*size];
}
MiniMap::~MiniMap() {
	delete [] m_data;
	m_texture.destroy();
}

Texture& MiniMap::getTexture() {
	return m_texture;
}

void MiniMap::resize(int w, int h) {
	// Need a way to recreate a texture on a specific texture unit?
}

void MiniMap::setWorld(HeightmapEditorInterface* map, const vec2& size, const vec2& offset) {
	m_map = map;
	m_worldSize = size;
	m_worldOffset = offset;
}
void MiniMap::setRange(float max, float min) {
	m_base = min;
	m_scale = 255 / (max - min);
}

inline float MiniMap::getWorldHeight(int px, int py) const {
	vec3 pos;
	pos.x = (float)px / m_texture.width();
	pos.z = (float)py / m_texture.height();
	pos.x = pos.x * m_worldSize.x + m_worldOffset.x;
	pos.z = pos.z * m_worldSize.y + m_worldOffset.y;
	return m_map->getHeight(pos);
}

inline unsigned char clampByte(float v) { return v>0? v<255? v: 255: 0; } 

void MiniMap::clear() {
	memset(m_data, 0, m_texture.width()*m_texture.height());
}
void MiniMap::build() {
	int w = m_texture.width();
	int h = m_texture.height();
	float min=1e8f, max=-1e8f;
	for(int x=0; x<w; ++x) {
		for(int y=0; y<h; ++y) {
			float h = getWorldHeight(x, y);
			if(h<min) min=h;
			if(h>max) max=h;
			m_data[x + y*w] = clampByte((h - m_base) * m_scale);
		}
	}
	m_texture.setPixels(w, h, Texture::R8, m_data);
	setRange(max, min);
}
void MiniMap::update(const vec2& a, const vec2& b) {
	int w = m_texture.width();
	int h = m_texture.height();
	int x0 = ((a.x + m_worldOffset.x) / m_worldSize.x) * w;
	int x1 = ((b.x + m_worldOffset.x) / m_worldSize.x) * w;
	int y0 = ((a.y + m_worldOffset.y) / m_worldSize.y) * h;
	int y1 = ((b.y + m_worldOffset.y) / m_worldSize.y) * h;
	#define clamp(v,a,b) (v>a? v<b? v: b: a)
	x0 = clamp(x0, 0, w-1);
	x1 = clamp(x1, 0, w-1);
	y0 = clamp(y0, 0, h-1);
	y1 = clamp(y1, 0, h-1);
	for(int x=x0; x<=x1; ++x) {
		for(int y=y0; y<=y1; ++y) {
			float h = getWorldHeight(x, y);
			m_data[x + y*w] = clampByte((h - m_base) * m_scale);
		}
	}
	m_texture.setPixels(w, h, Texture::R8, m_data);
}

