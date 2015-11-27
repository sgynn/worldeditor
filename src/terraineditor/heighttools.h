#ifndef _HEIGHT_TOOLS_
#define _HEIGHT_TOOLS_

#include "tool.h"

class HeightmapEditorInterface;

/** Basic height editing. flag = inverse */
class HeightTool : public Tool {
	protected:
	float* m_data;
	int    m_dataSize;
	void   resizeData(int s);

	public:
	HeightTool(HeightmapEditorInterface* h) : m_data(0), heightmap(h) {}
	virtual void paint(const Brush&, int flags);
	virtual void end();

	HeightmapEditorInterface* heightmap;
};

/** Terrain smoothing */
class SmoothTool : public HeightTool {
	public:
	SmoothTool(HeightmapEditorInterface* h) : HeightTool(h) {}
	virtual void paint(const Brush&, int flags);
};

/** Level tool. flag = use last sample */
class LevelTool : public HeightTool {
	public:
	LevelTool(HeightmapEditorInterface* h) : HeightTool(h), target(-1e8f) {}
	virtual void paint(const Brush&, int flags);
	float target;
};

/** Flatten tool. flag = use last sample */
class FlattenTool : public HeightTool {
	public:
	FlattenTool(HeightmapEditorInterface* h) : HeightTool(h), target(-1e8f) {}
	virtual void paint(const Brush&, int flags);
	vec3 normal;
	float target;
};


/** Noise tool. flag = inverse */
class NoiseTool : public HeightTool {
	public:
	NoiseTool(HeightmapEditorInterface* h) : HeightTool(h) {}
	virtual void paint(const Brush&, int flags);
};

/** Erosion tool */
class ErosionTool : public HeightTool {
	public:
	ErosionTool(HeightmapEditorInterface* h) : HeightTool(h) {}
	virtual void paint(const Brush&, int flags);
};


#endif

