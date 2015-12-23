#include "dynamicmaterial.h"
#include "streaming/texturestream.h"
#include <base/material.h>
#include <base/colour.h>

#include <cstdio>
#include <string>
#include <set>

using base::Shader;
using base::FragmentShader;
using base::VertexShader;
using base::Material;
using base::Texture;

DynamicMaterial::DynamicMaterial(bool stream) : m_material(0), m_stream(0), m_streaming(stream) {
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

// Warning: use carefuly as buffer is shared */
const char* addIndex(const char* base, int index) {
	static char buffer[32];
	sprintf(buffer, "%s%d", base, index);
	return buffer;
}

void DynamicMaterial::update(int index) {
	// Set all the variables
	MaterialLayer* layer = m_layers[index];
	m_material->setFloat( addIndex("opacity",index), layer->opacity);
	m_material->setFloatv( addIndex("scale",index), layer->triplanar? 3: 2, layer->scale);

	if(m_stream) {
		m_stream->copyParam( addIndex("opacity",index) );
		m_stream->copyParam( addIndex("scale",index) );
	}
	
	if(layer->type == LAYER_AUTO) {
		vec3 min(layer->height.min, layer->slope.min, layer->concavity.min);
		vec3 max(layer->height.max, layer->slope.max, layer->concavity.max);
		vec3 blend(layer->height.blend, layer->slope.blend, layer->concavity.blend);
		//vec3 noise(layer->height.noise, layer->slope.noise, layer->concavity.noise);
		m_material->setFloat3( addIndex("autoMin",index), min);
		m_material->setFloat3( addIndex("autoMax",index), max);
		m_material->setFloat3( addIndex("autoBlend",index), blend);
		//m_material->setFloat3( addIndex("autoNoise",index), noise);
		
		if(m_stream) {
			m_stream->copyParam( addIndex("autoMin",index) );
			m_stream->copyParam( addIndex("autoMax",index) );
			m_stream->copyParam( addIndex("autoBlend",index) );
		}
	}

	// Textures - needs access to EditableMaterial map
	if(layer->map) {
		
		
	}
}


// ------------------------------------------------------------------------------------------------------ //

base::Material* DynamicMaterial::getMaterial() const {
	return m_material;
}
MaterialStream* DynamicMaterial::getStream() const {
	return m_stream;
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
	"#version 130\n\n"
	"in vec3 worldNormal;\n"
	"in vec3 worldPos;\n\n"
	"uniform vec3 lightDirection;\n\n";
	
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
		if(m_layers[i]->triplanar)	source += "uniform vec3 scale" + index + ";\n";
		else                        source += "uniform vec2 scale" + index + ";\n";
		source += "uniform float opacity" + index + ";\n";
		if(m_layers[i]->type == LAYER_AUTO) source +=
			"uniform vec3 autoMin" + index + ";\n"
			"uniform vec3 autoMax" + index + ";\n"
			"uniform vec3 autoBlend" + index + ";\n";
	}

	source += "\n\n//Utility functions\n";

	// Functions
	source +=
	"vec4 sampleTriplanar(float map, vec3 coord, vec3 weights) {\n"
	"	vec4 c = vec4(coord.xyz, map);\n"
	"	return texture(diffuseArray, c.yzw)*weights.xxxx + texture(diffuseArray, c.zxw)*weights.yyyy + texture(diffuseArray, c.xyw)*weights.zzzz;\n"
	"}\n";

	source +=
	"vec4 sampleTriplanerNormal(float map, vec3 coord, vec3 normal, vec3 weights) {\n"
	"	vec4 c = vec4(coord.xyz, map);\n"
	"	vec4 nX = texture(normalArray, c.yzw);\n"
	"	vec4 nY = texture(normalArray, c.zxw);\n"
	"	vec4 nZ = texture(normalArray, c.xyw);\n"
	"	nX.xyz = nX.xyz * 2.0 - 1.0;\n"
	"	nY.xyz = nY.xyz * 2.0 - 1.0;\n"
	"	nZ.xyz = nZ.xyz * 2.0 - 1.0;\n"
	"	if(dot(nX.xyz, normal)<0.0) nX.x = -nX.x;\n"
	"	if(dot(nY.xyz, normal)<0.0) nY.y = -nY.y;\n"
	"	if(dot(nZ.xyz, normal)<0.0) nZ.z = -nZ.z;\n"
	"	vec3 result = normalize(nX.xyz*weights.xxx + nY.xyz*weights.yyy + nZ.xyz*weights.zzz);\n"
	"	return vec4(result, nX.w*weights.x + nY.w*weights.y + nZ.w*weights.z);\n"
	"}\n";

	source +=
	"float getAutoWeight(vec3 value, vec3 vmin, vec3 vmax, vec3 vblend) {\n"
	"	vec3 ctr = (vmin + vmax) * 0.5;\n"
	"	vec3 r = smoothstep(ctr - vmin + vblend, ctr - vmin, abs(value - ctr));\n"
	"	return dot(r, vec3(1,1,1));\n"
	"}\n";

	source +=
	"vec4 sampleMap(sampler2D map, vec4 info, vec2 coord) {\n"
	"	coord = (coord - info.xy) * info.zw;\n"
	"	return texture(map, coord);\n"
	"}\n";

	source +=
	"vec4 sampleDiffuse(float map, vec2 coord) {\n"
	"	return texture(diffuseArray, vec3(coord, map));\n"
	"}\n";

	source +=
	"vec4 sampleNormal(float map, vec2 coord) {\n"
	"	vec4 n = texture(diffuseArray, vec3(coord, map));\n"
	"	return vec4(n.xyz * 2.0 - 1.0, n.w);\n"
	"}\n\n\n";

	// Main function
	source +=
	"// Main shader function\n"
	"void main() {\n"
	"	vec4 diffuse = vec4(1,1,1,1);\n"
	"	vec4 normal = vec4(0,1,0,0);\n"
	"	float gloss = 0.0;\n"
	"	float height;\n"
	"	float weight;\n"
	"	vec4 diff, norm;\n"
	"	vec3 triplanar = max( (abs(worldNormal) - 0.2) * 0.7, 0.0);\n"
	"	triplanar /= dot(triplanar, vec3(1,1,1));\n"
	"	vec3 autoValue = vec3(worldPos.y, worldNormal.y, 0.0);\n"
	"	\n";

	// Sample maps
	if(!maps.empty()) {
		source +=  "	// Sample maps\n";
		for(MapList::iterator i=maps.begin(); i!=maps.end(); ++i) {
			source +=  "	vec4 " + *i + "Sample = sampleMap(" + *i + "Map, " + *i + "Info);\n";
		}
		source += "\n";
	}

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
			"	diff = sampleTriplanar("+str(layer->texture)+".0, worldPos * scale"+index+", triplanar);\n"
			"	norm = sampleTriplanerNormal("+str(layer->texture)+".0, worldPos * scale"+index+", worldNormal, triplanar);\n";
		else source += 
			"	diff = sampleDiffuse("+str(layer->texture)+".0, worldPos.xz * scale"+index+");\n"
			"	norm = sampleNormal("+str(layer->texture)+".0, worldPos.xz * scale"+index+");\n";


		// Blending
		switch(layer->blend) {
		case BLEND_NORMAL:
		case BLEND_HEIGHT:
			source += 
			"	diffuse = mix(diffuse, diff, weight * opacity" + index + ");\n"
			"	normal = mix(normal, norm, weight * opacity" + index + ");\n";
			break;
		case BLEND_MULTIPLY:
			source += 
			"	diffuse *= mix(vec4(1,1,1,1), diff, weight * opacity" + index + ");\n";
			break;
		case BLEND_ADD:
			source += 
			"	diffuse += diff * weight * opacity" + index + ";\n";
			break;
		}

		source += "\n";
	}

	// Lighting (basic diffuse);
	source +=  "	// Lighting\n";
	source +=  "	gl_FragColor = diffuse * 0.1 + diffuse * 0.9 * dot(worldNormal, lightDirection);\n";
	source += "}\n";

	// Vertex shader - todo: add concavity attribute
	static const char* vertexShader = 
	"varying vec3 worldNormal;\n"
	"varying vec3 worldPos;\n"
	"void main() {\n"
	"	gl_Position = ftransform();\n"
	"	worldPos = gl_Vertex.xyz;\n"
	"	worldNormal = gl_Normal;\n"
	"}\n";

	// Compile shader
	const char* fragmentShader = source.c_str();
	VertexShader vert = Shader::createVertex(&vertexShader);
	FragmentShader frag = Shader::createFragment(&fragmentShader);
	Shader shader = Shader::link(vert, frag);


	// Setup material
	if(!m_material) m_material = new Material();
	m_material->setShader(shader);

	// Streamed material
	if(m_streaming) {
		if(!m_stream) m_stream = new MaterialStream(m_material);
		m_stream->updateShader();
	}

	// Initialise variables
	for(size_t i=0; i<m_layers.size(); ++i) {
		update(i);
	}

	
	// DEBUG: save generated shader
	FILE* fp = fopen("shader.glsl", "w");
	if(fp) fwrite(source.c_str(), 1, source.length(), fp);
	if(fp) fclose(fp);


	return false;
}




