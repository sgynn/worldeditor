#include "editabletexture.h"
#include "streaming/texturestream.h"

#include <base/png.h>
#include <base/dds.h>
#include <base/tiff.h>

#include <base/opengl.h>
#include <cstring>
#include <cstdio>


using namespace base;

EditableTexture::EditableTexture(int w, int h, int ch, bool gpu) : m_mode(gpu? TEXTURE: IMAGE), m_width(w), m_height(h), m_channels(ch) {
	m_data = new ubyte[w * h * ch];
	memset(m_data, 0, w*h*ch);
	if(gpu) {
		m_texture = Texture::create(w, h, ch, m_data);
		m_texture.setWrap( Texture::CLAMP );
	}
}

EditableTexture::EditableTexture(const char* filename, bool gpu) : m_mode(gpu? TEXTURE: IMAGE), m_channels(0), m_data(0) {
	const char* ext = strrchr(filename, '.');
	Image image;
	if(strcmp(ext, ".png")==0) image = PNG::load(filename);
	else if(strcmp(ext, ".tif") == 0 || strcmp(ext, ".tiff")==0) image = Tiff::load(filename);
	else if(strcmp(ext, ".dds")==0) image = DDS::load(filename);
	else printf("Unknown texture format %s\n", filename);

	if(image) {
		m_width = image.getWidth();
		m_height = image.getHeight();
		m_channels = image.getChannels();
		m_data = image.stealData();
	}

	// Make texture
	if(m_data && gpu) {
		m_texture = Texture::create(m_width, m_height, m_channels, m_data);
		m_texture.setWrap( Texture::CLAMP );
	}
}

EditableTexture::EditableTexture(TextureStream* stream) : m_mode(TEXTURESTREAM), m_data(0), m_stream(stream) {
	m_width = stream->width();
	m_height = stream->height();
	m_channels = stream->channels();
}
EditableTexture::EditableTexture(BufferedStream* stream) : m_mode(STREAM), m_data(0), m_stream(stream) {
	m_width = stream->width();
	m_height = stream->height();
	m_channels = stream->channels();
}

EditableTexture::EditableTexture(Texture t) : m_mode(TEXTURE), m_texture(t), m_stream(0) {
	m_width = t.width();
	m_height = t.height();
	m_channels = t.depth();

	// read data from gpu
	t.bind();
	GLint format;
	m_data = new ubyte[m_width*m_height*m_channels];
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &format);
	glGetTexImage(GL_TEXTURE_2D, 0, format, GL_UNSIGNED_BYTE, m_data);
}

EditableTexture::~EditableTexture() {
	if(m_mode == IMAGE || m_mode == TEXTURE) {
		if(m_data) delete [] m_data;
	} else {
		if(m_stream) delete m_stream;
	}
}

// ------------------------------------------------------------------------------------------------------- //

int EditableTexture::getWidth() const {
	return m_width;
}
int EditableTexture::getHeight() const {
	return m_height;
}
const base::Texture* EditableTexture::getTexture(uint) const {
	return &m_texture;
}
EditableTexture::Mode EditableTexture::getMode() const {
	return m_mode;
}

// ------------------------------------------------------------------------------------------------------- //

bool EditableTexture::flush() {
	if(m_mode == STREAM || m_mode == TEXTURESTREAM) m_stream->flush();
	return true;
}
bool EditableTexture::save(const char* filename) {
	if(m_mode == STREAM || m_mode == TEXTURESTREAM) {
		m_stream->flush();
		// TODO: If filename is dufferent, copy file
		return true;
	}

	if(!m_data) return false;
	const char* ext = strrchr(filename, '.');
	if(strcmp(ext, ".png")==0) {
		Image image((Image::Format)m_channels, m_width, m_height, m_data);
		PNG::save(image, filename);
		image.stealData();
		printf("Saved %s\n", filename);
		return true;
	}
	else printf("Can only save png files\n");
	return false;
}

bool EditableTexture::updateGPU() {
	static GLenum fmt[] = { 0, GL_LUMINANCE, GL_LUMINANCE_ALPHA, GL_RGB, GL_RGBA };
	if(m_mode == TEXTURE) {
		GLenum f = fmt[ m_channels ];
		m_texture.bind();
		glTexImage2D( GL_TEXTURE_2D, 0, f, m_width, m_height, 0, f, GL_UNSIGNED_BYTE, m_data);
	}
	else if(m_mode == TEXTURESTREAM) {
		static_cast<TextureStream*>(m_stream)->updateTextures();
	}
	return true;
}


// ------------------------------------------------------------------------------------------------------- //


void EditableTexture::getPixel(int x, int y, ubyte* pixel) const {
	if(m_data) memcpy(pixel, m_data + (x+y*m_width)*m_channels, m_channels);
	else if(m_stream) m_stream->getPixel(x, y, pixel);
}
void EditableTexture::setPixel(int x, int y, ubyte* pixel) {
	if(m_data) memcpy(m_data + (x+y*m_width)*m_channels, pixel, m_channels);
	else if(m_stream) m_stream->setPixel(x, y, pixel);
}


void EditableTexture::getValue(int x, int y, float* v) const {
	ubyte pixel[4];
	getPixel(x,y,pixel);
	for(int i=0; i<m_channels; ++i) v[i] = pixel[i];
}
void EditableTexture::setValue(int x, int y, const float* v) {
	ubyte pixel[4];
	for(int i=0; i<m_channels; ++i) pixel[i] = v[i];
	setPixel(x,y,pixel);
}


// ========================================================================================================= //


void EditableTexture::clampPoint(Point& p) const {
	if(p.x < 0) p.x = 0;
	if(p.y < 0) p.y = 0;
	if(p.x >= m_width) p.x = m_width - 1;
	if(p.y >= m_height) p.y = m_height - 1;
}

void EditableTexture::clampRect(Rect& r) const {
	Rect tr(0,0,m_width,m_height);
	r.intersect(tr);
}

