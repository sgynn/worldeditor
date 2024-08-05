#include "colourpicker.h"
#include <base/gui/skin.h>
#include <base/gui/renderer.h>

using namespace gui;

ColourPicker::ColourPicker() : m_freeze(false) {}
ColourPicker::~ColourPicker() {
}

void ColourPicker::initialise(const Root* root, const PropertyMap& p) {
	Window::initialise(root,p);

	m_r = getWidget<Spinbox>("r");
	m_g = getWidget<Spinbox>("g");
	m_b = getWidget<Spinbox>("b");

	m_h = getWidget<Spinbox>("h");
	m_s = getWidget<Spinbox>("s");
	m_v = getWidget<Spinbox>("v");

	m_hex = getWidget<Textbox>("hex");
	m_current = getWidget("colour");
	m_previous = getWidget("previous");

	if(m_r) m_r->eventChanged.bind(this, &ColourPicker::changeRGB);
	if(m_g) m_g->eventChanged.bind(this, &ColourPicker::changeRGB);
	if(m_b) m_b->eventChanged.bind(this, &ColourPicker::changeRGB);
	if(m_h) m_h->eventChanged.bind(this, &ColourPicker::changeHSV);
	if(m_s) m_s->eventChanged.bind(this, &ColourPicker::changeHSV);
	if(m_v) m_v->eventChanged.bind(this, &ColourPicker::changeHSV);
	if(m_hex) m_hex->eventSubmit.bind(this, &ColourPicker::changeHex);

	m_swatch = getWidget<Image>("swatch");
	if(m_swatch) {
		m_swatch->eventMouseMove.bind(this, &ColourPicker::changeSwatch);
		m_swatch->eventMouseDown.bind(this, &ColourPicker::changeSwatch);
	}

	m_gradient = getWidget<Image>("lum");
	if(m_gradient) {
		m_gradient->eventMouseMove.bind(this, &ColourPicker::changeGradient);
		m_gradient->eventMouseDown.bind(this, &ColourPicker::changeGradient);
	}

	// Buttons
	Button* ok = getWidget<Button>("ok");
	Button* cancel = getWidget<Button>("cancel");
	if(ok) ok->eventPressed.bind(this, &ColourPicker::pressedOK);
	if(cancel) cancel->eventPressed.bind(this, &ColourPicker::pressedCancel);

	// Images
	if(root && m_swatch) {
		int img = root->getRenderer()->getImage("colourpicker");
		if(img < 0) img = createSwatchImage(root, 100);
		m_swatch->setImage(img);

	}
	if(root && m_gradient) {
		int img = root->getRenderer()->getImage("colourgradient");
		if(img < 0) img = createGradientImage(root);
		m_gradient->setImage(img);
	}

	setColour(white);
}

int ColourPicker::createSwatchImage(const Root* root, int s) {
	char* data = new char[s * s * 4];

	// Swatch
	float fs = s;
	for(int x=0; x<s; ++x) for(int y=0; y<s; ++y) {
		vec3 hsv(x/fs, (s-y)/fs, 1.f);
		Colour c = getRGB(hsv);
		int bgr = 0xff000000 | int(c.r*255) | (int(c.g*255)<<8) | (int(c.b*255)<<16);
		memcpy(data + (x + y * s) * 4, &bgr, 4);
	}
	int tex = root->getRenderer()->addImage("colourpicker", s, s, 4, data, true);
	delete [] data;
	return tex;
}

int ColourPicker::createGradientImage(const Root* root) {
	unsigned char* data = new unsigned char[32*4];
	for(int i=0; i<32; ++i) data[i*4]=data[i*4+1]=data[i*4+2] = (unsigned char)((1-i/31.f) * 255), data[i*4+3]=255;
	int tex = root->getRenderer()->addImage("colourgradient", 1, 32, 4, data, true);
	return tex;
}

void ColourPicker::setColour(const Colour& c) {
	m_colour = c;
	m_freeze = true;
	if(m_r) m_r->setValue(c.r*255+0.5);
	if(m_g) m_g->setValue(c.g*255+0.5);
	if(m_b) m_b->setValue(c.b*255+0.5);
	
	vec3 hsv = getHSV(c);
	if(m_h) m_h->setValue(hsv.x*360+0.5);
	if(m_s) m_s->setValue(hsv.y*100+0.5);
	if(m_v) m_v->setValue(hsv.z*100+0.5);

	if(m_hex) {
		char hex[8];
		sprintf(hex, "#%06X", c.toRGB());
		m_hex->setText(hex);
	}
	if(m_current) m_current->setColour(c);
	if(!isVisible() && m_previous) m_previous->setColour(c);
	if(!isVisible()) m_lastColour = c;

	if(m_swatch) m_swatch->setColour( Colour(hsv.z, hsv.z, hsv.z) );
	if(m_gradient) m_gradient->setColour( getRGB( vec3(hsv.x, hsv.y, 1) ) );

	m_freeze = false;
	if(eventChanged) eventChanged(c);
}
const Colour& ColourPicker::getColour() const {
	return m_colour;
}

void ColourPicker::changeSwatch(gui::Widget* w, const Point& p, int b) {
	if(b==1) {
		vec3 hsv = getHSV(m_colour);
		hsv.x = p.x / (float)w->getSize().x;
		hsv.y = 1 - p.y / (float)w->getSize().y;

		hsv.x = hsv.x>0.999? 0.999: hsv.x<0? 0: hsv.x;
		hsv.y = hsv.y>1? 1: hsv.y<0? 0: hsv.y;
		setColour( getRGB(hsv) );
	}
}
void ColourPicker::changeGradient(gui::Widget* w, const Point& p, int b) {
	if(b==1) {
		vec3 hsv = getHSV(m_colour);
		hsv.z = 1 - p.y / (float)w->getSize().y;
		hsv.z = hsv.z>1? 1: hsv.z<0? 0: hsv.z;
		setColour( getRGB(hsv) );
	}
}

void ColourPicker::changeRGB(Spinbox*, int) {
	if(!m_r || !m_g || !m_b || m_freeze) return;
	setColour( Colour(m_r->getValue()/255.f, m_g->getValue()/255.f, m_b->getValue()/255.f) );
}
void ColourPicker::changeHSV(Spinbox*, int) {
	if(!m_h || !m_s || !m_v || m_freeze) return;
	vec3 hsv(m_h->getValue()/360.f, m_s->getValue()/100.f, m_v->getValue()/100.f);
	setColour( getRGB(hsv) );
}
void ColourPicker::changeHex(Textbox* t) {
	const char* s = t->getText();
	if(s[0]=='#') ++s;
	char* e;
	int c = strtoll(s, &e, 16);
	if(e>s) setColour( Colour(c) );
}


void ColourPicker::pressedOK(Button*) {
	setVisible(false);
	if(eventSubmit) eventSubmit(m_colour);
}
void ColourPicker::pressedCancel(Button*) {
	setColour(m_lastColour);
	setVisible(false);
	if(eventCancel) eventCancel(m_lastColour);
}


// These should be in base::Colour
Colour ColourPicker::getRGB(const vec3& hsv) {
	if(hsv.y > 0) {
		float h = hsv.x * 6.0;
		float i = floor(h);
		float v1 = hsv.z * (1.0-hsv.y);
		float v2 = hsv.z * (1.0-hsv.y * (h-i));
		float v3 = hsv.z * (1.0-hsv.y * (1.0-(h-i)));
		if	(i==0) return Colour(hsv.z, v3,    v1);
		else if (i==1) return Colour(v2,    hsv.z, v1);
		else if (i==2) return Colour(v1,    hsv.z, v3);
		else if (i==3) return Colour(v1,    v2,    hsv.z);
		else if (i==4) return Colour(v3,    v1,    hsv.z);
		else	       return Colour(hsv.z, v1,    v2);
	} else return Colour(hsv.z, hsv.z, hsv.z);
}
vec3 ColourPicker::getHSV(const Colour& c) {
	vec3 hsv;
	float min = c.r<c.b? (c.r<c.g? c.r:c.g): c.b<c.g? c.b: c.g;
	float max = c.r>c.b? (c.r>c.g? c.r:c.g): c.b>c.g? c.b: c.g;
	float delta = max - min;
	hsv.z = max;
	if(delta>0) {
		hsv.y = delta / max;
		if	(c.r == max) hsv.x = (c.g - c.b) / delta;
		else if (c.g == max) hsv.x = 2 + (c.b - c.r) / delta;
		else if (c.b == max) hsv.x = 4 + (c.r - c.g) / delta;
		hsv.x /= 6.0;
		if(hsv.x<0)  hsv.x++;
		if(hsv.x>=1) hsv.x--;
	}
	return hsv;
}
