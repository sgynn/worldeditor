#ifndef _EDITOR_TOOL_
#define _EDITOR_TOOL_

#include <base/math.h>

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
		if(falloff == 0.5) return radius * 0.5;
		if(falloff > 0.5)  return pow(s, 1.0 / (1.0 + (falloff-0.5) * 10) ) * radius;
		else               return (1.0 - pow(s, 1.0 / (1.0+(0.5-falloff)*10) )) * radius;
	}
};

/** Editor tool base class */
class Tool {
	public:
	Tool() : m_resolution(1) {}
	virtual ~Tool() {}
	virtual void begin() {}									// Begin brush stroke (mousedown)
	virtual void paint(const Brush& brush, int flags) = 0;	// Paint (update / mousemove)
	virtual void commit() {};								// End of paint update when painting
	virtual void end() {}									// End brush stroke (mouseup)

	void setResolution(float resolution, const vec2& offset=vec2()) {
		m_resolution = resolution;
		m_offset = offset;
	}

	protected:
	float m_resolution;
	vec2  m_offset;


	protected:
	// Get affected pixels
	Rect getRect(const Brush& b) const {
		vec2 c = b.position - m_offset;
		vec2 min = ceil ((c - b.radius) / m_resolution);
		vec2 max = floor((c + b.radius) / m_resolution);
		return Rect(min.x, min.y, max.x-min.x+1, max.y-min.y+1);
	}
	// Get the world coordinates of a pixel
	vec2 getWorldPosition(int px, int pz) const {
		return vec2(px * m_resolution + m_offset.x, pz * m_resolution + m_offset.y);
	}
	// Get value at pixel
	float getValue(const Brush& b, int px, int py) const {
		vec2 p = getWorldPosition(px, py);
		float w = b.position.distance(p) / b.radius;
		if(w > 1) return 0;
		if(b.falloff > 0.5) w = 1.0 - pow(w, 1.0 + (b.falloff - 0.5) * 10);
		else w = pow(1.0 - w, 1.0 + (0.5 - b.falloff) * 10 );
		return (w<0? 0: w) * b.strength;
	}
};


struct ToolInstance {
	Tool* tool;
	int flags;
	int shift;
};



#endif

