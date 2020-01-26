#ifndef _TIFF_
#define _TIFF_

#include <base/math.h>
#include <cstdio>

/** Image base class - move this into base ? */
class Image {
	public:
	Image(): m_width(0), m_height(0), m_depth(0), m_channels(0), m_data(0) {}
	virtual ~Image() { delete [] m_data; }
	int channels() const { return m_channels; }
	int bitDepth() const { return m_depth; }
	int width() const { return m_width; }
	int height() const { return m_height; }
	const uint8* data() const { return m_data; }
	protected:
	int m_width, m_height;
	uint8 m_depth;		// bits per channel (8/16)
	uint8 m_channels;	// Number of channels
	uint8* m_data;		// Raw data
};


/** Only need these static functions for API */
class Tiff {
	public:
	static Image* loadImage(const char* filename);
	static bool save(const Image* image, const char* filename);
};


/** Allow streaming - only uncompressed images */
class TiffStream {
	public:
	enum Mode { READ=1, WRITE=2, READWRITE=3 };
	static TiffStream* openStream(const char* filename, Mode mode=READ);
	static TiffStream* createStream(const char* filename, int width, int height, int channels, int bitsPerChannel=8, Mode mode=WRITE, void* data=0, size_t length=0);
	bool   good() const     { return m_stream!=0; }
	uint   bpp()  const     { return m_bitsPerSample * m_samplesPerPixel; }
	uint   width() const    { return m_width; }
	uint   height() const   { return m_height; }
	uint   channels() const { return m_samplesPerPixel; }

	size_t getPixel(int x, int y, void* data) const;
	size_t readBlock(int x, int y, int width, int height, void* data) const;

	size_t setPixel(int x, int y, void* data);
	size_t writeBlock(int x, int y, int width, int height, void* data);

	~TiffStream();

	private:
	FILE* m_stream;
	uint  m_width, m_height;
	uint  m_bitsPerSample;
	uint  m_samplesPerPixel;
	uint  m_rowsPerStrip;
	uint  m_stripCount;
	uint* m_stripOffsets;

	size_t getAddress(int x, int y) const;
};

#endif

