#ifndef _HEIGHTMAP_INTERFACE_
#define _HEIGHTMAP_INTERFACE_

#include "terraineditor/editor.h"
#include "scene/scene.h"
#include "gui/gui.h"
#include <vector>
#include <map>

// List of editable maps
typedef std::vector<class EditableMap*> MapList;

/// Base class for the different heightmap implementations
class HeightmapInterface {
	public:
	virtual ~HeightmapInterface() {}
	virtual void setDetail(float) {}
	virtual scene::Drawable* createDrawable() = 0;
	virtual int castRay(const vec3& start, const vec3& direction, float& out) const = 0;
	virtual float getHeight(const vec3& point) const = 0;
	virtual void setMaterial(class DynamicMaterial*, const MapList&) = 0;
	virtual void setData(const float* data) = 0;
	virtual void getData(float* out) const = 0;
	virtual size_t getDataSize() const = 0;
	virtual void setHeightRange(const Rangef&) {}
};



// Terrain map data
struct TerrainMap {
	HeightmapInterface* heightMap;
	MapList     maps;
	int         size;
	gui::String name;
	gui::String file;
	bool        locked;
};


// Grid of heightmaps
class MapGrid : public TerrainEditorDataInterface, public scene::SceneNode {
	public:
	MapGrid(float size);
	~MapGrid();

	int getMaps(unsigned id, const Brush&, EditableMap**, vec3*, int*) override;
	int castRay(const vec3& start, const vec3& dir, float& out) const override;
	float getHeight(const vec3&) const override;
	float getResolution(unsigned id) const override;

	public:
	void assign(const Point&, TerrainMap*);
	void setVisible(const Point&, bool);
	void remove(const Point&);

	const BoundingBox& getBounds() const;
	Point       getTile(const vec3&) const;
	TerrainMap* getMap(const Point&) const;
	TerrainMap* getMap(const vec3&) const;
	vec3 getOffset(const Point&) const;

	int createTextureMap(int size, int channels, int flags); // Definition for creating EditibleImage maps
	void updateBounds();

	Delegate<void(TerrainMap*)> eventMapCreated;

	std::vector<Point> getUsedSlots() const;

	protected:
	struct MapDef { int size, channels, flags; };
	struct Slot { TerrainMap* map; scene::SceneNode* node; };
	std::map<Point, Slot> m_slots;
	std::vector<MapDef> m_mapDefinitions;
	BoundingBox m_bounds;
	float m_gridSize;
};

#endif

