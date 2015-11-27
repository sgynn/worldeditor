#ifndef _TOOL_GROUP_
#define _TOOL_GROUP_

#include "terraineditor/tool.h"
#include "terraineditor/editabletexture.h"
#include "terraineditor/texturetools.h"
#include "gui/delegate.h"
#include <vector>

namespace gui { class Widget; class Button; class Root; }

/** Group of tools - binds tools to gui */
class ToolGroup {
	public:
	ToolGroup(const char* name);
	virtual ~ToolGroup();

	virtual void          setup(gui::Root* gui);
	virtual void          setResolution(const vec2& offset, const vec2& size, float res) = 0;
	virtual ToolInstance* getTool() const;	// Get selected tool in this group
	virtual gui::Widget*  getPanel() const;	// Get gui panel for this tool group
	const char*           getName() const;	// Get name

	public:
	DelegateS<void(ToolInstance*)> eventToolSelected;

	protected:
	void addButton(const char* icon=0);
	void addTool(Tool*, int, int);
	void selectTool(gui::Button*);
	gui::Root* m_root;
	gui::Widget* m_panel;
	std::vector<ToolInstance*> m_tools;
	size_t m_currentTool;
	char m_name[64];
};



class GeometryToolGroup : public ToolGroup {
	public:
	GeometryToolGroup();
	~GeometryToolGroup();
	void addTool(const char* icon, Tool* tool, int flag, int shift);
	void setResolution(const vec2& offset, const vec2& size, float res);
};

class ColourToolGroup : public ToolGroup {
	public:
	ColourToolGroup(const char* name, EditableTexture*);
	~ColourToolGroup();
	void setup(gui::Root* gui);
	void openPicker(gui::Button*);
	void setResolution(const vec2& offset, const vec2& size, float res);
	protected:
	ColourTool* m_tool;
};

class WeightToolGroup : public ToolGroup {
	public:
	WeightToolGroup(const char* name, EditableTexture* image);
	~WeightToolGroup();
	void setup(gui::Root* gui);
	void setChannel(int index, const char* icon);
	void setResolution(const vec2& offset, const vec2& size, float res);

	protected:
	TextureTool* m_tool;
};


class MaterialToolGroup : public ToolGroup {
	public:
	MaterialToolGroup(EditableTexture* indexMap, EditableTexture* weightMap);
	~MaterialToolGroup();
	void addTexture(const char* icon, int index);
	void setResolution(const vec2& offset, const vec2& size, float res);

	protected:
	MaterialTool* m_tool;
};




#endif

