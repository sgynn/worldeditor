#ifndef _TEXTURE_TOOLS_
#define _TEXTURE_TOOLS_

#include "tool.h"
#include "editabletexture.h"


/* Common base class for texture editing tools */
template<class BT>
class TextureToolBase : public Tool {
	public:
	TextureToolBase(unsigned map) : m_map(map) { buffer = new PaintBuffer<BT>(64); }
	~TextureToolBase() { delete buffer; }
	uint getTarget() const override { return m_map; }
	void end() override { buffer->clear(); }

	protected:
	PaintBuffer<BT>* buffer;
	unsigned m_map;
};

/* Modify a single channel of the texture */
class TextureTool : public TextureToolBase<ubyte> {
	public:
	TextureTool(unsigned map) : TextureToolBase(map) {}
	void paint(BrushData&, const Brush&, int flags) override;
};

struct ColourToolBuffer { bool set; ubyte o[3]; float w[3]; };

/** Modify all channels of the texture */
class ColourTool : public TextureToolBase<ColourToolBuffer> {
	public:
	ColourTool(unsigned map) : TextureToolBase(map) {}
	void paint(BrushData&, const Brush&, int flags) override;
};

/** Set material index map */
class IndexTool : public TextureToolBase<ubyte> {
	public:
	IndexTool(unsigned map) : TextureToolBase(map) {}
	void paint(BrushData&, const Brush&, int flags) override;
};

/** Set material weight and index maps. EditableMap may contain two textures */
class IndexWeightTool : public IndexTool {
	public:
	IndexWeightTool(unsigned ix) : IndexTool(ix)  {}
	void paint(BrushData&, const Brush&, int flags) override;
};

#endif

