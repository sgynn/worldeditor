#include "arraytexture.h"
#include <base/image.h>
#include <base/opengl.h>
#include <cstring>
#include <cstdio>


// OpenGL extension stuff
#ifdef WIN32
#define GL_TEXTURE_2D_ARRAY 0x8C1A
typedef void (APIENTRYP PFNGLTEXSTORAGE3DPROC) (GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth);
PFNGLTEXSTORAGE3DPROC glTexStorage3D = 0;

extern PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC glCompressedTexSubImage3D; // defined in base/texture.cpp
extern PFNGLTEXSUBIMAGE3DPROC glTexSubImage3D;
#endif

using namespace base;

ArrayTexture::ArrayTexture() : m_blankLayer(0) {
	#ifdef WIN32
	if(glTexStorage3D==0) {
		glTexStorage3D = (PFNGLTEXSTORAGE3DPROC)wglGetProcAddress("glTexStorage3D");
	}
	#endif
}
ArrayTexture::~ArrayTexture() {
	// delete all data
	for(size_t i=0; i<m_layers.size(); ++i) delete m_layers[i];
	if(m_blankLayer) delete m_blankLayer;
}


// ------------------------------------------------------------ //

int ArrayTexture::addTexture(Image& src) {
	// Move to heap
	m_layers.push_back(new Image(std::move(src)));
	return layers()-1;
}

int ArrayTexture::addBlankTexture() {
	m_layers.push_back(0);
	return m_blankLayer? 0: -1;
}

int ArrayTexture::setTexture(int index, Image& src) {
	addTexture(src);
	// Move to index if valid
	if(index < layers()-1) {
		delete m_layers[index];
		m_layers[index] = m_layers.back();
		m_layers.pop_back();
		return index;
	}
	return layers()-1;
}

void ArrayTexture::swapTextures(int a, int b) {
	if(a<0 || b<0 || a>=layers() || b>=layers()) return;
	Image* tmp = m_layers[a];
	m_layers[a] = m_layers[b];
	m_layers[b] = tmp;
}

void ArrayTexture::moveTexture(int index, int target) {
	if(target<0) target = 0;
	if(target >= layers()) target = layers()-1;
	if(target == index || index<0 || index>=layers()) return;
	// Bubble
	int d = target>index? 1: -1;
	for(int i=index; i!=target; i+=d) {
		swapTextures(i, i+d);
	}
}

void ArrayTexture::removeTexture(int index) {
	if(index>=0 && index<layers()) {
		delete m_layers[index];
		m_layers.erase(m_layers.begin() + index);
	}
}

// ------------------------------------------------------------ //

void ArrayTexture::createBlankTexture(unsigned c, int ch, int w, int h) {
	int mips = 0;
	for(int i=w>h? w: h; i; i>>=1) ++mips;
	uint8** faces = new uint8*[mips];
	m_blankLayer = new Image(Image::FLAT, (Image::Format)ch, w, h, 1, mips, faces);
	// Create main image
	uint8* data = faces[0] = new uint8[w*h*ch];
	for(int i=0; i<w*h; ++i) memcpy(&data[i*ch], &c, ch);
	// Create mips - can just copy from data as all pixels are the same
	for(int i=1; i<mips; ++i) {
		w = w>>1? w>>1: 1;
		h = h>>1? h>>1: 1;
		faces[i] = new uint8[w*h*ch];
		memcpy(faces[i], data, w*h*ch);
	}
}

// ------------------------------------------------------------ //

void ArrayTexture::decompressAll() {
	for(size_t i=0; i<m_layers.size(); ++i) {
		if(m_layers[i] && m_layers[i]->isCompressed()) {
			Image decompressed = m_layers[i]->convert(Image::RGBA8);
			if(!decompressed) asm("int $3\nnop");
			delete m_layers[i];
			m_layers[i] = new Image(std::move(decompressed));
		}
	}
}

int ArrayTexture::build() {
	if(m_layers.empty()) return 0; // nothing to build
	GL_CHECK_ERROR;
	//decompressAll();

	// Check formats
	Image* firstLayer = m_layers[0]? m_layers[0]: m_blankLayer;
	for(size_t i=1; i<m_layers.size(); ++i) {
		Image* layer = m_layers[i]? m_layers[i]: m_blankLayer;
		if(!layer) continue;
		if(layer->isCompressed() != firstLayer->isCompressed()) {
			decompressAll();
			layer = m_layers[i];
		}
		if(layer->getFormat() != firstLayer->getFormat()) return i | 0x100; // Different formats
	}

	// Get mip level
	int mips = 99;
	for(size_t i=0; i<m_layers.size(); ++i) {
		Image* layer = m_layers[i]? m_layers[i]: m_blankLayer;
		if(!layer) continue;
		if(mips > layer->getMips()) mips = layer->getMips();
	}

	// Check sizes
	if(!firstLayer) return 0; // Invalid
	int w = firstLayer->getWidth() >> (firstLayer->getMips() - mips);
	int h = firstLayer->getHeight() >> (firstLayer->getMips() - mips);
	for(size_t i=1; i<m_layers.size(); ++i) {
		Image* layer = m_layers[i]? m_layers[i]: m_blankLayer;
		if(!layer) continue;
		int shift = layer->getMips() - mips;
		if(layer->getWidth() >> shift != w || layer->getHeight()>>shift != h) return i | 0x200; // invalid size
	}

	// Convert formats
	static const Texture::Format fmap[] = { Texture::NONE, Texture::R8, Texture::RG8, Texture::RGB8, Texture::RGBA8, Texture::BC1, Texture::BC2, Texture::BC3, Texture::BC4, Texture::BC5 };
	static const int glmap[] = { 0, GL_LUMINANCE, GL_LUMINANCE_ALPHA, GL_RGB, GL_RGBA, GL_COMPRESSED_RGB_S3TC_DXT1_EXT, GL_COMPRESSED_RGBA_S3TC_DXT3_EXT, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, GL_COMPRESSED_RED_RGTC1, GL_COMPRESSED_RG_RGTC2 };
	Texture::Format format = fmap[ m_layers[0]->getFormat() ];
	int glFormat = glmap[ format ];

	
	// Create texture
	m_texture = Texture::create(Texture::ARRAY2D, w, h, layers(), format, 0, 0);
	glTexStorage3D(GL_TEXTURE_2D_ARRAY, mips, glFormat, w, h, layers());
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, mips);
	m_texture.setFilter(Texture::ANISOTROPIC);
	GL_CHECK_ERROR;

	// Copy data - ToDo - Texture::setPixels() functions for this. Also use pixel buffer objects for speed. see ogre.
	for(size_t i=0; i<m_layers.size(); ++i) {
		Image* layer = m_layers[i]? m_layers[i]: m_blankLayer;
		if(layer) {
			int shift = layer->getMips() - mips;
			for(int mip=0; mip<mips; ++mip) {
				int mw = w>>mip? w>>mip: 1;
				int mh = h>>mip? h>>mip: 1;
				if(layer->isCompressed()) {
					unsigned size = Texture::getMemorySize(format, mw, mh);
					glCompressedTexSubImage3D(GL_TEXTURE_2D_ARRAY, mip, 0, 0, i, mw, mh, 1, glFormat, size, layer->getData(mip + shift));
				} else {
					glTexSubImage3D(GL_TEXTURE_2D_ARRAY, mip, 0, 0, i, mw, mh, 1, glFormat, GL_UNSIGNED_BYTE, layer->getData(mip + shift));
				}
				GL_CHECK_ERROR;
			}
		}
	}
	m_changed = 0;
	return 0;
}



