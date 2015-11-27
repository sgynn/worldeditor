#include "render.h"
#include <base/opengl.h>
#include <base/shader.h>
#include <base/camera.h>
#include <cstdio>

//// Render State ////
RenderInfo::RenderInfo(base::Camera* cam, uint flags) : m_flags(flags), m_state(0), m_material(0), m_camera(cam) {}
RenderInfo::~RenderInfo() {
	state(0);
}
void RenderInfo::state(uint code) {
	uint c = code ^ m_state;
	// Client state
	if(c&1) code&1? glEnableClientState(GL_VERTEX_ARRAY): glDisableClientState(GL_VERTEX_ARRAY);
	if(c&2) code&2? glEnableClientState(GL_NORMAL_ARRAY): glDisableClientState(GL_NORMAL_ARRAY);
	if(c&4) code&4? glEnableClientState(GL_TEXTURE_COORD_ARRAY): glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	if(c&8) code&8? glEnableClientState(GL_COLOR_ARRAY): glDisableClientState(GL_COLOR_ARRAY);

	// culling
	if(c&(DOUBLE_SIDED|INVERT_FACES)) {
		if(code&DOUBLE_SIDED) glDisable(GL_CULL_FACE_MODE);
		else glEnable(GL_CULL_FACE);
		glCullFace(code&INVERT_FACES? GL_FRONT: GL_BACK);
	}

	m_state = code;
}
void RenderInfo::material(Material* m) {
	if(m && m_material != m) m->bind();
	if(!m) {
		base::Shader::Null.bind();
		glDisable(GL_TEXTURE_2D);
		glDisable(GL_LIGHTING);
	} else if(!m_material) {
		glEnable(GL_TEXTURE_2D);
		glEnable(GL_LIGHTING);
	}
	m_material = m;
}

//// Drawable functions ////
void DMesh::draw( RenderInfo& r) {
	glPushMatrix();
	glMultMatrixf(m_transform);
	// Material
	r.material( Drawable::m_material );
	r.state( 1 + (getNormalPointer()? 2:0) + (getTexCoordPointer()?4:0) );
	glVertexPointer(3, GL_FLOAT, getStride(), getVertexPointer());
	glNormalPointer(   GL_FLOAT, getStride(), getNormalPointer());
	glTexCoordPointer(2, GL_FLOAT, getStride(), getTexCoordPointer());
	// Tangents?
	if(getTangentPointer() && base::Shader::current().ready()) {
		base::Shader::current().AttributePointer("tangent", 3, GL_FLOAT, 0, getStride(), getTangentPointer());
	}
	// Draw it
	if(hasIndices()) glDrawElements( getMode(), getSize(), GL_UNSIGNED_SHORT, getIndexPointer());
	else glDrawArrays( getMode(), 0, getSize());

	glPopMatrix();
}
void DMesh::updateBounds() {
	// calculate aabb from mesh obb
	const float* b = getBounds().min;
	// Transform 8 points to get min/max
	BoundingBox& box = Drawable::m_bounds;
	box.setInvalid();
	box.include( m_transform * vec3(b[0], b[1], b[2]) );
	box.include( m_transform * vec3(b[0], b[1], b[5]) );
	box.include( m_transform * vec3(b[0], b[4], b[2]) );
	box.include( m_transform * vec3(b[0], b[4], b[5]) );
	box.include( m_transform * vec3(b[1], b[1], b[2]) );
	box.include( m_transform * vec3(b[1], b[1], b[5]) );
	box.include( m_transform * vec3(b[1], b[4], b[2]) );
	box.include( m_transform * vec3(b[1], b[4], b[5]) );
}

//// Renderer object ////


Render::Render() {
}
Render::~Render() {
}


void Render::add(Drawable* d) {
	m_list.push_back( d );
}

void Render::remove(Drawable* d) {
	for(std::vector<Drawable*>::iterator i=m_list.begin(); i!=m_list.end(); ++i) {
		if(*i == d) {
			m_list.erase(i);
			return;
		}
	}
}

void Render::collect(base::Camera* cam) {
	m_camera = cam;
	m_collect = m_list; // Draw everything for now

	cam->updateFrustum();
	vec3 lp = cam->getPosition();
	float lpv[4] = { lp.x, lp.y, lp.z, 1.0 };
	glLightfv(GL_LIGHT0, GL_POSITION, lpv);
}


void Render::render(int flags) {
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);

	RenderInfo info(m_camera);
	for(std::vector<Drawable*>::iterator i=m_collect.begin(); i!=m_collect.end(); ++i) {
		(*i)->draw(info);
	}

	glDisable(GL_LIGHTING);
	base::Shader::Null.bind(); // unbind shader
}

