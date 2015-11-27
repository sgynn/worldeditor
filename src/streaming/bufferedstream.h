#ifndef _BUFFERED_STREAM_
#define _BUFFERED_STREAM_

#include <base/math.h>
#include <list>
#include <map>
#include <cstdlib>

class TiffStream;

/** Streamed tiff file with write buffering */
class BufferedStream {
	public:
	BufferedStream(int bufferSize=128);
	virtual ~BufferedStream();

	bool openStream(const char* file);
	bool createStream(const char* file, int width, int height, int channels, int bitsPerChannel=8, void* init=0);
	void setBufferSize(int width, int height);
	virtual void closeStream();

	int width() const;
	int height() const;
	int pixelSize() const;
	int channels() const;

	virtual int getPixel(int x, int y, void* pixel);
	virtual int setPixel(int x, int y, void* pixel);
	virtual int getPixels(const Rect& rect, void* data);
	virtual int setPixels(const Rect& rect, void* data);
	virtual int fillBuffer(int x, int y);
	virtual int flush();

	protected:
	TiffStream* m_stream;
	ubyte*      m_buffer;
	Rect        m_bufferRect;
	int         m_bytes;	// bytes per pixel
	bool        m_changed;

	size_t getBufferAddress(int x, int y) const;
	virtual void streamOpened() {}
};


#include "terraineditor/undo.h"

/** Undo action for streamed editing */
class StreamUndo : public UndoCommand {
	public:
	StreamUndo(BufferedStream* source, int blockSize=16);
	~StreamUndo();
	virtual const char* getName() const;
	virtual void execute();
	void addRect(const Rect& r);
	protected:
	BufferedStream* m_source;
	int             m_blockSize;
	std::map<Point, char*> m_data;
};



#endif

