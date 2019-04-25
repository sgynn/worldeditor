#ifndef _TEXTURE_TOOLS_
#define _TEXTURE_TOOLS_

#include "tool.h"
#include "editabletexture.h"


/* Common base class for texture editing tools */
template<class BT>
class TextureToolBase : public Tool {
	public:
	TextureToolBase() { buffer = new PaintBuffer<BT>(64); }
	virtual ~TextureToolBase() { delete buffer; }
	virtual void end() { buffer->clear(); }
	protected:
	PaintBuffer<BT>* buffer;
};

/* Modify a single channel of the texture */
class TextureTool : public TextureToolBase<ubyte> {
	public:
	TextureTool(EditableTexture* tex=0) : texture(tex) {}
	virtual void paint(const Brush&, int flags);
	virtual void commit() { texture->updateGPU(); }
	EditableTexture* texture;
};

struct ColourToolBuffer { bool set; ubyte o[3]; float w[3]; };

/** Modify all channels of the texture */
class ColourTool : public TextureToolBase<ColourToolBuffer> {
	public:
	ColourTool(EditableTexture* tex=0) : texture(tex) {}
	virtual void paint(const Brush&, int flags);
	virtual void commit() { texture->updateGPU(); }
	EditableTexture* texture;
};

/** Set material index map */
class IndexTool : public TextureToolBase<ubyte> {
	public:
	IndexTool(EditableTexture* ix=0) : indexMap(ix) {}
	virtual void paint(const Brush&, int flags);
	virtual void commit() { indexMap->updateGPU(); }

	public:
	EditableTexture* indexMap;
};

/** Set material weight and index maps */
class IndexWeightTool : public IndexTool {
	public:
	IndexWeightTool(EditableTexture* ix=0, EditableTexture* wt=0) : IndexTool(ix), weightMap(wt) {}
	virtual void paint(const Brush&, int flags);
	virtual void commit() { weightMap->updateGPU(); indexMap->updateGPU(); }

	public:
	EditableTexture* weightMap;
};

#endif

