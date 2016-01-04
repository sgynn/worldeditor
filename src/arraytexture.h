#ifndef _ARRAY_TEXTURE_
#define _ARRAY_TEXTURE_

#include <base/texture.h>
#include <vector>

namespace base { class DDS; class PNG; }

/** Texture array manager class - assembles and updates the texture */
class ArrayTexture {
	public:
	ArrayTexture();
	~ArrayTexture();

	int  addTexture(const base::DDS& src);			// Add a dds texture
	int  setTexture(int index, const base::DDS& src);// Change a texture
	int  addBlankTexture();							// Add blank texture
	void removeTexture(int index);					// Delete a texture
	void moveTexture(int layer, int targetIndex);	// Change the index of a texture
	void swapTextures(int a, int b);				// Swap texture indices
	int  build();									// Build opengl texture

	int layers() const { return m_layers.size(); }
	const base::Texture& getTexture() const { return m_texture; }
	void createBlankTexture(unsigned colour, int channels=4, int w=2048, int h=2048);

	protected:
	void decompressAll();						// Decompress all compressed textures
	std::vector< base::DDS* > m_layers;			// cache data for rebuilding
	base::DDS*    m_blankLayer;
	base::Texture m_texture;					// Hardware texture object
	int m_changed;								// Change flags
};

#endif

