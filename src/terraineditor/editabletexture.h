#ifndef _EDITABLE_TEXTURE_
#define _EDITABLE_TEXTURE_

// So, we need an editable texture class that can be a stream, or be in memory
// Needs to get and set pixels, and update opengl textures
// Preferably keep separate from the actual terrain, and be an interface only for the editor.

#include <base/texture.h>
#include <cstring>
#include <map>

class TextureStream;
class BufferedStream;

/** Unified wrapper for all texture types */
class EditableTexture {
	public:
	enum Mode { IMAGE, TEXTURE, STREAM, TEXTURESTREAM };


	EditableTexture(int width, int height, int channels, bool gpu);	// Create new texture
	EditableTexture(const char* filename, bool gpu);				// Load texture
	EditableTexture(base::Texture);									// Create from existing loaded texture
	EditableTexture(TextureStream* stream);							// Use texture stream
	EditableTexture(BufferedStream* stream);						// Use texture stream without gpu
	~EditableTexture();

	bool save(const char* filename);				// Save texture as file
	bool updateGPU();								// Update textute on GPU
	bool flush();									// Flush stream

	Mode getMode() const;							// Get texture usage mode
	const base::Texture& getTexture() const;		// Get gpu texture (if applicable)
	int getChannels() const;						// Get number of channels in image
	int getWidth() const;							// Get image width
	int getHeight() const;							// Get image height

	public:
	void getPixel(int x, int y, ubyte* pixel) const;
	void setPixel(int x, int y, ubyte* pixel);
	void clampPoint(Point& p) const;
	void clampRect(Rect& r) const;

	protected:
	Mode   m_mode;
	int    m_width, m_height;
	int    m_channels;
	ubyte* m_data;
	base::Texture   m_texture;
	BufferedStream* m_stream;
};

/** Buffer for managing maximum values when painting */
template<typename T>
class PaintBuffer {
	public:
	PaintBuffer(int size) : m_size(size) {}
	~PaintBuffer() { clear(); }
	void clear() {
		for(typename std::map<Point, T*>::iterator i=m_paintBuffer.begin(); i!=m_paintBuffer.end(); ++i) delete i->second;
		m_paintBuffer.clear();
	}
	T* value(int x, int y) {
		Point b(x/m_size, y/m_size);
		typename std::map<Point,T*>::iterator it = m_paintBuffer.find(b);
		T* buffer = it!=m_paintBuffer.end()? it->second: createPaintBuffer(b);
		return buffer + (x-(b.x*m_size) + (y-(b.y*m_size))*m_size);
	}

	private:
	int m_size;
	std::map<Point, T*> m_paintBuffer;
	T* createPaintBuffer(const Point& p) {
		T* data = new T[m_size*m_size];
		memset(data, 0, sizeof(T)*m_size*m_size);
		m_paintBuffer[p] = data;
		return data;
	}
};

#endif

