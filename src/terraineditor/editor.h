#ifndef _WORLD_EDITOR_MODULE_
#define _WORLD_EDITOR_MODULE_

#include <base/math.h>
#include "tool.h"
#include <vector>

/** Access interface for heightmap data */
class HeightmapEditorInterface {
	public:
	virtual ~HeightmapEditorInterface() {}
	virtual void  getInfo(float& resolution, vec3& offset) const = 0;
	virtual float getHeight(const vec3& pos) const = 0;
	virtual float getHeight(const vec3& pos, vec3& normal) const = 0;
	virtual int   castRay(const vec3& start, const vec3& direction, float& out) const = 0;

	virtual int getHeights(const Rect& rect, float* array) const = 0;
	virtual int setHeights(const Rect& rect, const float* array) = 0;
};


/** Main editor class - handles all painting stuff */
class TerrainEditor {
	public:
	TerrainEditor();
	~TerrainEditor();

	void setHeightmap(HeightmapEditorInterface*);

	void setTool(ToolInstance*);
	void addTool(ToolInstance*);

	const Brush& getBrush() const;
	void setBrush(const Brush&);

	void update(const vec3& rayStart, const vec3& rayDir, int btn, int wheel, int shift);
	void draw();

	private:
	void updateRing(std::vector<vec3>&, const vec3& centre, float radius) const;
	
	private:
	HeightmapEditorInterface*  m_heightmap;
	std::vector<ToolInstance*> m_tool;		 // The active tool. Can consist of multiple tools
	std::vector<vec3> m_ring0;
	std::vector<vec3> m_ring1;
	Brush             m_brush;
	
};

#endif

