#include "heightmap.h"
#include "terraineditor/editabletexture.h"


MapGrid::MapGrid(float size, const Range& range) : m_heightRange(range), m_gridSize(size) {
	m_mapDefinitions.push_back( MapDef{0,0} ); // Null definition for heightmap map
}

MapGrid::~MapGrid() {
	for(auto& s: m_slots) delete s.second.node;
}

vec3 MapGrid::getOffset(const Point& p) const {
	return vec3( p.x*m_gridSize, 0, p.y*m_gridSize);
}

void MapGrid::assign(const Point& p, TerrainMap* map) {
	remove(p);
	if(!map) return;
	Slot& slot = m_slots[p];
	vec3 offset(p.x * m_gridSize, 0, p.y * m_gridSize);
	char nodeName[32];
	snprintf(nodeName, 32, "Tile %d,%d", p.x, p.y);

	slot.map = map;
	slot.node = createChild(nodeName, offset);
	slot.node->attach( map->heightMap->createDrawable() );
	slot.node->setPosition(offset);
	updateBounds();
}

void MapGrid::remove(const Point& p) {
	auto it = m_slots.find(p);
	if(it!=m_slots.end() && it->second.node) {
		// ToDo: Delete drawable - tracked by HeightMap class
		delete it->second.node;
		it->second.node = 0;
		it->second.map = 0;
		updateBounds();
	}
}

Point MapGrid::getTile(const vec3& p) const {
	vec2 t = floor(p.xz() / m_gridSize);
	return Point(t.x, t.y);
}

TerrainMap* MapGrid::getMap(const Point& index) const {
	auto it = m_slots.find(index);
	if(it != m_slots.end()) return it->second.map;
	else return 0;
}

TerrainMap* MapGrid::getMap(const vec3& point) const {
	Point index;
	index.x = floor(point.x / m_gridSize);
	index.y = floor(point.z / m_gridSize);
	return getMap(index);
}

void MapGrid::setVisible(const Point& p, bool v) {
	std::map<Point, Slot>::iterator it = m_slots.find(p);
	if(it!=m_slots.end() && it->second.node) it->second.node->setVisible(v);
}

int MapGrid::getMaps(unsigned id, const Brush& brush, EditableMap** maps, vec3* offsets, int* flags) {
	int result = 0;
	vec2 a = floor( (brush.position - brush.radius) / m_gridSize );
	vec2 b = floor( (brush.position + brush.radius) / m_gridSize );
	for(Point p(a.x,a.y); p.x<=b.x; ++p.x) {
		for(p.y=a.y; p.y<=b.y; ++p.y) {
			auto it = m_slots.find(p);
			if(it != m_slots.end() && it->second.map) {
				TerrainMap* data = it->second.map;
				if(!data) continue;

				// Create new map if it doesn't exist
				if( (id>=data->maps.size() || !data->maps[id]) && id<m_mapDefinitions.size() && m_mapDefinitions[id].size) {
					const MapDef& def = m_mapDefinitions[id];
					printf("Creating map %u %dx%d with %d channels\n", id, def.size, def.size, def.channels);
					EditableTexture* newTex = new EditableTexture(def.size, def.size, def.channels, true);
					if(data->maps.size() <= id) data->maps.resize(id+1, 0);
					data->maps[id] = newTex;
					if(def.flags>1) newTex->getTexture(0)->setFilter(base::Texture::NEAREST);
					if(eventMapCreated) eventMapCreated(it->second.map);
				}

				maps[result] = data->maps[id];
				offsets[result] = getOffset(p);
				flags[result] = data->locked? 1: 0;
				if(maps[result]) ++result;
			}
		}
	}
	return result;
}

float MapGrid::getResolution(unsigned id) const {
	if(id==0) return 1; // Need Heightmap resolution.
	if(id<m_mapDefinitions.size() && m_mapDefinitions[id].size) {
		return m_gridSize / m_mapDefinitions[id].size;;
	}
	return 1;
}

int MapGrid::castRay(const vec3& start, const vec3& dir, float& out) const {
	out = 1e16f;
	int result = 0;
	for(auto& s: m_slots) {
		if(!s.second.map) continue;
		float tmp = out;
		vec3 offset = getOffset(s.first);
		if(s.second.map->heightMap->castRay(start-offset, dir, tmp) && tmp < out) {
			out = tmp;
			result = 1;
		}
	}
	return result;
}

float MapGrid::getHeight(const vec3& point) const {
	Point index;
	index.x = floor(point.x / m_gridSize);
	index.y = floor(point.z / m_gridSize);
	auto it = m_slots.find(index);
	if(it != m_slots.end() && it->second.map) {
		return it->second.map->heightMap->getHeight(point - getOffset(index));
	}
	return 0;
}

const BoundingBox& MapGrid::getBounds() const {
	return m_bounds;
}


int MapGrid::createTextureMap(int size, int channels, int flags) {
	int id = m_mapDefinitions.size();
	m_mapDefinitions.push_back( MapDef{size, channels, flags} );
	return id;
}

void MapGrid::updateBounds() {
	m_bounds.setInvalid();
	for(auto& s: m_slots) {
		if(s.second.map) {
			vec3 p(s.first.x*m_gridSize, 0, s.first.y*m_gridSize);
			m_bounds.include(p);
			p.x += m_gridSize;
			p.z += m_gridSize;
			m_bounds.include(p);
		}
	}
}

std::vector<Point> MapGrid::getUsedSlots() const {
	std::vector<Point> used;
	used.reserve(m_slots.size());
	for(auto i: m_slots) if(i.second.map) used.push_back(i.first);
	return used;
}

