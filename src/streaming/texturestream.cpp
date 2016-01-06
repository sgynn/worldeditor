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
		m_textures[k] = Texture::create(r.width, r.height, (Texture::Format) channels(), data);
		m_textures[k].setWrap( Texture::CLAMP );
	} else {
		const GLenum formats[] = { 0, GL_LUMINANCE, GL_LUMINANCE_ALPHA, GL_RGB, GL_RGBA };
		GLenum fmt = formats[ channels() ];
		m_textures[k].bind();
		glTexImage2D( GL_TEXTURE_2D, 0, fmt, r.width, r.height, 0, fmt, GL_UNSIGNED_BYTE, data);
	}
	delete [] data;
}

void TextureStream::createGlobalTexture(int size) {
	// On the offchance that the image is small
	if(width()<=size && height()<=size) {
		char* data = new char[ width() * height() * pixelSize() ];
		m_global = Texture::create(width(), height(), (Texture::Format) channels(), data);
		delete [] data;
		printf("No global texture\n");
		return;
	}

	// white
	char d[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	m_global = Texture::create(4, 4, Texture::R8, d);
	return;

	// lets just use nearest pixel for now (super slow - perhaps back thread it)
	char* data = new char[ size * size * pixelSize() ];
	for(int x=0; x<size; ++x) for(int y=0; y<size; ++y) {
		char* p = data + (x + y*size) * pixelSize();
		getPixel(x*width()/size, y*height()/size, p);
	}
	m_global = Texture::create(size, size, (Texture::Format)channels(), data);
	printf("Global texture created\n");
	delete [] data;
}

void TextureStream::updateTextures() {
	if(m_dirty.width==0 || m_dirty.height==0) return;
	// Which textures need updating
	int div = width() / m_divisions;
	Point a(m_dirty.x / div, m_dirty.y / div);
	Point b(m_dirty.right() / div, m_dirty.bottom() / div);
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
	: m_template(base), m_global(0), m_materials(0), m_divisions(0), m_offset(0,0), m_size(1,1) {
}
MaterialStream::~MaterialStream() {
	for(uint i=0; i<m_streams.size(); ++i) delete m_streams[i].texture;
	if(m_materials) {
		for(int i=0; i<m_divisions*m_divisions; ++i) delete m_materials[i].material;
		delete [] m_materials;
	}
	for(uint i=0; i<m_delete.size(); ++i) delete m_delete[i].material;
	if(m_global) delete m_global;
}

void MaterialStream::setTexture(const char* name, const Texture& tex) {
	if(m_template) m_template->setTexture(name, tex);
	if(m_global) m_global->setTexture(name, tex);
	if(m_materials) {
		for(int i=0; i<m_divisions*m_divisions; ++i) {
			if(m_materials[i].material) m_materials[i].material->setTexture(name, tex);
		}
	}
}

void MaterialStream::copyParam(const char* name) {
	float values[4];
	int n = m_template->getFloatv(name, values);
	if(n==0) return;
	if(m_global) m_global->setFloatv(name, n, values);
	if(m_materials) {
		for(int i=0; i<m_divisions*m_divisions; ++i) {
			if(m_materials[i].ref) m_materials[i].material->setFloatv(name, n, values);
		}
	}
}

void MaterialStream::updateShader() {
	if(m_global) m_global->setShader( m_template->getShader() );
	if(m_materials) {
		for(int i=0; i<m_divisions*m_divisions; ++i) {
			if(m_materials[i].ref) m_materials[i].material->setShader( m_template->getShader() );
		}
	}
}

void MaterialStream::addStream(const char* name, TextureStream* texture) {
	// Add stream to list
	Stream stream;
	stream.texture = texture;
	strcpy(stream.name, name);
	strcpy(stream.infoName, name);
	strcat(stream.infoName, "Info");
	m_streams.push_back(stream);

	// Rebuild submaterial list if we need more divisions
	if(texture->getDivisions() > m_divisions) build();

	// Add any loaded textures
	if(m_materials) {
		for(int x=0; x<m_divisions; ++x) for(int y=0; y<m_divisions; ++y) {
			SubMaterial& m = m_materials[ x + y*m_divisions ];
			if(m.ref > 0) {
				setStreamTexture(m.material, stream, x, y);
			}
		}
	}

	// Add global texture
	if(m_global) m_global->setTexture(name, texture->getGlobalTexture() );
}
void MaterialStream::removeStream(TextureStream* s) {
	for(uint i=0; i<m_streams.size(); ++i) {
		if(m_streams[i].texture == s) {
			m_streams.erase( m_streams.begin() + i );
			break;
		}
	}

	// Rebuild with no submaterials
	if(m_streams.empty()) build();
}

void MaterialStream::setCoordinates(const vec2& size, const vec2& offset) {
	m_size = size;
	m_offset = offset;
}

void MaterialStream::build() {
	printf("Build material stream\n");

	// Get needed divisions
	int divisions = 1;
	for(uint i=0; i<m_streams.size(); ++i) {
		int d = m_streams[i].texture->getDivisions();
		if(d > divisions) divisions = d;
	}

	// Is it different
	if(divisions != m_divisions) {
		// Clear out old sub materials
		if(m_divisions > 1) {
			for(int i=0; i<m_divisions * m_divisions; ++i) {
				if(m_materials[i].ref > 0) m_delete.push_back(m_materials[i]);
				else delete m_materials[i].material;
			}
			delete [] m_materials;
			m_materials = 0;
		}

		// Create sub materials
		if(divisions > 1) {
			int count = divisions * divisions;
			m_materials = new SubMaterial[ count ];
			for(int i=0; i<count; ++i) {
				m_materials[i].material = 0;
				m_materials[i].ref = 0;
				m_materials[i].index.x = i%divisions;
				m_materials[i].index.y = i/divisions;
				m_materials[i].divisions = divisions;
			}
		}
		m_divisions = divisions;
	}
}


int MaterialStream::getDivisions() const {
	return m_divisions;
}

Material* MaterialStream::getTemplate() const {
	return m_template;
}

Material* MaterialStream::getGlobal() {
	if(!m_global) {
		m_global = new Material(*m_template);
		float info[4] = { m_offset.x, m_offset.y, m_size.x, m_size.y };
		for(uint i=0; i<m_streams.size(); ++i) {
			m_global->setTexture(m_streams[i].name, m_streams[i].texture->getGlobalTexture() );
			m_global->setFloat4(m_streams[i].infoName, info);
		}
	}
	return m_global;
}

Material* MaterialStream::getMaterial(int x, int y) {
	if(!m_materials) return getGlobal();
	int k = x + y * m_divisions;
	SubMaterial& m = m_materials[k];
	if(!m.material) {
		// Create material
		m.material = new Material(*m_template);

		// Add streamed sub textures
		for(uint i=0; i<m_streams.size(); ++i) {
			setStreamTexture(m.material, m_streams[i], x, y);
		}
	}
	++m.ref;
	return m.material;
}

void MaterialStream::dropMaterial(Material* m) {
	if(!m || m==m_global) return;
	// Find material in list
	for(int i=0; i<m_divisions*m_divisions; ++i) {
		if(m_materials[i].material == m) {
			dropMaterial(m_materials[i]);
			return;
		}
	}
	// Drop any in delete list
	for(uint i=0; i<m_delete.size(); ++i) {
		if(m_delete[i].material == m) {
			dropMaterial(m_delete[i]);
			if(m_delete[i].ref==0) m_delete.erase(m_delete.begin()+i);
			return;
		}
	}
}
void MaterialStream::dropMaterial(SubMaterial& m) {
	if(--m.ref == 0) {
		for(uint i=0; i<m_streams.size(); ++i) {
			int d = m.divisions / m_streams[i].texture->getDivisions();
			m_streams[i].texture->dropTexture( m.index.x/d, m.index.y/d );
		}
		delete m.material;
		m.material = 0;
	}
}

void MaterialStream::setStreamTexture(Material* m, const Stream& stream, int x, int y) {
	int d = m_divisions / stream.texture->getDivisions();
	m->setTexture( stream.name, stream.texture->getTexture(x/d, y/d) );
	// Offet values - ToDo: fix for overlap
	float info[4];
	info[0] = (float) (x/d) * d / m_divisions * m_size.x + m_offset.x;
	info[1] = (float) (y/d) * d / m_divisions * m_size.y + m_offset.y;
	info[2] = 1.0 / stream.texture->getDivisions() * m_size.x;
	info[3] = 1.0 / stream.texture->getDivisions() * m_size.y;
	m->setFloat4( stream.infoName, info );
}


