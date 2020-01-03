#ifndef _DYNAMIC_HEIGHTMAP_
#define _DYNAMIC_HEIGHTMAP_

#include "object.h"
#include "terraineditor/editor.h"
#include "scene/material.h"


class DynamicHeightmap : public Object {
	friend class DynamicHeightmapEditor;
	public:

	DynamicHeightmap();
	~DynamicHeightmap();

	void create(int w, int h, float res, const uint8* data, int stride, float scale, float offset);
	void create(int w, int h, float res, const uint16* data, int stride, float scale, float offset);
	void create(int w, int h, float res, const float* data);
	void create(int w, int h, float res, float height);

	void setMaterial(scene::Material*);
	void setDetail(float);

	float height( float x, float z ) const;
	float height( float x, float z, vec3& normal) const;
	int   ray(const vec3& start, const vec3& direction, float& out) const;
	int   ray(const vec3& start, const vec3& direction, vec3& out) const;

	private:
	void setup(int w, int h, float r);
	float getHeight(int x, int z) const;
	vec3  getNormal(int x, int z) const;
	float heightFunc(const vec3&);

	int    m_width, m_height;
	float  m_resolution;
	float* m_heightData;
	class Landscape* m_land;
};


/** The editor interface for this heightmap */
class DynamicHeightmapEditor : public HeightmapEditorInterface {
	DynamicHeightmap* m_map;
	public:
	DynamicHeightmapEditor(DynamicHeightmap* map) : m_map(map) {}
	void  getInfo(float& r, vec3& o) const                        { r = m_map->m_resolution; o = vec3(0,0,0); }
	float getHeight(const vec3& pos) const                        { return m_map->height(pos.x, pos.z); }
	float getHeight(const vec3& pos, vec3& normal) const          { return m_map->height(pos.x, pos.z, normal); }
	int   castRay(const vec3& s, const vec3& d, float& out) const { return m_map->ray(s,d,out); }

	int setHeights(const Rect&, const float*);
	int getHeights(const Rect&, float*) const;

	void setMaterial(const DynamicMaterial* m);
};

#endif

