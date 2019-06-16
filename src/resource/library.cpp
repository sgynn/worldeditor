#include "library.h"
#include "file.h"

#include <base/opengl.h>
#include <base/model.h>
#include <base/png.h>
#include <base/dds.h>
#include <base/xmodel.h>
#include <base/material.h>


#include <cstring>
#include <cstdio>

using base::Material;
using base::Texture;

Library::Library() {
	addPath(".");

	// Create some default textures
	createDefaultTexture("white", 0xffffff);
	createDefaultTexture("black", 0);
	createDefaultTexture("transparent", 0, 0);
	createDefaultTexture("flat", 0x008000);
}
Library::~Library() {
	// Delete everything
}


//// File path stuff ////
void Library::addPath(const char* path) {
	for(uint i=0; i<m_paths.size(); ++i)
		if(strcmp(path, m_paths[i])==0) return;
	m_paths.push_back(path);
}
void Library::removePath(const char* path) {
	for(uint i=0; i<m_paths.size(); ++i) {
		if(strcmp(path, m_paths[i])==0) {
			m_paths.erase( m_paths.begin() + i );
			return;
		}
	}
}
bool Library::findFile(const char* file, char* path) {
	for(uint i=0; i<m_paths.size(); ++i) {
		if(i==0) strcpy(path, file);
		else sprintf(path, "%s/%s", m_paths[i], file);
		FILE* tmp = fopen(path, "r");
		if(tmp) { fclose(tmp); return true; }
	}
	printf("Error: File not found %s\n", path);
	return false;
}


Material* Library::material(const char* name) {
	MaterialMap::iterator it = m_materials.find(name);
	if(it==m_materials.end()) {
		// Load material
		char path[1024];
		if(findFile(name, path)) {
			File file = File::load(path);
			base::XML xml = base::XML::parse(file);
			Material* m = loadMaterial(xml.getRoot());
			m_materials.insert(name, m);
			return m;
		} else m_materials.insert(name, 0);
	} else return it->value;
	return 0;
}

Texture Library::texture(const char* name, bool mip) {
	static const Texture::Format formats[] = {
		Texture::NONE, Texture::R8, Texture::RG8, Texture::RGB8, Texture::RGBA8,
		Texture::BC1, Texture::BC2, Texture::BC3, Texture::BC4, Texture::BC5
	};

	TextureMap::iterator it = m_textures.find(name);
	if(it==m_textures.end()) {
		// Load material
		char path[1024];
		if(findFile(name, path)) {
			// Examine extension
			const char* ext = strrchr(name, '.');
			if(strcmp(ext, ".dds")==0) {
				base::DDS dds = base::DDS::load(path);
				if(dds.format) {
					// Create texture from DDS data
					printf("Load %s mip=%d\n", name, dds.mipmaps);
					void** data = (void**)dds.data;
					base::Texture tex = base::Texture::create(Texture::TEX2D, dds.width, dds.height, 1, formats[dds.format], data, dds.mipmaps+1);
					m_textures.insert(name, tex);
					return tex;
				}
			} else if(strcmp(ext, ".png")==0) {
				base::PNG png = base::PNG::load(path);
				if(!png.data) printf("Invalid image %s\n", name);
				else {
					printf("Load %s mip=%d\n", name, mip);
					base::Texture tex = base::Texture::create(png.width, png.height, formats[png.bpp/8], png.data, mip);
					if(mip) tex.setFilter(base::Texture::TRILINEAR);
					m_textures.insert(name, tex);
					return tex;
				}
			}
		}
		// Save blank texture to avoid trying to re-load
		m_textures.insert(name, Texture());
	} else return it->value;
	return Texture();
}

base::model::Model* Library::model(const char* name) {
	ModelMap::iterator it = m_models.find(name);
	if(it==m_models.end()) {
		char path[1024];
		if(findFile(name, path)) {
			base::model::Model* m = base::model::XModel::load(path);
			if(m) m_models.insert(name, m);
			return m;
		} else m_models.insert(name, 0);
	} else return it->value;
	return 0;
}

// Utilities
unsigned Library::hex(const char* s) {
	unsigned hex = 0;
	for(const char* c=s; *c; ++c) {
		hex = hex<<4;
		if(*c>='0' && *c<='9') hex += *c-'0';
		else if(*c>='a' && *c<='f') hex += *c-'a'+10;
		else if(*c>='A' && *c<='F') hex += *c-'A'+10;
		else return 0; // Parse error
	}
	return hex;
}
int Library::intArray(const char* s, int* array, int max) {
	int count;
	const char* c = s;
	for(count=0; count<max; ++count) {
		while(*c==' ' || *c=='\t' || *c=='\n') ++c; //whitespace
		if(*c==0) break; // End of string
		array[count] = atoi(c);
		while((*c>='0' && *c<='9') || *c=='-') ++c; //number
	}
	return count;
}
int Library::floatArray(const char* s, float* array, int max) {
	int count;
	const char* c = s;
	for(count=0; count<max; ++count) {
		while(*c==' ' || *c=='\t' || *c=='\n') ++c; //whitespace
		if(*c==0) break; // End of string
		array[count] = atof(c);
		while((*c>='0' && *c<='9') || *c=='-' || *c=='.' || *c=='e') ++c; //number
	}
	return count;
}

//// Load Material form XML file ////

Material* Library::loadMaterial(const base::XML::Element& xml) {
	if(strcmp(xml.name(), "material")!=0) {
		printf("Invalid material descriptor [%s]\n", xml.name());
		return 0;
	} else {
		Material* mat = new Material();
		for(base::XML::iterator i = xml.begin(); i!=xml.end(); ++i) {
			if(strcmp(i->name(), "texture")==0) {
				// Load texture TODO resource manager to load texture
				printf("Load texture %s [%s]\n", i->attribute("name"), i->attribute("file"));
				Texture tex = texture(i->attribute("file"), i->attribute("mip", false));
				if(tex.width()) mat->setTexture(i->attribute("name"), tex);

			} else if(strcmp(i->name(), "shader")==0) {
				// load shader - TODO resource manager to load shader
				printf("Load shader [%s]\n", i->attribute("file"));

			} else if(strcmp(i->name(), "variable")==0) {
				// Switch type
				const char* name = i->attribute("name");
				const char* t = i->attribute("type");
				if(strcmp(t, "int")==0) mat->setInt(name, i->attribute("value", 0));
				else if(strcmp(t, "float")==0) mat->setFloat(name, i->attribute("value", 0.0f));
				else if(strncmp(t, "vec", 3)==0 && t[4]==0) {
					int size = t[3]-'0';
					if(size<2 || size>4) printf("Invalid vector size\n");
					else {
						float v[4] = { 0,0,0,0 };
						if(!floatArray( i->attribute("value", ""), v, size)) {
							v[0] = i->attribute("x", v[0]);
							v[1] = i->attribute("y", v[1]);
							v[2] = i->attribute("z", v[2]);
							v[3] = i->attribute("w", v[3]);

							v[0] = i->attribute("r", v[0]);
							v[1] = i->attribute("b", v[1]);
							v[2] = i->attribute("b", v[2]);
							v[3] = i->attribute("a", v[3]);
						}
						mat->setFloatv(name, size, v);
					}
				}
				else if(strncmp(t, "mat", 3)==0) {
					printf("Warning: Matrix attributes not yet supported");
				}
				// ToDo: Automatic variables auto="name"
				// -> camera, time, direction, ...

			} else if(strcmp(i->name(), "shininess")==0) {
				mat->setShininess( i->attribute("value", 128.0f) );

			} else if(strcmp(i->name(), "diffuse")==0 ||  strcmp(i->name(), "specular")==0) {
				// Read colour from hex code
				Colour col;
				const char* cs = i->attribute("colour", "");
				if(!cs[0]) cs = i->attribute("color", "");
				if(cs[0]) {
					col = Colour( hex(cs) );
				} else {	// Colour as rgb
					col.r = i->attribute("r", 1.0f);
					col.g = i->attribute("g", 1.0f);
					col.b = i->attribute("b", 1.0f);
				}
				col.a = i->attribute("alpha", 1.0f);
				if(i->name()[0]=='d') mat->setDiffuse(col);
				else mat->setSpecular(col);
			}
		}
		return mat;
	}
}


void Library::createDefaultTexture(const char* name, int rgb, int a) {
	if(m_textures.find(name) != m_textures.end()) return; // Already created
	ubyte* data = new ubyte[8*8*4]; // 8x8 texture
	data[0] = (rgb>>16) & 0xff;
	data[1] = (rgb>>8) & 0xff;
	data[2] = rgb&0xff;
	data[3] = a&0xff;
	for(int i=1; i<64; ++i) {
		memcpy(data + i*4, data, 4);
	}
	m_textures[name] = Texture::create(8, 8, Texture::RGBA8, data);
}

