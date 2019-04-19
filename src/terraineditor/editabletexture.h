#ifndef _EDITABLE_TEXTURE_
#define _EDITABLE_TEXTURE_

// So, we need an editable texture class that can be a stream, or be in memory
// Needs to get and set pixels, and update opengl textures
// Preferably keep separate from the actual terrain, and be an interface only for the editor.

#include <base/texture.h>
#include <cstring>
#include <map>
#include <unordered_map>

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
	TextureStream* getTextureStream() const;		// Get texture stream (if applicable)
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
	bool resize(int s) {
		if(m_paintBuffer.empty()) m_size = s;
		return m_paintBuffer.empty();
	}
	void clear() {
		for(auto i : m_paintBuffer) delete i.second;
		m_paintBuffer.clear();
	}
	T* value(int x, int y) {
		Point b(x/m_size, y/m_size);
		auto it = m_paintBuffer.find(b);
		T* buffer = it!=m_paintBuffer.end()? it->second: createPaintBuffer(b);
		return buffer + (x-(b.x*m_size) + (y-(b.y*m_size))*m_size);
	}

	private:
	inline static size_t hash_combine(size_t a, size_t b) { return a ^ (b + 0x9e3779b9 + (a<<6) + (a>>2)); }
	struct hashPoint { size_t operator()(const Point& p) const { return hash_combine(p.x, p.y); } };

	int m_size;
	//std::map<Point, T*> m_paintBuffer;
	std::unordered_map<Point, T*, hashPoint> m_paintBuffer;
	T* createPaintBuffer(const Point& p) {
		T* data = new T[m_size*m_size];
		memset(data, 0, sizeof(T)*m_size*m_size);
		m_paintBuffer[p] = data;
		return data;
	}
};

#endif

