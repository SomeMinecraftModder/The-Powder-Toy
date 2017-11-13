#include <cstring>
#include "graphics.h"
#include "powdergraphics.h"
#include "Renderer.h"
#include "game/Save.h"

Renderer::Renderer():
	renderModes(std::set<unsigned int>()),
	displayModes(std::set<unsigned int>()),
	colorMode(0)
{
	AddRenderMode(RENDER_FIRE);
	AddRenderMode(RENDER_SPRK);
	AddRenderMode(RENDER_EFFE);
	AddRenderMode(RENDER_BASC);

	InitRenderPresets();
}

void Renderer::InitRenderPresets()
{
	for (int i = 0; i < CM_COUNT; i++)
	{
		renderPresets[i].renderModes.insert(RENDER_BASC);
		renderPresets[i].colorMode = COLOR_DEFAULT;
	}

	renderPresets[CM_VEL].renderModes.insert(RENDER_EFFE);
	renderPresets[CM_VEL].displayModes.insert(DISPLAY_AIRV);
	renderPresets[CM_VEL].tooltip = "Velocity Display";

	renderPresets[CM_PRESS].renderModes.insert(RENDER_EFFE);
	renderPresets[CM_PRESS].displayModes.insert(DISPLAY_AIRP);
	renderPresets[CM_PRESS].tooltip = "Pressure Display";

	renderPresets[CM_PERS].renderModes.insert(RENDER_EFFE);
	renderPresets[CM_PERS].displayModes.insert(DISPLAY_PERS);
	renderPresets[CM_PERS].tooltip = "Persistent Display";

	renderPresets[CM_FIRE].renderModes.insert(RENDER_FIRE);
	renderPresets[CM_FIRE].renderModes.insert(RENDER_SPRK);
	renderPresets[CM_FIRE].renderModes.insert(RENDER_EFFE);
	renderPresets[CM_FIRE].tooltip = "Fire Display";

	renderPresets[CM_BLOB].renderModes.insert(RENDER_FIRE);
	renderPresets[CM_BLOB].renderModes.insert(RENDER_SPRK);
	renderPresets[CM_BLOB].renderModes.insert(RENDER_EFFE);
	renderPresets[CM_BLOB].renderModes.insert(RENDER_BLOB);
	renderPresets[CM_BLOB].tooltip = "Blob Display";

	renderPresets[CM_HEAT].displayModes.insert(DISPLAY_AIRH);
	renderPresets[CM_HEAT].colorMode = COLOR_HEAT;
	renderPresets[CM_HEAT].tooltip = "Heat Display";

	renderPresets[CM_FANCY].renderModes.insert(RENDER_FIRE);
	renderPresets[CM_FANCY].renderModes.insert(RENDER_SPRK);
	renderPresets[CM_FANCY].renderModes.insert(RENDER_GLOW);
	renderPresets[CM_FANCY].renderModes.insert(RENDER_BLUR);
	renderPresets[CM_FANCY].renderModes.insert(RENDER_EFFE);
	renderPresets[CM_FANCY].displayModes.insert(DISPLAY_WARP);
	renderPresets[CM_FANCY].tooltip = "Fancy Display";

	renderPresets[CM_NOTHING].tooltip = "Nothing Display";

	renderPresets[CM_GRAD].colorMode = COLOR_GRAD;
	renderPresets[CM_GRAD].tooltip = "Heat Gradient Display";

	renderPresets[CM_LIFE].colorMode = COLOR_LIFE;
	renderPresets[CM_LIFE].tooltip = "Life Gradient Display";

	renderPresets[CM_CRACK].renderModes.insert(RENDER_EFFE);
	renderPresets[CM_CRACK].displayModes.insert(DISPLAY_AIRC);
	renderPresets[CM_CRACK].tooltip = "Alternate Velocity Display";
}

bool Renderer::LoadRenderPreset(int preset)
{
	if (preset < 0 || preset >= CM_COUNT)
		return false;

	renderModes = renderPresets[preset].renderModes;
	displayModes = renderPresets[preset].displayModes;
	colorMode = renderPresets[preset].colorMode;

	// update global variables
	render_mode = GetRenderModesRaw();
	display_mode = GetDisplayModesRaw();

	if (HasRenderMode(RENDER_FIRE))
	{
		memset(fire_r, 0, sizeof(fire_r));
		memset(fire_g, 0, sizeof(fire_g));
		memset(fire_b, 0, sizeof(fire_b));
	}

	if (HasDisplayMode(DISPLAY_PERS))
	{
		memset(pers_bg, 0, (XRES+BARSIZE)*YRES*PIXELSIZE);
	}

	return true;
}

std::string Renderer::GetRenderPresetToolTip(int preset)
{
	if (preset < 0 || preset >= CM_COUNT)
		return "Invalid Render Mode Preset";
	return renderPresets[preset].tooltip;
}

bool Renderer::HasRenderMode(unsigned int renderMode)
{
	return renderModes.find(renderMode) != renderModes.end();
}

void Renderer::AddRenderMode(unsigned int renderMode)
{
	renderModes.insert(renderMode);
	render_mode = GetRenderModesRaw();
}

void Renderer::RemoveRenderMode(unsigned int renderMode)
{
	renderModes.erase(renderMode);
	render_mode = GetRenderModesRaw();
}

void Renderer::ToggleRenderMode(unsigned int renderMode)
{
	if (HasRenderMode(renderMode))
		RemoveRenderMode(renderMode);
	else
		AddRenderMode(renderMode);
	render_mode = GetRenderModesRaw();
}

void Renderer::ClearRenderModes()
{
	renderModes.clear();
	render_mode = GetRenderModesRaw();
}

std::set<unsigned int> Renderer::GetRenderModes()
{
	return renderModes;
}

unsigned int Renderer::GetRenderModesRaw()
{
	unsigned int render_mode = 0;
	for (std::set<unsigned int>::iterator it = renderModes.begin(), end = renderModes.end(); it != end; it++)
		render_mode |= (*it);
	return render_mode;
}

void Renderer::SetRenderModes(std::set<unsigned int> newRenderModes)
{
	renderModes = std::set<unsigned int>(newRenderModes);
	render_mode = GetRenderModesRaw();
}


bool Renderer::HasDisplayMode(unsigned int displayMode)
{
	return displayModes.find(displayMode) != displayModes.end();
	display_mode = GetDisplayModesRaw();
}

void Renderer::AddDisplayMode(unsigned int displayMode)
{
	displayModes.insert(displayMode);
	display_mode = GetDisplayModesRaw();
}

void Renderer::RemoveDisplayMode(unsigned int displayMode)
{
	displayModes.erase(displayMode);
	display_mode = GetDisplayModesRaw();
}

void Renderer::ToggleDisplayMode(unsigned int displayMode)
{
	if (HasDisplayMode(displayMode))
		RemoveDisplayMode(displayMode);
	else
		AddDisplayMode(displayMode);
	display_mode = GetDisplayModesRaw();

	if (displayMode == DISPLAY_PERS)
		memset(pers_bg, 0, (XRES+BARSIZE)*YRES*PIXELSIZE);
}

void Renderer::ClearDisplayModes()
{
	displayModes.clear();
	display_mode = GetDisplayModesRaw();
}

std::set<unsigned int> Renderer::GetDisplayModes()
{
	return displayModes;
}

unsigned int Renderer::GetDisplayModesRaw()
{
	unsigned int display_mode = 0;
	for (std::set<unsigned int>::iterator it = displayModes.begin(), end = displayModes.end(); it != end; it++)
		display_mode |= (*it);
	return display_mode;
}

void Renderer::SetDisplayModes(std::set<unsigned int> newDisplayModes)
{
	displayModes = std::set<unsigned int>(newDisplayModes);
	display_mode = GetDisplayModesRaw();
}

void Renderer::SetColorMode(unsigned int color_mode)
{
	colorMode = color_mode;
}

void Renderer::XORColorMode(unsigned int color_mode)
{
	colorMode ^= color_mode;
}

unsigned int Renderer::GetColorMode()
{
	return colorMode;
}

// Called after Simulation::LoadSave. Used to load some renderer settings for specific saves
void Renderer::LoadSave(Save *save, int replace)
{
	if (!save)
		return;

	if (save->renderModesPresent)
	{
		render_mode = 0;
		ClearRenderModes();
		for (std::vector<unsigned int>::const_iterator iter = save->renderModes.begin(), end = save->renderModes.end(); iter != end; ++iter)
		{
			renderModes.insert(*iter);
			Renderer::Ref().AddRenderMode(*iter);
		}
	}

	if (save->displayModesPresent)
	{
		display_mode = 0;
		ClearDisplayModes();
		for (std::vector<unsigned int>::const_iterator iter = save->displayModes.begin(), end = save->displayModes.end(); iter != end; ++iter)
		{
			displayModes.insert(*iter);
			Renderer::Ref().AddDisplayMode(*iter);
		}
	}

	if (save->colorModePresent)
		colorMode = save->colorMode;
}
