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
	HeightTool() : m_data(0) {}
	void paint(BrushData&, const Brush&, int flags) override;
};

/** Terrain smoothing */
class SmoothTool : public HeightTool {
	public:
	void paint(BrushData&, const Brush&, int flags) override;
};

/** Level tool. flag = use last sample */
class LevelTool : public HeightTool {
	public:
	LevelTool() : target(-1e8f) {}
	void paint(BrushData&, const Brush&, int flags) override;
	void end() override { HeightTool::end(); target=-1e8f; }
	protected:
	float target;
};

/** Flatten tool. flag = use last sample */
class FlattenTool : public LevelTool {
	public:
	void paint(BrushData&, const Brush&, int flags) override;
	protected:
	vec3 normal;
};


/** Noise tool. flag = inverse */
class NoiseTool : public HeightTool {
	public:
	void paint(BrushData&, const Brush&, int flags) override;
};

/** Erosion tool */
class ErosionTool : public HeightTool {
	public:
	void paint(BrushData&, const Brush&, int flags) override;
};


#endif

