#include "dynamicmaterial.h"
#include "materialeditor.h"
#include "streaming/texturestream.h"
#include "terraineditor/editabletexture.h"
#include "scene/material.h"
#include "scene/shader.h"
#include <base/colour.h>

#include <base/opengl.h>

#include <cstdio>
#include <string>
#include <set>

using base::Texture;
using scene::ShaderProgram;
using scene::Material;
using scene::Shader;
using scene::Pass;

DynamicMaterial::DynamicMaterial(bool stream) : m_material(0), m_stream(0), m_streaming(stream), m_needsCompile(true) {
	Shader* shader = new Shader();
	m_vertexShader = new ShaderProgram(scene::ShaderProgram::VERTEX_SHADER);
	m_fragmentShader = new ShaderProgram(scene::ShaderProgram::FRAGMENT_SHADER);
	shader->attach(m_vertexShader);
	shader->attach(m_fragmentShader);
	m_vars = new scene::ShaderVars();
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
	GL_CHECK_ERROR;

	// Set all the variables
	MaterialLayer* layer = m_layers[index];
	m_vars->set( addIndex("opacity",index), layer->visible? layer->opacity: 0);

	// Map coordinates
	if(layer->map) {
		char buffer[32];
		sprintf(buffer, "%sInfo", layer->map.str());
		if(!m_vars->contains(buffer)) {
			m_vars->set(buffer, 4, 1, m_coords);
			sprintf(buffer, "%sSize", layer->map.str());
			m_vars->set(buffer, 1024.f, 1024.f);
			sprintf(buffer, "%sMap", layer->map.str());
			m_material->getPass(0)->getParameters().set(buffer, 0);
		}
	}


	// Splat texture scaling
	if(layer->type != LAYER_COLOUR) {
		if(layer->projection == PROJECTION_FLAT) m_vars->set( addIndex("scale",index), 1.0 / layer->scale.xy());
		else m_vars->set( addIndex("scale",index), 1.0 / layer->scale);
	}

	// Auto parameters
	if(layer->type == LAYER_AUTO) {
		vec3 min(layer->height.min, layer->slope.min, layer->concavity.min);
		vec3 max(layer->height.max, layer->slope.max, layer->concavity.max);
		vec3 blend(layer->height.blend, layer->slope.blend, layer->concavity.blend);
		for(int i=0; i<3; ++i) if(blend[i]<=0) blend[i] = 0.001; // avoid div0

		//vec3 noise(layer->height.noise, layer->slope.noise, layer->concavity.noise);
		m_vars->set( addIndex("autoMin",index), min);
		m_vars->set( addIndex("autoMax",index), max);
		m_vars->set( addIndex("autoBlend",index), blend);
		//m_vars->set( addIndex("autoNoise",index), noise);
	}
	GL_CHECK_ERROR;
}

void DynamicMaterial::setTextures(MaterialEditor* src) {
	if(!m_material) return;
	if(m_stream) m_stream->build();

	// Textures
	m_material->getPass(0)->setTexture("diffuseArray", &src->getDiffuseArray());
	m_material->getPass(0)->setTexture("normalArray", &src->getNormalArray());
	if(m_stream) {
		m_stream->setTexture("diffuseArray", src->getDiffuseArray());
		m_stream->setTexture("normalArray", src->getNormalArray());
		printf("Textures set\n");
	}

	// Bind all the maps
	char buffer[32];
	typedef std::set<const char*> MapList;
	MapList maps;
	for(size_t i=0; i<m_layers.size(); ++i) {
		if(m_layers[i]->map  && m_layers[i]->map[0])  maps.insert( m_layers[i]->map );
		if(m_layers[i]->map2 && m_layers[i]->map2[0]) maps.insert( m_layers[i]->map2 );
		update(i);
	}
	for(MapList::iterator i=maps.begin(); i!=maps.end(); ++i) {
		EditableTexture* map = src->getMap(*i);
		if(map) {
			const Texture& tex = map->getTexture();
			tex.setFilter(Texture::NEAREST);
			sprintf(buffer, "%sMap", *i);
			m_material->getPass(0)->setTexture(buffer, &tex);
			sprintf(buffer, "%sInfo", *i);
			m_vars->set(buffer, 4, 1, m_coords);
			sprintf(buffer, "%sSize", *i);
			m_vars->set(buffer, (float)map->getTexture().width(), (float)map->getTexture().height());
			if(m_stream) {
				if(map->getTextureStream()) m_stream->addStream(*i, map->getTextureStream());
				else m_stream->setOverlayTexture(*i, map->getTexture());
			}
		}
	}
	GL_CHECK_ERROR;
}

// ------------------------------------------------------------------------------------------------------ //

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

	// Inputs
	source +=
	"#version 130\n\n"
	"in vec3 worldNormal;\n"
	"in vec3 worldPos;\n"
	"out vec4 fragment;\n\n"
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
	for(MapList::iterator i=maps.begin(); i!=maps.end(); ++i) source += "uniform vec4 " + *i + "Info;\n"  "uniform vec2 " + *i + "Size;\n";
	source += "\n\n";

	// Variables
	for(size_t i=0; i<m_layers.size(); ++i) {
		std::string index = str(i);
		if(m_layers[i]->projection == PROJECTION_FLAT) source += "uniform vec2 scale" + index + ";\n";
		else source += "uniform vec3 scale" + index + ";\n";
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
//	"	vec3 ctr = (vmin + vmax) * 0.5;\n"
//	"	vec3 r = smoothstep(ctr - vmin, ctr - vmin + vblend, abs(value - ctr));\n"
	"	vec3 r = smoothstep(vmin-vblend, vmin, value) * smoothstep(vmax+vblend, vmax, value);\n"
	"	return r.x * r.y * r.z;\n"
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
	"}\n";
	
	// Barycentric coordinates for index maps
	source +=
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
	source +=
	"void sampleIndexed(sampler2D map, vec4 info, vec2 size, vec2 coord,  out vec4 diff, out vec4 norm) {\n"
	"	vec2 a,b,c;\n"
	"	vec3 bary = barycentric(info, size, coord, a,b,c);\n"
	"	// Sample index map\n"
	"	float ia = texture(map, a).r * 255;\n"
	"	float ib = texture(map, b).r * 255;\n"
	"	float ic = texture(map, c).r * 255;\n"
	"	// Sample textures\n"
	"	diff  = texture(diffuseArray, vec3(coord,ia)) * bary.x;\n"
	"	diff += texture(diffuseArray, vec3(coord,ib)) * bary.y;\n"
	"	diff += texture(diffuseArray, vec3(coord,ic)) * bary.z;\n"
	"	norm  = texture(normalArray, vec3(coord,ia)) * bary.x;\n"
	"	norm += texture(normalArray, vec3(coord,ib)) * bary.y;\n"
	"	norm += texture(normalArray, vec3(coord,ic)) * bary.z;\n"
	"	norm = vec4(norm.xyz * 2.0 - 1.0, norm.w);\n"
	"}\n";

	// Alternative indexed sampling
	source +=
	"void sampleIndexedQuad(sampler2D map, vec4 info, vec2 size, vec2 coord,  out vec4 diff, out vec4 norm) {\n"
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
	source +=
	"void sampleIndexedWeighted(sampler2D map, sampler2D weights, vec4 info, vec2 size, vec2 coord,  out vec4 diff, out vec4 norm) {\n"
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
	"	vec4 wa = texture(weights, a) * 255;\n"
	"	vec4 wb = texture(weights, b) * 255;\n"
	"	vec4 wc = texture(weights, c) * 255;\n"
	"	vec4 wd = texture(weights, d) * 255;\n"
	"	// Sample textures\n"
	"	diff  = texture(diffuseArray, vec3(coord,ia.x)) * w.x * wa.x;\n"
	"	diff += texture(diffuseArray, vec3(coord,ib.x)) * w.y * wb.x;\n"
	"	diff += texture(diffuseArray, vec3(coord,ic.x)) * w.z * wc.x;\n"
	"	diff += texture(diffuseArray, vec3(coord,id.x)) * w.w * wd.x;\n"
	"	diff  = texture(diffuseArray, vec3(coord,ia.y)) * w.x * wa.y;\n"
	"	diff += texture(diffuseArray, vec3(coord,ib.y)) * w.y * wb.y;\n"
	"	diff += texture(diffuseArray, vec3(coord,ic.y)) * w.z * wc.y;\n"
	"	diff += texture(diffuseArray, vec3(coord,id.y)) * w.w * wd.y;\n"
	"	diff  = texture(diffuseArray, vec3(coord,ia.z)) * w.x * wa.z;\n"
	"	diff += texture(diffuseArray, vec3(coord,ib.z)) * w.y * wb.z;\n"
	"	diff += texture(diffuseArray, vec3(coord,ic.z)) * w.z * wc.z;\n"
	"	diff += texture(diffuseArray, vec3(coord,id.z)) * w.w * wd.z;\n"
	"	diff  = texture(diffuseArray, vec3(coord,ia.w)) * w.x * wa.w;\n"
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
	"	vec4 normal = vec4(0,1,0,0);\n"
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
	"	vec3 vertical = vec3(triplanar.x, 0, triplanar.z);\n"
	"	vertical /= triplanar.y>0.99? 1: dot(vertical, vec3(1,1,1));\n\n";
	


	// Sample maps
	if(!maps.empty()) {
		source +=  "	// Sample maps\n";
		for(MapList::iterator i=maps.begin(); i!=maps.end(); ++i) {
			source +=  "	vec4 " + *i + "Sample = sampleMap(" + *i + "Map, " + *i + "Info, worldPos.xz);\n";
		}
		source += "\n";
	}

	// Apply layers
	const std::string rgba[] = { "r", "g", "b", "a" };
	for(size_t i=0; i<m_layers.size(); ++i) {
		std::string index = str(i);
		MaterialLayer* layer = m_layers[i];
		Colour colour(layer->colour);
		bool valid = true;

		source +=  "	// Layer " + str(layer->name) + "\n";

		switch(layer->type) {
		case LAYER_AUTO:
			source +=  "	weight = getAutoWeight(autoValue, autoMin"+index+", autoMax"+index+", autoBlend"+index+");\n";
			break;

		case LAYER_WEIGHT:
			if(!layer->map || !layer->map[0]) valid = false;
			else if(layer->mapData<4)
				source +=  "	weight = " + str(layer->map) + "Sample." + rgba[layer->mapData] + ";\n";
			else
				source +=  "	weight = 1.0 - dot(" + str(layer->map) + "Sample, vec4(1,1,1,1));\n";
			break;

		case LAYER_COLOUR:
			if(!layer->map.empty()) valid = false;
			else source +=  "	diff = " + str(layer->map) + "Sample;\n	weight = 1.0;\n";
			break;
		case LAYER_INDEXED:
			if(layer->map.empty()) valid = false;
			else {
				source += "	weight=1;\n";
				if(layer->map2.empty()) 
					source += "	sampleIndexedQuad(" + str(layer->map)  + "Map, " + str(layer->map) + "Info, " + str(layer->map) + "Size, worldPos.xz, diff, norm);\n";
				else
					source += "	sampleIndexedWeighted(" + str(layer->map)  + "Map, " + str(layer->map) + "Info, " + str(layer->map) + "Size, worldPos.xz, diff, norm);\n";
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

		// Sample textures
		if(layer->type == LAYER_AUTO || layer->type==LAYER_WEIGHT) {
			if(layer->texture < 0) source +=
				"	diff = vec4(" + str(colour.r) + ", " + str(colour.g) + ", " + str(colour.b) + ", 0);\n"
				"	norm = vec4(0, 0, 1, 0);\n";
			else if(layer->projection == PROJECTION_VERTICAL) source +=
				"	diff = sampleTriplanar("+str(layer->texture)+".0, worldPos * scale"+index+", vertical);\n"
				"	norm = sampleTriplanerNormal("+str(layer->texture)+".0, worldPos * scale"+index+", worldNormal, vertical);\n";
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

	// Lighting (basic diffuse);
	source +=
	"	// Lighting\n"
	"	float l = dot( normalize(worldNormal), normalize(lightDirection) );\n"
	"	float s = (l+1)/1.3 * 0.2 + 0.1;\n"
	"	fragment = diffuse * max(l, s);\n"
	"}\n";

	// Vertex shader - todo: add concavity attribute
	static const char* vertexShader = 
	"#version 130\n"
	"in vec4 vertex;\n"
	"in vec3 normal;\n"
	"out vec3 worldNormal;\n"
	"out vec3 worldPos;\n"
	"void main() {\n"
	"	gl_Position = gl_ModelViewProjectionMatrix * vertex;\n"
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




