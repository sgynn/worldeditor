#include <base/opengl.h>
#include <base/xml.h>
#include "editor.h"
#include <cstring>
#include <cstdio>

#include <base/game.h>
#include <base/input.h>

#include "scene/debuggeometry.h"
#include "scene/scene.h"
#include "scene/mesh.h"
#include "model/mesh.h"

void BrushData::reset(const Brush& brush, float resolution, int channels) {
	m_resolution = resolution;
	vec2 min = ceil ((brush.position - brush.radius) / resolution);
	vec2 max = floor((brush.position + brush.radius) / resolution);
	m_intOffset.set(min.x, min.y);
	m_offset = min * resolution;
	m_size.x = max.x - min.x + 1;
	m_size.y = max.y - min.y + 1;
	m_channels = channels;

	int count = m_size.x * m_size.y * m_channels;
	int lock = m_size.x * m_size.y / 64 + 1;
	if(m_dataSize < count) {
		delete [] m_data;
		m_data = new float[count];
		m_dataSize = count;

		delete [] m_lock;
		m_lock = new uint64[lock];
	}

	m_mx = m_channels;
	m_my = m_channels * m_size.x;
	memset(m_data, 0, count * sizeof(float));
	memset(m_lock, 0, lock * sizeof(uint64));
}

// --------------------------------------------- //


inline float clamp(float f, float min=0, float max=1) {
	if(f<min) return min;
	if(f>max) return max;
	return f;
}

TerrainEditor::TerrainEditor(TerrainEditorDataInterface* t) : m_target(t), m_tool(0), m_locked(false), m_stroke(false) {
	m_brushNode = new scene::SceneNode("Brush");
	m_brush.radius = 10;
	m_brush.falloff = 0.5;
	m_brush.strength = 1;
}

TerrainEditor::~TerrainEditor() {
	scene::DrawableMesh* drawable = static_cast<scene::DrawableMesh*>(m_brushNode->getAttachment(0));
	if(drawable) {
		delete drawable->getMesh();
		delete drawable;
	}
	delete m_brushNode;
}

void TerrainEditor::close() {
	setTool(0);
}

void TerrainEditor::setTool(ToolInstance* t) {
	m_brushNode->setVisible(false);
	m_tool = t;
}

const Brush& TerrainEditor::getBrush() const {
	return m_brush;
}

void TerrainEditor::setBrush(const Brush& b) {
	m_brush.radius = b.radius;
	m_brush.strength = b.strength;
	m_brush.falloff = b.falloff;
}

void TerrainEditor::update(const Mouse& mouse, const Ray& ray, base::Camera*, InputState& state) {
	if(!m_tool || !m_target) return;

	// update current brush
	float hitDistance = 0;
	int hit = m_target->castRay(ray.start, ray.direction, hitDistance);
	vec3 position = ray.point(hitDistance);
	if(hit) m_brush.position = position.xz();
	else {
		// y=0 plane outside map bounds
		float t = -ray.start.y / ray.direction.y;
		position = ray.point(t);
		hit = t > 0;
	}

	if(!m_stroke && state.overGUI) hit = false;

	// Change brush size
	if(mouse.wheel && !state.consumedMouseWheel) {
		if(state.keyMask==0)               m_brush.radius   = clamp( m_brush.radius * (1.0 + mouse.wheel * 0.1), 1, 100);
		else if(state.keyMask==SHIFT_MASK) m_brush.strength = clamp( m_brush.strength + mouse.wheel * 0.01 );
		else if(state.keyMask==CTRL_MASK)  m_brush.falloff  = clamp( m_brush.falloff + mouse.wheel * 0.01 );
		printf("radius: %g strength: %g falloff: %g\n", m_brush.radius, m_brush.strength, m_brush.falloff);
		state.consumedMouseWheel = true;
	}

	// Update rings
	m_brushNode->setVisible(hit);
	if(hit) {
		updateBrushRings(position, m_brush.radius, m_brush.getRadius(0.5));

		m_locked = true;
		m_brush.position = position.xz();
		EditableMap* maps[9]; vec3 offsets[9]; int flags[9];
		int mapCount = m_target->getMaps(0, m_brush, maps, offsets, flags);
		for(int i=0; i<mapCount; ++i) if(flags[i]==0) { m_locked = false; break; }
	}

	// Start/Stop brush stroke
	if(!m_stroke && (mouse.pressed&1) && !state.consumedMouseDown) {
		state.consumedMouseDown = true;
		m_tool->tool->begin(m_brush);
		m_last = position.xz();
		m_stroke = true;
	}
	else if(m_stroke && (mouse.released&1)) {
		m_tool->tool->end();
		m_stroke = false;
	}

	// Paint
	if(m_stroke) {
		float distance = m_brush.position.distance(m_last);
		m_last = position.xz();
		if(distance > 40) {
			printf("Warning: long brush stroke : (%g, %g) - (%g, %g)\n", m_last.x, m_last.y, m_brush.position.x, m_brush.position.y);
			return;
		}
		int toolFlags = state.keyMask&SHIFT_MASK? m_tool->shift: m_tool->flags;
		Tool* tool = m_tool->tool;
		float spacing = fmin(m_brush.getRadius(0.8), m_brush.radius * 0.4) * 0.5;
		int samples = (int) floor(distance / spacing) + 1;
		vec2 step = (m_last - m_brush.position) / distance * spacing;
		float resolution = 1; //m_tool->getResolution(); // should be from maps?
		vec3 offsets[9];
		EditableMap* maps[9];
		Rect local[9];
		Point base[9];
		int flags[9];
		int mapCount;
		//uint64 ticks = base::Game::getTicks();
		if(samples==1) step.set(0,0);
		for(int j=0; j<samples; ++j) {
			m_brush.position = position.xz() + step * j;
			mapCount = m_target->getMaps(tool->getTarget(), m_brush, maps, offsets, flags);
			if(mapCount==0) continue;

			// Fill buffer
			resolution = m_target->getResolution(tool->getTarget());
			m_buffer.reset(m_brush, resolution, maps[0]->getChannels());
			for(int k=0; k<mapCount; ++k) {
				vec2 basef = floor((m_brush.position - offsets[k].xz() - m_brush.radius) / resolution);
				base[k].set(basef.x, basef.y);
				local[k].set(base[k], m_buffer.getSize());
				local[k].intersect(maps[k]->getRect());
				maps[k]->prepare(local[k]);
				const Point& end = local[k].bottomRight();
				for(int x=local[k].x; x<end.x; ++x) for(int y=local[k].y; y<end.y; ++y) {
					if(m_buffer.locked(x-base[k].x, y-base[k].y)) continue;
					float* data = m_buffer.getValue(x-base[k].x, y-base[k].y);
					maps[k]->getValue(x, y, data);
				}
				if(flags[k]&1) for(int x=local[k].x; x<end.x; ++x) for(int y=local[k].y; y<end.y; ++y) {
					m_buffer.lock(x-base[k].x, y-base[k].y);
				}
			}

			// Run tool
			tool->paint(m_buffer, m_brush, toolFlags);

			// Write from buffer 
			for(int k=0; k<mapCount; ++k) {
				if(flags[k]&1) continue; // Read only flag
				const Point& end = local[k].bottomRight();
				for(int x=local[k].x; x<end.x; ++x) for(int y=local[k].y; y<end.y; ++y) {
					float* data = m_buffer.getValue(x-base[k].x, y-base[k].y);
					maps[k]->setValue(x, y, data);
				}
			}

			// Apply changes
			for(int k=0; k<mapCount; ++k) {
				maps[k]->apply(local[k]);
			}
		}
	}
}

void TerrainEditor::updateBrushRings(const vec3& centre, float r1, float r2) {
	scene::DrawableMesh* drawable = static_cast<scene::DrawableMesh*>(m_brushNode->getAttachment(0));
	if(!drawable) {
		drawable = new scene::DrawableMesh();
		base::HardwareVertexBuffer* buffer = new base::HardwareVertexBuffer();
		base::bmodel::Mesh* mesh = new base::bmodel::Mesh();
		buffer->attributes.add(base::VA_VERTEX, base::VA_FLOAT3);
		buffer->attributes.add(base::VA_COLOUR, base::VA_ARGB);
		buffer->createBuffer();
		mesh->setVertexBuffer(buffer);
		mesh->setPolygonMode(base::bmodel::LINE_STRIP);
		drawable->setMesh(mesh);
		drawable->setMaterial(scene::DebugGeometryManager::getInstance()->getDefaultMaterial());
		m_brushNode->attach(drawable);
	}

	struct Vertex { vec3 pos; uint colour; };
	static std::vector<Vertex> data(70, Vertex{centre, 0xffffff00});
	int k = 0;

	auto addRing = [this](const vec3& centre, float r, std::vector<Vertex>& data, int& k) {
		int sides = TWOPI * r * 2;
		if(sides > 32) sides = 32;
		float step = TWOPI / sides;
		for(int i=0; i<=sides; ++i) {
			Vertex& vx = data[k++];
			vx.pos.x = centre.x + r * sin(i*step);
			vx.pos.z = centre.z + r * cos(i*step);
			vx.pos.y = m_target->getHeight(vx.pos) + 0.1;
			data.push_back(vx);
		}
	};
	addRing(centre, r1, data, k);
	data[k++].pos.y = INFINITY;
	addRing(centre, r2, data, k);

	drawable->getMesh()->getVertexBuffer()->copyData(&data[0], k, sizeof(Vertex));
}

base::XMLElement TerrainEditor::save(const TerrainMap* context) const {
	return base::XMLElement();
}

void TerrainEditor::load(const base::XMLElement&, const TerrainMap* context) {
}

