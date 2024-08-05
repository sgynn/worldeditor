#ifndef _COLOUR_PICKER_
#define _COLOUR_PICKER_

#include <base/gui/widgets.h>
#include <base/math.h>
#include <base/colour.h>

class ColourPicker : public gui::Window {
	WIDGET_TYPE(ColourPicker);
	ColourPicker();
	~ColourPicker();
	void initialise(const gui::Root*, const gui::PropertyMap&);

	void setColour(const Colour& c);
	const Colour& getColour() const;

	public:
	Delegate<void(const Colour&)> eventChanged;
	Delegate<void(const Colour&)> eventSubmit;
	Delegate<void(const Colour&)> eventCancel;

	private:
	void pressedOK(gui::Button*);
	void pressedCancel(gui::Button*);
	void changeSwatch(gui::Widget*, const Point&, int);
	void changeGradient(gui::Widget*, const Point&, int);
	void changeHex(gui::Textbox*);
	void changeHSV(gui::Spinbox*, int);
	void changeRGB(gui::Spinbox*, int);
	Colour m_colour;
	Colour m_lastColour;

	static int    createSwatchImage(const gui::Root*, int size);
	static int    createGradientImage(const gui::Root*);
	static vec3   getHSV(const Colour& rgb);
	static Colour getRGB(const vec3& hsv);

	private:
	bool m_freeze;
	gui::Spinbox* m_r;
	gui::Spinbox* m_g;
	gui::Spinbox* m_b;
	gui::Spinbox* m_h;
	gui::Spinbox* m_s;
	gui::Spinbox* m_v;
	gui::Textbox* m_hex;
	gui::Widget* m_current;
	gui::Widget* m_previous;
	gui::Image* m_swatch;
	gui::Image* m_gradient;
};

#endif

