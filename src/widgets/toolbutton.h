#ifndef _TOOL_BUTTON_
#define _TOOL_BUTTON_

#include <base/gui/widgets.h>

/** Tool button - only one selected */
class ToolButton : public gui::Button {
	WIDGET_TYPE(ToolButton);
	void setSelected(bool s) override {
		Button::setSelected(s);
		// Deselect other options
		if(isSelected()) {
			for(int i=0, j=m_parent->getWidgetCount(); i<j; ++i) {
				ToolButton* b = cast<ToolButton>(m_parent->getWidget(i));
				if(b && b!=this) b->setSelected(false);
			}
		}
	}
	void onMouseButton(const Point& p, int d, int u) override {
		setSelected(true);
		Button::onMouseButton(p,d,u);
	}
};

#endif

