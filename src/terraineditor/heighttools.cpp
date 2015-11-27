#include "heighttools.h"
#include "editor.h"
#include <cstdio>

static const Point one(1,1);
void HeightTool::resizeData(int s) {
	if(!m_data || s > m_dataSize || s < m_dataSize * 0.6) {
		if(m_data) delete [] m_data;
		m_data = new float[s];
		m_dataSize = s;
	}
}

void HeightTool::end() {
	// Update normals and foliage ?
}

void HeightTool::paint(const Brush& b, int flags) {
	Rect r = getRect(b);
	resizeData( r.width * r.height );
	heightmap->getHeights(r, m_data);
	float direction = flags? -1: 1;
	Point e = r.position() + r.size();
	for(int x=r.x; x<e.x; ++x) for(int y=r.y; y<e.y; ++y) {
		float w = getValue(b, x, y);
		float& v = m_data[(x-r.x) + (y-r.y) * r.width];
		v += w * direction;
	}
	heightmap->setHeights(r, m_data);
}

// ----------------------------------------------- //


void SmoothTool::paint(const Brush& b, int flags) {
	static const float blur[3] = { 0.27249597f, 0.12475775f, 0.05711826f };
	float target;

	Rect r = getRect(b);
	resizeData( r.width * r.height );
	heightmap->getHeights(r, m_data);
	Point e = r.position() + r.size();
	for(int x=r.x+1; x<e.x-1; ++x) for(int y=r.y+1; y<e.y-1; ++y) {
		int k = (x-r.x) + (y-r.y) * r.width;
		// Sample 9 points
		target  = m_data[k-1-r.width] * blur[2] + m_data[k-r.width] * blur[1] + m_data[k-r.width+1] * blur[2];
		target += m_data[k-1] * blur[1]         + m_data[k] * blur[0]         + m_data[k+1] * blur[1];
		target += m_data[k-1+r.width] * blur[2] + m_data[k+r.width] * blur[1] + m_data[k+r.width+1] * blur[2];

		float w = getValue(b, x, y);
		m_data[k] += (target - m_data[k]) * w;
	}
	heightmap->setHeights(r, m_data);
}

// ----------------------------------------------- //

void LevelTool::paint(const Brush& b, int flags) {
	if(flags==0 || target<=-1e8f) {
		vec3 pos( b.position.x, 0, b.position.y);
		target = heightmap->getHeight(pos);
	}
	Rect r = getRect(b);
	resizeData( r.width * r.height );
	heightmap->getHeights(r, m_data);
	Point e = r.position() + r.size();
	for(int x=r.x; x<e.x; ++x) for(int y=r.y; y<e.y; ++y) {
		float w = getValue(b, x, y);
		float& v = m_data[(x-r.x) + (y-r.y) * r.width];
		v += (target-v) * w * 0.1;
	}
	heightmap->setHeights(r, m_data);
}

// ----------------------------------------------- //

void FlattenTool::paint(const Brush& b, int flags) {
	if(flags==0 || target<=-1e8f) {
		vec3 pos( b.position.x, 0, b.position.y);
		pos.y = heightmap->getHeight(pos, normal);
		target = normal.dot( pos );
	}
	Rect r = getRect(b);
	resizeData( r.width * r.height );
	heightmap->getHeights(r, m_data);
	Point e = r.position() + r.size();
	for(int x=r.x; x<e.x; ++x) for(int y=r.y; y<e.y; ++y) {
		float w = getValue(b, x, y);
		float& v = m_data[(x-r.x) + (y-r.y) * r.width];
		vec2 wp = getWorldPosition(x, y);
		float t = (target - normal.x * wp.x - normal.z * wp.y) / normal.y; // Project to plane
		v += (t-v) * w * 0.1;
	}
	heightmap->setHeights(r, m_data);
}

// ----------------------------------------------- //

void NoiseTool::paint(const Brush& b, int flags) {
	Rect r = getRect(b);
	resizeData( r.width * r.height );
	heightmap->getHeights(r, m_data);
	Point e = r.position() + r.size();
	for(int x=r.x; x<e.x; ++x) for(int y=r.y; y<e.y; ++y) {
		float w = getValue(b, x, y);
		float& v = m_data[(x-r.x) + (y-r.y) * r.width];
		v += w;	// Here
	}
	heightmap->setHeights(r, m_data);
}

// ----------------------------------------------- //

void ErosionTool::paint(const Brush& b, int flags) {
	// Complicated :  simulate water ?
}

