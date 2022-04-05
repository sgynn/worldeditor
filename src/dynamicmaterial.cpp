#include "dynamicmaterial.h"
#include "materialeditor.h"
#include "streaming/texturestream.h"
#include "terraineditor/editabletexture.h"
#include <base/material.h>
#include <base/shader.h>
#include <base/autovariables.h>
#include <base/colour.h>

#include <base/opengl.h>

#include <cstdio>
#include <string>
#include <set>

using base::Texture;
using base::ShaderPart;
using base::Material;
using base::Shader;
using base::Pass;

DynamicMaterial::DynamicMaterial(bool stream) : m_material(0), m_stream(0), m_streaming(stream), m_needsCompile(true) {
	Shader* shader = new Shader();
	m_vertexShader = new ShaderPart(base::VERTEX_SHADER);
	m_fragmentShader = new ShaderPart(base::FRAGMENT_SHADER);
	shader->attach(m_vertexShader);
	shader->attach(m_fragmentShader);
	m_vars = new base::ShaderVars();
	m_material = new Material();
	m_material->addPass()->setShader(shader);
	m_material->getPass(0)->addShared(m_vars);
}

DynamicMaterial::~DynamicMaterial() {
	for(size_t i=0; i<m_layers.size(); ++i) delete m_layers[i];
	delete m_material->getPass(0)->getShader();
	delete m_vars;
	delete m_material;
	delete m_vertexShader;
	delete m_fragmentShader;
}


void DynamicMaterial::setName(const char* name) {
	m_name = name;
}

const char* DynamicMaterial::getName() const {
	return m_name;
}

void DynamicMaterial::setMode(MaterialMode mode) {
	m_mode = mode;
	m_needsCompile = true;
}

void DynamicMaterial::setTilingData(float* data) {
	m_vars->set("textureTiling", 1, 32, data);
}

void DynamicMaterial::setCoordinates(const vec2& s, const vec2& o) const {
	m_coords[0] = o.x;
	m_coords[1] = o.y;
	m_coords[2] = 1.f / s.x;
	m_coords[3] = 1.f / s.y;
}

// ------------------------------------------------------------------------------------------------------ //

MaterialLayer* DynamicMaterial::addLayer(LayerType type) {
	MaterialLayer* layer = new MaterialLayer();
	layer->type = type;
	layer->blend = BLEND_NORMAL;
	layer->visible = true;
	layer->blendScale = 1.0;
	layer->opacity = 1.0;
	layer->texture = 0;
	layer->colour = 0xffffff;
	layer->scale = vec3(100,100,100);
	layer->mapIndex = 0;
	layer->mapData = 0;

	layer->projection = PROJECTION_FLAT;
	layer->slope.min = 0;
	layer->slope.max = 1;
	layer->slope.blend = 0;
	layer->height.min = 0;
	layer->height.max = 1000;
	layer->height.blend = 0;
	layer->concavity.min = -1;
	layer->concavity.max = 1;
	layer->concavity.blend = 0;

	m_layers.push_back(layer);
	m_needsCompile = true;
	return layer;
}

MaterialLayer* DynamicMaterial::getLayer(int index) const {
	return m_layers[index];
}

void DynamicMaterial::removeLayer(int index) {
	if(index<(int)size()) {
		delete m_layers[index];
		m_layers.erase( m_layers.begin() + index );
		m_needsCompile = true;
	}
}

void DynamicMaterial::moveLayer(int from, int to) {
	if(from == to) return;
	MaterialLayer* layer = m_layers[from];
	int d = from < to? 1: -1;
	for(int i=from; i!=to; i+=d) {
		m_layers[i] = m_layers[i+d];
	}
	m_layers[to] = layer;
	m_needsCompile = true;
}

size_t DynamicMaterial::size() const {
	return m_layers.size();
}

// ------------------------------------------------------------------------------------------------------ //


#define MakeString(format, value) static char b[32]; sprintf(b,format,value); return b;
std::string str(int v) { MakeString("%d", v); }
std::string str(size_t v) { MakeString("%u", (unsigned)v); }
std::string str(float v) { MakeString("%g", v); }
std::string str(const char* s) { return s; }
std::string str(const String& s) { return (const char*)s; }


// Warning: use carefuly as buffer is shared */
const char* addIndex(const char* base, int index) {
	static char buffer[32];
	sprintf(buffer, "%s%d", base, index);
	return buffer;
}

void DynamicMaterial::update(int index) {
	if(!m_material) return;

	// Set all the variables
	MaterialLayer* layer = m_layers[index];
	if(layer->texture!=TEXTURE_CLIP) {
		m_vars->set( addIndex("opacity",index), layer->visible? layer->opacity: 0);
	}

	// Splat texture scaling
	if(layer->type != LAYER_COLOUR && layer->texture>=0) {
		if(layer->projection == PROJECTION_FLAT) m_vars->set( addIndex("scale",index), 1.0 / layer->scale.xy());
		else m_vars->set( addIndex("scale",index), 1.0 / layer->scale);
	}

	// Auto parameters
	if(layer->type == LAYER_AUTO) {
		float minHeight = layer->height.min==0? -9999: layer->height.min; // height min special case
		vec3 min(minHeight, layer->slope.min, layer->concavity.min);
		vec3 max(layer->height.max, layer->slope.max, layer->concavity.max);
		vec3 blend(layer->height.blend, layer->slope.blend, layer->concavity.blend);
		for(int i=0; i<3; ++i) if(blend[i]<=0) blend[i] = 0.001; // avoid div0

		//vec3 noise(layer->height.noise, layer->slope.noise, layer->concavity.noise);
		m_vars->set( addIndex("autoMin",index), min);
		m_vars->set( addIndex("autoMax",index), max);
		m_vars->set( addIndex("autoBlend",index), blend);
		//m_vars->set( addIndex("autoNoise",index), noise);
	}
}

void DynamicMaterial::setTextures(MaterialEditor* src) {
	if(!m_material) return;
	if(m_stream) m_stream->build();

	// Map bounds
	m_vars->set("mapBounds", 4, 1, m_coords);


	// Textures
	m_material->getPass(0)->setTexture("diffuseArray", &src->getDiffuseArray());
	m_material->getPass(0)->setTexture("normalArray", &src->getNormalArray());
	if(m_stream) {
		m_stream->setTexture("diffuseArray", src->getDiffuseArray());
		m_stream->setTexture("normalArray", src->getNormalArray());
		printf("Textures set\n");
	}

	// Bind all the map data
	char buffer[32];
	uint64 used = 1;
	std::set<uint> usedMaps;
	for(size_t i=0; i<m_layers.size(); ++i) {
		MaterialLayer* layer = m_layers[i];
		if(~used&(1<<layer->mapIndex)) {
			used |= 1<<layer->mapIndex;
			const MaterialEditor::MapData& map = src->getMap(layer->mapIndex);
			sprintf(buffer, "map%dSize", layer->mapIndex);
			m_vars->set(buffer, (float)map.size, (float)map.size);
		}
		update(i);
	}
	m_material->getPass(0)->compile();
	GL_CHECK_ERROR;
}

// ------------------------------------------------------------------------------------------------------ //

int str(char* out, int v) { return sprintf(out, "%d", v); }
int str(char* out, uint v) { return sprintf(out, "%u", v); }
int str(char* out, size_t v) { return sprintf(out, "%lu", v); }
int str(char* out, float v) { return sprintf(out, "%g", v); }
int str(char* out, const char* v) { return sprintf(out, "%s", v); }

template<typename T>
int formatarg(char* out, int index, T& v) {
	if(index==0) return str(out, v);
	return 0;
}
template<typename T, typename... R>
int formatarg(char* out, int index, T& v, R&... args) {
	if(index==0) return str(out, v);
	else return formatarg(out, index-1, args...);
}
template<typename... T>
const char* format(char* out, const char* s, T&... args) {
	char* start = out;
	for(const char* c=s; *c; ++c) {
		if(c[0]=='{' && c[1]>='0' && c[1]<='9' && c[2]=='}') {
			out += formatarg(out, c[1]-'1', args...);
			c+=2;
		}
		else if(c[0]=='{' && c[1]=='}') {
			out += formatarg(out, 0, args...);
			c+=1;
		}
		else *out=*c, ++out;
	}
	*out = 0;
	return start;
}
// --------------------------------------------------------------- //


Material* DynamicMaterial::getMaterial() const {
	return m_material;
}
MaterialStream* DynamicMaterial::getStream() const {
	return m_stream;
}

bool DynamicMaterial::needsCompiling() const {
	return m_needsCompile;
}

void DynamicMaterial::flagRecompile() {
	m_needsCompile = true;
}

bool DynamicMaterial::compile() {
	std::string source;
	char buf[1024];
	#define format(...) format(buf, __VA_ARGS__)

	// Properties
	bool hasTextureArray = false;
	bool hasIndexed = false;
	bool hasMaps = false;
	bool hasTriplanar = false;
	for(MaterialLayer* layer: m_layers) {
		if(layer->type==LAYER_WEIGHT || layer->type == LAYER_AUTO) hasTextureArray=true;
		if(layer->type==LAYER_WEIGHT || layer->type == LAYER_COLOUR) hasMaps = true; 
		if(layer->type == LAYER_INDEXED) hasIndexed = hasTextureArray = hasMaps = true;
		if(layer->projection != PROJECTION_FLAT) hasTriplanar = true;
	}


	// Inputs
	source +=
	"#version 130\n\n"
	"in vec3 worldNormal;\n"
	"in vec3 worldPos;\n"
	"out vec4 fragment;\n\n"
	"uniform vec3 lightDirection;\n"
	"uniform vec4 mapBounds;\n\n";
	
	// Texture array samplers
	if(hasTextureArray) source +=
	"uniform sampler2DArray diffuseArray;\n"
	"uniform sampler2DArray normalArray;\n"
	"uniform float textureTiling[128];\n\n";

	// Image map samplers
	uint64 mapMask = 0;
	uint64 doubleMapMask = 0;
	for(MaterialLayer* layer: m_layers) {
		if(~mapMask & 1<<layer->mapIndex) {
			mapMask |= 1<<layer->mapIndex;
			source += format("uniform sampler2D map{1};\n", layer->mapIndex);
			if(layer->mapData&0x100) {
				doubleMapMask |= 1<<layer->mapIndex;
				source += format("uniform sampler2D map{1}W;\n", layer->mapIndex);
			}
		}
	}
	for(int i=0; mapMask>>i; ++i) {
		if(mapMask&(1<<i)) source += format("uniform vec2 map{}Size;\n", i);
	}
	source += "\n\n";

	// Variables
	for(size_t i=0; i<m_layers.size(); ++i) {
		std::string index = str(i);
		if(m_layers[i]->texture>=0) {
			if(m_layers[i]->projection == PROJECTION_FLAT) source += format("uniform vec2 scale{};\n", i);
			else source += format("uniform vec3 scale{};\n", i);
		}
		if(m_layers[i]->texture!=TEXTURE_CLIP) {
			source += format("uniform float opacity{};\n", i);
		}
		if(m_layers[i]->type == LAYER_AUTO) source +=
			"uniform vec3 autoMin" + index + ";\n"
			"uniform vec3 autoMax" + index + ";\n"
			"uniform vec3 autoBlend" + index + ";\n";
	}

	source += "\n\n//Utility functions\n";

	// Functions
	if(hasTextureArray && hasTriplanar) source +=
	"vec4 sampleTriplanar(float map, vec3 coord, vec3 weights) {\n"
	"	vec4 c = vec4(coord.xyz, map);\n"
	"	return texture(diffuseArray, c.yzw)*weights.xxxx + texture(diffuseArray, c.zxw)*weights.yyyy + texture(diffuseArray, c.xyw)*weights.zzzz;\n"
	"}\n";

	if(hasTextureArray && hasTriplanar) source +=
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
//	"	vec3 ctr = (vmin + vmax) * 0.5;\n"
//	"	vec3 r = smoothstep(ctr - vmin, ctr - vmin + vblend, abs(value - ctr));\n"
	"	vec3 r = smoothstep(vmin-vblend, vmin, value) * smoothstep(vmax+vblend, vmax, value);\n"
	"	return r.x * r.y * r.z;\n"
	"}\n";

	if(hasMaps) source +=
	"vec4 sampleMap(sampler2D map, vec4 info, vec2 coord) {\n"
	"	coord = (coord - info.xy) * info.zw;\n"
	"	return texture(map, coord);\n"
	"}\n";

	if(hasTextureArray) source +=
	"vec4 sampleDiffuse(float map, vec2 coord) {\n"
	"	return texture(diffuseArray, vec3(coord, map));\n"
	"}\n";

	if(hasTextureArray) source +=
	"vec4 sampleNormal(float map, vec2 coord) {\n"
	"	vec4 n = texture(normalArray, vec3(coord, map));\n"
	"	return vec4(n.xyz * 2.0 - 1.0, n.w);\n"
	"}\n";
	
	// Barycentric coordinates for index maps
	if(hasIndexed) source +=
	"vec3 barycentric(vec4 info, vec2 size, vec2 coord, out vec2 a, out vec2 b, out vec2 c) {\n"
	"	// Get sample points\n"
	"	vec2 one = vec2(1,0);\n"
	"	vec2 step = 1.0 / size;\n"
	"	vec2 base = (coord - info.xz) * info.zw;\n"
	"	a = floor(base * size) * step;\n"
	"	b = step * one.xy + base;\n"
	"	c = step * one.yx + base;\n"
	"	// Barycentric coordinates - Check if this condition compiles without branching\n"
	"	vec3 bary;\n"
	"	vec2 local = (base - a) * size; // 0-1\n"
	"	if(local.x+local.y < 1.0) bary.yz = local;\n"
	"	else { bary.yz = 1.0 - local.yx; a += step; }\n"
	"	bary.x = 1.0 - bary.y - bary.z;\n"
	"	return bary;\n"
	"}\n";

	// Basic indexed texture sampling
	if(hasIndexed) source +=
	"void sampleIndexed(sampler2D map, vec4 info, vec2 size, vec2 coord, vec2 tile,  out vec4 diff, out vec4 norm) {\n"
	"	vec2 a,b,c;\n"
	"	vec3 bary = barycentric(info, size, coord, a,b,c);\n"
	"	// Sample index map\n"
	"	float ia = texture(map, a).r * 255;\n"
	"	float ib = texture(map, b).r * 255;\n"
	"	float ic = texture(map, c).r * 255;\n"
	"	// Sample textures\n"
	"	coord *= tile;\n"
	"	diff  = texture(diffuseArray, vec3(coord,ia)) * bary.x;\n"
	"	diff += texture(diffuseArray, vec3(coord,ib)) * bary.y;\n"
	"	diff += texture(diffuseArray, vec3(coord,ic)) * bary.z;\n"
	"	norm  = texture(normalArray, vec3(coord,ia)) * bary.x;\n"
	"	norm += texture(normalArray, vec3(coord,ib)) * bary.y;\n"
	"	norm += texture(normalArray, vec3(coord,ic)) * bary.z;\n"
	"	norm = vec4(norm.xyz * 2.0 - 1.0, norm.w);\n"
	"}\n";

	// Alternative indexed sampling
	if(hasIndexed) source +=
	"void sampleIndexedQuad(sampler2D map, vec4 info, vec2 size, vec2 coord, vec2 tile,  out vec4 diff, out vec4 norm) {\n"
	"	vec2 one = vec2(1,0);\n"
	"	vec2 step = 1.0 / size;\n"
	"	vec2 base = (coord - info.xz) * info.zw;\n"
	"	vec2 a = floor(base * size) * step;\n"
	"	vec2 b = step * one.xy + base;\n"
	"	vec2 c = step * one.yx + base;\n"
	"	vec2 d = step * one.xx + base;\n"
	"	// Calculate weights\n"
	"	vec2 local = (base - a) * size;\n"
	"	vec4 w = vec4(1-local.yy, local.yy);\n"
	"	w.xz *= 1-local.xx;\n"
	"	w.yw *= local.xx;\n"
	"	// Sample index map\n"
	"	float ia = texture(map, a).r * 255;\n"
	"	float ib = texture(map, b).r * 255;\n"
	"	float ic = texture(map, c).r * 255;\n"
	"	float id = texture(map, d).r * 255;\n"
	"	// Sample textures\n"
	"	coord *= tile;\n"
	"	diff  = texture(diffuseArray, vec3(coord,ia)) * w.x;\n"
	"	diff += texture(diffuseArray, vec3(coord,ib)) * w.y;\n"
	"	diff += texture(diffuseArray, vec3(coord,ic)) * w.z;\n"
	"	diff += texture(diffuseArray, vec3(coord,id)) * w.w;\n"
	"	norm  = texture(normalArray, vec3(coord,ia)) * w.x;\n"
	"	norm += texture(normalArray, vec3(coord,ib)) * w.y;\n"
	"	norm += texture(normalArray, vec3(coord,ic)) * w.z;\n"
	"	norm += texture(normalArray, vec3(coord,id)) * w.w;\n"
	"	norm = vec4(norm.xyz * 2.0 - 1.0, norm.w);\n"
	"}\n";
	

	// Weighted indexed sampling - so many texture lookups...
	if(hasIndexed) source +=
	"void sampleIndexedWeighted(sampler2D map, sampler2D weights, vec4 info, vec2 size, vec2 coord, vec2 tile,  out vec4 diff, out vec4 norm) {\n"
	"	vec2 one = vec2(1,0);\n"
	"	vec2 step = 1.0 / size;\n"
	"	vec2 base = (coord - info.xz) * info.zw;\n"
	"	vec2 a = floor(base * size) * step;\n"
	"	vec2 b = step * one.xy + base;\n"
	"	vec2 c = step * one.yx + base;\n"
	"	vec2 d = step * one.xx + base;\n"
	"	// Calculate weights\n"
	"	vec2 local = (base - a) * size;\n"
	"	vec4 w = vec4(1-local.yy, local.yy);\n"
	"	w.xz *= 1-local.xx;\n"
	"	w.yw *= local.xx;\n"
	"	// Sample index map\n"
	"	vec4 ia = texture(map, a) * 255;\n"
	"	vec4 ib = texture(map, b) * 255;\n"
	"	vec4 ic = texture(map, c) * 255;\n"
	"	vec4 id = texture(map, d) * 255;\n"
	"	// Sample weights\n"
	"	vec4 wa = texture(weights, a);\n"
	"	vec4 wb = texture(weights, b);\n"
	"	vec4 wc = texture(weights, c);\n"
	"	vec4 wd = texture(weights, d);\n"
	"	// Sample textures\n"
	"	coord *= tile;\n"
	"	diff  = texture(diffuseArray, vec3(coord,ia.x)) * w.x * wa.x;\n"
	"	diff += texture(diffuseArray, vec3(coord,ib.x)) * w.y * wb.x;\n"
	"	diff += texture(diffuseArray, vec3(coord,ic.x)) * w.z * wc.x;\n"
	"	diff += texture(diffuseArray, vec3(coord,id.x)) * w.w * wd.x;\n"
	"	diff += texture(diffuseArray, vec3(coord,ia.y)) * w.x * wa.y;\n"
	"	diff += texture(diffuseArray, vec3(coord,ib.y)) * w.y * wb.y;\n"
	"	diff += texture(diffuseArray, vec3(coord,ic.y)) * w.z * wc.y;\n"
	"	diff += texture(diffuseArray, vec3(coord,id.y)) * w.w * wd.y;\n"
	"	diff += texture(diffuseArray, vec3(coord,ia.z)) * w.x * wa.z;\n"
	"	diff += texture(diffuseArray, vec3(coord,ib.z)) * w.y * wb.z;\n"
	"	diff += texture(diffuseArray, vec3(coord,ic.z)) * w.z * wc.z;\n"
	"	diff += texture(diffuseArray, vec3(coord,id.z)) * w.w * wd.z;\n"
	"	diff += texture(diffuseArray, vec3(coord,ia.w)) * w.x * wa.w;\n"
	"	diff += texture(diffuseArray, vec3(coord,ib.w)) * w.y * wb.w;\n"
	"	diff += texture(diffuseArray, vec3(coord,ic.w)) * w.z * wc.w;\n"
	"	diff += texture(diffuseArray, vec3(coord,id.w)) * w.w * wd.w;\n"
	"	// Normals\n"
	"	norm  = texture(normalArray, vec3(coord,ia.x)) * w.x * wa.x;\n"
	"	norm += texture(normalArray, vec3(coord,ib.x)) * w.y * wb.x;\n"
	"	norm += texture(normalArray, vec3(coord,ic.x)) * w.z * wc.x;\n"
	"	norm += texture(normalArray, vec3(coord,id.x)) * w.w * wd.x;\n"
	"	norm = vec4(norm.xyz * 2.0 - 1.0, norm.w);\n"
	"}\n";

	// Main function
	source +=
	"\n\n\n// Main shader function\n"
	"void main() {\n"
	"	vec4 diffuse = vec4(1,1,1,1);\n"
	"	vec4 normal = vec4(0,0,1,0);\n"
	"	float gloss = 0.0;\n"
	"	float height;\n"
	"	float weight;\n"
	"	vec4 diff, norm;\n"
	"	vec3 triplanar = max( (abs(worldNormal) - 0.2) * 0.7, 0.0);\n"
	"	triplanar /= dot(triplanar, vec3(1,1,1));\n"
	"	vec3 autoValue = vec3(worldPos.y, 1.0 - worldNormal.y, 0.0);\n"
	"	\n";

	// Vertical projection data
	source +=
	"	vec3 vertical = vec3(triplanar.x, 0.0, triplanar.z);\n"
;//	"	vertical /= max(0.001, dot(vertical, vec3(1,1,1)));\n\n";
	


	// Sample maps
	std::string id;
	if(hasMaps) {
		source +=  "	// Sample maps\n";
		for(int i=0; mapMask>>i; ++i) {
			if(mapMask & 1<<i) {
				source +=  format("	vec4 map{}Sample = sampleMap(map{}, mapBounds, worldPos.xz);\n", i);
				if(doubleMapMask & 1<<i) {
					source +=  format("	vec4 map{}WSample = sampleMap(map{}W, mapBounds, worldPos.xz);\n", i);
				}
			}
		}
		source += "\n";
	}

	// Apply layers
	const char* rgba[] = { "r", "g", "b", "a" };
	for(size_t i=0; i<m_layers.size(); ++i) {
		std::string index = str(i);
		MaterialLayer* layer = m_layers[i];
		Colour colour(layer->colour);
		bool valid = true;

		source +=  format("	// Layer {}\n", layer->name);

		switch(layer->type) {
		case LAYER_AUTO:
			source +=  format("	weight = getAutoWeight(autoValue, autoMin{}, autoMax{}, autoBlend{});\n", i);
			break;

		case LAYER_WEIGHT:
			if(!layer->mapIndex) valid = false;
			else if(layer->mapData<4)
				source +=  format("	weight = map{1}Sample.{2};\n", layer->mapIndex, rgba[layer->mapData]);
			else
				source +=  format("	weight = 1.0 - dot(map{}Sample, vec4(1,1,1,1));\n", layer->mapIndex);
			break;

		case LAYER_COLOUR:
			if(!layer->mapIndex) valid = false;
			else source +=  format("	diff = map{}Sample;\n	weight = 1.0;\n", layer->mapIndex);
			break;
		case LAYER_INDEXED:
			if(!layer->mapIndex) valid = false;
			else {
				source += "	weight=1;\n";
				if(layer->mapData&0x100) 
					source += format(" 	sampleIndexedWeighted(map{1}, map{1}W, mapBounds, map{1}Size, worldPos.xz, scale{2}, diff, norm);\n", layer->mapIndex, i);
				else
					source += format("	sampleIndexedQuad(map{1}, mapBounds, map{1}Size, worldPos.xz, scale{2}, diff, norm);\n", layer->mapIndex, i);
			}
			break;
		case LAYER_GRADIENT:
			// Need a gradient texture, and an input parameter (height/slope/convex?)
			break;
		}
		if(!valid) {
			printf("Error: Layer %s references an invalid map\n", (const char*)layer->name);
			continue;
		}

		// CLip layer
		if(layer->texture == TEXTURE_CLIP) {
			source += "	if(weight>0.5) discard;\n";
			continue;
		}

		// Sample textures
		if(layer->type == LAYER_AUTO || layer->type==LAYER_WEIGHT) {
			if(layer->texture == TEXTURE_COLOUR) source +=
				"	diff = vec4(" + str(colour.r) + ", " + str(colour.g) + ", " + str(colour.b) + ", 0);\n"
				"	norm = vec4(0, 0, 1, 0);\n";
			else if(layer->projection == PROJECTION_VERTICAL) source +=
				"	diff = sampleTriplanar("+str(layer->texture)+".0, worldPos * scale"+index+", vertical);\n"
				"	norm = sampleTriplanerNormal("+str(layer->texture)+".0, worldPos * scale"+index+", worldNormal, vertical);\n"
				"	if(triplanar.y > 0.999) norm = vec4(0,1,0,0);\n";
			else if(layer->projection == PROJECTION_TRIPLANAR) source +=
				"	diff = sampleTriplanar("+str(layer->texture)+".0, worldPos * scale"+index+", triplanar);\n"
				"	norm = sampleTriplanerNormal("+str(layer->texture)+".0, worldPos * scale"+index+", worldNormal, triplanar);\n";
			else source += 
				"	diff = sampleDiffuse("+str(layer->texture)+".0, worldPos.xz * scale"+index+");\n"
				"	norm = sampleNormal("+str(layer->texture)+".0, worldPos.xz * scale"+index+");\n";
		}


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

	// Mapped normal calculation
	source += "\n"
	"	vec3 nmult = vec3(1.0, 0.0, -1.0);\n"
	"	vec3 finalNormal = normalize(worldNormal + normal.xzy * nmult);\n\n";

	// Lighting (basic diffuse);
	switch(m_mode) {
	case COMPOSITE:
		source +=
		"	// Lighting\n"
		"	float l = dot(finalNormal, normalize(lightDirection));\n"
		"	float s = (l+1)/1.3 * 0.2 + 0.1;\n"
		"	fragment = diffuse * max(l, s);\n"
		"}\n";
		break;
	case DIFFUSE:
		source +=
		"	// Diffuse output\n"
		"	fragment = diffuse;\n"
		"}\n";
		break;
	case NORMALS:
		source +=
		"	// Normal output\n"
		"	vec3 output = finalNormal * 0.5 + 0.5;"
		"	fragment = vec4(output, 1);\n"
		"}\n";
		break;
	case LIGHTING:
		source +=
		"	// Lighting\n"
		"	float l = dot(finalNormal, normalize(lightDirection));\n"
		"	float s = (l+1)/1.3 * 0.2 + 0.1;\n"
		"	l = max(l, s);\n"
		"	fragment = vec4(l,l,l,1);\n"
		"}\n";
		break;
	}

	// Vertex shader - todo: add concavity attribute
	static const char* vertexShader = 
	"#version 130\n"
	"in vec4 vertex;\n"
	"in vec3 normal;\n"
	"out vec3 worldNormal;\n"
	"out vec3 worldPos;\n"
	"uniform mat4 transform;\n"
	"void main() {\n"
	"	gl_Position = transform * vertex;\n"
	"	worldPos = vertex.xyz;\n"
	"	worldNormal = normal;\n"
	"}\n";

	// Compile shader
	Shader* shader = m_material->getPass(0)->getShader();
	m_vertexShader->setSource(vertexShader);
	m_fragmentShader->setSource( source.c_str() );
	shader->bindAttributeLocation("vertex", 0);
	shader->bindAttributeLocation("normal", 1);
	m_vars->set("lightDirection", 1,1,1);
	m_vars->setAuto("transform", base::AUTO_MODEL_VIEW_PROJECTION_MATRIX);
	m_material->getPass(0)->compile();

	char buffer[2048]; buffer[0] = 0;
	shader->getLog(buffer, sizeof(buffer));
	printf(buffer);

	if(!shader->isCompiled()) printf("Shader not actually compiled\n");


	// Streamed material
	if(m_streaming) {
		if(!m_stream) m_stream = new MaterialStream(m_material);
		m_stream->updateShader();
	}

	// Initialise variables
	for(size_t i=0; i<m_layers.size(); ++i) {
		update(i);
	}

	m_material->getPass(0)->compile();

	
	// DEBUG: save generated shader
	FILE* fp = fopen("shader_cache.glsl", "w");
	if(fp) fwrite(source.c_str(), 1, source.length(), fp);
	if(fp) fclose(fp);
	printf("Compiled shader %p\n", shader);


	m_needsCompile = false;
	return false;
}

#include <base/xml.h>
using base::XMLElement;
void DynamicMaterial::exportMaterial() const {
	// export shader file
	Pass* pass = m_material->getPass(0);
	const char* vs = "";
	const char* fs = "";
	for(ShaderPart* s: pass->getShader()->getParts()) {
		if(s->getType() == base::VERTEX_SHADER) vs = s->getSource() + 12;
		if(s->getType() == base::FRAGMENT_SHADER) fs = s->getSource() + 12;
	}

	char shaderFile[128];
	sprintf(shaderFile, "%s.glsl", getName());
	FILE* fp = fopen(shaderFile, "w");
	fprintf(fp, "#version 130\n\n");
	fprintf(fp, "#pragma vertex_shader\n%s\n", vs);
	fprintf(fp, "#pragma fragment_shader\n%s\n", fs);
	fclose(fp);

	// export material file
	base::XML xml("material");
	xml.getRoot().add("shader").setAttribute("file", shaderFile);
	XMLElement& transform = xml.getRoot().add("variable");
	transform.setAttribute("type", "auto");
	transform.setAttribute("name", "transform");
	transform.setAttribute("value", "modelviewprojection_matrix");

	auto addVar1 = [&xml](const char* name, float value) { XMLElement& e=xml.getRoot().add("variable"); e.setAttribute("name", name); e.setAttribute("type", "float"); e.setAttribute("value", value); };
	auto addVar2 = [&xml](const char* name, vec2 value) { XMLElement& e=xml.getRoot().add("variable"); e.setAttribute("name", name); e.setAttribute("type", "vec2"); char buf[64]; sprintf(buf,"%g %g",value.x, value.y); e.setAttribute("value", buf); };
	auto addVar3 = [&xml](const char* name, vec3 value) { XMLElement& e=xml.getRoot().add("variable"); e.setAttribute("name", name); e.setAttribute("type", "vec3"); char buf[64]; sprintf(buf,"%g %g %g",value.x, value.y, value.z); e.setAttribute("value", buf); };

	for(size_t index=0; index<m_layers.size(); ++index) {
		const MaterialLayer* layer = m_layers[index];
		if(layer->texture != TEXTURE_CLIP) {
			addVar1( addIndex("opacity",index), layer->visible? layer->opacity: 0);
		}

		// Splat texture scaling
		if(layer->type != LAYER_COLOUR && layer->texture>=0) {
			if(layer->projection == PROJECTION_FLAT) addVar2( addIndex("scale",index), 1.0 / layer->scale.xy());
			else addVar3( addIndex("scale",index), 1.0 / layer->scale);
		}

		// Auto parameters
		if(layer->type == LAYER_AUTO) {
			vec3 min(layer->height.min, layer->slope.min, layer->concavity.min);
			vec3 max(layer->height.max, layer->slope.max, layer->concavity.max);
			vec3 blend(layer->height.blend, layer->slope.blend, layer->concavity.blend);
			for(int i=0; i<3; ++i) if(blend[i]<=0) blend[i] = 0.001; // avoid div0

			//vec3 noise(layer->height.noise, layer->slope.noise, layer->concavity.noise);
			addVar3( addIndex("autoMin",index), min);
			addVar3( addIndex("autoMax",index), max);
			addVar3( addIndex("autoBlend",index), blend);
			//m_vars->set( addIndex("autoNoise",index), noise);
		}
	}
	addVar3("lightDirection", vec3(1,1,1));
	sprintf(shaderFile, "%s.mat", getName());
	xml.save(shaderFile);

	printf("Exported material to %s\n", shaderFile);
}



