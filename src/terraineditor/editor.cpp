#include <base/opengl.h>
#include "editor.h"
#include <cstring>
#include <cstdio>

inline float clamp(float f, float min=0, float max=1) {
	if(f<min) return min;
	if(f>max) return max;
	return f;
}

TerrainEditor::TerrainEditor() : m_heightmap(0) {
	m_brush.radius = 10;
	m_brush.falloff = 0.5;
	m_brush.strength = 1;
}

TerrainEditor::~TerrainEditor() {
}

void TerrainEditor::setHeightmap(HeightmapEditorInterface* map) {
	m_heightmap = map;
	// Setup height tools
}

void TerrainEditor::setTool(ToolInstance* t) {
	m_ring0.clear();
	m_ring1.clear();
	m_tool.clear();
	addTool(t);
}
void TerrainEditor::addTool(ToolInstance* t) {
	if(t) m_tool.push_back(t);
}

const Brush& TerrainEditor::getBrush() const {
	return m_brush;
}
void TerrainEditor::setBrush(const Brush& b) {
	m_brush.radius = b.radius;
	m_brush.strength = b.strength;
	m_brush.falloff = b.falloff;
}

void TerrainEditor::update(const vec3& rayStart, const vec3& rayDir, int btn, int wheel, int shift) {
	if(m_tool.empty() || !m_heightmap) return;

	// update current brush
	float hitDistance = 0;
	int r = m_heightmap->castRay(rayStart, rayDir, hitDistance);
	vec3 position = rayStart + rayDir * hitDistance;
	if(r) m_brush.position = position.xz();

	// Change brush size
	if(wheel) {
		if(shift==0)      m_brush.radius   = clamp( m_brush.radius * (1.0 + wheel * 0.1), 1, 100);
		else if(shift==1) m_brush.strength = clamp( m_brush.strength + wheel * 0.01 );
		else if(shift==2) m_brush.falloff  = clamp( m_brush.falloff + wheel * 0.01 );
		printf("radius: %g strength: %g falloff: %g\n", m_brush.radius, m_brush.strength, m_brush.falloff);
	}

	// Update rings
	if(wheel || r) {
		updateRing(m_ring0, position, m_brush.radius);
		updateRing(m_ring1, position, m_brush.getRadius(0.5));
	}

	// Paint
	static int lb = 0;
	static vec2 lp;
	if(btn&1) {
		// Start
		if(~lb&1) {
			for(uint i=0; i<m_tool.size(); ++i) m_tool[i]->tool->begin();
			lp = position.xz();
		}
		// Paint
		float distance = m_brush.position.distance(lp);
		float spacing = fmin(m_brush.getRadius(0.8), m_brush.radius * 0.4);
		int samples = (int) floor(distance / spacing) + 1;
		vec2 step = (lp - m_brush.position) / distance * spacing;
		if(samples==1) step.set(0,0);
		for(uint i=0; i<m_tool.size(); ++i) {
			ToolInstance* t = m_tool[i];
			for(int j=0; j<samples; ++j) {
				m_brush.position = position.xz() + step * j;
				t->tool->paint(m_brush, shift&1? t->shift: t->flags);
			}
			t->tool->commit();
		}
		lp = position.xz();
	} else if(lb&1) {
		// End
		for(uint i=0; i<m_tool.size(); ++i) m_tool[i]->tool->end();
	}
	lb = btn;
}

void TerrainEditor::updateRing(std::vector<vec3>& v, const vec3& c, float r) const {
	v.clear();
	if(r <= 0) return;
	int sides = TWOPI * r * 2;
	if(sides<32) sides = 32;

	vec3 p;
	float step = TWOPI / sides;
	for(int i=0; i<=sides; ++i) {
		p.x = c.x + r*sin(i*step);
		p.z = c.z + r*cos(i*step);
		p.y = m_heightmap->getHeight(p);
		v.push_back(p);
	}
}

void TerrainEditor::draw() {
	if(m_ring0.empty()) return;

	// Draw brush rings
	glColor4f(0, 1, 1, 1);
	glDisable(GL_TEXTURE_2D);
	glEnableClientState(GL_VERTEX_ARRAY);
	glDisable(GL_DEPTH_TEST);

	glVertexPointer(3, GL_FLOAT, 0, m_ring0[0]);
	glDrawArrays(GL_LINE_STRIP, 0, m_ring0.size());

	glVertexPointer(3, GL_FLOAT, 0, m_ring1[0]);
	glDrawArrays(GL_LINE_STRIP, 0, m_ring1.size());
	
	glDisableClientState(GL_VERTEX_ARRAY);
	glEnable(GL_DEPTH_TEST);
}


