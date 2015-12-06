#include "dynamicmaterial.h"
#include <base/material.h>
#include <base/colour.h>

#include <cstdio>
#include <string>
#include <set>

DynamicMaterial::DynamicMaterial() : m_material(0) {
}

DynamicMaterial::~DynamicMaterial() {
	for(size_t i=0; i<m_layers.size(); ++i) delete m_layers[i];
	if(m_material) delete m_material;
}


void DynamicMaterial::setName(const char* name) {
	m_name = name;
}

const char* DynamicMaterial::getName() const {
	return m_name;
}

// ------------------------------------------------------------------------------------------------------ //

MaterialLayer* DynamicMaterial::addLayer(LayerType type) {
	MaterialLayer* layer = new MaterialLayer();
	layer->type = type;
	layer->blend = BLEND_NORMAL;
	layer->blendScale = 1.0;
	layer->opacity = 1.0;
	layer->texture = 0;
	layer->colour = 0xffffff;
	layer->scale = vec3(1,1,1);
	layer->mapData = 0;

	layer->triplanar = false;
	layer->slope.min = 0;
	layer->slope.max = 1;
	layer->slope.blend = 0;
	layer->height.min = 0;
	layer->height.max = 1000;
	layer->height.blend = 10;
	layer->concavity.min = -1;
	layer->concavity.max = 1;
	layer->concavity.blend = 0;

	m_layers.push_back(layer);
	return layer;
}

MaterialLayer* DynamicMaterial::getLayer(int index) const {
	return m_layers[index];
}
size_t DynamicMaterial::size() const {
	return m_layers.size();
}


// ------------------------------------------------------------------------------------------------------ //

base::Material* DynamicMaterial::getMaterial() const {
	return m_material;
}

#define MakeString(format, value) static char b[32]; sprintf(b,format,value); return b;
std::string str(int v) { MakeString("%d", v); }
std::string str(size_t v) { MakeString("%lu", v); }
std::string str(float v) { MakeString("%g", v); }
std::string str(const char* s) { return s; }
std::string str(const String& s) { return (const char*)s; }

bool DynamicMaterial::compile() {
	std::string source;

	// Inputs
	source +=
	"varying vec3 worldNormal;\n"
	"varying vec3 worldPos;\n\n"
	"uniform vec3 lightDirection\n\n";
	
	// Samplers
	source +=
	"uniform sampler2DArray diffuseArray;\n"
	"uniform sampler2DArray normalArray;\n";

	// Maps
	typedef std::set<std::string> MapList;
	MapList maps;
	for(size_t i=0; i<m_layers.size(); ++i) {
		if(m_layers[i]->map  && m_layers[i]->map[0])  maps.insert( str(m_layers[i]->map) );
		if(m_layers[i]->map2 && m_layers[i]->map2[0]) maps.insert( str(m_layers[i]->map2) );
	}
	for(MapList::iterator i=maps.begin(); i!=maps.end(); ++i) source += "uniform sampler2D " + *i + "Map;\n";
	for(MapList::iterator i=maps.begin(); i!=maps.end(); ++i) source += "uniform vec4 " + *i + "Info;\n";
	source += "\n\n";

	// Variables
	for(size_t i=0; i<m_layers.size(); ++i) {
		std::string index = str(i);
		source += "uniform vec3 scale" + index + "\n";
		source += "uniform float opacity" + index + "\n";
		if(m_layers[i]->type == LAYER_AUTO) source +=
			"uniform vec4 autoMin" + index + ";\n"
			"uniform vec4 autoMax" + index + ";\n"
			"uniform vec4 autoBlend" + index + ";\n";
	}

	source += "\n\n";

	// Functions
	source +=
	"vec4 sampleTriplanar(float map, vec3 coord, vec3 weights) {\n"
	"	vec4 c = vec4(coord.xyz, map);\n"
	"	return texture3D(diffuseArray, c.yzw)*weights.xxxx + texture3D(diffuseArray, c.zxw)*weights.yyyy + texture3D(diffuseArray, c.xyw)*weights.zzzz;\n"
	"}\n";

	source +=
	"vec4 sampleTriplanerNormal(float map, vec3 coord, vec3 normal, vec3 weights) {\n"
	"	vec4 c = vec4(coord.xyz, map);\n"
	"	vec4 nX = texture3D(normalArray, c.yzw);\n"
	"	vec4 nY = texture3D(normalArray, c.zxw);\n"
	"	vec4 nZ = texture3D(normalArray, c.xyw);\n"
	"	nX.xyz = nX.xyz * 2.0 - 1.0;\n"
	"	nY.xyz = nY.xyz * 2.0 - 1.0;\n"
	"	nZ.xyz = nZ.xyz * 2.0 - 1.0;\n"
	"	if(dot(nX, normal)<0.0) nX.x = -nX.x;\n"
	"	if(dot(nY, normal)<0.0) nY.y = -nY.y;\n"
	"	if(dot(nZ, normal)<0.0) nZ.z = -nZ.z;\n"
	"	vec3 normal = normalize(nX.xyz*weights.xxx + nY.xyz*weights.yyy + nZ.xyz*weights.zzz);\n"
	"	return vec4(normal, nX.w*weights.x + nY.w*weights.y + nZ.w*weights.z);\n"
	"}\n";

	source +=
	"float getAutoWeight(vec3 value, vec3 vmin, vec3 vmax, vec3 vblend) {\n"
	"	vec3 ctr = (vmin + vmax) * 0.5;\n"
	"	vec3 r = smoothStep(ctr - min + blend, ctr - min, abs(value - ctr));\n"
	"	return dot(r, vec3(1,1,1));\n"
	"}\n";

	source +=
	"vec4 sampleMap(sampler2D map, vec4 info, vec2 coord) {\n"
	"	coord = (coord - info.xy) * info.zw;\n"
	"	return texture2d(map, coord);\n"
	"}\n";

	source +=
	"vec4 sampleDiffuse(float map, vec2 coord) {\n"
	"	return texture3D(diffuseArray, vec3(coord, map));\n"
	"}\n";

	source +=
	"vec4 sampleNormal(float map, vec2 coord) {\n"
	"	vec4 n texture3D(diffuseArray, vec3(coord, map));\n"
	"	return vec4(n.xyz * 2.0 - 1.0, n.w);\n"
	"}\n\n\n";

	// Main function
	source +=
	"void main() {\n"
	"	vec3 diffuse = vec3(1,1,1);\n"
	"	vec3 normal = vec3(0,1,0);\n"
	"	float gloss = 0;\n"
	"	float height;\n"
	"	float weight;\n"
	"	vec4 diff, norm;\n"
	"	vec3 triplanar = max( (abs(worldNormal) - 0.2) * 0.7, 0.0);\n"
	"	triplanar /= dot(triplanar, vec3(1,1,1));\n"
	"	\n";

	// Sample maps
	for(MapList::iterator i=maps.begin(); i!=maps.end(); ++i) {
		source +=  "	vec4 " + *i + "Sample = sampleMap(" + *i + "Map, " + *i + "Info);\n";
	}
	source += "\n\n";

	// Apply layers
	const std::string rgba[] = { "r", "g", "b", "a" };
	for(size_t i=0; i<m_layers.size(); ++i) {
		std::string index = str(i);
		MaterialLayer* layer = m_layers[i];
		Colour colour(layer->colour);

		source +=  "	// Layer " + str(layer->name) + "\n";

		switch(layer->type) {
		case LAYER_AUTO:
			source +=  "	weight = getAutoWeight(autoValue, autoMin"+index+", autoMax"+index+", autoBlend"+index+");\n";
			break;

		case LAYER_WEIGHT:
			if(layer->mapData<4)
				source +=  "	weight = " + str(layer->map) + "Sample." + rgba[layer->mapData] + ";\n";
			else
				source +=  "	weight = 1.0 - dot(" + str(layer->map) + "Sample, vec4(1,1,1,1));\n";
			break;

		case LAYER_COLOUR:
			source +=  "	diff = " + str(layer->map) + "Sample;\n";
			break;
		case LAYER_INDEXED:
			// ToDo this one
			break;
		}

		// Sample textures
		if(layer->texture < 0) source +=
			"	diff = vec4(" + str(colour.r) + ", " + str(colour.g) + ", " + str(colour.b) + ", 0);\n"
			"	norm = vec4(0,1 0,0);\n";
		else if(layer->triplanar) source +=
			"	diff = sampleTriplanar("+str(layer->texture)+", worldPos * scale"+index+", triplanar);\n"
			"	norm = sampleTriplanerNormal("+str(layer->texture)+", worldPos * scale"+index+", worldNormal, triplanar);\n";
		else source += 
			"	diff = sampleDiffuse("+str(layer->texture)+", worldPos * scale"+index+");\n"
			"	norm = sampleNormal("+str(layer->texture)+", worldPos * scale"+index+");\n";


		// Blending
		switch(layer->blend) {
		case BLEND_NORMAL:
		case BLEND_HEIGHT:
			source += 
			"	diffuse = mix(diffuse, diff, weight * opacity" + index + ")\n"
			"	normal = mix(normal, norm, weight * opacity" + index + ")\n";
			break;
		case BLEND_MULTIPLY:
			source += 
			"	diffuse *= mix(vec4(1,1,1,1), diff, weight * opacity" + index + ")\n";
			break;
		case BLEND_ADD:
			source += 
			"	diffuse += diff * weight * opacity" + index + "\n";
			break;
			
			
			
		}


		source += "\n";
	}

	// Lighting (basic diffuse);
	source +=  "	gl_FragColor = diffuse * 0.1 + diffuse * 0.9 * dot(worldNormal, lightDirection);\n";
	source += "}\n";

	// Compile it ???
	
	FILE* fp = fopen("shader.glsl", "w");
	if(fp) fwrite(source.c_str(), 1, source.length(), fp);
	if(fp) fclose(fp);

	return false;
}




