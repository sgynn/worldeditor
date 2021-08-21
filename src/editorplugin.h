#pragma once

#include <base/vec.h>
#include <base/point.h>

class MapGrid;
class TerrainMap;
class FileSystem;
namespace gui { class Widget; }
namespace base { class XMLElement; class Camera; struct Mouse; }

enum KeyMask { CTRL_MASK=1, SHIFT_MASK=2, ALT_MASK=4 };

using Mouse = base::Mouse;

struct InputState {
	const int keyMask;
	const bool overGUI;
	bool consumedMouseWheel;
	bool consumedMouseDown;
};

// Generic plugin interface
class EditorPlugin {
	public:
	virtual ~EditorPlugin() {}
	virtual void setup(gui::Widget* toolPanel) {}
	virtual void update(const base::Mouse&, const Ray&, base::Camera*, InputState& state) {}
	virtual void setContext(const TerrainMap*) {}
	virtual base::XMLElement save(const TerrainMap* context) const = 0;
	virtual void load(const base::XMLElement&, const TerrainMap* context) = 0;
	virtual void clear() {}
	virtual void close() {}
};




