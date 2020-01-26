#include "heighttools.h"
#include "editor.h"
#include <cstdio>

static const Point one(1,1);
void HeightTool::resizeData(int s) {
	if(!m_data || s > m_dataSize || s < m_dataSize * 0.6) {
		if(m_data) delete [] m_data;
		m_data = new float[s];
		m_dataSize = s;
	}
}

void HeightTool::paint(BrushData& data, const Brush& brush, int flags) {
	float direction = flags? -1: 1;
	float weight;
	vec2 worldPosition;
	const Point& e = data.getSize();
	for(int x=0; x<e.x; ++x) for(int y=0; y<e.y; ++y) {
		if(data.locked(x,y)) continue;
		worldPosition = data.getWorldPosition(x, y);
		weight = brush.getWeight(worldPosition);
		*data.getValue(x,y) += weight * direction;
	}
}

// ----------------------------------------------- //


void SmoothTool::paint(BrushData& data, const Brush& brush, int flags) {
	static const float blur[3] = { 0.27249597f, 0.12475775f, 0.05711826f };

	vec2 worldPosition;
	float target, weight;
	const Point e(data.getSize().x-1, data.getSize().y-1);
	for(int x=1; x<e.x; ++x) for(int y=1; y<e.y; ++y) {
		worldPosition = data.getWorldPosition(x, y);
		weight = brush.getWeight(worldPosition);
		if(weight<=0) continue;
		if(data.locked(x,y)) continue;
		// Sample 9 points
		float& value = *data.getValue(x,y);
		target = value * blur[0];
		target += *data.getValue(x-1,y) * blur[1];
		target += *data.getValue(x+1,y) * blur[1];
		target += *data.getValue(x,y-1) * blur[1];
		target += *data.getValue(x,y+1) * blur[1];
		target += *data.getValue(x-1,y-1) * blur[2];
		target += *data.getValue(x+1,y-1) * blur[2];
		target += *data.getValue(x-1,y+1) * blur[2];
		target += *data.getValue(x+1,y+1) * blur[2];
		value += (target - value) * weight;
	}
}

// ----------------------------------------------- //

void LevelTool::paint(BrushData& data, const Brush& brush, int flags) {
	if(data.getSize().x<2 || data.getSize().y<2) return; // Not enough data
	// Get target height
	if(flags==0 || target<=-1e8f) {
		Point sample(data.getSize().x/2, data.getSize().y/2);
		float values[4];
		values[0] = *data.getValue(sample.x, sample.y);
		values[1] = *data.getValue(sample.x+1, sample.y);
		values[2] = *data.getValue(sample.x, sample.y+1);
		values[3] = *data.getValue(sample.x+1, sample.y+1);
		// Interpolate to get actual height
		vec2 f = brush.position / data.getResolution();
		f -= floor(f);
		values[0] += (values[1] - values[0]) * f.x;
		values[2] += (values[3] - values[2]) * f.x;
		values[0] += (values[2] - values[0]) * f.y;
		target = values[0];
	}

	// Paint
	float weight;
	vec2 worldPosition;
	const Point& e = data.getSize();
	for(int x=0; x<e.x; ++x) for(int y=0; y<e.y; ++y) {
		if(data.locked(x,y)) continue;
		worldPosition = data.getWorldPosition(x, y);
		weight = brush.getWeight(worldPosition);
		float& value = *data.getValue(x,y);
		value += (target - value) * weight * 0.1;
	}
}

// ----------------------------------------------- //

void FlattenTool::paint(BrushData& data, const Brush& brush, int flags) {
	if(data.getSize().x<2 || data.getSize().y<2) return; // Not enough data
	// Get target normal
	if(flags==0 || target<=-1e8f) {
		Point sample(data.getSize().x/2, data.getSize().y/2);
		float values[4];
		values[0] = *data.getValue(sample.x, sample.y);
		values[1] = *data.getValue(sample.x+1, sample.y);
		values[2] = *data.getValue(sample.x, sample.y+1);
		values[3] = *data.getValue(sample.x+1, sample.y+1);
		// Calculate approximate normal (faceted)
		float res = data.getResolution();
		vec2 f = brush.position / res;
		f -= floor(f);
		int b = f.x+f.y > 1? 1: 0;
		normal = vec3(res,values[1]-values[b], 0).cross( vec3(0,values[2]-values[b],res) );
		if(b) normal *= -1;
		// Calculate target
		values[0] += (values[1] - values[0]) * f.x;
		values[2] += (values[3] - values[2]) * f.x;
		values[0] += (values[2] - values[0]) * f.y;
		vec2 tp = data.getWorldPosition(sample.x, sample.y) + f * res;
		target = normal.dot(vec3(tp.x, values[0], tp.y));
	}

	// Paint
	float weight, t;
	vec2 worldPosition;
	const Point& e = data.getSize();
	for(int x=0; x<e.x; ++x) for(int y=0; y<e.y; ++y) {
		if(data.locked(x,y)) continue;
		worldPosition = data.getWorldPosition(x, y);
		weight = brush.getWeight(worldPosition);
		t = (target - normal.x * worldPosition.x - normal.z * worldPosition.y) / normal.y; // Project to plane
		float& value = *data.getValue(x,y);
		value += (t - value) * weight * 0.1;
	}
}

// ----------------------------------------------- //

void NoiseTool::paint(BrushData& data, const Brush& brush, int flags) {
	float weight;
	vec2 worldPosition;
	const Point& e = data.getSize();
	for(int x=0; x<e.x; ++x) for(int y=0; y<e.y; ++y) {
		if(data.locked(x,y)) continue;
		worldPosition = data.getWorldPosition(x, y);
		weight = brush.getWeight(worldPosition);
		*data.getValue(x,y) += weight * (rand() / RAND_MAX) - 0.5;
	}
}

// ----------------------------------------------- //

void ErosionTool::paint(BrushData&, const Brush& brush, int flags) {
	// Complicated :  simulate water ?
}

