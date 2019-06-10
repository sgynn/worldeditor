#ifndef _DYNAMIC_MATERIAL_
#define _DYNAMIC_MATERIAL_

#include <base/math.h>
#include <vector>
#include "gui/gui.h"

namespace scene { class Material; class ShaderProgram; class ShaderVars; }
using gui::String;
class MaterialStream;
class MaterialEditor;


enum LayerType { LAYER_AUTO, LAYER_WEIGHT, LAYER_COLOUR, LAYER_INDEXED, LAYER_GRADIENT };
enum BlendMode { BLEND_NORMAL, BLEND_HEIGHT, BLEND_MULTIPLY, BLEND_ADD };
enum TexProjection { PROJECTION_FLAT, PROJECTION_TRIPLANAR, PROJECTION_VERTICAL };

struct AutoParams { float min, max, blend, noise; };

struct MaterialLayer {
	String name;		// Layer name
	LayerType type;		// Layer type
	BlendMode blend;	// Layer blend mode
	bool  visible;		// Allow layers to be turned off - uses opacity
	float blendScale;	// Used for height blend
	float opacity;		// Multiplier

	int  texture;		// Texture unit to use. set -1 to use colour
	int  colour;		// Colour to use instead of a texture
	vec3 scale;			// Texture scaling. Use z for triplanar maps
	TexProjection projection;	// Texture projection mode

	// Auto parameters
	AutoParams slope;
	AutoParams height;
	AutoParams concavity;

	// Maps
	String map;
	String map2;
	int mapData;	// IndexOffset or Channel
};

class DynamicMaterial {
	public:
	DynamicMaterial(bool stream=false);
	~DynamicMaterial();

	void setName(const char*);
	const char* getName() const;
	void setCoordinates(const vec2& size, const vec2& offset) const;
	void setTilingData(float*);

	size_t         size() const;
	MaterialLayer* addLayer(LayerType);
	MaterialLayer* getLayer(int index) const;
	void           removeLayer(int index);
	void           moveLayer(int from, int to);

	bool            compile();				// Generate the shader
	void			setTextures(MaterialEditor* src);	// Bind textures to material
	void            update(int layer);		// Update constant buffers (need shared params)
	bool			needsCompiling() const;	// do we need to recompile
	void			flagRecompile();		// Flag material to be compiled when activated

	scene::Material* getMaterial() const;	// Get material pointer
	MaterialStream* getStream() const;		// Get material stream

	protected:
	String m_name;
	std::vector<MaterialLayer*> m_layers;
	scene::Material*   m_material;
	scene::ShaderVars* m_vars;
	scene::ShaderProgram* m_vertexShader;
	scene::ShaderProgram* m_fragmentShader;
	MaterialStream* m_stream;
	bool            m_streaming;
	bool            m_needsCompile;
	mutable float   m_coords[4];
	float           m_textureTiling[256];
};



#endif

