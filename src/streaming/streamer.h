#ifndef _STREAMER_
#define _STREAMER_

#include "object.h"
#include "bufferedstream.h"
#include "render/drawable.h"
#include <base/texture.h>
#include <base/thread.h>

#include "terraineditor/editor.h"

class Landscape;
class TiffStream;
class TextureStream;
class MaterialStream;

/** Interface between scene and landscape */
class StreamerDrawable : public Drawable {
	friend class Streamer;
	Landscape* m_land;
	MaterialStream* m_overlay;
	public:
	StreamerDrawable(Landscape*);
	virtual void draw( RenderInfo& );
	void updateStreamedMaterials(const vec3&, float threshold);
};

/** Alternative heightmap object that streams to a file for editing huge landscapes */
class Streamer : public Object, public BufferedStream {
	friend class StreamingHeightmapEditor;
	public:
	Streamer(float heightScale);
	~Streamer();

	void streamOpened();
	void closeStream();
	const vec3& getOffset() const { return m_offset; }

	virtual void   setLod(float value);

	// Collision functions
	virtual float  height( float x, float z, vec3& normal) const;
	virtual float  height( float x, float z ) const;
	virtual int    ray(const vec3& start, const vec3& direction, float& out) const;

	// Fix overload error
	int height() const { return BufferedStream::height(); }

	public: // Object functions
	void addToScene(Render* r);
	void removeFromScene(Render* r);
	void setMaterial(Material* m);

	void addTexture(const char* name, TextureStream*);
	void addTexture(const Texture&);

	protected:
	float         m_heightScale;
	vec3          m_offset;
	float         m_resolution;
	Landscape*    m_land;

	StreamerDrawable* m_drawable;
	Material*         m_material;
	MaterialStream*   m_overlay;
	
	float m_encode, m_decode;


	// Landscape needs static functions to interface data
	static Streamer* s_streamer;
	static float heightFunc(const vec3&);
	static Material* materialFunc(const BoundingBox&);
	static void dropMaterialFunc(Material*);
};


/** The editor interface for this heightmap */
class StreamingHeightmapEditor : public HeightmapEditorInterface {
	Streamer* m_map;
	public:
	StreamingHeightmapEditor(Streamer* map) : m_map(map) {}
	void  getInfo(float& r, vec3& o) const                        { r = m_map->m_resolution; o = vec3(0,0,0); }
	float getHeight(const vec3& pos) const                        { return m_map->height(pos.x, pos.z); }
	float getHeight(const vec3& pos, vec3& normal) const          { return m_map->height(pos.x, pos.z, normal); }
	int   castRay(const vec3& s, const vec3& d, float& out) const { return m_map->ray(s,d,out); }

	int getHeights(const Rect&, float*) const;
	int setHeights(const Rect&, const float*);
};


#endif

