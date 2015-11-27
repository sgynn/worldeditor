#ifndef _TEXTURE_STREAM_
#define _TEXTURE_STREAM_

#include "bufferedstream.h"
#include <base/material.h>
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
	void initialise();											// Initialise data

	int       getDivisions() const;
	Material* getGlobal();
	Material* getMaterial(int x, int y);
	void      dropMaterial(int x, int y);
	void      dropMaterial(Material* m);

	protected:
	struct Stream { TextureStream* stream; char name[32]; char infoName[32]; };
	void setStreamTexture(Material* m, const Stream&, int x, int y);

	protected:
	std::vector<Stream> m_streams;
	Material*      m_template;		// Template material to create sub materials from
	Material*      m_materials;		// Array of sub materials
	Material*      m_global;		// Global material
	int*           m_ref;			// Reference counrers for materials
	int            m_divisions;		// How many divisions the whole is divided into
	bool           m_overlap;		// Do sub textures overlap by a pixel (to remove seams)
	vec2           m_offset, m_size;	// World coordinates for generated UVs
};

#endif

