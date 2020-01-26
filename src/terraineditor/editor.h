#ifndef _WORLD_EDITOR_MODULE_
#define _WORLD_EDITOR_MODULE_

#include <base/math.h>
#include "tool.h"
#include <vector>

namespace base { class Texture; }

/// Interface for maps that can be painted on
class EditableMap {
	public:
	virtual ~EditableMap() {}
	virtual int  getChannels() const = 0;
	virtual Rect getRect() const = 0;
	virtual void prepare(const Rect&) {}
	virtual void getValue(int x, int y, float* values) const = 0;	// assume x,y valid.
	virtual void setValue(int x, int y, const float* values) = 0;
	virtual void apply(const Rect&) {}
	virtual const base::Texture* getTexture(uint flags=0) const { return 0; }	// For texture type maps
};

/// Editor target data interface
class TerrainEditorDataInterface {
	public:
	virtual ~TerrainEditorDataInterface() {}
	virtual int getMaps(unsigned id, const Brush&, EditableMap**, vec3* offsets, int* flags) = 0;
	virtual int castRay(const vec3& start, const vec3& direction, float& out) const = 0;
	virtual float getHeight(const vec3& point) const = 0;
	virtual float getResolution(unsigned id) const = 0;
};

/// Main editor class - handles all painting stuff
class TerrainEditor {
	public:
	TerrainEditor(TerrainEditorDataInterface*);
	~TerrainEditor();

	void setTool(ToolInstance*);
	void setBrush(const Brush&);
	const Brush& getBrush() const;

	void update(const vec3& rayStart, const vec3& rayDir, int btn, int wheel, int shift);
	void draw();

	private:
	void updateRing(std::vector<vec3>&, const vec3& centre, float radius) const;
	
	private:
	TerrainEditorDataInterface*   m_target;		// Editable data access
	ToolInstance*                 m_tool;		// Active tool
	std::vector<vec3> m_ring0;
	std::vector<vec3> m_ring1;
	Brush             m_brush;
	BrushData         m_buffer;
	bool              m_locked;
	
	
};

#endif

