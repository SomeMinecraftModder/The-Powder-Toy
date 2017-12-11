/**
 * Powder Toy - Main source
 *
 * Copyright (c) 2008 - 2011 Stanislaw Skowronek.
 * Copyright (c) 2010 - 2011 Simon Robertshaw
 * Copyright (c) 2010 - 2011 Skresanov Savely
 * Copyright (c) 2010 - 2011 Bryan Hoyle
 * Copyright (c) 2010 - 2011 Nathan Cousins
 * Copyright (c) 2010 - 2011 cracker64
 * Copyright (c) 2011 jacksonmj
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "gui/profile/ProfileViewer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "SDLCompat.h"
#include "common/tpt-thread.h"
#include <bzlib.h>
#include <time.h>
#include <signal.h>
#include <list>
#include <sstream>

#ifdef WIN
#include <direct.h>
#ifdef _MSC_VER
#undef chdir
#define chdir _chdir //chdir is deprecated in visual studio
#endif
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

#ifdef X86_SSE
#include <xmmintrin.h>
#endif
#ifdef X86_SSE3
#include <pmmintrin.h>
#endif

#ifdef LIN
#include "images.h"
#endif
#include "defines.h"
#include "misc.h"
#include "font.h"
#include "powder.h"
#include "gravity.h"
#include "graphics.h"
#include "powdergraphics.h"
#include "http.h"
#include "interface.h"
#include "md5.h"
#include "update.h"
#include "console.h"
#include "luaconsole.h"
#include "luascriptinterface.h"
#include "save.h"
#include "hud.h"
#include "benchmark.h"

#include "common/Platform.h"
#include "common/tpt-minmax.h"
#include "game/Authors.h"
#include "game/Brush.h"
#include "game/Menus.h"
#include "game/Save.h"
#include "game/Sign.h"
#include "game/ToolTip.h"
#include "game/Download.h"
#include "game/DownloadManager.h"
#include "simulation/Simulation.h"
#include "simulation/Snapshot.h"
#include "simulation/Tool.h"
#include "simulation/ToolNumbers.h"
#include "simulation/elements/LIFE.h"
#include "simulation/elements/STKM.h"

// new interface stuff
#include "graphics/Renderer.h"
#include "graphics/VideoBuffer.h"
#include "interface/Engine.h"
#include "gui/dialogs/ErrorPrompt.h"
#include "gui/dialogs/InfoPrompt.h"
#include "gui/game/PowderToy.h"

pixel *vid_buf;
pixel *vid3d = NULL;

#ifdef TOUCHUI
static const char *it_msg =
	"\blThe Powder Toy - Version " MTOS(SAVE_VERSION) "." MTOS(MINOR_VERSION) " - http://powdertoy.co.uk, irc.freenode.net #powder\n"
	"\x7F\x7F\x7F\x7F\x7F\x7F\x7F\x7F\x7F\x7F\x7F\x7F\x7F\x7F\x7F\x7F\x7F\x7F\x7F\n"
	"Android version " MTOS(MOBILE_MAJOR) "." MTOS(MOBILE_MINOR) "\n"
	"\n"
	"\bgTouch and drag the menus on the right side to show each menusection.\n"
	"For scrollable menusections, the menus can be dragged around.\n"
	"Use the volume keys to change the tool size for particles.\n"
	"To open online saves, tap the 'Find & open a simulation' button on the very bottom left.\n"
	"To pause and unpause the game, use the pause button on the very bottom right.\n\n"
	"\boThe buttons on the right do the following actions respectively:\n"
	"\bo   Toggle erase tool (hold button down to clear screen)\n"
	"\bo   Open the console (hold button to show the on screen keyboard)\n"
	"\bo   Toggle Newtonian gravity, or decorations while in deco menu (hold to open simulation settings)\n"
	"\bo   Place or remove a zoom window\n"
	"\bo   Save a stamp (hold to load a stamp)\n\n"
	"\boWhen placing a stamp, you may do the following:\n"
	"\bo   Drag the stamp around wherever you want.\n"
	"\bo   Tap the stamp without moving it to place.\n"
	"\bo   Touch any location outside of the stamp to shift it by one pixel.\n"
	"\bo   Rotate the stamp by first tapping around the edges and then dragging in a circle.\n"
	"\n"
	"Contributors: \bgStanislaw K Skowronek (Designed the original Powder Toy),\n"
	"\bgSimon Robertshaw, Skresanov Savely, cracker64, Catelite, Bryan Hoyle, Nathan Cousins, jacksonmj,\n"
	"\bgFelix Wallin, Lieuwe Mosch, Anthony Boot, Matthew \"me4502\", MaksProg, jacob1, mniip\n"
	"\n"
	"\bgTo use online features such as saving, you need to register at: \brhttp://powdertoy.co.uk/Register.html\n"
	"\n"
	"\bt" MTOS(SAVE_VERSION) "." MTOS(MINOR_VERSION) "." MTOS(BUILD_NUM) " "
#else
static const char *it_msg =
	"\blThe Powder Toy - Version " MTOS(SAVE_VERSION) "." MTOS(MINOR_VERSION) " - http://powdertoy.co.uk, irc.freenode.net #powder\n"
	"\x7F\x7F\x7F\x7F\x7F\x7F\x7F\x7F\x7F\x7F\x7F\x7F\x7F\x7F\x7F\x7F\x7F\x7F\x7F\n"
	"\brJ\bla\boc\bgo\btb\bb1\bp'\bws \bbMod version " MTOS(MOD_VERSION) "." MTOS(MOD_MINOR_VERSION) "\bg   Codebase based on C version 83.0\n"
	"\n"
	"\bgControl+C/V/X are Copy, Paste and cut respectively.\n"
	"\bgTo choose a material, hover over one of the icons on the right, it will show a selection of elements in that group.\n"
	"\bgPick your material from the menu using mouse left/right buttons.\n"
	"Draw freeform lines by dragging your mouse left/right button across the drawing area.\n"
	"Shift+drag will create straight lines of particles.\n"
	"Ctrl+drag will result in filled rectangles.\n"
	"Ctrl+Shift+click will flood-fill a closed area.\n"
	"Use the mouse scroll wheel, or '[' and ']', to change the tool size for particles.\n"
	"Middle click or Alt+Click to \"sample\" the particles.\n"
	"Ctrl+Z will act as Undo.\n"
	"\n\boUse 'Z' for a zoom tool. Click to make the drawable zoom window stay around. Use the wheel to change the zoom strength\n"
	"The spacebar can be used to pause and unpause physics.\n"
	"Use 'S' to save parts of the window as 'stamps'.\n"
	"'L' will load the most recent stamp, 'K' shows a library of stamps you saved.\n"
	"The numbers on the keyboard will change the display mode\n"
	"\n"
	"Contributors: \bgStanislaw K Skowronek (Designed the original Powder Toy),\n"
	"\bgSimon Robertshaw, Skresanov Savely, cracker64, Catelite, Bryan Hoyle, Nathan Cousins, jacksonmj,\n"
	"\bgFelix Wallin, Lieuwe Mosch, Anthony Boot, Matthew \"me4502\", MaksProg, jacob1, mniip\n"
	"Thanks to cracker64 for the update server for my mod\n"
	"\n"
	"\bgTo use online features such as saving, you need to register at: \brhttp://powdertoy.co.uk/Register.html\n"
	"\n"
	"\bt" MTOS(SAVE_VERSION) "." MTOS(MINOR_VERSION) "." MTOS(BUILD_NUM) " "
#endif


#ifdef X86
	"X86 "
#endif
#ifdef X86_SSE
	"X86_SSE "
#endif
#ifdef X86_SSE2
	"X86_SSE2 "
#endif
#ifdef X86_SSE3
	"X86_SSE3 "
#endif
#ifdef LIN
#ifdef _64BIT
	"LIN64 "
#else
	"LIN32 "
#endif
#endif
#ifdef WIN
#ifdef _64BIT
	"WIN64 "
#else
	"WIN32 "
#endif
#endif
#ifdef MACOSX
	"MACOSX "
#endif
#ifdef ANDROID
	"ANDROID "
#endif
#ifdef LUACONSOLE
	"LUACONSOLE "
#endif
#ifdef GRAVFFT
	"GRAVFFT "
#endif
    ;

typedef struct
{
	int start, inc;
	pixel *vid;
} upstruc;

char new_message_msg[255];
float mheat = 0.0f;

int ptsaveOpenID = 0;
int saveURIOpen = 0;
Save * saveDataOpen = NULL;

#ifdef INTERNAL
	int vs = 0;
#endif
bool firstRun = false;
bool doubleScreenDialog = false;
int screenWidth = 0;
int screenHeight = 0;
int do_open = 0;
bool sys_pause = false;
int sys_shortcuts = 1;
bool legacy_enable = false; //Used to disable new features such as heat, will be set by save.
bool aheat_enable = false; //Ambient heat
bool decorations_enable = true;
bool hud_enable = true;
int active_menu = 0;
int last_active_menu = 0;
int last_fav_menu = SC_FAV;
int framerender = 0;
bool pretty_powder = false;
int limitFPS = 60;
int main_loop = 1;
int finding = 0;
int highesttemp = MAX_TEMP;
int lowesttemp = MIN_TEMP;
int heatmode = 0;
int secret_els = 0;
int tab_num = 1;
int num_tabs = 1;
bool show_tabs = false;
Brush* currentBrush;
Tool* activeTools[3];
float toolStrength = 1.0f;
int autosave = 0;
int realistic = 0;
int unlockedstuff = 0x08;
int old_menu = 0;
int loop_time = 0;
bool doUpdates = true;

int drawinfo = 0;
int elapsedTime = 0;
int currentTime = 0;
int totaltime = 0;
int totalafktime = 0;
int afktime = 0;
double totalfps = 0;
int frames = 0;
int prevafktime = 0;
int timesplayed = 0;

bool console_mode = false;
bool REPLACE_MODE = false;
bool SPECIFIC_DELETE = false;
int GRID_MODE;
int DEBUG_MODE;
int debug_flags = 0;
int debug_perf_istart = 1;
int debug_perf_iend = 0;
long debug_perf_frametime[DEBUG_PERF_FRAMECOUNT];
long debug_perf_partitime[DEBUG_PERF_FRAMECOUNT];
long debug_perf_time = 0;

int numCores = 4;

int core_count()
{
	int numCPU = 1;
#ifdef MT
#ifdef WIN
	SYSTEM_INFO sysinfo;
	GetSystemInfo( &sysinfo );
	numCPU = sysinfo.dwNumberOfProcessors;
#else
#ifdef MACOSX
	numCPU = 4;
#else
	numCPU = sysconf( _SC_NPROCESSORS_ONLN );
#endif
#endif

	printf("Cpus: %d\n", numCPU);
	if (numCPU>1)
		printf("Multithreading enabled\n");
	else
		printf("Multithreading disabled\n");
#endif
	return numCPU;
}

int kiosk_enable = 0;

int frame_idx=0;
void dump_frame(pixel *src, int w, int h, int pitch)
{
	char frame_name[32];
	int j,i;
	unsigned char c[3];
	FILE *f;
	sprintf(frame_name,"frame%04d.ppm",frame_idx);
	f=fopen(frame_name,"wb");
	fprintf(f,"P6\n%d %d\n255\n",w,h);
	for (j=0; j<h; j++)
	{
		for (i=0; i<w; i++)
		{
			c[0] = PIXR(src[i]);
			c[1] = PIXG(src[i]);
			c[2] = PIXB(src[i]);
			fwrite(c,3,1,f);
		}
		src+=pitch;
	}
	fclose(f);
	frame_idx++;
}

void clear_sim()
{
	for (int i = 0; i < NPART; i++)
	{
		if (parts[i].animations)
		{
			free(parts[i].animations);
			parts[i].animations = NULL;
		}
	}
	memset(bmap, 0, sizeof(bmap));
	globalSim->Clear();
	memset(emap, 0, sizeof(emap));
	ClearSigns();
	memset(parts, 0, sizeof(particle)*NPART);
	for (int i = 0; i < NPART-1; i++)
		parts[i].life = i+1;
	parts[NPART-1].life = -1;
	memset(pmap, 0, sizeof(pmap));
	memset(photons, 0, sizeof(photons));
	memset(pers_bg, 0, (XRES+BARSIZE)*YRES*PIXELSIZE);
	memset(fire_r, 0, sizeof(fire_r));
	memset(fire_g, 0, sizeof(fire_g));
	memset(fire_b, 0, sizeof(fire_b));
	memset(fire_alpha, 0, sizeof(fire_alpha));
	prepare_alpha(1.0f);
	if(gravmask)
		memset(gravmask, 0xFFFFFFFF, (XRES/CELL)*(YRES/CELL)*sizeof(unsigned));
	if(gravy)
		memset(gravy, 0, (XRES/CELL)*(YRES/CELL)*sizeof(float));
	if(gravx)
		memset(gravx, 0, (XRES/CELL)*(YRES/CELL)*sizeof(float));
	if(gravp)
		memset(gravp, 0, (XRES/CELL)*(YRES/CELL)*sizeof(float));
	gravity_cleared = 1;
	finding &= 0x8;
#ifdef LUACONSOLE
	if (LuaCode)
	{
		free(LuaCode);
		LuaCode = NULL;
	}
#endif
	authors.clear();
}

void NewSim()
{
	clear_sim();
	clear_save_info();
	legacy_enable = 0;
	gravityMode = 0;
	airMode = 0;
}

// stamps library

stamp stamps[STAMP_MAX];//[STAMP_X*STAMP_Y];

int stamp_count = 0;

unsigned last_time=0, last_name=0;
void stamp_gen_name(char *fn)
{
	unsigned t=(unsigned)time(NULL);

	if (last_time!=t)
	{
		last_time=t;
		last_name=0;
	}
	else
		last_name++;

	sprintf(fn, "%08x%02x", last_time, last_name);
}

void stamp_update(void)
{
	FILE *f;
	int i;
	f=fopen("stamps" PATH_SEP "stamps.def", "wb");
	if (!f)
		return;
	for (i=0; i<STAMP_MAX; i++)
	{
		if (!stamps[i].name[0])
			break;
		if (stamps[i].dodelete!=1)
		{
			fwrite(stamps[i].name, 1, 10, f);
		}
		else
		{
			char name[30] = {0};
			sprintf(name,"stamps%s%s.stm",PATH_SEP,stamps[i].name);
			remove(name);
		}
	}
	fclose(f);
}

void stamp_gen_thumb(int i)
{
	char fn[64];
	void *data;
	int size, factor_x, factor_y;
	pixel *tmp;

	if (stamps[i].thumb)
	{
		free(stamps[i].thumb);
		stamps[i].thumb = NULL;
	}

	sprintf(fn, "stamps" PATH_SEP "%s.stm", stamps[i].name);
	data = file_load(fn, &size);

	if (data)
	{
		stamps[i].thumb = prerender_save(data, size, &(stamps[i].thumb_w), &(stamps[i].thumb_h));
		if (stamps[i].thumb && (stamps[i].thumb_w>XRES/GRID_S || stamps[i].thumb_h>YRES/GRID_S))
		{
			factor_x = (int)ceil((float)stamps[i].thumb_w/(float)(XRES/GRID_S));
			factor_y = (int)ceil((float)stamps[i].thumb_h/(float)(YRES/GRID_S));
			if (factor_y > factor_x)
				factor_x = factor_y;
			tmp = rescale_img(stamps[i].thumb, stamps[i].thumb_w, stamps[i].thumb_h, &(stamps[i].thumb_w), &(stamps[i].thumb_h), factor_x);
			free(stamps[i].thumb);
			stamps[i].thumb = tmp;
		}
	}

	free(data);
}

int clipboard_ready = 0;
void *clipboard_data = 0;
int clipboard_length = 0;

char* stamp_save(int x, int y, int w, int h, bool includePressure)
{
	FILE *f;
	char fn[64], sn[16];

	// Generate filename
	stamp_gen_name(sn);
	sprintf(fn, "stamps" PATH_SEP "%s.stm", sn);

	Json::Value stampInfo;
	stampInfo["type"] = "stamp";
	stampInfo["username"] = svf_user;
	stampInfo["name"] = fn;
	stampInfo["date"] = (Json::Value::UInt64)time(NULL);
	if (authors.size())
	{
		// This is a stamp, always append full authorship info (even if same user)
		stampInfo["links"].append(authors);
	}

	Save *save = globalSim->CreateSave(x, y, x+w, y+h, includePressure);
	save->authors = stampInfo;
	try
	{
		save->BuildSave();
	}
	catch (BuildException e)
	{
		ErrorPrompt *error = new ErrorPrompt("Error building stamp: " + std::string(e.what()));
		Engine::Ref().ShowWindow(error);
		delete save;
		return NULL;
	}

#ifdef WIN
	_mkdir("stamps");
#else
	mkdir("stamps", 0755);
#endif

	f = fopen(fn, "wb");
	if (!f)
		return NULL;
	fwrite(save->GetSaveData(), save->GetSaveSize(), 1, f);
	fclose(f);

	delete save;

	if (stamps[STAMP_MAX-1].thumb)
		free(stamps[STAMP_MAX-1].thumb);
	memmove(stamps+1, stamps, sizeof(struct stamp)*(STAMP_MAX-1));
	memset(stamps, 0, sizeof(struct stamp));
	if (stamp_count<STAMP_MAX)
		stamp_count++;

	strcpy(stamps[0].name, sn);
	stamp_gen_thumb(0);

	stamp_update();
	return mystrdup(sn);
}

void tab_save(int num)
{
	FILE *f;
	int fileSize;
	char fileName[64];

	sprintf(fileName, "tabs" PATH_SEP "%d.stm", num);

	Json::Value tabInfo;
	tabInfo["type"] = "tab";
	tabInfo["username"] = svf_user;
	tabInfo["num"] = num;
	tabInfo["date"] = (Json::Value::UInt64)time(NULL);
	SaveAuthorInfo(&tabInfo);

	//build the tab
	Save *tab = globalSim->CreateSave(0, 0, XRES, YRES, true);
	Renderer::Ref().CreateSave(tab);
	try
	{
		tab->BuildSave();
	}
	catch (BuildException e)
	{
		ErrorPrompt * error = new ErrorPrompt("Could not create tab: " + std::string(e.what()));
		Engine::Ref().ShowWindow(error);
		delete tab;
		return;
	}

#ifdef WIN
	_mkdir("tabs");
#else
	mkdir("tabs", 0755);
#endif

	//save the tab
	f = fopen(fileName, "wb");
	if (!f)
		return;
	fwrite(tab->GetSaveData(), tab->GetSaveSize(), 1, f);
	fclose(f);
	delete tab;

	//set the tab's name
	if (strlen(svf_name))
		sprintf(tabNames[num-1], "%s", svf_name);
	else if (strlen(svf_filename))
		sprintf(tabNames[num-1], "%s", svf_filename);
	else
		sprintf(tabNames[num-1], "Untitled Simulation %i", num);
	
	//set the tab's thumbnail
	if (tabThumbnails[num-1])
		free(tabThumbnails[num-1]);
	//the fillrect is to cover up the quickoptions so they aren't in the thumbnail. the &filesize is just a dummy int variable that's unused
	fillrect(vid_buf, XRES, 0, BARSIZE, YRES, 0, 0, 0, 255);
	tabThumbnails[num-1] = rescale_img(vid_buf, XRES+BARSIZE, YRES, &fileSize, &fileSize, 3);
}

Save *stamp_load(int i, int reorder)
{
	char fn[64];
	struct stamp tmp;

	if (!stamps[i].thumb || !stamps[i].name[0])
		return NULL;

	sprintf(fn, "stamps" PATH_SEP "%s.stm", stamps[i].name);
	int size;
	char *data = (char*)file_load(fn, &size);
	if (!data)
		return NULL;
	Save *save = new Save(data, size);

	if (reorder && i>0)
	{
		memcpy(&tmp, stamps+i, sizeof(struct stamp));
		memmove(stamps+1, stamps, sizeof(struct stamp)*i);
		memcpy(stamps, &tmp, sizeof(struct stamp));

		stamp_update();
	}

	return save;
}

int tab_load(int tabNum, bool del)
{
	char *saveData;
	int saveSize;
	char fileName[64];

	sprintf(fileName, "tabs" PATH_SEP "%d.stm", tabNum);
	saveData = (char*)file_load(fileName, &saveSize);
	if (saveData)
	{
		// Prevent crash loops on startup
		if (del)
			remove(fileName);
		Snapshot::TakeSnapshot(globalSim);

		Save *save = new Save(saveData, saveSize);
		bool ret = false;
		try
		{
			globalSim->LoadSave(0, 0, save, 2);
			Renderer::Ref().LoadSave(save);
			authors = save->authors;
			the_game->SetReloadPoint(save);
			ret = true;
		}
		catch (ParseException e)
		{
			Engine::Ref().ShowWindow(new InfoPrompt("Error loading save", e.what()));
		}

		delete save;
		free(saveData);
		return ret;
	}
	return 0;
}

void stamp_init()
{
	int i;
	FILE *f;

	memset(stamps, 0, sizeof(stamps));

	f=fopen("stamps" PATH_SEP "stamps.def", "rb");
	if (!f)
		return;
	for (i=0; i<STAMP_MAX; i++)
	{
		int readsize = fread(stamps[i].name, 1, 10, f);
		if (readsize != 10 || !stamps[i].name[0])
			break;
		stamp_count++;
		stamp_gen_thumb(i);
	}
	fclose(f);
}

void stamps_free()
{
	for (int i = 0; i < STAMP_MAX; i++)
		if (stamps[i].thumb)
		{
			free(stamps[i].thumb);
			stamps[i].thumb = NULL;
		}
}

void del_stamp(int d)
{
	stamps[d].dodelete = 1;
	stamp_update();
	stamp_count = 0;
	stamp_init();
}

void thumb_cache_inval(char *id);

char *thumb_cache_id[THUMB_CACHE_SIZE];
void *thumb_cache_data[THUMB_CACHE_SIZE];
int thumb_cache_size[THUMB_CACHE_SIZE];
int thumb_cache_lru[THUMB_CACHE_SIZE];

void thumb_cache_inval(char *id)
{
	int i,j;
	for (i=0; i<THUMB_CACHE_SIZE; i++)
		if (thumb_cache_id[i] && !strcmp(id, thumb_cache_id[i]))
			break;
	if (i >= THUMB_CACHE_SIZE)
		return;
	free(thumb_cache_id[i]);
	free(thumb_cache_data[i]);
	thumb_cache_id[i] = NULL;
	for (j=0; j<THUMB_CACHE_SIZE; j++)
		if (thumb_cache_lru[j] > thumb_cache_lru[i])
			thumb_cache_lru[j]--;
}
void thumb_cache_add(char *id, void *thumb, int size)
{
	int i,m=-1,j=-1;
	thumb_cache_inval(id);
	if (thumb || size == 0)
		return;
	for (i=0; i<THUMB_CACHE_SIZE; i++)
	{
		if (!thumb_cache_id[i])
			break;
		if (thumb_cache_lru[i] > m)
		{
			m = thumb_cache_lru[i];
			j = i;
		}
	}
	if (i >= THUMB_CACHE_SIZE)
	{
		thumb_cache_inval(thumb_cache_id[j]);
		i = j;
	}
	for (j=0; j<THUMB_CACHE_SIZE; j++)
		thumb_cache_lru[j] ++;
	thumb_cache_id[i] = mystrdup(id);
	thumb_cache_data[i] = malloc(size);
	memcpy(thumb_cache_data[i], thumb, size);
	thumb_cache_size[i] = size;
	thumb_cache_lru[i] = 0;
}
bool thumb_cache_find(char *id, void **thumb, int *size)
{
	int i,j;
	for (i=0; i<THUMB_CACHE_SIZE; i++)
		if (thumb_cache_id[i] && !strcmp(id, thumb_cache_id[i]))
			break;
	if (i >= THUMB_CACHE_SIZE)
		return false;
	for (j=0; j<THUMB_CACHE_SIZE; j++)
		if (thumb_cache_lru[j] < thumb_cache_lru[i])
			thumb_cache_lru[j]++;
	thumb_cache_lru[i] = 0;
	*thumb = malloc(thumb_cache_size[i]);
	*size = thumb_cache_size[i];
	memcpy(*thumb, thumb_cache_data[i], *size);
	return true;
}

char http_proxy_string[256] = "";

unsigned short last_major=0, last_minor=0, last_build=0, update_flag=0;

// Particle debugging function
void ParticleDebug(int mode, int x, int y)
{
	int debug_currentParticle = globalSim->debug_currentParticle;
	int i;
	std::stringstream logmessage;

	// update one particle at a time
	if (mode == 0)
	{
		if (!NUM_PARTS)
			return;
		i = debug_currentParticle;
		while (i < NPART && !globalSim->parts[i].type)
			i++;
		if (i == NPART)
			logmessage << "End of particles reached, updated sim";
		else
			logmessage << "Updated particle #" << i;
	}
	// update all particles up to particle under mouse (or to end of sim)
	else if (mode == 1)
	{
		if (x < 0 || x >= XRES || y < 0 || y >= YRES || !pmap[y][x] || (i = (pmap[y][x]>>8)) < debug_currentParticle)
		{
			i = NPART;
			logmessage << "Updated particles from #" << debug_currentParticle << " to end, updated sim";
		}
		else
			logmessage << "Updated particles #" << debug_currentParticle << " through #" << i;
	}
#ifdef LUACONSOLE
	luacon_log(mystrdup(logmessage.str().c_str()));
#endif

	// call simulation functions run before updating particles if we are updating #0
	if (debug_currentParticle == 0)
	{
		framerender = 1;
		globalSim->RecalcFreeParticles(true);
		globalSim->UpdateBefore();
		framerender = 0;
	}
	// update the particles
	globalSim->UpdateParticles(debug_currentParticle, i);
	if (i < NPART-1)
		globalSim->debug_currentParticle = i+1;
	// we reached the end, call simulation functions run after updating particles
	else
	{
		globalSim->UpdateAfter();
		globalSim->currentTick++;
		globalSim->debug_currentParticle = 0;
	}
}

#ifdef RENDERER
int main(int argc, char *argv[])
{
	pixel *vid_buf = (pixel*)calloc((XRES+BARSIZE)*(YRES+MENUSIZE), PIXELSIZE);
	int load_size, i=0, j=0;
	void *load_data = file_load(argv[1], &load_size);
	unsigned char c[3];
	char ppmfilename[256], ptifilename[256], ptismallfilename[256];
	FILE *f;

	//init some new c++ stuff
	Simulation *mainSim = new Simulation();
	mainSim->InitElements();
	globalSim = mainSim;

	render_mode = Renderer::Ref().GetRenderModesRaw();
	display_mode = Renderer::Ref().GetDisplayModesRaw();
	Renderer::Ref().SetColorMode(COLOR_DEFAULT);
	init_display_modes();
	TRON_init_graphics();

	sys_pause = 1;
	parts = (particle*)calloc(sizeof(particle), NPART);
	pers_bg = (pixel*)calloc((XRES+BARSIZE)*YRES, PIXELSIZE);
	clear_sim();

	prepare_alpha(CELL, 1.0f);
	prepare_graphicscache();
	flm_data = generate_gradient(flm_data_colours, flm_data_pos, flm_data_points, 200);
	plasma_data = generate_gradient(plasma_data_colours, plasma_data_pos, plasma_data_points, 200);

	sprintf(ppmfilename, "%s.ppm", argv[2]);
	sprintf(ptifilename, "%s.pti", argv[2]);
	sprintf(ptismallfilename, "%s-small.pti", argv[2]);

	if (load_data && load_size)
	{
		int parsestate = 0;
		//parsestate = parse_save(load_data, load_size, 1, 0, 0);
		parsestate = parse_save(load_data, load_size, 1, 0, 0, bmap, vx, vy, pv, fvx, fvy, signs, parts, pmap);

		//decorations_enable = 0;
		render_before(vid_buf);
		render_after(vid_buf, vid_buf, Point(0,0));
		for (i=0; i<30; i++)
		{
			memset(vid_buf, 0, (XRES+BARSIZE)*YRES*PIXELSIZE);
			render_parts(vid_buf, Point(0,0));
			render_fire(vid_buf);
		}
		render_before(vid_buf);
		render_after(vid_buf, vid_buf, Point(0,0));

		if (parsestate>0)
		{
			//return 0;
			info_box(vid_buf, "Save file invalid or from newer version");
		}

		//Save PTi images
		char * datares = NULL, *scaled_buf;
		int res = 0, sw, sh;
		datares = (char*)ptif_pack(vid_buf, XRES, YRES, &res);
		if (datares!=NULL)
		{
			f=fopen(ptifilename, "wb");
			fwrite(datares, res, 1, f);
			fclose(f);
			free(datares);
			datares = NULL;
		}
		scaled_buf = (char*)resample_img(vid_buf, XRES, YRES, XRES/GRID_Z, YRES/GRID_Z);
		datares = (char*)ptif_pack((pixel*)scaled_buf, XRES/GRID_Z, YRES/GRID_Z, &res);
		if (datares!=NULL)
		{
			f=fopen(ptismallfilename, "wb");
			fwrite(datares, res, 1, f);
			fclose(f);
			free(datares);
			datares = NULL;
		}
		free(scaled_buf);
		//Save PPM image
		f=fopen(ppmfilename, "wb");
		fprintf(f,"P6\n%d %d\n255\n",XRES,YRES);
		for (j=0; j<YRES; j++)
		{
			for (i=0; i<XRES; i++)
			{
				c[0] = PIXR(vid_buf[i]);
				c[1] = PIXG(vid_buf[i]);
				c[2] = PIXB(vid_buf[i]);
				fwrite(c,3,1,f);
			}
			vid_buf+=XRES+BARSIZE;
		}
		fclose(f);
		
		return 1;
	}

	return 0;
}
#else

void BlueScreen(const char * detailMessage)
{
	//std::string errorDetails = "Details: " + std::string(detailMessage);
	SDL_Event event;
	const char * errorHelp = "An unrecoverable fault has occurred, please report this to jacob1:\n"
		" http://powdertoy.co.uk/Discussions/Thread/View.html?Thread=11117\n"
		" OR the built in bug reporter.\n\n"
		"Note: TPT will now restart and reload your work";
	int positionX = (XRES+BARSIZE)/2-textwidth(errorHelp)/2-50, positionY = (YRES+MENUSIZE)/2-100;

	fillrect(vid_buf, -1, -1, XRES+BARSIZE+1, YRES+MENUSIZE+1, 17, 114, 169, 210);
	
	drawtext(vid_buf, positionX, positionY, "ERROR", 255, 255, 255, 255);
	drawtext(vid_buf, positionX, positionY + 14, detailMessage, 255, 255, 255, 255);
	drawtext(vid_buf, positionX, positionY  + 28, errorHelp, 255, 255, 255, 255);

	pixel* vid_buf2 = (pixel*)calloc((XRES+BARSIZE)*(YRES+MENUSIZE), PIXELSIZE);
	memcpy(vid_buf2, vid_buf, (XRES+BARSIZE)*(YRES+MENUSIZE)*PIXELSIZE);
	std::vector<Point> food;
	std::list<Point> tron;
	unsigned int tronSize = 5, score = 0;
	int tronDirection = 2;
	char scoreString[20];
	bool gameRunning = false, gameLost = false;
	for (int i = 0; i < 10; i++)
		food.push_back(Point(rand()%(XRES+BARSIZE-6)+3, rand()%(YRES+MENUSIZE-6)+3));
	for (unsigned int i = 0; i < tronSize; i++)
		tron.push_back(Point(20, 10+i));

	//Death loop
	while(1)
	{
		while (SDL_PollEvent(&event))
		{
			if (event.type == SDL_QUIT)
			{
				Platform::DoRestart(true);
			}
			else if (event.type == SDL_KEYDOWN)
			{
				switch (event.key.keysym.sym)
				{
				case SDLK_UP:
					if (tronDirection != 2)
						tronDirection = 0;
					break;
				case SDLK_RIGHT:
					if (tronDirection != 3)
						tronDirection = 1;
					break;
				case SDLK_DOWN:
					if (tronDirection != 0)
						tronDirection = 2;
					break;
				case SDLK_LEFT:
					if (tronDirection != 1)
						tronDirection = 3;
					break;
				default:
#ifdef ANDROID
					Platform::DoRestart(true);
#endif
					break;
				}
				gameRunning = true;
			}
			else if (event.type == SDL_MOUSEBUTTONDOWN)
			{
				if (event.button.button != SDL_BUTTON_WHEELUP && event.button.button != SDL_BUTTON_WHEELDOWN)
				{
					int mx = event.motion.x/sdl_scale, my = event.motion.y/sdl_scale;
					if (my < (YRES+MENUSIZE)/4)
					{
						if (tronDirection != 2)
							tronDirection = 0;
					}
					else if (my > YRES+MENUSIZE-(YRES+MENUSIZE)/4)
					{
						if (tronDirection != 0)
							tronDirection = 2;
					}
					else if (mx < (XRES+BARSIZE)/4)
					{
						if (tronDirection != 1)
							tronDirection = 3;
					}
					else if (mx > XRES+BARSIZE-(XRES+BARSIZE)/4)
					{
						if (tronDirection != 3)
							tronDirection = 1;
					}
					gameRunning = true;
				}
			}
		}
		if (gameRunning && !gameLost)
		{
			Point tronHead = ((Point)tron.back());
			tronHead.X -= (tronDirection-2)%2;
			tronHead.Y += (tronDirection-1)%2;
			if (tronHead.X > 1 && tronHead.X < XRES+BARSIZE-2 && tronHead.Y > 1 && tronHead.Y < YRES+MENUSIZE-2)
				tron.push_back(tronHead);
			else
				gameLost = true;

			sprintf(scoreString, "Score: %i", score);
			drawtext(vid_buf, XRES-BARSIZE-50, 10, scoreString, 255, 255, 255, 255);
			for (std::vector<Point>::iterator iter = food.begin(); iter != food.end(); ++iter)
			{
				Point moo = (Point)*iter;
				vid_buf[moo.X + moo.Y*(XRES+BARSIZE)] = PIXRGB(255, 0, 0);
				vid_buf[moo.X + moo.Y*(XRES+BARSIZE)+1] = PIXRGB(100, 0, 0);
				vid_buf[moo.X + moo.Y*(XRES+BARSIZE)-1] = PIXRGB(100, 0, 0);
				vid_buf[moo.X + (moo.Y+1)*(XRES+BARSIZE)] = PIXRGB(100, 0, 0);
				vid_buf[moo.X + (moo.Y-1)*(XRES+BARSIZE)] = PIXRGB(100, 0, 0);
				if (tronHead.X >= moo.X - 3 && tronHead.X <= moo.X + 3 && tronHead.Y >= moo.Y - 3 && tronHead.Y <= moo.Y + 3)
				{
					food.erase(iter, iter+1);
					food.push_back(Point(rand()%(XRES+BARSIZE-6)+3, rand()%(YRES+MENUSIZE-6)+3));
					food.push_back(Point(rand()%(XRES+BARSIZE-6)+3, rand()%(YRES+MENUSIZE-6)+3));
					tronSize += 20;
					score++;
					break;
				}
			}
			if (!(rand()%20))
				tronSize++;
			int i = tronSize;
			for (std::list<Point>::iterator iter = tron.begin(); iter != tron.end(); ++iter)
			{
				Point point = *iter;
				vid_buf[point.X + point.Y*(XRES+BARSIZE)] = PIXRGB(0, 255-(int)(200.0f/tronSize*i), 0);
				i--;
				if (iter != --tron.end() && point == tronHead)
					gameLost = true;
			}
			while (tron.size() > tronSize)
				tron.pop_front();
		}
		sdl_blit(0, 0, XRES+BARSIZE, YRES+MENUSIZE, vid_buf, XRES+BARSIZE);
		if (gameRunning && !gameLost)
			memcpy(vid_buf, vid_buf2, (XRES+BARSIZE)*(YRES+MENUSIZE)*PIXELSIZE);
		SDL_Delay(5);
	}
}

void SigHandler(int signal)
{
	switch(signal){
	case SIGSEGV:
		BlueScreen("Memory read/write error");
		break;
	case SIGFPE:
		BlueScreen("Floating point exception");
		break;
	case SIGILL:
		BlueScreen("Program execution exception");
		break;
	case SIGABRT:
		BlueScreen("Unexpected program abort");
		break;
	}
}

//int main(int argc, char *argv[])
//{
	pixel *part_vbuf; //Extra video buffer
	pixel *part_vbuf_store;
	int new_message_len = 0;
	int afk = 0, afkstart = 0, lastx = 1, lasty = 0; // afk tracking for stats
	bool mouseInZoom = false;
	int username_flash = 0, username_flash_t = 1;
	Download *sessionCheck = NULL;
#if !defined(DEBUG) && !defined(_DEBUG)
	int signal_hooks = 0;
#endif

bool sendNewEvents = false;
bool openConsole = false;
bool openSign = false;
bool openProp = false;
PowderToy *the_game;
int main(int argc, char *argv[])
{
	bool benchmark_enable = false;

#ifdef PTW32_STATIC_LIB
	pthread_win32_process_attach_np();
	pthread_win32_thread_attach_np();
#endif

#ifdef X86_SSE
	_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
#endif
#ifdef X86_SSE3
	_MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
#endif

	// initialize hud with defaults before loading powder.pref
	HudDefaults();
	memcpy(currentHud,normalHud,sizeof(currentHud));

	render_mode = Renderer::Ref().GetRenderModesRaw();
	display_mode = Renderer::Ref().GetDisplayModesRaw();
	Renderer::Ref().SetColorMode(COLOR_DEFAULT);

	// initialize this first so simulation gets inited
	the_game = new PowderToy(); // you just lost
	
	limitFPS = 60;
	vid_buf = (pixel*)calloc((XRES+BARSIZE)*(YRES+MENUSIZE), PIXELSIZE);
	part_vbuf = (pixel*)calloc((XRES+BARSIZE)*(YRES+MENUSIZE), PIXELSIZE); //Extra video buffer
	part_vbuf_store = part_vbuf;
	pers_bg = (pixel*)calloc((XRES+BARSIZE)*YRES, PIXELSIZE);

	gravity_init();

#ifdef MT
	numCores = core_count();
#endif
	currentBrush = new Brush(Point(5, 5), CIRCLE_BRUSH);

#ifdef LUACONSOLE
	luacon_open();
#endif

	TRON_init_graphics();
	init_color_boxes();

	for (int i = 1; i < argc; i++)
	{
		if (!strncmp(argv[i], "ddir", 5) && i+1<argc)
		{
			int ret = chdir(argv[i+1]);
			if (ret)
				std::cout << "Error, could not change directory\n";
			i++;
		}
		else if (!strncmp(argv[i], "ptsave", 7) && i+1<argc)
		{
			//Prevent reading of any arguments after ptsave for security
			i++;
			argc = i+2;
			break;
		}
		else if (!strncmp(argv[i], "open", 5) && i+1<argc)
		{
			int openDataSize;
			char *openData = (char*)file_load(argv[i+1], &openDataSize);
			saveDataOpen = new Save(openData, openDataSize);
			free(openData);
			i++;
		}
	}

	timesplayed = timesplayed + 1;
	if (DEBUG_MODE)
		memcpy(currentHud,debugHud,sizeof(currentHud));
	else
		memcpy(currentHud,normalHud,sizeof(currentHud));
	memset(parts, 0, sizeof(particle)*NPART);
	clear_sim();

	for (int i = 1; i < argc; i++)
	{
		if (!strncmp(argv[i], "scale:", 6))
		{
			sdl_scale = (argv[i][6]=='2') ? 2 : 1;
		}
		else if (!strncmp(argv[i], "proxy:", 6))
		{
			memset(http_proxy_string, 0, sizeof(http_proxy_string));
			strncpy(http_proxy_string, argv[i]+6, 255);
		}
		else if (!strncmp(argv[i], "nohud", 5))
		{
			hud_enable = false;
		}
		else if (!strncmp(argv[i], "kiosk", 5))
		{
			kiosk_enable = 1;
			//sdl_scale = 2; //Removed because some displays cannot handle the resolution
			hud_enable = false;
		}
		else if (!strncmp(argv[i], "scripts", 8))
		{
			file_script = 1;
		}
		else if (!strncmp(argv[i], "open", 5) && i+1<argc)
		{
			i++;
		}
		else if (!strncmp(argv[i], "ddir", 5) && i+1<argc)
		{
			i++;
		}
		else if (!strncmp(argv[i], "ptsave", 7) && i+1<argc)
		{
			int ci = 0, ns = 0, okay = 0, newprocess = 1;
			char * tempString = argv[i+1];
			int tempStringLength = strlen(argv[i+1])-7;
			int tempSaveID = 0;
			char tempNumberString[32];
			puts("Got ptsave");
			i++;
			tempNumberString[31] = 0;
			tempNumberString[0] = 0;
			if (!strncmp(tempString, "noopen:", 7))
				newprocess = 0;
			if ((!strncmp(tempString, "ptsave:", 7) && tempStringLength) || !newprocess)
			{
				puts("ptsave:// protocol");
				tempString+=7;
				while(tempString[ci] && ns<30 && ci<tempStringLength)
				{
					if(tempString[ci]>=48 && tempString[ci]<=57)
					{
						tempNumberString[ns++] = tempString[ci];
						tempNumberString[ns] = 0;
					}
					else if(tempString[ci]=='#')
					{
						okay = 1;
						break;
					}
					else
					{
						puts("ptsave: invalid save ID");
						break;
					}
					ci++;
				}
				if(!tempString[ci])
				{
					okay = 1;
				}
				if(okay)
				{
					tempSaveID = atoi(tempNumberString);
				}
			}
			if(tempSaveID > 0)
			{
#ifdef WIN
				if (newprocess)
				{
					HWND modWindow = FindWindow(NULL, "Jacob1's Mod");
					if (modWindow)
					{
						//tell the already open window to open the save instead
						SendMessage(modWindow, WM_USER+614, (WPARAM)NULL, (LPARAM)tempSaveID);
						exit(0);
					}
				}
#endif
				puts("Got ptsave:id");
				saveURIOpen = tempSaveID;
				UpdateToolTip(it_msg, Point(16, 20), INTROTIP, 0);
			}
			break;
		}
		else if (!strcmp(argv[i], "benchmark"))
		{
			benchmark_enable = true;
			if (i+1<argc)
			{
				benchmark_file = argv[i+1];
				i++;
			}
		}
	}

	stamp_init();

	if (!sdl_open())
	{
		sdl_scale = 1;
		kiosk_enable = 0;
		if (!sdl_open()) exit(1);
	}
	save_presets();
	http_init(http_proxy_string[0] ? http_proxy_string : NULL);

	prepare_alpha(1.0f);
	prepare_graphicscache();
	flm_data = generate_gradient(flm_data_colours, flm_data_pos, flm_data_points, 200);
	plasma_data = generate_gradient(plasma_data_colours, plasma_data_pos, plasma_data_points, 200);

#ifdef LUACONSOLE
	lua_vid_buf = the_game->GetVid()->GetVid();
	char *autorun_result = NULL;
	if (file_exists("autorun.lua") && luacon_eval("dofile(\"autorun.lua\")", &autorun_result)) //Autorun lua script
	{
		luacon_log(mystrdup(luacon_geterror()));
	}
	if (autorun_result)
	{
		luacon_log(autorun_result);
	}

	if (file_exists("scriptmanager.lua")) //Script manager updates
	{
		if (luacon_eval("dofile(\"scriptmanager.lua\")", &autorun_result))
		{
			//scriptmanager.lua errored, log error and open the included one
			luacon_log(mystrdup(luacon_geterror()));
			luacon_openscriptmanager();
		}
	}
	else
		luacon_openscriptmanager();
	// TPTMP through the script manager, use that version instead
	if (!file_exists("scripts/downloaded/2 cracker64-TPTMulti.lua"))
		luacon_openmultiplayer();
#endif
	for (int i = 0; i < 10; i++)
	{
		sprintf(tabNames[i], "Untitled Simulation %i", i+1);
		tabThumbnails[i] = NULL;
	}
	if (tab_load(1, true))
	{
		char fn[64];
		for (int i = 2; i <= 9; i++)
		{
			sprintf(fn, "tabs" PATH_SEP "%d.stm", i);
			if (file_exists(fn))
				num_tabs++;
			else
				break;
		}
	}

	if (benchmark_enable)
	{
		benchmark_run();
		exit(0);
	}

	UpdateToolTip(it_msg, Point(16, 20), INTROTIP, 10235);

	Engine::Ref().ShowWindow(the_game);
	Engine::Ref().MainLoop();
	//delete engine;

	return 0;
}

	//while (!sdl_poll()) //the main loop
int main_loop_temp(int b, int bq, int sdl_key, int sdl_rkey, unsigned short sdl_mod, int x, int y, int sdl_wheel)
{
	if (true)
	{
		main_loop = 2;
		//change mouse position while it is in a zoom window
		//tmpMouseInZoom is used later, so that it only interrupts drawing, not things like copying / saving stamps
		Point cursor = the_game->AdjustCoordinates(Point(x, y));
		int mx = cursor.X;
		int my = cursor.Y;
		bool tmpMouseInZoom = false;
		if (x >= 0 && y >= 0 && x < XRES && y < YRES)
			tmpMouseInZoom = (x != mx || y != my);

		if(ngrav_enable && (display_mode & DISPLAY_WARP))
		{
			part_vbuf = part_vbuf_store;
			memset(vid_buf, 0, (XRES+BARSIZE)*YRES*PIXELSIZE);
		} else {
			part_vbuf = vid_buf;
		}

		render_before(part_vbuf, globalSim);
		globalSim->Tick(); //update everything
		render_after(part_vbuf, vid_buf, globalSim, Point(mx, my));

		// draw preview of stamp
		if (the_game->GetStampState() == PowderToy::LOAD && !the_game->PlacingZoomWindow())
		{
			Point loadPos = the_game->GetStampPos();
			Point loadSize = the_game->GetStampSize();
			pixel *img = the_game->GetStampImg();
			if (img)
			{
				draw_image(vid_buf, img, loadPos.X, loadPos.Y, loadSize.X, loadSize.Y, 128);
				xor_rect(vid_buf, loadPos.X, loadPos.Y, loadSize.X, loadSize.Y);
			}
		}
		// draw dotted lines for selection
		// other modes besides LOAD and NONE are SAVE, CUT, and COPY, which all select an area like this
		else if (the_game->GetStampState() != PowderToy::NONE && !the_game->PlacingZoomWindow())
		{
			if (the_game->GetStampState() != PowderToy::LOAD && the_game->PlacedInitialStampCoordinate())
			{
				Point savePos = the_game->GetSavePos();
				Point saveSize = the_game->GetSaveSize();

				if (saveSize.X < 0)
				{
					savePos.X = savePos.X + saveSize.X - 1;
					saveSize.X = abs(saveSize.X) + 2;
				}
				if (saveSize.Y < 0)
				{
					savePos.Y = savePos.Y + saveSize.Y - 1;
					saveSize.Y = abs(saveSize.Y) + 2;
				}
				// dim the area not inside the selected area
				fillrect(vid_buf, -1, -1, savePos.X+1, YRES+1, 0, 0, 0, 100);
				fillrect(vid_buf, savePos.X-1, -1, saveSize.X+1, savePos.Y+1, 0, 0, 0, 100);
				fillrect(vid_buf, savePos.X-1, savePos.Y+saveSize.Y-1, saveSize.X+1, YRES-savePos.Y-saveSize.Y+1, 0, 0, 0, 100);
				fillrect(vid_buf, savePos.X+saveSize.X-1, -1, XRES-savePos.X-saveSize.X+1, YRES+1, 0, 0, 0, 100);
				// dotted rectangle
				xor_rect(vid_buf, savePos.X, savePos.Y, saveSize.X, saveSize.Y);
			}
			else
				fillrect(vid_buf, -1, -1, XRES+1, YRES+1, 0, 0, 0, 100);
		}

		// draw normal cursor
		if (!the_game->MouseClicksIgnored())
		{
			PowderToy::DrawState drawState = the_game->GetDrawState();
			Point initialDrawPoint = the_game->GetInitialDrawPoint();
			Point snappedCursor = the_game->AdjustCoordinates(Point(x, y));
			bool isMouseDown = the_game->IsMouseDown();
			if (sdl_mod & KMOD_ALT)
			{
				if (drawState == PowderToy::LINE)
					snappedCursor = the_game->LineSnapCoords(initialDrawPoint, snappedCursor);
				else if (drawState == PowderToy::RECT)
					snappedCursor = the_game->RectSnapCoords(initialDrawPoint, snappedCursor);
			}
			initialDrawPoint = the_game->SnapCoordinatesWall(initialDrawPoint, snappedCursor);
			snappedCursor = the_game->SnapCoordinatesWall(snappedCursor, initialDrawPoint);
			if (drawState != PowderToy::RECT || !isMouseDown)
			{
				if (isMouseDown && (sdl_mod & KMOD_ALT))
					render_cursor(vid_buf, snappedCursor.X, snappedCursor.Y, activeTools[the_game->GetToolIndex()], currentBrush);
				else if ((x < XRES && y < YRES) || isMouseDown)
					render_cursor(vid_buf, mx, my, activeTools[the_game->GetToolIndex()], currentBrush);
			}

			if (isMouseDown)
			{
				if (drawState == PowderToy::LINE)
					xor_line(initialDrawPoint.X, initialDrawPoint.Y, snappedCursor.X, snappedCursor.Y, vid_buf);
				else if (drawState == PowderToy::RECT)
				{
					int width = snappedCursor.X-initialDrawPoint.X;
					int height = snappedCursor.Y-initialDrawPoint.Y;
					if (width < 0)
					{
						initialDrawPoint.X += width;
						width *= -1;
					}
					if (height < 0)
					{
						initialDrawPoint.Y += height;
						height *= -1;
					}
					xor_line(initialDrawPoint.X, initialDrawPoint.Y, initialDrawPoint.X+width, initialDrawPoint.Y, vid_buf);
					if (height > 0)
					{
						xor_line(initialDrawPoint.X, initialDrawPoint.Y+height, initialDrawPoint.X+width, initialDrawPoint.Y+height, vid_buf);
						if (height > 1)
						{
							xor_line(initialDrawPoint.X, initialDrawPoint.Y+1, initialDrawPoint.X, initialDrawPoint.Y+height-1, vid_buf);
							if (width > 0)
								xor_line(initialDrawPoint.X+width, initialDrawPoint.Y+1, initialDrawPoint.X+width, initialDrawPoint.Y+height-1, vid_buf);
						}
					}
				}
			}
		}
		if (the_game->ZoomWindowShown())
			render_zoom(vid_buf);

		// Only update air if not paused
		if (!sys_pause||framerender)
		{
			globalSim->air->UpdateAir();
			globalSim->air->UpdateAirHeat();
		}

		if (gravwl_timeout)
		{
			if (gravwl_timeout == 1)
				gravity_mask();
			gravwl_timeout--;
		}
		
		gravity_update_async(); //Check for updated velocity maps from gravity thread
		if (!sys_pause||framerender) //Only update if not paused
			memset(gravmap, 0, (XRES/CELL)*(YRES/CELL)*sizeof(float)); //Clear the old gravmap

		if (framerender)
			framerender--;

		memset(vid_buf+((XRES+BARSIZE)*YRES), 0, (PIXELSIZE*(XRES+BARSIZE))*MENUSIZE);//clear menu areas
		clearrect(vid_buf, XRES, 1, BARSIZE, YRES-1);

		DrawToolTips();
		
		if(debug_flags)
		{
			PowderToy::DrawState drawState = the_game->GetDrawState();
			Point initialDrawPoint = the_game->AdjustCoordinates(the_game->GetInitialDrawPoint());
			Point snappedCursor = the_game->AdjustCoordinates(Point(x, y));
			if (sdl_mod & KMOD_ALT)
			{
				if (drawState == PowderToy::LINE)
					snappedCursor = the_game->LineSnapCoords(initialDrawPoint, snappedCursor);
				else if (drawState == PowderToy::RECT)
					snappedCursor = the_game->RectSnapCoords(initialDrawPoint, snappedCursor);

			}
			draw_debug_info(vid_buf, globalSim, initialDrawPoint.X, initialDrawPoint.Y, x, y, snappedCursor.X, snappedCursor.Y);
		}

		if (saveDataOpen)
		{
			//Clear all settings and simulation data
			NewSim();
			UpdateToolTip(it_msg, Point(16, 20), INTROTIP, 0);

			try
			{
				globalSim->LoadSave(0, 0, saveDataOpen, 1);
				authors = saveDataOpen->authors;
				the_game->SetReloadPoint(saveDataOpen);
			}
			catch (ParseException e)
			{
				Engine::Ref().ShowWindow(new ErrorPrompt("Unable to open save file", e.what()));
			}
			delete saveDataOpen;
		}

		if(saveURIOpen)
		{
			char saveURIOpenString[512];
			sprintf(saveURIOpenString, "%d", saveURIOpen);
			open_ui(vid_buf, saveURIOpenString, NULL, 0);
			saveURIOpen = 0;
		}
		if (!ptsaveOpenID)
			ptsaveOpenID = Platform::CheckLoadedPtsave();
		if (ptsaveOpenID)
		{
			if (num_tabs < 22-GetNumMenus())
			{
				char ptsaveOpenString[512];
				int oldTabNum = tab_num;
				tab_save(tab_num);
				num_tabs++;
				tab_num = num_tabs;
				NewSim();
				sprintf(ptsaveOpenString, "%d", ptsaveOpenID);

				//if they didn't open it, don't make the new tab
				if (!open_ui(vid_buf, ptsaveOpenString, NULL, 0))
				{
					num_tabs--;
					tab_num = oldTabNum;
					tab_load(oldTabNum);
				}
				tab_save(tab_num);
			}
			ptsaveOpenID = 0;
		}

		if (!deco_disablestuff && sys_shortcuts==1)//all shortcuts can be disabled by lua scripts
		{
			if (sdl_key)
				UpdateToolTip(it_msg, Point(16, 20), INTROTIP, 255);
			if (the_game->GetStampState() != PowderToy::LOAD)
				((STKM_ElementDataContainer*)globalSim->elementData[PT_STKM])->HandleKeys(sdl_key, sdl_rkey);
			if (sdl_key=='f')
			{
				if (debug_flags & DEBUG_PARTICLE_UPDATES)
				{
					the_game->SetPause(1);
					if (sdl_mod & (KMOD_ALT))
					{
						ParticleDebug(0, 0, 0);
					}
					else if (sdl_mod & KMOD_SHIFT)
					{
						ParticleDebug(1, mx, my);
					}
					else if (sdl_mod & (KMOD_CTRL|KMOD_META))
					{
						if (!(finding & 0x1))
							finding |= 0x1;
						else
							finding &= ~0x1;
					}
					else
					{
						if  (globalSim->debug_currentParticle)
							ParticleDebug(1, -1, -1);
						else
							framerender = 1;
					}
				}
				else
				{
					if (sdl_mod & (KMOD_CTRL|KMOD_META))
					{
						if (!(finding & 0x1))
							finding |= 0x1;
						else
							finding &= ~0x1;
					}
					else
					{
						the_game->SetPause(1);
						if  (globalSim->debug_currentParticle)
							ParticleDebug(1, -1, -1);
						else
							framerender = 1;
					}
				}
			}
			if (sdl_key=='s')
			{
				// if stkm2 is out, you must be holding right ctrl, else just either ctrl
				if ((globalSim->elementCount[PT_STKM2]>0 && (sdl_mod&KMOD_RCTRL)) || (globalSim->elementCount[PT_STKM2]<=0 && (sdl_mod&(KMOD_CTRL|KMOD_META))))
					tab_save(tab_num);
			}
			if (sdl_key=='o')
			{
#ifdef TOUCHUI
				catalogue_ui(vid_buf);
#else
				if  (sdl_mod & (KMOD_CTRL|KMOD_META))
				{
					catalogue_ui(vid_buf);
				}
				else
				{
					old_menu = !old_menu;
					if (old_menu)
						UpdateToolTip("Experimental old menu activated, press 'o' to turn off", Point(XCNTR-textwidth("Experimental old menu activated, press 'o' to turn off")/2, YCNTR-10), INFOTIP, 500);
				}
#endif
			}
			if(sdl_key=='e')
			{
				element_search_ui(vid_buf, &activeTools[0], &activeTools[1]);
			}
			if (active_menu == SC_FAV2 && (sdl_mod & KMOD_RCTRL) && (sdl_mod & KMOD_RSHIFT))
			{
				active_menu = SC_CRACKER;
			}
			if (sdl_key==SDLK_TAB)
			{
				if (!(sdl_mod & KMOD_CTRL))
					currentBrush->SetShape((currentBrush->GetShape()+1)%BRUSH_NUM);
			}
			if (sdl_key==SDLK_LEFTBRACKET) {
				if (!the_game->PlacingZoomWindow())
				{
					if (sdl_mod & (KMOD_LALT|KMOD_RALT) && !(sdl_mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_META)))
					{
						currentBrush->ChangeRadius(Point(-1, -1));
					}
					else if (sdl_mod & (KMOD_SHIFT) && !(sdl_mod & (KMOD_CTRL|KMOD_META)))
					{
						currentBrush->ChangeRadius(Point(-1, 0));
					}
					else if (sdl_mod & (KMOD_CTRL|KMOD_META) && !(sdl_mod & (KMOD_SHIFT)))
					{
						currentBrush->ChangeRadius(Point(0, -1));
					}
					else
					{
						currentBrush->ChangeRadius(Point(-(int)ceil((currentBrush->GetRadius().X/5)+0.5f), -(int)ceil((currentBrush->GetRadius().Y/5)+0.5f)));
					}
				}
			}
			if (sdl_key==SDLK_RIGHTBRACKET) {
				if (!the_game->PlacingZoomWindow())
				{
					if (sdl_mod & (KMOD_LALT|KMOD_RALT) && !(sdl_mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_META)))
					{
						currentBrush->ChangeRadius(Point(1, 1));
					}
					else if (sdl_mod & (KMOD_SHIFT) && !(sdl_mod & (KMOD_CTRL|KMOD_META)))
					{
						currentBrush->ChangeRadius(Point(1, 0));
					}
					else if (sdl_mod & (KMOD_CTRL|KMOD_META) && !(sdl_mod & (KMOD_SHIFT)))
					{
						currentBrush->ChangeRadius(Point(0, 1));
					}
					else
					{
						currentBrush->ChangeRadius(Point((int)ceil((currentBrush->GetRadius().X/5)+0.5f), (int)ceil((currentBrush->GetRadius().Y/5)+0.5f)));
					}
				}
			}
			if ((sdl_key=='d' && ((sdl_mod & (KMOD_CTRL|KMOD_META)) || globalSim->elementCount[PT_STKM2]<=0)) || sdl_key == SDLK_F3)
			{
				DEBUG_MODE = !DEBUG_MODE;
				SetCurrentHud();
			}
			if (sdl_key==SDLK_INSERT)
				REPLACE_MODE = !REPLACE_MODE;
			else if (sdl_key == SDLK_DELETE && active_menu != SC_DECO)
				SPECIFIC_DELETE = !SPECIFIC_DELETE;
			else if (sdl_key == SDLK_SEMICOLON)
			{
				if (sdl_mod&(KMOD_CTRL|KMOD_META))
					SPECIFIC_DELETE = !SPECIFIC_DELETE;
				else
					REPLACE_MODE = !REPLACE_MODE;
			}
			if (sdl_key==SDLK_BACKQUOTE)
			{
				console_mode = !console_mode;
				//hud_enable = !console_mode;
			}
			if (sdl_key=='g')
			{
				if(sdl_mod & (KMOD_CTRL|KMOD_META))
				{
					drawgrav_enable =! drawgrav_enable;
				}
				else
				{
					if (sdl_mod & (KMOD_SHIFT))
						GRID_MODE = (GRID_MODE+9)%10;
					else
						GRID_MODE = (GRID_MODE+1)%10;
				}
			}
			if (sdl_key=='=')
			{
				if (sdl_mod & (KMOD_CTRL|KMOD_META))
				{
					for (int i = 0; i < NPART; i++)
						if (parts[i].type == PT_SPRK)
						{
							if (parts[i].ctype >= 0 && parts[i].ctype < PT_NUM && ptypes[parts[i].ctype].enabled)
							{
								parts[i].type = parts[i].ctype;
								parts[i].life = parts[i].ctype = 0;
							}
							else
								kill_part(i);
						}
					globalSim->elementData[PT_WIFI]->Simulation_Cleared(globalSim);
				}
				else
				{
					for (int nx = 0; nx < XRES/CELL; nx++)
						for (int ny = 0; ny < YRES/CELL; ny++)
						{
							globalSim->air->pv[ny][nx] = 0;
							globalSim->air->vx[ny][nx] = 0;
							globalSim->air->vy[ny][nx] = 0;
						}
					for (int i = 0; i < NPART; i++)
						if (parts[i].type == PT_QRTZ || parts[i].type == PT_GLAS || parts[i].type == PT_TUNG)
						{
							parts[i].pavg[0] = parts[i].pavg[1] = 0;
						}
				}
			}

			if (sdl_key=='w' && (globalSim->elementCount[PT_STKM2]<=0 || (sdl_mod & (KMOD_CTRL|KMOD_META)))) //Gravity, by Moach
			{
				++gravityMode; // cycle gravity mode

				std::string toolTip;
				switch (gravityMode)
				{
				default:
					gravityMode = 0;
				case 0:
					toolTip = "Gravity: Vertical";
					break;
				case 1:
					toolTip = "Gravity: Off";
					break;
				case 2:
					toolTip = "Gravity: Radial";
					break;
				}
				UpdateToolTip(toolTip, Point(XCNTR-textwidth(toolTip.c_str())/2, YCNTR-10), INFOTIP, 255);
			}
			if (sdl_key=='y' && !(sdl_mod & (KMOD_CTRL|KMOD_META)))
			{
				++airMode;

				std::string toolTip;
				switch (airMode)
				{
				default:
					airMode = 0;
				case 0:
					toolTip = "Air: On";
					break;
				case 1:
					toolTip = "Air: Pressure Off";
					break;
				case 2:
					toolTip = "Air: Velocity Off";
					break;
				case 3:
					toolTip = "Air: Off";
					break;
				case 4:
					toolTip = "Air: No Update";
					break;
				}
				UpdateToolTip(toolTip, Point(XCNTR-textwidth(toolTip.c_str())/2, YCNTR-10), INFOTIP, 255);
			}

			if (sdl_key=='t')
				show_tabs = !show_tabs;
			if (sdl_key=='u')
			{
				if (sdl_mod & (KMOD_CTRL|KMOD_META))
					Platform::OpenLink("http://powdertoy.co.uk/Discussions/Thread/View.html?Thread=11117");
				else
					aheat_enable = !aheat_enable;

			}
			if (sdl_key=='h' && !(sdl_mod & (KMOD_CTRL|KMOD_META)))
			{
				hud_enable = !hud_enable;
			}
			if (sdl_key==SDLK_F1 || (sdl_key=='h' && (sdl_mod & (KMOD_CTRL|KMOD_META))))
			{
				if (!GetToolTipAlpha(INTROTIP))
				{
					UpdateToolTip(it_msg, Point(16, 20), INTROTIP, 10235);
				}
				else
				{
					UpdateToolTip(it_msg, Point(16, 20), INTROTIP, 0);
				}
			}
			if (sdl_key=='n')
			{
				if (sdl_mod & (KMOD_CTRL|KMOD_META))
				{
					if (num_tabs < 22-GetNumMenus())
					{
						tab_save(tab_num);
						num_tabs++;
						tab_num = num_tabs;
						NewSim();
						tab_save(tab_num);
					}
				}
				else
				{
					if(ngrav_enable)
						stop_grav_async();
					else
						start_grav_async();
				}
			}
			if (sdl_key=='p' || sdl_key == SDLK_F2)
				dump_frame(vid_buf, XRES, YRES, XRES+BARSIZE);
			//TODO: Superseded by new display mode switching, need some keyboard shortcuts
			/*else if (sdl_key=='c')
			{
				set_cmode((cmode+1) % CM_COUNT);
				if (it > 50)
					it = 50;
			}*/
			if (!the_game->MouseClicksIgnored())
			{
				if (sdl_key == SDLK_UP && (sdl_mod & KMOD_CTRL) && tab_num > 1)
				{
					tab_save(tab_num);
					tab_num--;
					tab_load(tab_num);
				}
				else if (sdl_key == SDLK_DOWN && (sdl_mod & KMOD_CTRL) && tab_num < num_tabs)
				{
					tab_save(tab_num);
					tab_num++;
					tab_load(tab_num);
				}
			}
		}
#ifdef INTERNAL
		int counterthing;
		if (sdl_key=='v'&&!(sdl_mod & (KMOD_CTRL|KMOD_META)))//frame capture
		{
			if (sdl_mod & (KMOD_SHIFT)) {
				if (vs>=1)
					vs = 0;
				else
					vs = 3;//every other frame
			}
			else
			{
				if (vs>=1)
					vs = 0;
				else
					vs = 1;
			}
			counterthing = 0;
		}
		if (vs)
		{
			if (counterthing+1>=vs)
			{
				dump_frame(vid_buf, XRES, YRES, XRES+BARSIZE);
				counterthing = 0;
			}
			counterthing = (counterthing+1)%3;
		}
#endif

		if (svf_messages)
		{
			sprintf(new_message_msg, "You have %d new message%s, Click to view", svf_messages, (svf_messages>1)?"s":"");
			new_message_len = textwidth(new_message_msg);

			clearrect(vid_buf, XRES-20-new_message_len, YRES-38, new_message_len+8, 16);
			drawtext(vid_buf, XRES-16-new_message_len, YRES-34, new_message_msg, 255, 186, 32, 255);
			drawrect(vid_buf, XRES-19-new_message_len, YRES-37, new_message_len+5, 13, 255, 186, 32, 255);
		}

		if (sdl_wheel)
		{
			if (!the_game->PlacingZoomWindow())
			{
				if (!(sdl_mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_META)))
				{
					currentBrush->ChangeRadius(Point(sdl_wheel, sdl_wheel));
				}
				else if (sdl_mod & (KMOD_SHIFT) && !(sdl_mod & (KMOD_CTRL|KMOD_META)))
				{
					currentBrush->ChangeRadius(Point(sdl_wheel, 0));
				}
				else if (sdl_mod & (KMOD_CTRL|KMOD_META) && !(sdl_mod & (KMOD_SHIFT)))
				{
					currentBrush->ChangeRadius(Point(0, sdl_wheel));
				}
			}
		}

#ifdef TOUCHUI
		int hover = DrawMenusTouch(vid_buf, b, bq, x, y);
		if (hover >= 0)
		{
			if (y > 150)
				UpdateToolTip(menuSections[hover]->name, Point(menuStartPosition-5-textwidth(menuSections[hover]->name.c_str()), YRES-67), QTIP, -1);
			if (hover == SC_DECO && active_menu != SC_DECO)
				last_active_menu = active_menu;
			if (hover == SC_FAV)
				active_menu = last_fav_menu;
			else
				active_menu = hover;
		}
		menu_ui_v3(vid_buf, active_menu, b, bq, x, y); //draw the elements in the current menu
#else
		if (!old_menu)
		{
			QuickoptionsMenu(vid_buf, b, bq, x, y);

			int hover = DrawMenus(vid_buf, active_menu, y);
			if (hover >= 0 && x>=menuStartPosition && x<XRES+BARSIZE-1)
			{
				UpdateToolTip(menuSections[hover]->name, Point(menuStartPosition-5-textwidth(menuSections[hover]->name.c_str()), std::min(((y-8)/16)*16+12, YRES-9)), QTIP, -1);
				if (((!menuSections[hover]->click && !b) || (menuSections[hover]->click && b && !bq)))
				{
					if (menuSections[hover]->click && !menuSections[active_menu]->click)
						last_active_menu = active_menu;
					if (hover == SC_FAV)
						active_menu = last_fav_menu;
					else
						active_menu = hover;

					if (hover == SC_SEARCH)
					{
						element_search_ui(vid_buf, &activeTools[0], &activeTools[1]);
						active_menu = last_active_menu;
					}
				}
			}
			menu_ui_v3(vid_buf, active_menu, b, bq, x, y); //draw the elements in the current menu
		}
		else
			old_menu_v2(active_menu, x, y, b, bq);
#endif

		//TODO: only when entering / exiting menu
		if (active_menu == SC_DECO && activeTools[0]->GetType() != DECO_TOOL)
		{
			the_game->SwapToDecoToolset();
		}
		else if (active_menu != SC_DECO && activeTools[0]->GetType() == DECO_TOOL)
		{
			the_game->SwapToRegularToolset();
			deco_disablestuff = 0;
		}
		if (deco_disablestuff)
			b = 0;

		sdl_wheel = 0;

		if (b && !bq && x>=(XRES-19-new_message_len) &&
		        x<=(XRES-14) && y>=(YRES-37) && y<=(YRES-24) && svf_messages)
		{
			if (b == 1)
				Platform::OpenLink("http://" SERVER "/Conversations.html");
			svf_messages = 0;
			b = 0;
		}
		if (update_flag)
		{
			info_box(vid_buf, "Finalizing update...");
			if (update_finish())
				Engine::Ref().ShowWindow(new ErrorPrompt("Update failed - try downloading a new version."));
			else
				Engine::Ref().ShowWindow(new InfoPrompt("Update success", "You have successfully updated Jacob1's Mod!"));
			update_flag = 0;
		}

		if (mx < XRES && my < YRES)
		{
			int signID = InsideSign(globalSim, mx, my, false);
			if (signID != -1 && signs[signID]->GetType() != Sign::Normal && signs[signID]->GetType() != Sign::Spark)
			{
				std::stringstream tooltip;
				switch (signs[signID]->GetType())
				{
				case Sign::SaveLink:
					tooltip << "Go to save ID:" << signs[signID]->GetLinkText();
					break;
				case Sign::ThreadLink:
					tooltip << "Open forum thread " << signs[signID]->GetLinkText() << " in browser";
					break;
				case Sign::SearchLink:
					tooltip << "Search for " << signs[signID]->GetLinkText();
					break;
				default:
					break;
				}
				UpdateToolTip(tooltip.str(), Point(16, YRES-24), TOOLTIP, -1);
			}
		}

		//mouse clicks ignored when placing zoom, or when loading / saving stamps
		if (the_game->MouseClicksIgnored())
		{
			// certain thing the main window handles need prevent stuff from being drawn to the screen
		}
		//there is a click
		else if (b)
		{
			UpdateToolTip(it_msg, Point(16, 20), INTROTIP, 255);

			/*if (tmpMouseInZoom != mouseInZoom && !lm)
				b = lb = 0;
			else if ((y<YRES && x<XRES) || lb)// mouse is in playing field
			{
				activeToolID = ((b&1) || b == 2) ? 0 : 1;
				Tool* activeTool = activeTools[activeToolID];
				if (activeTools[0]->GetType() == DECO_TOOL && b == 4)
					activeTool = GetToolFromIdentifier("DEFAULT_DECOR_CLR");

				//for the click functions, lx and ly, are the positions of where the FIRST click happened.  mx,my are current mouse position.
				if (lb) //lb means you are holding mouse down
				{
					toolStrength = 1.0f;
					if (lm == 1)//line tool
					{
						if (sdl_mod & KMOD_ALT)
						{
							Point point1 = Point(lx, ly), point2 = Point(mx, my);
							Point diff = point2 - point1, line;
							if (abs(diff.X / 2) > abs(diff.Y)) // vertical
								line = point1 + Point(diff.X, 0);
							else if(abs(diff.X) < abs(diff.Y / 2)) // horizontal
								line = point1 + Point(0, diff.Y);
							else if(diff.X * diff.Y > 0) // NW-SE
								line = point1 + Point((diff.X + diff.Y)/2, (diff.X + diff.Y)/2);
							else // SW-NE
								line = point1 + Point((diff.X - diff.Y)/2, (diff.Y - diff.X)/2);
							line_x = line.X;
							line_y = line.Y;
						}
						else
						{
							line_x = mx;
							line_y = my;
						}
						//WIND tool works before you even draw the line while holding shift
						if (((ToolTool*)activeTool)->GetID() == TOOL_WIND)
						{
							for (int j = -currentBrush->GetRadius().Y; j <= currentBrush->GetRadius().Y; j++)
								for (int i = -currentBrush->GetRadius().X; i <= currentBrush->GetRadius().X; i++)
									if (lx+i>0 && ly+j>0 && lx+i<XRES && ly+j<YRES && currentBrush->IsInside(i, j))
									{
										vx[(ly+j)/CELL][(lx+i)/CELL] += (line_x-lx)*0.002f;
										vy[(ly+j)/CELL][(lx+i)/CELL] += (line_y-ly)*0.002f;
									}
						}
					}
					else if (lm == 2)//box tool
					{
						if (sdl_mod & KMOD_ALT)
						{
							Point point1 = Point(lx, ly), point2 = Point(mx, my);
							Point diff = point2 - point1, line;
							if (diff.X * diff.Y > 0) // NW-SE
								line = point1 + Point((diff.X + diff.Y)/2, (diff.X + diff.Y)/2);
							else // SW-NE
								line = point1 + Point((diff.X - diff.Y)/2, (diff.Y - diff.X)/2);
							line_x = line.X;
							line_y = line.Y;
						}
						else
						{
							line_x = mx;
							line_y = my;
						}
					}
					//flood fill
					else if (lm == 3)
					{
						if (!((sdl_mod&(KMOD_CTRL|KMOD_META)) && (sdl_mod&KMOD_SHIFT)))
							lb = 0;
						else
							activeTool->FloodFill(globalSim, currentBrush, Point(mx, my));
					}
					//while mouse is held down, this draws lines between previous and current positions
					else if (!lm)
					{
						if (sdl_mod & KMOD_SHIFT)
							toolStrength = 10.0f;
						else if (sdl_mod & (KMOD_CTRL|KMOD_META))
							toolStrength = .1f;
						activeTool->DrawLine(globalSim, currentBrush, Point(lx, ly), Point(mx, my), true, toolStrength);
						lx = mx;
						ly = my;
					}
				}
				//it is the first click (don't start drawing if we are moving a sign though)
				else if (!bq && MSIGN == -1 && InsideSign(mx, my, sdl_mod&KMOD_CTRL) == -1)
				{
					toolStrength = 1.0f;
					lx = line_x = mx;
					ly = line_y = my;
					lb = b;
					lm = 0;
					//start line tool
					if ((sdl_mod & (KMOD_SHIFT)) && !(sdl_mod & (KMOD_CTRL|KMOD_META)))
					{
						lm = 1;//line
					}
					//start box tool
					else if ((sdl_mod & (KMOD_CTRL|KMOD_META)) && !(sdl_mod & (KMOD_SHIFT)))
					{
						lm = 2;//box
					}
					//flood fill
					else if ((sdl_mod & (KMOD_CTRL|KMOD_META)) && (sdl_mod & (KMOD_SHIFT)) && (((ToolTool*)activeTool)->GetID() == -1 || ((ToolTool*)activeTool)->GetID() == TOOL_PROP))
					{
						ctrlzSnapshot();
						activeTool->FloodFill(globalSim, currentBrush, Point(mx, my));
						lm = 3;
					}
					//sample
					else if (((sdl_mod & (KMOD_ALT)) && !(sdl_mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_META))) || b==SDL_BUTTON_MIDDLE)
					{
						activeTools[activeToolID] = activeTool->Sample(globalSim, Point(mx, my));
						lb = 0;
					}
					else //normal click, spawn element
					{
						//Copy state before drawing any particles (for undo)
						ctrlzSnapshot();

						//get tool strength, shift (or ctrl+shift) makes it faster and ctrl makes it slower
						if (sdl_mod & KMOD_SHIFT)
							toolStrength = 10.0f;
						else if (sdl_mod & (KMOD_CTRL|KMOD_META))
							toolStrength = .1f;

						activeTool->DrawPoint(globalSim, currentBrush, Point(mx, my), toolStrength);
					}
				}
			}*/
		}
		else
		{
			/*if (lb)
			{
				activeToolID = ((lb&1) || lb == 2) ? 0 : 1;
				Tool* activeTool = activeTools[activeToolID];
				//lm is box/line tool (3 is floodfill)
				if (lm)
				{
					if (lm != 3)
					{
						ctrlzSnapshot();
						if (activeTools[0]->GetType() == DECO_TOOL && lb == 4)
							activeTool = GetToolFromIdentifier("DEFAULT_DECOR_CLR");

						if (lm == 1)
							activeTool->DrawLine(globalSim, currentBrush, Point(lx, ly), Point(line_x, line_y), false, 1.0f);
						else if (lm == 2)
							activeTool->DrawRect(globalSim, currentBrush, Point(lx, ly), Point(line_x, line_y));
						lm = 0;
					}
				}
				else
				{
					//plop tool (STKM, STKM2, FIGH)
					activeTool->Click(globalSim, Point(lx, ly));
				}
			}
			else if (bq)
			{

			}
			lb = 0;*/
		}
		mouseInZoom = tmpMouseInZoom;

		if (elapsedTime != currentTime && main_loop == 2)
		{
			frames = frames + 1;
			totalfps = totalfps + FPSB2;
		}
		if (lastx == x && lasty == y)
		{
			if (!afk)
			{
				afk = 1;
				afkstart = currentTime;
			}
		}
		else if (afk)
		{
			afk = 0;
			totalafktime = totalafktime + afktime;
			afktime = 0;
		}
		if (afk && currentTime - afkstart > 30000)
			afktime = currentTime - afkstart - 30000;
		lastx = x;
		lasty = y;

		if (autosave > 0 && frames%autosave == 0)
		{
			tab_save(1);
		}
		if (hud_enable)
		{
			SetLeftHudText(globalSim, FPSB2);
			SetRightHudText(globalSim, x, y);

			DrawHud(GetToolTipAlpha(INTROTIP), GetToolTipAlpha(QTIP));

			if (drawinfo)
				DrawRecordsInfo(globalSim);

			DrawLuaLogs();
		}

		if (console_mode)
		{
			openConsole = true;
		}

		//sdl_blit(0, 0, XRES+BARSIZE, YRES+MENUSIZE, vid_buf, XRES+BARSIZE);

#if !defined(DEBUG) && !defined(_DEBUG)
		if (!signal_hooks)
		{
			signal(SIGSEGV, SigHandler);
			signal(SIGFPE, SigHandler);
			signal(SIGILL, SigHandler);
			signal(SIGABRT, SigHandler);
			signal_hooks = 1;
		}
#endif
	}
	return true;
}

void main_end_hack()
{
	SaveWindowPosition();
	save_presets();
	DownloadManager::Ref().Shutdown();
	http_done();
	gravity_cleanup();
#ifdef LUACONSOLE
	luacon_close();
#endif
	ClearMenusections();
	ClearSigns();
	delete currentBrush;
	for (int i = toolTips.size()-1; i >= 0; i--)
		delete toolTips[i];
	toolTips.clear();
#ifdef PTW32_STATIC_LIB
	pthread_win32_thread_detach_np();
	pthread_win32_process_detach_np();
#endif
	if (part_vbuf_store)
		free(part_vbuf_store);
	if (vid3d)
		free(vid3d);
	for (int i = 1; i < 10; i++)
	{
		char name[30] = {0};
		sprintf(name,"tabs%s%d.stm",PATH_SEP,i);
		remove(name);
	}
#ifdef WIN
	_rmdir("tabs");
#else
	rmdir("tabs");
#endif
	stamps_free();
}
#endif
