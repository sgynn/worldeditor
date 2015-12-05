#ifndef _ORDERABLE_ITEM_
#define _ORDERABLE_ITEM_

#include "gui/gui.h"

/** Reorderable items for textures and material layers */
class OrderableItem : public gui::Widget {
	WIDGET_TYPE(OrderableItem);
	OrderableItem(const Rect& r, gui::Skin* s);
	void setSize(int w, int h);
	public:
	int  getIndex() const;
	void setIndex(uint index);
	void moveUp();
	void moveDown();
	public:
	DelegateS<void()> eventReordered;
	protected:
	void onMouseButton(const Point&, int, int, int);
	void onMouseMove(const Point&, const Point&, int);
	void updateParentLayout();
	int m_held;
	int m_startIndex;
	int m_targetIndex;
};


#endif

