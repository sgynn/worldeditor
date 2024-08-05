#pragma once

#include <base/gui/gui.h>
#include <base/gui/renderer.h>
#include "base/camera.h"
#include <cstdio>

class BoxSelect : public gui::Widget {
	public:
	BoxSelect(int colour=0xffffff) {
		setColour(colour);
		setTangible(gui::Tangible::NONE);
		setVisible(false);
	}
	void draw() const override {
		if(!isVisible()) return;
		m_root->getRenderer()->drawRect(Rect(m_rect.x, m_rect.y, 1, m_rect.height), m_colour);
		m_root->getRenderer()->drawRect(Rect(m_rect.x, m_rect.y, m_rect.width, 1), m_colour);
		m_root->getRenderer()->drawRect(Rect(m_rect.right()-1, m_rect.y, 1, m_rect.height), m_colour);
		m_root->getRenderer()->drawRect(Rect(m_rect.x, m_rect.bottom(), m_rect.width, 1), m_colour);
	}

	void clear() {
		m_rect.set(0,0,0,0);
		setVisible(false);
	}

	void start() {
		m_start = m_root->getMousePos();
		setVisible(false);
		setMouseFocus();
	}

	bool isValid() const {
		return m_rect.width>2 && m_rect.height>2;
	}

	void onMouseMove(const Point&, const Point& mouse, int) override {
		m_rect.x = mouse.x<m_start.x? mouse.x: m_start.x;
		m_rect.y = mouse.y<m_start.y? mouse.y: m_start.y;
		m_rect.width = abs(mouse.x - m_start.x);
		m_rect.height = abs(mouse.y - m_start.y);
		setVisible(isValid());
	}
	void onMouseButton(const Point&, int dn, int up) override {
		if(up) setVisible(false);
	}

	void updatePlanes(base::Camera* camera) {
		const Point& view = getParent()->getSize();
		const vec3 cp = camera->getPosition();
		vec3 p1 = camera->unproject(vec3(m_rect.x,       view.y-m_rect.y, 1), view) - cp;
		vec3 p2 = camera->unproject(vec3(m_rect.right(), view.y-m_rect.y, 1), view) - cp;
		vec3 p3 = camera->unproject(vec3(m_rect.right(), view.y-m_rect.bottom(), 1), view) - cp;
		vec3 p4 = camera->unproject(vec3(m_rect.x,       view.y-m_rect.bottom(), 1), view) - cp;
		m_planes[0].normal = p1.cross(p2).normalise();
		m_planes[1].normal = p2.cross(p3).normalise();
		m_planes[2].normal = p3.cross(p4).normalise();
		m_planes[3].normal = p4.cross(p1).normalise();
		for(auto& p: m_planes) p.d = p.normal.dot(cp);
	}

	bool inside(const vec3& point, float radius=0) const {
		for(const auto& p: m_planes) if(p.normal.dot(point) - p.d < -radius) return false;
		return true;
	}

	// Inaccurate - FIXME
	bool inside(const BoundingBox& box) const {
		for(int i=0; i<8; ++i) {
			if(inside(box.getCorner(i))) return true;
		}
		return false;
	}

	private:
	Point m_start;
	bool m_changed;
	struct { vec3 normal; float d; } m_planes[4];
};

