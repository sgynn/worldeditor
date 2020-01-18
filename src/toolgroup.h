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
	virtual void          setTextures(int count) {}
	virtual void          setActive();
	virtual ToolInstance* getTool() const;	// Get selected tool in this group
	virtual gui::Widget*  getPanel() const;	// Get gui panel for this tool group
	const char*           getName() const;	// Get name
	void                  deselect();		// Deselect active tool


	public:
	Delegate<void(ToolInstance*)> eventToolSelected;

	protected:
	gui::Button* addButton(const char* icon=0);
	void addTool(Tool*, int flags, int shift);
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
};

class ColourToolGroup : public ToolGroup {
	public:
	ColourToolGroup(const char* name, unsigned map);
	~ColourToolGroup();
	void setActive();
	void setup(gui::Root* gui);
	void openPicker(gui::Button*);
	protected:
	void colourChanged(const Colour&);
	ColourTool* m_tool;
	Colour m_colour;
};

class WeightToolGroup : public ToolGroup {
	public:
	WeightToolGroup(const char* name, int channels, unsigned map);
	~WeightToolGroup();
	void setup(gui::Root* gui);
	void setChannel(int index, const char* icon);

	protected:
	TextureTool* m_tool;
};


class IndexToolGroup : public ToolGroup {
	public:
	IndexToolGroup(const char* name, unsigned map);
	~IndexToolGroup();
	void setTextures(int count);

	protected:
	IndexTool* m_tool;
};


class IndexWeightToolGroup : public IndexToolGroup {
	public:
	IndexWeightToolGroup(const char* name, unsigned map);
};




#endif

