#ifndef _DYNAMIC_HEIGHTMAP_
#define _DYNAMIC_HEIGHTMAP_

#include "terraineditor/editor.h"
#include "scene/material.h"
#include "heightmap.h"


/// Heightmap object.
class DynamicHeightmap : public HeightmapInterface {
	friend class DynamicHeightmapEditor;
	public:
	DynamicHeightmap();
	~DynamicHeightmap();
	
	void setDetail(float) override;
	scene::Drawable* createDrawable() override;
	int castRay(const vec3& start, const vec3& direction, float& out) const override;
	float getHeight(const vec3& point) const override;
	void setMaterial(class DynamicMaterial*, const MapList&) override;

	public:

	void create(int w, int h, float res, const uint8* data, int stride, float scale, float offset);
	void create(int w, int h, float res, const uint16* data, int stride, float scale, float offset);
	void create(int w, int h, float res, const float* data);
	void create(int w, int h, float res, float height);

	void setMaterial(scene::Material*);

	float height( float x, float z ) const;
	float height( float x, float z, vec3& normal) const;

	private:
	void setup(int w, int h, float r);
	float getHeight(int x, int z) const;
	vec3  getNormal(int x, int z) const;
	float heightFunc(const vec3&);

	int    m_width, m_height;
	float  m_resolution;
	float* m_heightData;
	class Landscape* m_land;
	std::vector<scene::Drawable*> m_drawables;
	scene::Material* m_material;
};

// Heightmap editor interface
class DynamicHeightmapEditor : public EditableMap {
	public:
	DynamicHeightmapEditor(DynamicHeightmap* map) : m_map(map) {}
	~DynamicHeightmapEditor() {}

	int  getChannels() const { return 1; }
	Rect getRect() const { return Rect(0,0,m_map->m_width,m_map->m_height); }
	void getValue(int x, int y, float* values) const;
	void setValue(int x, int y, const float* values);
	void apply(const Rect&);

	private:
	DynamicHeightmap* m_map;
};

#endif

