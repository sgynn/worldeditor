#ifndef _TOOL_BUTTON_
#define _TOOL_BUTTON_

#include "gui/widgets.h"

/** Tool button - only one selected */
class ToolButton : public gui::Button {
	WIDGET_TYPE(ToolButton);
	ToolButton(const Rect& r, gui::Skin* s) : Button(r, s) {}
	void setSelected(bool s) {
		Button::setSelected(s);
		// Deselect other options
		if(isSelected()) {
			for(int i=0, j=m_parent->getWidgetCount(); i<j; ++i) {
				ToolButton* b = m_parent->getWidget(i)->cast<ToolButton>();
				if(b && b!=this) b->setSelected(false);
			}
		}
	}
	void onMouseButton(const Point& p, int d, int u, int w) {
		setSelected(true);
		Button::onMouseButton(p,d,u,w);
	}
};

#endif

