#ifndef _MINIMAP_
#define _MINIMAP_

#include <base/vec.h>
#include <base/texture.h>

class HeightmapEditorInterface;

// Minimap texture for gui
class MiniMap {
	public:
	MiniMap(int size=256);
	~MiniMap();
	base::Texture& getTexture();
	void resize(int width, int height);

	void setWorld(HeightmapEditorInterface* world, const vec2& size, const vec2& offset=vec2());
	void setRange(float max, float min=0);

	void clear();
	void build();
	void update(const vec2& a, const vec2& b);

	protected:
	float getWorldHeight(int px, int py) const;

	protected:
	HeightmapEditorInterface* m_map;
	base::Texture             m_texture;
	vec2                      m_worldSize;
	vec2                      m_worldOffset;
	float                     m_base;
	float                     m_scale;
	unsigned char*            m_data;
};

#endif

