#include "dynamicmaterial.h"
#include <base/material.h>

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

bool DynamicMaterial::compile() {
	// Complicated
	

	// Get materials
	//   maps (need mapInfo coordinates)
	//   Texture arrays for the rest
	
	
	// We only want to sample the maps once as they may be used multple times, so do this first.
	
	// for each layer
	//   Sample textures
	//   Blend with world

	// Functions needed
	//    vec4  sampleTriplaner(vec3 coord, vec3 normal)
	//    vec3  sampleTriplanerNormal(vec3 coord, vec3 normal)
	//    float getAutoWeight(vec3 input, vec3 slope, vec3 height, vec3 concave)
	//    vec2  getIndexedData(vec2 coord, sampler2D indexMap, sampler2D weightMap, vec2 inverseResolution)
	//    vec4  sampleMap(sampler2D map, vec4 info, vec2 coord);
	//    vec4  sampleDiffuse(int index, vec2 coord)
	//    vec4  sampleNormal(int index, vec2 coord)
	//    + blend functions


	/* Sample
	 * 		void main() {
	 * 			// Null data
	 * 			vec3 diffuse = vec3(1,1,1);
	 * 			vec3 normal = vec3(0,1,0);
	 * 			float gloss = 0.1;
	 * 			float height = 0;
	 * 			float weight;
	 *
	 *			// Sample maps
	 *			vec4 weightSample = sampleMap(weightMap, weightInfo, worldCoord);
	 *			vec4 colourSample = sampleMap(colourMap, colourInfo, worldCoord);
	 *			
	 *			// Layers
	 *			weight = getAutoWeight(autoInput, autoSlope1, autoHeight1, autoConcave1) * opacity1;
	 *			diffuse = mix(diffuse, sampleDiffuse(1, worldCoord * scale1.xy), weight);
	 *			normal  = mix(diffuse, sampleNormal(1, worldCoord * scale1.xy), weight);
	 *
	 * 			weight = weightSample.x * opacity2;
	 *			diffuse = mix(diffuse, sampleDiffuse(4, worldCoord * scale2.xy), weight);
	 *			normal  = mix(diffuse, sampleNormal(4, worldCoord * scale2.xy), weight);
	 *
	 * 			weight = weightSample.y * opacity3;
	 *			diffuse = mix(diffuse, sampleDiffuse(5, worldCoord * scale2.xy), weight);
	 *			normal  = mix(diffuse, sampleNormal(5, worldCoord * scale2.xy), weight);
	 *
	 * 			diffuse = diffuse * colourSample.rgb;
	 *
	 *			// Lighting
	 *			gl_FragColor = light(diffuse, normal, gloss, lightDir, lightColour);
	 * 		}
	 */

	// Need to decide the texture packing layout before we do this.
	// or perhaps just use diffuse+height, normal; for now. Canalways change it later

	return false;
}

