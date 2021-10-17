#include "erosion.h"
#include "heightmap.h"
#include "gui/widgets.h"
#include <base/input.h>
#include <cstdlib>

using namespace gui;
using namespace base;

extern String appPath;


ErosionEditor::ErosionEditor(gui::Root* gui, FileSystem*, MapGrid* terrain, scene::SceneNode*) {
	m_terrain = terrain;
	createPanel(gui, "erosioneditor", "erosion.xml");
	createToolButton(gui, "noise");

	m_panel->getWidget<Button>("run")->eventPressed.bind(this, &ErosionEditor::execute);
	m_panel->getWidget<Button>("undo")->eventPressed.bind(this, &ErosionEditor::undo);
}

ErosionEditor::~ErosionEditor() {
	delete [] m_data;
	delete [] m_backup;
}

void ErosionEditor::update(const Mouse& mouse, const Ray& ray, Camera*, InputState& state) {
	// Update progressbar and block other input when running
	if(mouse.pressed) {
		float hit;
		if(m_terrain->castRay(ray.start, ray.direction, hit)) setContext(m_terrain->getMap(ray.point(hit)));
	}
	
	if(m_progress > 0) {
		m_panel->getWidget<ProgressBar>("progress")->setValue(m_progress);
		state.consumedMouseDown = true;
		if(m_progress >= m_particles) {
			bool done = true;
			for(Thread& t:m_threads) if(t.running()) done = false;
			if(done) finished(m_panel->isVisible());
		}
	}

}
void ErosionEditor::setContext(const TerrainMap* map) {
	if(m_particles > 0) return;
	if(m_context != map) {
		m_size = 0;
		m_panel->getWidget("undo")->setEnabled(false);
	}
	m_context = map;
	m_panel->getWidget("run")->setEnabled(map);
}

void ErosionEditor::activate() {
	if(!m_context) setContext(m_terrain->getMap(Point(0,0)));
}

void ErosionEditor::close() {
	m_progress = m_particles; // cancel generation
	m_size = 0;
}

void ErosionEditor::execute(Button*) {
	if(!m_context) return;

	// Stop generation
	if(m_particles > 0) {
		m_progress = m_particles;
		return;
	}

	auto getValue = [this](const char* name) { Scrollbar* s = m_panel->getWidget<Scrollbar>(name); return s?s->getValue()/1000.f:0.f; };
	auto getInteger = [this](const char* name) { Spinbox* s = m_panel->getWidget<Spinbox>(name); return s?s->getValue():0; };
	m_radius = getValue("radius");
	m_evaporation = getValue("evaporation");
	m_erosion = getValue("erosion");
	m_deposition = getValue("deposition");
	m_capacity = getValue("capacity");
	m_minSlope = getValue("minslope");
	m_inertia = getValue("inertia");
	m_gravity = getValue("gravity");

	m_particles = getInteger("particles");
	m_limit = getInteger("iterations");
	m_progress = 0;

	if(m_size != m_context->heightMap->getDataSize()) {
		m_size = m_context->heightMap->getDataSize();
		delete [] m_data;
		delete [] m_backup;
		m_data = new float[m_size];
		m_backup = new float[m_size];
		m_context->heightMap->getData(m_backup);
	}
	m_context->heightMap->getData(m_data);

	m_panel->getWidget("undo")->setEnabled(false);
	m_panel->getWidget<Button>("run")->setCaption("Stop");

	ProgressBar* progress = m_panel->getWidget<ProgressBar>("progress");
	progress->setRange(0, m_particles);
	progress->setValue(m_progress);
	
	m_threads.resize(8);
	for(size_t i=0; i<m_threads.size(); ++i) {
		m_threads[i].begin(this, &ErosionEditor::runThread);
	}
}

void ErosionEditor::undo(Button* b) {
	m_context->heightMap->setData(m_backup);
	b->setEnabled(false);
}

// ------------------------------------------------------------------------ //

void ErosionEditor::runThread() {
	while(m_progress < m_particles) {
		simulateDrop(m_limit);
		++m_progress;
	}
}

void ErosionEditor::finished(bool completed) {
	if(completed) m_context->heightMap->setData(m_data);
	m_panel->getWidget("undo")->setEnabled(true);
	m_panel->getWidget<Button>("run")->setCaption("Run");
	m_progress = m_particles = 0;
}


inline bool ErosionEditor::getData(const vec2& p, float& height, vec2& slope) const {
	int ix = floor(p.x);
	int iy = floor(p.y);
	if(ix<0 || iy<0 || ix>=m_context->size || iy>=m_context->size) return false;
	float fx = p.x - ix;
	float fy = p.y - iy;
	float h00 = m_data[ix + iy * m_context->size];
	float h10 = m_data[ix+1 + iy * m_context->size];
	float h01 = m_data[ix + (iy + 1) * m_context->size];
	float h11 = m_data[ix+1 + (iy+1) * m_context->size];
	slope.x = (h10 - h00) * (1-fy) + (h11 - h01) * fy;
	slope.y = (h01 - h00) * (1-fx) + (h11 - h10) * fx;

	float ha = h00 * (1-fx) + h10 * fx;
	float hb = h01 * (1-fx) + h11 * fx;
	height = ha * (1-fy) + hb * fy;
	return true;
}

void ErosionEditor::modHeight(const vec2& p, float amount, float radius) {
	if(amount==0) return;
	int x0 = ceil(p.x - radius);
	int y0 = ceil(p.y - radius);
	int x1 = ceil(p.x + radius);
	int y1 = ceil(p.y + radius);
	float total = 0;
	for(int x=x0; x<x1; ++x) for(int y=y0; y<y1; ++y) {
		vec2 rel(p.x-x, p.y-y);
		float w = fmax(0, radius - rel.length());
		total += w;
	}
	const int s = m_context->size;
	if(x0<0) x0=0;
	if(y0<0) y0=0;
	if(x1>s) x1=s;
	if(y1>s) y1=s;
	for(int x=x0; x<x1; ++x) for(int y=y0; y<y1; ++y) {
		vec2 rel(p.x-x, p.y-y);
		float w = fmax(0, radius - rel.length());
		m_data[x+y*s] += amount * w / total;
	}
}

void ErosionEditor::simulateDrop(int limit) {
	vec2 pos;
	float sediment = 0;
	float water = 1;
	float speed = 0;
	pos.x = (float)rand() / RAND_MAX * m_context->size;
	pos.y = (float)rand() / RAND_MAX * m_context->size;

	vec2 slope;
	float height, lastHeight;
	vec2 direction = slope.normalised();
	getData(pos, lastHeight, slope);

	while(water>0 || sediment>1e-6) {
		// Update direction
		direction  = direction * m_inertia - slope * (1-m_inertia);
		if(direction.length2() < 0.001) {
			float angle = rand() * TWOPI / RAND_MAX;
			direction.x = sin(angle);
			direction.y = cos(angle);
		}
		else direction.normalise();
		vec2 lastPos = pos;
		pos += direction;

		// get slope at new point
		if(!getData(pos, height, slope)) break;
		
		float diff = height - lastHeight;
		float capacity = fmax(-diff, m_minSlope) * speed * water * m_capacity;
		speed = fmax(0, speed - diff * m_gravity); // sqrt(speed*speed + diff * m_gravity);
		water *= 1 - m_evaporation;
		lastHeight = height;
		
		if(--limit<=0) water = 0;
		if(sediment >= capacity) {
			float amount = (sediment - capacity) * m_deposition;
			sediment -= amount;
			modHeight(lastPos, amount, m_radius);
		}
		else {
			float amount = fmin((sediment - capacity) * m_erosion, -diff);
			sediment -= amount;
			modHeight(lastPos, amount, m_radius);
		}
	}
}




