#ifndef _DYNAMIC_MATERIAL_
#define _DYNAMIC_MATERIAL_

#include <base/math.h>
#include <vector>
#include "gui/gui.h"

namespace base { class Material; }
using gui::String;



enum LayerType { LAYER_AUTO, LAYER_WEIGHT, LAYER_COLOUR, LAYER_INDEXED };
enum BlendMode { BLEND_NORMAL, BLEND_HEIGHT, BLEND_MULTIPLY, BLEND_ADD };

struct AutoParams { float min, max, blend, noise; };

struct MaterialLayer {
	String name;		// Layer name
	LayerType type;		// Layer type
	BlendMode blend;	// Layer blend mode
	float blendScale;	// Used for height blend
	float opacity;		// Multiplier

	int  texture;		// Texture unit to use. set -1 to use colour
	int  colour;		// COlour to use instead of a texture
	vec3 scale;			// Texture scaling. Use z for triplanar maps
	bool triplanar;		// Triplanar projection

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
	DynamicMaterial();
	~DynamicMaterial();

	void setName(const char*);
	const char* getName() const;

	MaterialLayer* addLayer(LayerType);
	MaterialLayer* getLayer(int index) const;
	size_t size() const;

	bool            compile();				// Generate the shader
	void            update(int layer);		// Update constant buffers (need shared params)
	base::Material* getMaterial() const;	// Get material pointer

	protected:
	String m_name;
	std::vector<MaterialLayer*> m_layers;
	base::Material* m_material;
};



#endif

