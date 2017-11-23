#ifndef RENDERER_H
#define RENDERER_H

#include <set>
#include <string>
#include "common/Singleton.h"

#define CM_VEL 0
#define CM_PRESS 1
#define CM_PERS 2
#define CM_FIRE 3
#define CM_BLOB 4
#define CM_HEAT 5
#define CM_FANCY 6
#define CM_NOTHING 7
#define CM_GRAD 8
#define CM_CRACK 9
#define CM_LIFE 10
#define CM_COUNT 11

class Save;

struct RenderPreset
{
	std::set<unsigned int> renderModes;
	std::set<unsigned int> displayModes;
	unsigned int colorMode;
	std::string tooltip;
};

// This class is mostly unused at the moment, but is used for controlling render / display modes
class Renderer : public Singleton<Renderer>
{
	std::set<unsigned int> renderModes;
	std::set<unsigned int> displayModes;
	unsigned int colorMode;

	RenderPreset renderPresets[11];

	void InitRenderPresets();

public:
	Renderer();

	bool LoadRenderPreset(int preset);
	std::string GetRenderPresetToolTip(int preset);

	// render modes
	bool HasRenderMode(unsigned int renderMode);
	void AddRenderMode(unsigned int renderMode);
	void RemoveRenderMode(unsigned int renderMode);
	void ToggleRenderMode(unsigned int renderMode);
	void ClearRenderModes();

	std::set<unsigned int> GetRenderModes();
	unsigned int GetRenderModesRaw();
	void SetRenderModes(std::set<unsigned int> newRenderModes);

	// display modes
	bool HasDisplayMode(unsigned int displayMode);
	void AddDisplayMode(unsigned int displayMode);
	void RemoveDisplayMode(unsigned int displayMode);
	void ToggleDisplayMode(unsigned int displayMode);
	void ClearDisplayModes();

	std::set<unsigned int> GetDisplayModes();
	unsigned int GetDisplayModesRaw();
	void SetDisplayModes(std::set<unsigned int> newDisplayModes);

	// color modes
	void SetColorMode(unsigned int color_mode);
	void XORColorMode(unsigned int color_mode);
	unsigned int GetColorMode();

	void LoadSave(Save *save);
};

#endif // RENDERER_H
