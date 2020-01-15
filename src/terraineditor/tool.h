#ifndef _EDITOR_TOOL_
#define _EDITOR_TOOL_

#include <base/math.h>

class EditableMap;

class Brush {
	public:
	vec2  position;
	float radius;
	float strength;
	float falloff;

	public:
	// Get radius of a normalised strength
	float getRadius(float s = 0.5) const {
		// x = y^k => y = x^(1/k);
		if(falloff == 0.5) return (1.0 - s) * radius;
		if(falloff > 0.5)  return pow(s, 1.0 / (1.0 + (falloff-0.5) * 10) ) * radius;
		else               return (1.0 - pow(s, 1.0 / (1.0+(0.5-falloff)*10) )) * radius;
	}
	
	// Get brush effect weight at a point
	float getWeight(const vec2& p) const {
		float w = position.distance(p) / radius;
		if(w > 1) return 0;
		if(falloff > 0.5) w = 1.0 - pow(w, 1.0 + (falloff - 0.5) * 10);
		else w = pow(1.0 - w, 1.0 + (0.5 - falloff) * 10 );
		return (w<0? 0: w) * strength;
	}
};

/** Cached block of map data to apply tool on */
class BrushData {
	public:
	BrushData() : m_data(0), m_dataSize(0) {}
	~BrushData() { delete [] m_data; }
	void   reset(const Brush&, float resolution, int channels);
	int    getChannels() const { return m_channels; }
	float  getResolution() const { return m_resolution; }
	const Point& getSize() const { return m_size; }
	const Point& getOffset() const { return m_intOffset; }
	vec2   getWorldPosition(int x, int y) const { return vec2(x*m_resolution, y*m_resolution) + m_offset; }
	float* getValue(int x, int y) { return m_data + x*m_mx + y*m_my; }
	private:
	float* m_data;
	int m_dataSize;
	int m_channels;
	int m_mx, m_my;
	vec2 m_offset;
	Point m_size;
	Point m_intOffset;
	float m_resolution;
};

/** Editor tool base class */
class Tool {
	public:
	virtual ~Tool() {}
	virtual uint getTarget() const { return 0; }
	virtual void begin(const Brush&) {}								// Begin brush stroke (mousedown)
	virtual void paint(BrushData&, const Brush&, int flags) = 0;	// Paint (update / mousemove)
	virtual void end() {}											// End brush stroke (mouseup)

};


struct ToolInstance {
	Tool* tool;
	int flags;
	int shift;
};



#endif

