#pragma once

#include "editorplugin.h"

namespace gui { class Root; class Button; }
namespace scene { class SceneNode; }

class ErosionEditor : public EditorPlugin {
	public:
	ErosionEditor(gui::Root* gui, FileSystem*, MapGrid* terrain, scene::SceneNode* scene);
	~ErosionEditor();
	void setup(gui::Widget* toolPanel) override;
	void update(const base::Mouse&, const Ray&, base::Camera*, InputState& state) override;
	void setContext(const TerrainMap*) override;
	void close() override;

	private:
	void toggleEditor(gui::Button*);
	void execute(gui::Button*);
	void undo(gui::Button*);
	bool advance(int max);
	void simulateDrop(int limit);
	bool getData(const vec2& p, float& height, vec2& slope) const;
	void modHeight(const vec2& p, float amount, float radius);

	private:
	gui::Widget* m_panel;
	gui::Button* m_toolButton;
	MapGrid* m_terrain;
	const TerrainMap* m_context;
	float m_radius; // droplet radius
	float m_evaporation;
	float m_erosion;
	float m_deposition;
	float m_capacity;
	float m_minSlope;
	float m_inertia;
	float m_gravity;
	float* m_data = 0;
	float* m_backup = 0;
	size_t m_size = 0;
	int m_particles=0;
	int m_progress=0;
	int m_limit=0;
};

