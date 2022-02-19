#ifndef _TEXTURE_STREAM_
#define _TEXTURE_STREAM_

#include "bufferedstream.h"
#include <base/material.h>
#include <base/texture.h>
#include <vector>

using base::Texture;
using base::Material;


class TextureStream : public BufferedStream {
	public:
	TextureStream();
	virtual ~TextureStream();
	virtual int  setPixel(int x, int y, void* pixel);
	virtual int  setPixels(const Rect& rect, void* data);

	void     initialise(int maxResolution, bool overlap);	// Determine split size
	int      getDivisions() const;							// Get split count
	Rect     getPixelRect(int x, int y) const;				// get rectangle of a sub-texture
	Texture& getTexture(int x, int y);						// Get sub-texture (reference counted. creates if null)
	void     dropTexture(int x, int y);						// Drop a sub-texture (reference counted)
	Texture& getGlobalTexture();							// Get the global texture lod
	void     updateTextures();								// Reload any changes to the gpu texture

	protected:
	void createTexture(int x, int y);						// Create sub-texture
	void createGlobalTexture(int size);						// Generate global texture

	protected:
	Rect     m_dirty;
	int      m_divisions;
	Texture* m_textures;
	Texture  m_global;
	int*     m_ref;
	bool     m_overlap;
};


class MaterialStream {
	public:
	MaterialStream(Material* base);
	~MaterialStream();

	void setCoordinates(const vec2& size, const vec2& offset);	// Set texture world coordinate mapping
	bool addStream(const char* name, TextureStream*);			// Add a texture layer
	void removeStream(TextureStream*);							// remove a texture layer

	void setOverlayTexture(const char* name, const Texture&);	// add texture as overlay, like streams
	void setTexture(const char* name, const Texture& texture);	// Set non-streamed texture
	void updateShader();										// Copy shader program from template
	void build();												// Ensure submaterias are created

	int       getDivisions() const;			// Number of subdivisions
	Material* getTemplate() const;			// Get template material. This is the one sent in the constructor.
	Material* getGlobal();					// Get/create global texture.
	Material* getMaterial(int x, int y);	// Get a sub material
	void      dropMaterial(Material* m);	// Dereference a sub material

	protected:
	struct Stream {
		TextureStream* texture;		// Streamed texture source
		char name[32];				// texture sampler name
		char infoName[32];			// texture transform variable name
	};
	struct SubMaterial {
		Material* material;			// Sub Material
		int ref;					// Reference count
		Point index;				// Sub material index
		int divisions;				// Divisions when created
	};

	void setStreamTexture(Material* m, const Stream&, int x, int y);
	void dropMaterial(SubMaterial&);

	protected:
	std::vector<Stream> m_streams;	// Attached texture streams
	Material*      m_template;		// Template material to create sub materials from
	Material*      m_global;		// Global material
	SubMaterial*   m_materials;		// Array of sub materials
	int            m_divisions;		// How many divisions the whole is divided into
	vec2           m_offset, m_size;	// World coordinates for generated UVs
	std::vector<SubMaterial> m_delete;	// Materials to delete that are still referenced
};

#endif

