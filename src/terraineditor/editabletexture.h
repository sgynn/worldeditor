#ifndef _EDITABLE_TEXTURE_
#define _EDITABLE_TEXTURE_

// So, we need an editable texture class that can be a stream, or be in memory
// Needs to get and set pixels, and update opengl textures
// Preferably keep separate from the actual terrain, and be an interface only for the editor.

#include "editor.h"
#include <base/texture.h>
#include <cstring>
#include <map>
#include <unordered_map>
#include <assert.h>

class TextureStream;
class BufferedStream;

/** Unified wrapper for all texture types */
class EditableTexture : public EditableMap {
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
	const base::Texture* getTexture(uint) const override;	// Get gpu texture (if applicable)
	int getWidth() const;							// Get image width
	int getHeight() const;							// Get image height

	public: // EditableMap interface
	int getChannels() const override { return m_channels; }
	Rect getRect() const override { return Rect(0,0,m_width,m_height); }
	void getValue(int x, int y, float* v) const override;
	void setValue(int x, int y, const float* v) override;
	void apply(const Rect& r) override { updateGPU(); }

	public:
	void getPixel(int x, int y, ubyte* pixel) const;
	void setPixel(int x, int y, ubyte* pixel);
	void clampPoint(Point& p) const;
	void clampRect(Rect& r) const;
	const ubyte* getData() const { return m_data; }

	protected:
	Mode   m_mode;
	int    m_width, m_height;
	int    m_channels;
	ubyte* m_data;
	base::Texture   m_texture;
	BufferedStream* m_stream;
};

/// Map type that writes to two images simultaneously - images must be the same resolution
class MultiTexture : public EditableMap {
	public:
	MultiTexture(EditableTexture* a, EditableTexture* b) : m_a(a), m_b(b) {}
	int getChannels() const override { return m_a->getChannels() + m_b->getChannels(); }
	Rect getRect() const override { return m_a->getRect(); }
	void getValue(int x, int y, float* v) const override { m_a->getValue(x,y,v); m_b->getValue(x,y,v+m_a->getChannels()); }
	void setValue(int x, int y, const float* v) override { m_a->setValue(x,y,v); m_b->setValue(x,y,v+m_a->getChannels()); }
	void apply(const Rect& r) override { m_a->updateGPU(); m_b->updateGPU(); }
	protected:
	EditableTexture* m_a;
	EditableTexture* m_b;
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
		for(auto i : m_paintBuffer) delete [] i.second;
		m_paintBuffer.clear();
	}
	// Standard integer division truncates which is bad for negative values
	inline static int divFloor(int v, int div) { return v / div - (v<0 && v % div != 0); }

	T* value(int x, int y) {
		Point b(divFloor(x, m_size), divFloor(y, m_size));
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

