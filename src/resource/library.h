#ifndef _LIBRARY_
#define _LIBRARY_

#include <base/hashmap.h>
#include <base/shader.h>
#include <base/xml.h>
namespace base { class Font; class Material; class Texture; namespace model { class Model; } };

/** Library of all models, textures, shaders, materials in the game */
class Library {
	public:
	Library();
	~Library();

	base::Material*     material(const char* name);
	base::Texture       texture(const char* name, bool mip=false);
	base::model::Model* model(const char* name);
	base::Shader        shader(const char* name);

	/** Get font TODO multiple fonts */
	base::Font* font() { return m_font; }

	/** Add an file lookup path */
	void addPath(const char* path);
	void removePath(const char* path);
	bool findFile(const char* file, char* path);

	private:
	/** Find a file in lookup paths */
	std::vector<const char*> m_paths;

	// Content maps //
	typedef base::HashMap<base::Material*> MaterialMap;
	typedef base::HashMap<base::model::Model*> ModelMap;
	typedef base::HashMap<base::Shader> ShaderMap;
	typedef base::HashMap<base::Texture> TextureMap;

	MaterialMap m_materials;
	TextureMap m_textures;
	ShaderMap m_shaders;
	ModelMap m_models;
	base::Font* m_font;

	// Additional loading functions //
	base::Material* loadMaterial(const base::XML::Element& xml);

	void createDefaultTexture(const char* name, int rgb, int a=0xff);
	
	static unsigned hex(const char* s); // Hex string to int
	static int intArray(const char* s, int* array, int max);
	static int floatArray(const char* s, float* array, int max);
};

#endif

