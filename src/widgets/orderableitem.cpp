#include "orderableitem.h"

OrderableItem::OrderableItem(const Rect& r, gui::Skin* s) : Widget(r, s), m_held(-1), m_targetIndex(-1) {
}

void OrderableItem::setSize(int w, int h) {
	Widget::setSize(w, h);
	updateParentLayout();
}

int OrderableItem::getIndex() const {
	if(!m_parent) return 0;
	for(int i=0; i<m_parent->getWidgetCount(); ++i) {
		if(m_parent->getWidget(i) == this) return i;
	}
	return 0; // error
}

void OrderableItem::setIndex(uint index) {
	if(!m_parent) return;
	if(m_parent->getWidget(index) == this) return; // already there
	raise();
	// Not the fastest method as we dont have direct access to m_parent->m_children, but it should work
	for(int i=(int)index+1; i<m_parent->getWidgetCount(); ++i) {
		m_parent->getWidget(index)->raise();
	}
	updateParentLayout();
}
void OrderableItem::moveUp() {
	int i = getIndex();
	if(i > 0) setIndex(i-1);
}
void OrderableItem::moveDown() {
	int i = getIndex();
	setIndex(i+1);
}

// -------------------------------- //

void OrderableItem::onMouseButton(const Point& p, int d, int u) {
	if(d==1) {
		m_held = p.y - m_rect.y;
		m_startIndex = m_targetIndex = getIndex();
		raise();
	}
	if(u&1 && m_held>=0) {
		m_held = -1;
		setIndex(m_targetIndex);
		updateParentLayout();
		if(eventReordered) eventReordered(m_startIndex, m_targetIndex);
	}
	Widget::onMouseButton(p, d, u);
}
void OrderableItem::onMouseMove(const Point& lp, const Point& cp, int b) {
	if(b==1 && m_held>0) {
		// Set position
		const Rect& client = m_parent->getAbsoluteClientRect();
		int py = cp.y - m_held;
		if(py < client.y) py = client.y;
		if(py + m_rect.height > client.bottom()) py = client.bottom() - m_rect.height;
		setPosition(0, py - m_parent->getAbsoluteClientRect().y);

		// Shift other widgets
		for(int i=0; i<m_parent->getWidgetCount(); ++i) {
			Widget* w = m_parent->getWidget(i);
			if(w!=this) {
				int ix = i;
				if(ix >= m_targetIndex) ++ix;
				int ctr = w->getAbsolutePosition().y + w->getSize().y / 2;
				// up
				if(m_rect.y < ctr && ix < m_targetIndex) {
					w->setPosition(0, w->getPosition().y + m_rect.height);
					--m_targetIndex;
				}
				// Down
				if(m_rect.bottom() > ctr && ix > m_targetIndex) {
					w->setPosition(0, w->getPosition().y - m_rect.height);
					++m_targetIndex;
				}
			}
		}
	}
	Widget::onMouseMove(lp, cp, b);
}

void OrderableItem::updateParentLayout() {
	if(!m_parent) return;
	int y = 0;
	for(int i=0; i<m_parent->getWidgetCount(); ++i) {
		Widget* w = m_parent->getWidget(i);
		w->setPosition(0, y);
		y += w->getSize().y;
	}
}


