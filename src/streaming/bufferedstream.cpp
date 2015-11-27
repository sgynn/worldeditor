#include <cstdio>
#include <cstring>

#include "bufferedstream.h"
#include "tiff.h"

BufferedStream::BufferedStream(int bs) : m_stream(0), m_buffer(0), m_changed(false) {
	m_bufferRect.set(-65535,-65535, bs, bs);
}
BufferedStream::~BufferedStream() {
	closeStream();
	if(m_buffer) delete [] m_buffer;
}

void BufferedStream::setBufferSize(int w, int h) {
	flush();
	m_bufferRect.width = w;
	m_bufferRect.height = h;
	if(m_buffer) delete [] m_buffer;
	m_buffer = 0;
}

bool BufferedStream::openStream(const char* file) {
	closeStream();
	printf("Opening stream %s\n", file);
	m_stream = TiffStream::openStream(file, TiffStream::READWRITE);
	if(!m_stream) return printf("ERROR: Failed to open stream\n"), false;
	m_bytes = m_stream->bpp() / 8;
	streamOpened();
	return true;
}
bool BufferedStream::createStream(const char* file, int width, int height, int ch, int bpc, void* init) {
	closeStream();
	printf("Creating stream %s\n", file);
	m_stream = TiffStream::createStream(file, width, height, ch, bpc, TiffStream::READWRITE, init, ch*(bpc/8));
	if(!m_stream) return printf("ERROR: Failed to open stream\n"), false;
	m_bytes = m_stream->bpp() / 8;
	streamOpened();
	return true;
}
void BufferedStream::closeStream() {
	if(!m_stream) return;
	flush();
	delete m_stream;
	m_stream = 0;
}

int BufferedStream::width() const {
	return m_stream? m_stream->width(): 0;
}
int BufferedStream::height() const {
	return m_stream? m_stream->height(): 0;
}
int BufferedStream::pixelSize() const {
	return m_stream? m_bytes: 0;
}
int BufferedStream::channels() const {
	return m_stream? m_stream->channels(): 0;
}

// =========================================================================== //

inline size_t BufferedStream::getBufferAddress(int x, int y) const {
	return ((x-m_bufferRect.x) + (y-m_bufferRect.y)*m_bufferRect.width) * m_bytes;
}

int BufferedStream::getPixel(int x, int y, void* pixel) {
	if(m_bufferRect.contains(x, y)) {
		memcpy(pixel, m_buffer + getBufferAddress(x,y), m_bytes);
		return m_bytes;
	} else return m_stream->getPixel(x, y, pixel);
}

int BufferedStream::setPixel(int x, int y, void* pixel) {
	if(!m_bufferRect.contains(x, y)) fillBuffer(x-m_bufferRect.width/2, y-m_bufferRect.height/2);
	memcpy(m_buffer + getBufferAddress(x, y), pixel, m_bytes);
	m_changed = true;
	return 1;
}


int BufferedStream::getPixels(const Rect& r, void* data) {
	if(m_bufferRect.contains( r )) {
		// Read fully from buffer
		for(int i=0; i<r.height; ++i) {
			memcpy(((char*)data) + i*r.width*m_bytes, m_buffer + getBufferAddress(r.x,r.y+i), r.width*m_bytes);
		}
		return r.width * r.height;
	} else if(m_bufferRect.intersects( r )) {
		// Read partially from buffer
		size_t count = m_stream->readBlock(r.x, r.y, r.width, r.height, data);
		int off = r.x<m_bufferRect.x? m_bufferRect.x-r.x: 0;
		int len = r.x+r.width>m_bufferRect.right()? m_bufferRect.right()-r.x-off: r.width-off;
		int y0 = r.y<m_bufferRect.y? m_bufferRect.y-r.y: 0;
		int y1 = r.y+r.height>m_bufferRect.bottom()? m_bufferRect.bottom()-r.y: r.height;
		for(int i=y0; i<y1; ++i) {
			memcpy(((char*)data) + (i*r.width+off)*m_bytes, m_buffer + getBufferAddress(r.x+off,r.y+i), len*m_bytes);
		}
		return count;
	} else {	// Read fully from file
		return m_stream->readBlock(r.x, r.y, r.width, r.height, data);
	}
}
int BufferedStream::setPixels(const Rect& r, void* data) {
	if(!m_bufferRect.contains(r)) {
		fillBuffer(r.x+r.width/2-m_bufferRect.width/2, r.y+r.height/2-m_bufferRect.height/2);
	}
	for(int i=0; i<r.height; ++i) {
		memcpy(m_buffer + getBufferAddress(r.x, r.y+i), ((char*)data) + i*r.width*m_bytes, r.width*m_bytes);
	}
	m_changed = true;
	return r.width * r.height;
}

// =========================================================================== //


int BufferedStream::flush() {
	if(!m_changed) return 0;
	if(!m_stream) return 0;
	m_changed = false;
	return m_stream->writeBlock(m_bufferRect.x, m_bufferRect.y, m_bufferRect.width, m_bufferRect.height, m_buffer);
}
int BufferedStream::fillBuffer(int x, int y) {
	flush();
	m_bufferRect.x = x;
	m_bufferRect.y = y;
	int byteCount = m_bufferRect.width * m_bufferRect.height * m_bytes;
	if(!m_buffer) m_buffer = new ubyte[ byteCount ];
	memset(m_buffer, 0, byteCount);
	return m_stream->readBlock(m_bufferRect.x, m_bufferRect.y, m_bufferRect.width, m_bufferRect.height, m_buffer);
}


// =========================================================================== //




StreamUndo::StreamUndo(BufferedStream* source, int s) : m_source(source), m_blockSize(s) {}
StreamUndo::~StreamUndo() {
	for(std::map<Point, char*>::iterator i=m_data.begin(); i!=m_data.end(); ++i) delete i->second;
}
const char* StreamUndo::getName() const {
	return "Stream edit";
}
void StreamUndo::addRect(const Rect& r) {
	int x0 = r.x / m_blockSize;
	int y0 = r.y / m_blockSize;
	int x1 = r.right() / m_blockSize;
	int y1 = r.bottom() / m_blockSize;
	Rect block(0, 0, m_blockSize, m_blockSize);

	for(Point p(x0, y0); p.y<y1; ++p.y) {
		for(p.x = x0; p.x<x1; ++p.x) {
			if(m_data.find(p)==m_data.end()) {
				char* data = new char[ m_blockSize * m_blockSize * m_source->pixelSize() ];
				block.x = p.x * m_blockSize;
				block.y = p.y * m_blockSize;
				m_source->getPixels(block, data);
				m_data[p] = data;
			}
		}
	}
}

void StreamUndo::execute() {
	Rect r(0, 0, m_blockSize, m_blockSize);
	for(std::map<Point, char*>::iterator i=m_data.begin(); i!=m_data.end(); ++i) {
		r.x = i->first.x * m_blockSize;
		r.y = i->first.y * m_blockSize;
		m_source->setPixels(r, i->second);
	}
}



