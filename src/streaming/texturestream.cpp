#include "texturestream.h"
#include <base/opengl.h>
#include <cstdio>

TextureStream::TextureStream(): m_divisions(0), m_textures(0), m_ref(0), m_overlap(false) {}
TextureStream::~TextureStream() {
	// Destroy all textures
	if(m_textures) {
		m_global.destroy();
		for(int i=0; i<m_divisions * m_divisions; ++i) {
			if(m_ref[i]>0) m_textures[i].destroy();
		}
		delete [] m_textures;
		delete [] m_ref;
	}
}

void TextureStream::initialise(int max, bool overlap) {
	if(m_textures) return;
	// Calculate number of divisions for valid texture sizes
	int w = width(), h=height();
	m_divisions = 1;
	m_overlap = overlap;
	while(w>max || h>max) {
		m_divisions *= 2;
		w/=2;
		h/=2;
	}
	// Initialise arrays
	int count = m_divisions * m_divisions;
	m_textures = new Texture[count];
	m_ref = new int[count];
	memset(m_ref, 0, count * sizeof(int));

	const int M = 0x7fffffff;
	m_dirty.set(M,M,-M,-M);
}

int TextureStream::setPixel(int x, int y, void* pixel) {
	m_dirty.include(x, y);
	return BufferedStream::setPixel(x, y, pixel);
}
int TextureStream::setPixels(const Rect& rect, void* data) {
	m_dirty.include(rect);
	return BufferedStream::setPixels(rect, data);
}

Texture& TextureStream::getGlobalTexture() {
	if(m_global.width()==0) createGlobalTexture(2048);
	return m_global;
}

Texture& TextureStream::getTexture(int x, int y) {
	int k = x + y * m_divisions;
	if(m_ref[k]==0) createTexture(x, y);
	++m_ref[k];
	return m_textures[k];
}
void TextureStream::dropTexture(int x, int y) {
	int k = x + y * m_divisions;
	if(m_ref[k]==1) {
//		printf("Destroy texture %d,%d : %u\n", x, y, m_textures[k].unit());
		m_textures[k].destroy();
	}
	if(m_ref[k]>0) --m_ref[k];
}

int TextureStream::getDivisions() const {
	return m_divisions;
};

Rect TextureStream::getPixelRect(int x, int y) const {
	vec2 size( (float) width() / m_divisions, (float) height() / m_divisions );
	vec2 fa( size.x*x, size.y*y);
	vec2 fb = fa + size;
	if(m_overlap) {
		// Round with overlap
		fa = floor(fa);
		fb.x = fb.x==floor(fb.x) && x<m_divisions-1? fb.x+1: ceil(fb.x);
		fb.y = fb.y==floor(fb.y) && y<m_divisions-1? fb.y+1: ceil(fb.y);
	} else {
		// Round
		const vec2 half(0.5,0.5);
		fa = floor(fa+half);
		fb = floor(fb+half);
	}
	return Rect((int)fa.x, (int)fa.y, (int)(fb.x-fa.x), (int)(fb.y-fa.y));
}

void TextureStream::createTexture(int x, int y) {
	int k = x + y * m_divisions;
	Rect r = getPixelRect(x, y);
	char* data = new char[ r.width * r.height * pixelSize() ];
	getPixels(r, data);
	if(m_textures[k].width()!=r.width) {
		m_textures[k] = Texture::create(r.width, r.height, channels(), data);
//		printf("Created texture %d,%d (%d %d %d %d) : %u\n", x, y, r.x, r.y, r.width, r.height, m_textures[k].unit());
	} else {
		const GLenum formats[] = { 0, GL_LUMINANCE, GL_LUMINANCE_ALPHA, GL_RGB, GL_RGBA };
		GLenum fmt = formats[ channels() ];
		m_textures[k].bind();
		glTexImage2D( GL_TEXTURE_2D, 0, fmt, r.width, r.height, 0, fmt, GL_UNSIGNED_BYTE, data);
//		printf("Updated texture %d,%d (%d %d %d %d)\n", x, y, r.x, r.y, r.width, r.height);
	}

	delete [] data;
}

void TextureStream::createGlobalTexture(int size) {
	// On the offchance that the image is small
	if(width()<=size && height()<=size) {
		char* data = new char[ width() * height() * pixelSize() ];
		m_global = Texture::create(width(), height(), channels(), data);
		delete [] data;
		printf("No global texture\n");
		return;
	}

	// white
	char d[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	m_global = Texture::create(4,4,1,d);
	return;

	// lets just use nearest pixel for now
	char* data = new char[ size * size * pixelSize() ];
	for(int x=0; x<size; ++x) for(int y=0; y<size; ++y) {
		char* p = data + (x + y*size) * pixelSize();
		getPixel(x*width()/size, y*height()/size, p);
	}
	m_global = Texture::create(size, size, channels(), data);
	printf("Global texture created\n");
	delete [] data;
}

void TextureStream::updateTextures() {
	if(m_dirty.width==0 || m_dirty.height==0) return;
	// Which textures need updating
	Point a(m_dirty.x / m_divisions, m_dirty.y / m_divisions);
	Point b(m_dirty.right() / m_divisions, m_dirty.bottom() / m_divisions);
	// Recreate these textures if they are loaded
	for(int x=a.x; x<=b.x; ++x) for(int y=a.y; y<=b.y; ++y) {
		int k = x + y * m_divisions;
		if(m_ref[k]>0) {
			createTexture(x, y);
		}
	}
	// Also perhaps update global texture (part of)

	const int M = 0x7fffffff;
	m_dirty.set(M,M,-M,-M);
}


// ======================================================================================= //

MaterialStream::MaterialStream(Material* base) 
	: m_template(base), m_materials(0), m_global(0), m_ref(0), m_divisions(0), m_offset(0,0), m_size(1,1) {
}
MaterialStream::~MaterialStream() {
	for(uint i=0; i<m_streams.size(); ++i) delete m_streams[i].stream;
	if(m_materials) {
		delete [] m_materials;
		delete [] m_ref;
	}
	if(m_global) delete m_global;
}
bool MaterialStream::addStream(const char* name, TextureStream* stream) {
	// Can't add a stream with a higher tile count
	if(m_materials && stream->getDivisions() < m_divisions) {
		return false;
	}
	// Add stream to list
	Stream s;
	s.stream = stream;
	strcpy(s.name, name);
	strcpy(s.infoName, name);
	strcat(s.infoName, "Info");
	m_streams.push_back(s);

	// Add any loaded textures
	if(m_materials) {
		for(int x=0; x<m_divisions; ++x) for(int y=0; y<m_divisions; ++y) {
			Material* m = m_materials + x + y*m_divisions;
			setStreamTexture(m, s, x, y);
		}
	}

	// Add global texture
	if(m_global) m_global->setTexture(name, stream->getGlobalTexture() );

	return true;
}
void MaterialStream::removeStream(TextureStream* s) {
	uint index = 0;
	while(index<m_streams.size() && m_streams[index].stream!=s) ++index;
	if(index == m_streams.size()) return; // Stream not in list

	// destroy stream end textures
	delete m_streams[index].stream;
	m_streams.erase( m_streams.begin() + index );
}

void MaterialStream::setCoordinates(const vec2& s, const vec2& o) {
	m_size = s;
	m_offset = o;
}

void MaterialStream::initialise() {
	if(m_streams.empty()) return; // must call initialise after streams are added ??
	if(!m_materials) {
		m_divisions = 1;
		for(uint i=0; i<m_streams.size(); ++i) {
			if(m_streams[i].stream->getDivisions() > m_divisions) m_divisions = m_streams[i].stream->getDivisions();
		}
		int count = m_divisions * m_divisions;
		m_materials = new Material[ count ];
		m_ref = new int[ count ];
		memset(m_ref, 0, count * sizeof(int));
	}
}

int MaterialStream::getDivisions() const {
	return m_divisions;
}

Material* MaterialStream::getGlobal() {
	if(!m_global) {
		m_global = new Material(*m_template);
		float info[4] = { m_offset.x, m_offset.y, m_size.x, m_size.y };
		for(uint i=0; i<m_streams.size(); ++i) {
			m_global->setTexture(m_streams[i].name, m_streams[i].stream->getGlobalTexture() );
			m_global->setFloat4(m_streams[i].infoName, info);
		}
	}
	return m_global;
}

Material* MaterialStream::getMaterial(int x, int y) {
	int k = x + y * m_divisions;
	if(m_ref[k] == 0) {
		// Create material - need to clone m_template
		m_materials[k] = *m_template; // does this work?

		// Add textures
		for(uint i=0; i<m_streams.size(); ++i) {
			setStreamTexture(m_materials+k, m_streams[i], x, y);
		}
	}
	++m_ref[k];
	return m_materials + k;
}

void MaterialStream::dropMaterial(int x, int y) {
	int k = x + y * m_divisions;
	if(--m_ref[k]==0) {
		// Destroy material
		for(uint i=0; i<m_streams.size(); ++i) {
			int d = m_divisions / m_streams[i].stream->getDivisions();
			m_streams[i].stream->dropTexture(x/d, y/d);
		}
	}
}
void MaterialStream::dropMaterial(Material* m) {
	for(int i=0; i<m_divisions*m_divisions; ++i) {
		if(m_materials+i == m) {
			dropMaterial(i%m_divisions, i/m_divisions);
			break;
		}
	}
}

void MaterialStream::setStreamTexture(Material* m, const Stream& stream, int x, int y) {
	int d = m_divisions / stream.stream->getDivisions();
	m->setTexture( stream.name, stream.stream->getTexture(x/d, y/d) );
	// Offet values - ToDo: fix for overlap
	float info[4];
	info[0] = (float) (x/d) * d / m_divisions * m_size.x + m_offset.x;
	info[1] = (float) (y/d) * d / m_divisions * m_size.y + m_offset.y;
	info[2] = 1.0 / stream.stream->getDivisions() * m_size.x;
	info[3] = 1.0 / stream.stream->getDivisions() * m_size.y;
	m->setFloat4( stream.infoName, info );
}


