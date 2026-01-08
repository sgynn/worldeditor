#include "objectspline.h"

#include <base/random.h>
#include <base/debuggeometry.h>
#include <base/gui/lists.h>
#include <base/collision.h>
#include <base/input.h>

using namespace base;
using namespace gui;


template<class V>
V bezierPoint(const V* points, float t) {
	static const int factorial[] = { 1,1,2,6,24,120 };
	V result;
	for(int k=0; k<4; ++k) {
		float kt = t>0||k>0? powf(t,k): 1;
		float rt = t<1||k<3? powf(1-t,3-k): 1;
		float ni = factorial[3] / (factorial[k] * factorial[3-k]);
		float bias = ni * kt * rt;
		result += points[k] * bias;
	}
	return result;
}

void ObjectSplineEditor::setup(ObjectEditor* parent, Widget* panel) {
	ObjectGroupEditor::setup(parent, panel);

	#define CONNECT(Type, name, event, ...) { Type* t=m_panel->getWidget<Type>(name); \
		if(t) t->event.bind(__VA_ARGS__); \
		else printf("Error: Missing widget %s\n", name); }

	CONNECT(Textbox, "name", eventSubmit, [this](Textbox* t) {
		m_data->name = t->getText();
		m_parent->notifyTemplateRenamed(m_data);
	});
	CONNECT(Combobox, "mode", eventSelected, [this](Combobox*, ListItem& i) {
		m_data->mode = (ObjectSplineData::Mode)i.getIndex();
		m_parent->notifyTemplateChanged(m_data);
	});
	CONNECT(SpinboxFloat, "slope", eventChanged, [this](SpinboxFloat*, float value) {
		m_data->maxSlope = value;
		m_parent->notifyTemplateChanged(m_data);
	});
	CONNECT(SpinboxFloat, "spacing", eventChanged, [this](SpinboxFloat*, float value) {
		m_data->separation = value;
		m_parent->notifyTemplateChanged(m_data);
	});
}

void ObjectSplineEditor::showPanel(ObjectGroupData* group) {
	if(!m_panel) return;
	if(!group || group->editor != this) {
		m_panel->setVisible(false);
		return;
	}

	m_data = (ObjectSplineData*) group;
	const ObjectSplineData& data = *m_data;
	m_panel = m_panel->getRoot()->getWidget("objectspline");
	m_panel->getWidget<Textbox>("name")->setText(data.name);
	m_panel->getWidget<Combobox>("mode")->selectItem(data.mode);
	m_panel->getWidget<SpinboxFloat>("slope")->setValue(data.maxSlope);
	m_panel->getWidget<SpinboxFloat>("spacing")->setValue(data.separation);
	Listbox* seg = m_panel->getWidget<Listbox>("segments");
	Listbox* nod = m_panel->getWidget<Listbox>("nodes");
	seg->clearItems();
	nod->clearItems();
	for(const ObjectSplineData::Item& i: data.segments) seg->addItem(i.mesh.getName(), i.chance);
	for(const ObjectSplineData::Item& i: data.nodes) nod->addItem(i.mesh.getName(), i.chance);
	m_panel->setVisible(true);
}

bool ObjectSplineEditor::dropMesh(Widget* over, const MeshReference& mesh) {
	if(Listbox* list = cast<Listbox>(over)) {
		for(ListItem& i: list->items()) if(i.getData(2) == mesh) return true; // already there
		list->addItem(mesh.getName(), 100);
		std::vector<ObjectSplineData::Item>& meshList = list->getName() == "segments"? m_data->segments: m_data->nodes;
		meshList.push_back({mesh, 100, 3});
		m_parent->notifyTemplateChanged(m_data);
		return true;
	}
	return false;
}

bool ObjectSplineEditor::updateEditor(ObjectGroup* object, const Mouse& mouse, const Ray& ray, base::Camera* camera, InputState& state) {
	// Drag the handles
	if(mouse.pressed == 1 && !state.overGUI) {
		float closest = 4;
		vec3 closestPoint;
		Ray localRay = ray;
		auto overPoint = [&](const vec3& point) {
			vec3 r = base::closestPointOnLine(point, localRay.start, localRay.point(1000));
			float d = r.distance2(point);
			if(d<closest) {
				closest = d;
				closestPoint = point;
				return true;
			}
			return false;
		};
		auto overSpline = [&](const ObjectSpline::Node& a, const ObjectSpline::Node& b, float dist, float& t) {
			const vec3 control[4] = { a.point, a.point+a.direction*a.b, b.point-b.direction*b.a, b.point };
			vec3 last = a.point;
			float u,v;
			bool hit = false;
			for(int i=0; i<=16; ++i) {
				vec3 p = bezierPoint(control, i/16.f);
				float d = base::closestPointBetweenLines(localRay.start, localRay.point(1000), last, p, u,v);
				if(d<dist && v>=0 && v<=1) {
					dist = d;
					closestPoint = lerp(last, p, v);
					t = (i+v) / 16.f;
					hit = true;
				}
				last = p;
			}
			return hit;
		};

		ObjectSpline* spline = (ObjectSpline*)object;
		for(size_t i=0; i<spline->m_nodes.size(); ++i) {
			const ObjectSpline::Node& n = spline->m_nodes[i];
			if(overPoint(n.point)) m_heldObject = spline, m_heldPart = 1, m_heldIndex = i;
			if(!m_heldObject) {
				if(overPoint(n.point - n.direction*n.a)) m_heldObject=spline, m_heldPart=2, m_heldIndex=i;
				if(overPoint(n.point + n.direction*n.b)) m_heldObject=spline, m_heldPart=3, m_heldIndex=i;
			}
		}
		// Create new node
		if(!m_heldObject) {
			float split = -1;
			for(size_t i=1; i<spline->m_nodes.size(); ++i) {
				if(overSpline(spline->m_nodes[i-1], spline->m_nodes[i], 1, split)) {
					spline->m_nodes.insert(spline->m_nodes.begin()+i, spline->m_nodes[i]);
					m_heldObject = spline;
					m_heldIndex = i;
					m_heldPart = 1;
					break;
				}
			}
		}
	}
	else if(mouse.pressed == 4 && m_heldObject) {
		ObjectSpline* spline = (ObjectSpline*)object;
		if(spline->m_nodes.size() > 2) {
			spline->m_nodes.erase(spline->m_nodes.begin() + m_heldIndex);
			m_heldObject = nullptr;
			createObjects(spline);
		}
	}
	else if(mouse.button == 1 && m_heldObject) {
		if(m_heldPart == 1) { // Node
			float t;
			if(m_parent->trace(ray, t)) {
				m_heldObject->m_nodes[m_heldIndex].point = ray.point(t);
				createObjects(object);
			}
		}
		else {
			// planar
		}
	}
	else if(m_heldObject) {
		m_heldObject = nullptr;
		return true;
	}
	return m_heldObject;
}

void ObjectSplineEditor::updateLines(ObjectGroup* object, base::DebugGeometry& g) {
	ObjectSpline& spline = *(ObjectSpline*)object;
	// Spline
	for(size_t i=1;i<spline.m_nodes.size(); ++i) {
		const ObjectSpline::Node& a = spline.m_nodes[i-1];
		const ObjectSpline::Node& b = spline.m_nodes[i];
		vec3 in[4] = { a.point, a.point+a.direction*a.b, b.point-b.direction*b.a, b.point };
		vec3 last = a.point;
		for(int j=1; j<32; ++j) {
			vec3 next = bezierPoint(in, j/32.f);
			g.line(last, next, 0x000000);
			last = next;
		}
		g.line(last, b.point, 0x000000);
	}
	// Control lines
	for(const ObjectSpline::Node& n : spline.m_nodes) {
		g.circle(n.point, n.direction, 0.2, 18, 0xff8000);
		g.line(n.point - n.a * n.direction, n.point + n.b * n.direction, 0xff8000);
	}
}

ObjectGroupData* ObjectSplineEditor::createTemplate(std::vector<Object*>& m_selection) {
	return new ObjectSplineData { this, "New Spline" };
}

ObjectGroup* ObjectSplineEditor::createGroup(const ObjectGroupData* data, const vec3& position) const {
	ObjectSpline* spline = new ObjectSpline(data);
	spline->m_nodes.push_back({position - vec3(2,0,0), vec3(1,0,0), 1, 1});
	spline->m_nodes.push_back({position + vec3(2,0,0), vec3(1,0,0), 1, 1});
	createObjects(spline);
	return spline;
}

void ObjectSplineEditor::moveGroup(ObjectGroup* group, const vec3& position) const {
	vec3 centre;
	ObjectSpline* spline = (ObjectSpline*) group;
	for(ObjectSpline::Node& n: spline->m_nodes) centre += n.point;
	centre /= spline->m_nodes.size();
	for(ObjectSpline::Node& n: spline->m_nodes) n.point += position - centre;
	createObjects(spline);
}

void ObjectSplineEditor::placeGroup(ObjectGroup* group, const vec3& position) {
	moveGroup(group, position);
	m_heldObject = (ObjectSpline*)group;
	m_heldIndex = 1;
	m_heldPart = 1;
}

void ObjectSplineEditor::createObjects(ObjectGroup* object) const {
	ObjectSpline& spline = *(ObjectSpline*)object;
	ObjectSplineData& data = *(ObjectSplineData*)object->getData();
	if(data.segments.empty()) return;

	vec3 start = spline.m_nodes[0].point;
	float end = spline.m_nodes.size() - 1;
	RNG rng(rand());

	const float step = 0.01; 
	auto getPoint = [&](float t) {
		if(t <= 0) return spline.m_nodes[0].point;
		if(t >= end) return spline.m_nodes.back().point + spline.m_nodes.back().direction * (t-end);
		int index = floor(t);
		const ObjectSpline::Node& a = spline.m_nodes[index];
		const ObjectSpline::Node& b = spline.m_nodes[index+1];
		vec3 in[4] = { a.point, a.point+a.direction*a.b, b.point-b.direction*b.a, b.point };
		return bezierPoint(in, t - index);
	};
	auto projectPoint = [this](vec3& p) {
		p.y = m_parent->getTerrainHeight(p);
	};
	auto selectMesh = [&rng](std::vector<ObjectSplineData::Item>& list) {
		float accum = 0;
		const ObjectSplineData::Item* chosen = nullptr;
		for(const ObjectSplineData::Item& i: list) {
			if(i.chance <= 0) continue;
			if(rng.randf()*accum + i.chance >= accum) chosen = &i;
			accum += i.chance;
		}
		return chosen;
	};

	// ToDo: reuse objects ?
	for(Object* o: spline.objects) delete o;
	spline.objects.clear();

	float t = step;
	vec3 p = getPoint(t);
	const Quaternion fix(0.707107, 0, 0.707107, 0);
	if(data.mode == ObjectSplineData::PROJECTED) projectPoint(start);
	while(t < end) {
		const ObjectSplineData::Item* post = selectMesh(data.nodes);
		if(post) {
			vec3 n = getPoint(t+0.001);
			if(data.mode == ObjectSplineData::PROJECTED) projectPoint(p);
			Object* o = m_parent->createObject(post->mesh, object);
			o->setTransform(start, fix * Quaternion(n-start));
			o->updateBounds();
			spline.objects.push_back(o);
		}

		const ObjectSplineData::Item* model = selectMesh(data.segments);
		const float length2 = powf(model->length + data.separation, 2);
		while(p.distance2(start) < length2) {
			t += step;
			p = getPoint(t);
			if(data.mode == ObjectSplineData::PROJECTED) projectPoint(p);
		}
		// Tighten margin
		float substep = -step / 2;
		for(int i=0; i<12; ++i) {
			t += substep;
			p = getPoint(t);
			if(data.mode == ObjectSplineData::PROJECTED) projectPoint(p);
			if(p.distance2(start) < length2) substep = fabs(substep/2);
			else substep = -fabs(substep/2);
		}
		// Place object
		Object* o = m_parent->createObject(model->mesh, object);
		o->setTransform((start + p)/2, fix*Quaternion(p-start));
		o->updateBounds();
		spline.objects.push_back(o);
		start = p;
	}
}

// ======================================================== //

base::XMLElement ObjectSplineEditor::saveGroup(ObjectGroup*) const {
	return {};
}

void ObjectSplineEditor::loadGroup(const XMLElement&, ObjectGroup*) const {
}

base::XMLElement ObjectSplineEditor::saveTemplate(const ObjectGroupData*) const {
	return {};
}

ObjectGroupData* ObjectSplineEditor::loadTemplate(const XMLElement&) const {
	return nullptr;
}
