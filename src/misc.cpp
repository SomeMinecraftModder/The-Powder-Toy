/**
 * Powder Toy - miscellaneous functions
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include <sys/types.h>
#include <math.h>
#include "misc.h"
#include "defines.h"
#include "interface.h"
#include "graphics.h"
#include "powdergraphics.h"
#include "powder.h"
#include "gravity.h"
#include "hud.h"
#include "images.h"
#include <update.h>
#include <dirent.h>
#include <sys/stat.h>
#if defined WIN32
#include <shlobj.h>
#include <shlwapi.h>
#include <windows.h>
#include <direct.h>
#else
#include <unistd.h>
#endif
#ifdef MACOSX
#include <ApplicationServices/ApplicationServices.h>
#endif
#include "cJSON.h"

char *clipboard_text = NULL;

//Signum function
TPT_GNU_INLINE int isign(float i)
{
	if (i<0)
		return -1;
	if (i>0)
		return 1;
	return 0;
}

TPT_GNU_INLINE unsigned clamp_flt(float f, float min, float max)
{
	if (f<min)
		return 0;
	if (f>max)
		return 255;
	return (int)(255.0f*(f-min)/(max-min));
}

TPT_GNU_INLINE float restrict_flt(float f, float min, float max)
{
	if (f<min)
		return min;
	if (f>max)
		return max;
	return f;
}

char *mystrdup(const char *s)
{
	char *x;
	if (s)
	{
		x = (char*)malloc(strlen(s)+1);
		strcpy(x, s);
		return x;
	}
	return NULL;
}

void strlist_add(struct strlist **list, char *str)
{
	struct strlist *item = (struct strlist *)malloc(sizeof(struct strlist));
	item->str = mystrdup(str);
	item->next = *list;
	*list = item;
}

int strlist_find(struct strlist **list, char *str)
{
	struct strlist *item;
	for (item=*list; item; item=item->next)
		if (!strcmp(item->str, str))
			return 1;
	return 0;
}

void strlist_free(struct strlist **list)
{
	struct strlist *item;
	while (*list)
	{
		item = *list;
		*list = (*list)->next;
		free(item);
	}
}

void clean_text(char *text, int vwidth)
{
	int i = 0;
	if(vwidth>=0 && textwidth(text) > vwidth){
		text[textwidthx(text, vwidth)] = 0;	
	}
	for(i = 0; i < strlen(text); i++){
		if(! (text[i]>=' ' && text[i]<127)){
			text[i] = ' ';
		}
	}
}

void draw_bframe()
{
	int i;
	for(i=0; i<(XRES/CELL); i++)
	{
		bmap[0][i]=WL_WALL;
		bmap[YRES/CELL-1][i]=WL_WALL;
	}
	for(i=1; i<((YRES/CELL)-1); i++)
	{
		bmap[i][0]=WL_WALL;
		bmap[i][XRES/CELL-1]=WL_WALL;
	}
}

void erase_bframe()
{
	int i;
	for(i=0; i<(XRES/CELL); i++)
	{
		bmap[0][i]=0;
		bmap[YRES/CELL-1][i]=0;
	}
	for(i=1; i<((YRES/CELL)-1); i++)
	{
		bmap[i][0]=0;
		bmap[i][XRES/CELL-1]=0;
	}
}

void save_console_history(cJSON **historyArray, command_history *commandList)
{
	if (!commandList)
		return;
	save_console_history(historyArray, commandList->prev_command);
	cJSON_AddItemToArray(*historyArray, cJSON_CreateString(commandList->command.str));
}

void save_presets(int do_update)
{
	int i = 0, count;
	char * outputdata;
	char mode[32];
	cJSON *root, *userobj, *userobj2, *versionobj, *recobj, *graphicsobj, *graphicsobj2, *hudobj, *simulationobj, *consoleobj, *tmpobj;
	FILE *f = fopen("powder.pref", "wb");
	if(!f)
		return;
	root = cJSON_CreateObject();
	
	cJSON_AddStringToObject(root, "Powder Toy Preferences", "Don't modify this file unless you know what you're doing. P.S: editing the admin/mod fields in your user info doesn't give you magical powers");
	
	//Tpt++ User Info
	if(svf_login){
		cJSON_AddItemToObject(root, "User", userobj2=cJSON_CreateObject());
		cJSON_AddStringToObject(userobj2, "Username", svf_user);
		cJSON_AddNumberToObject(userobj2, "ID", atoi(svf_user_id));
		cJSON_AddStringToObject(userobj2, "SessionID", svf_session_id);
		cJSON_AddStringToObject(userobj2, "SessionKey", svf_session_key);
		if(svf_admin){
			cJSON_AddStringToObject(userobj2, "Elevation", "Admin");
		} else if(svf_mod){
			cJSON_AddStringToObject(userobj2, "Elevation", "Mod");
		} else {
			cJSON_AddStringToObject(userobj2, "Elevation", "None");
		}
	}

	//Tpt++ Renderer settings
	cJSON_AddItemToObject(root, "Renderer", graphicsobj2=cJSON_CreateObject());
	sprintf(mode, "%i", colour_mode);
	cJSON_AddStringToObject(graphicsobj2, "ColourMode", mode);
	if (DEBUG_MODE)
		cJSON_AddTrueToObject(graphicsobj2, "DebugMode");
	else
		cJSON_AddFalseToObject(graphicsobj2, "DebugMode");
	tmpobj = cJSON_CreateStringArray(NULL, 0);
	while(display_modes[i])
	{
		sprintf(mode, "%x", display_modes[i]);
		cJSON_AddItemToArray(tmpobj, cJSON_CreateString(mode));
		i++;
	}
	cJSON_AddItemToObject(graphicsobj2, "DisplayModes", tmpobj);
	tmpobj = cJSON_CreateStringArray(NULL, 0);
	i = 0;
	while(render_modes[i])
	{
		sprintf(mode, "%x", render_modes[i]);
		cJSON_AddItemToArray(tmpobj, cJSON_CreateString(mode));
		i++;
	}
	cJSON_AddItemToObject(graphicsobj2, "RenderModes", tmpobj);
	if (drawgrav_enable)
		cJSON_AddTrueToObject(graphicsobj2, "GravityField");
	else
		cJSON_AddFalseToObject(graphicsobj2, "GravityField");
	if (decorations_enable)
		cJSON_AddTrueToObject(graphicsobj2, "Decorations");
	else
		cJSON_AddFalseToObject(graphicsobj2, "Decorations");
	
	//Tpt++ Simulation setting(s)
	cJSON_AddItemToObject(root, "Simulation", simulationobj=cJSON_CreateObject());
	sprintf(mode, "%i", edgeMode);
	cJSON_AddStringToObject(simulationobj, "EdgeMode", mode);
	cJSON_AddNumberToObject(simulationobj, "NewtonianGravity", ngrav_enable);
	cJSON_AddNumberToObject(simulationobj, "AmbientHeat", aheat_enable);
	cJSON_AddNumberToObject(simulationobj, "PrettyPowder", pretty_powder);

	//Tpt++ install check, prevents annoyingness
	cJSON_AddTrueToObject(root, "InstallCheck");

	//Tpt++ console history
	cJSON_AddItemToObject(root, "Console", consoleobj=cJSON_CreateObject());
	tmpobj = cJSON_CreateStringArray(NULL, 0);
	save_console_history(&tmpobj, last_command);
	cJSON_AddItemToObject(consoleobj, "History", tmpobj);
	tmpobj = cJSON_CreateStringArray(NULL, 0);
	save_console_history(&tmpobj, last_command_result);
	cJSON_AddItemToObject(consoleobj, "HistoryResults", tmpobj);

	//Tpt User info
	if (svf_login)
	{
		cJSON_AddItemToObject(root, "user", userobj=cJSON_CreateObject());
		cJSON_AddStringToObject(userobj, "name", svf_user);
		cJSON_AddStringToObject(userobj, "id", svf_user_id);
		cJSON_AddStringToObject(userobj, "session_id", svf_session_id);
		if(svf_admin){
			cJSON_AddTrueToObject(userobj, "admin");
			cJSON_AddFalseToObject(userobj, "mod");
		} else if(svf_mod){
			cJSON_AddFalseToObject(userobj, "admin");
			cJSON_AddTrueToObject(userobj, "mod");
		} else {
			cJSON_AddFalseToObject(userobj, "admin");
			cJSON_AddFalseToObject(userobj, "mod");
		}
	}

	//Version Info
	cJSON_AddItemToObject(root, "version", versionobj=cJSON_CreateObject());
	cJSON_AddNumberToObject(versionobj, "major", SAVE_VERSION);
	cJSON_AddNumberToObject(versionobj, "minor", MINOR_VERSION);
	cJSON_AddNumberToObject(versionobj, "build", BUILD_NUM);
	if(do_update){
		cJSON_AddTrueToObject(versionobj, "update");
	} else {
		cJSON_AddFalseToObject(versionobj, "update");
	}
	
	//Fav Menu/Records
	cJSON_AddItemToObject(root, "records", recobj=cJSON_CreateObject());
	cJSON_AddNumberToObject(recobj, "num elements in menu", locked);
	for (i = 0; i < locked; i++)
	{
		char eltext[128] = "";
		sprintf(eltext,"element %i",i);
		cJSON_AddNumberToObject(recobj, eltext, favMenu[18-i]);
	}
	cJSON_AddNumberToObject(recobj, "Total Time Played", ((double)currentTime/1000)+((double)totaltime/1000)-((double)totalafktime/1000)-((double)afktime/1000));
	cJSON_AddNumberToObject(recobj, "Average FPS", totalfps/frames);
	cJSON_AddNumberToObject(recobj, "Number of frames", frames);
	cJSON_AddNumberToObject(recobj, "Max FPS", maxfps);
	cJSON_AddNumberToObject(recobj, "Total AFK Time", ((double)totalafktime/1000)+((double)afktime/1000)+((double)prevafktime/1000));
	cJSON_AddNumberToObject(recobj, "Times Played", timesplayed);

	//Display settings
	cJSON_AddItemToObject(root, "graphics", graphicsobj=cJSON_CreateObject());
	cJSON_AddNumberToObject(graphicsobj, "colour", colour_mode);
	count = 0; i = 0; while(display_modes[i++]){ count++; }
	cJSON_AddItemToObject(graphicsobj, "display", cJSON_CreateIntArray((int*)display_modes, count));
	count = 0; i = 0; while(render_modes[i++]){ count++; }
	cJSON_AddItemToObject(graphicsobj, "render", cJSON_CreateIntArray((int*)render_modes, count));

	//HUDs
	cJSON_AddItemToObject(root, "HUD", hudobj=cJSON_CreateObject());
	cJSON_AddItemToObject(hudobj, "normal", cJSON_CreateIntArray(normalHud, HUD_OPTIONS));
	cJSON_AddItemToObject(hudobj, "debug", cJSON_CreateIntArray(debugHud, HUD_OPTIONS));

	//General settings
	cJSON_AddStringToObject(root, "proxy", http_proxy_string);
	cJSON_AddNumberToObject(root, "scale", sdl_scale);
	cJSON_AddNumberToObject(root, "Debug mode", DEBUG_MODE);
	cJSON_AddNumberToObject(root, "heatmode", heatmode);
	cJSON_AddNumberToObject(root, "autosave", autosave);
	cJSON_AddNumberToObject(root, "aheat_enable", aheat_enable);
	cJSON_AddNumberToObject(root, "decorations_enable", decorations_enable);
	cJSON_AddNumberToObject(root, "ngrav_enable", ngrav_enable);
	cJSON_AddNumberToObject(root, "kiosk_enable", kiosk_enable);
	cJSON_AddNumberToObject(root, "realistic", realistic);
	if (unlockedstuff & 0x01)
		cJSON_AddNumberToObject(root, "cracker_unlocked", 1);
	if (unlockedstuff & 0x08)
		cJSON_AddNumberToObject(root, "show_votes", 1);
	if (unlockedstuff & 0x10)
		cJSON_AddNumberToObject(root, "EXPL_unlocked", 1);
	if (old_menu)
		cJSON_AddNumberToObject(root, "old_menu", 1);
	cJSON_AddNumberToObject(root, "save_as", save_as);
	cJSON_AddNumberToObject(root, "drawgrav_enable", drawgrav_enable);
	cJSON_AddNumberToObject(root, "edgeMode", edgeMode);
	if (finding & 0x8)
		cJSON_AddNumberToObject(root, "alt_find", 1);
	cJSON_AddNumberToObject(root, "dateformat", dateformat);
	cJSON_AddNumberToObject(root, "ShowIDs", show_ids);
	cJSON_AddNumberToObject(root, "decobox_hidden", decobox_hidden);
	cJSON_AddNumberToObject(root, "fastquit", fastquit);
	cJSON_AddNumberToObject(root, "doUpdates", doUpdates);
	cJSON_AddNumberToObject(root, "WindowX", savedWindowX);
	cJSON_AddNumberToObject(root, "WindowY", savedWindowY);
	
	outputdata = cJSON_Print(root);
	cJSON_Delete(root);
	
	fwrite(outputdata, 1, strlen(outputdata), f);
	fclose(f);
	free(outputdata);
}

void load_console_history(cJSON *tmpobj, command_history **last_command, int count)
{
	int i;
	command_history *currentcommand = *last_command;
	for(i = 0; i < count; i++)
	{
		currentcommand = (command_history*)malloc(sizeof(command_history));
		memset(currentcommand, 0, sizeof(command_history));
		currentcommand->prev_command = *last_command;
		ui_label_init(&currentcommand->command, 15, 0, 0, 0);
		strncpy(currentcommand->command.str, cJSON_GetArrayItem(tmpobj, i)->valuestring, 1023);
		*last_command = currentcommand;
	}
}

void load_presets(void)
{
	int prefdatasize = 0, i, count;
	char * prefdata = (char*)file_load("powder.pref", &prefdatasize);
	cJSON *root;
	if(prefdata && (root = cJSON_Parse(prefdata)))
	{
		cJSON *userobj, *versionobj, *recobj, *tmpobj, *graphicsobj, *hudobj, *simulationobj, *consoleobj, *tmparray;
		
		//Read user data
		userobj = cJSON_GetObjectItem(root, "User");
		if (userobj && (tmpobj = cJSON_GetObjectItem(userobj, "SessionKey"))) { //tpt++ format
			svf_login = 1;
			if((tmpobj = cJSON_GetObjectItem(userobj, "Username")) && tmpobj->type == cJSON_String) strncpy(svf_user, tmpobj->valuestring, 63); else svf_user[0] = 0;
			if((tmpobj = cJSON_GetObjectItem(userobj, "ID")) && tmpobj->type == cJSON_Number) sprintf(svf_user_id, "%i", tmpobj->valueint, 63); else svf_user_id[0] = 0;
			if((tmpobj = cJSON_GetObjectItem(userobj, "SessionID")) && tmpobj->type == cJSON_String) strncpy(svf_session_id, tmpobj->valuestring, 63); else svf_session_id[0] = 0;
			if((tmpobj = cJSON_GetObjectItem(userobj, "SessionKey")) && tmpobj->type == cJSON_String) strncpy(svf_session_key, tmpobj->valuestring, 63); else svf_session_key[0] = 0;
			if((tmpobj = cJSON_GetObjectItem(userobj, "Elevation")) && tmpobj->type == cJSON_String) {
				if (!strcmp(tmpobj->valuestring, "Admin")) {
					svf_admin = 1;
					svf_mod = 0;
				} else if (!strcmp(tmpobj->valuestring, "Mod")) {
					svf_mod = 1;
					svf_admin = 0;
				} else {
					svf_admin = 0;
					svf_mod = 0;
				}
			}
		}
		else if (userobj && cJSON_GetObjectItem(userobj, "name")) //tpt format
		{
			svf_login = 1;
			if((tmpobj = cJSON_GetObjectItem(userobj, "name")) && tmpobj->type == cJSON_String) strncpy(svf_user, tmpobj->valuestring, 63); else svf_user[0] = 0;
			if((tmpobj = cJSON_GetObjectItem(userobj, "id")) && tmpobj->type == cJSON_String) strncpy(svf_user_id, tmpobj->valuestring, 63); else svf_user_id[0] = 0;
			if((tmpobj = cJSON_GetObjectItem(userobj, "session_id")) && tmpobj->type == cJSON_String) strncpy(svf_session_id, tmpobj->valuestring, 63); else svf_session_id[0] = 0;
			if((tmpobj = cJSON_GetObjectItem(userobj, "admin")) && tmpobj->type == cJSON_True) {
				svf_admin = 1;
				svf_mod = 0;
			} else if((tmpobj = cJSON_GetObjectItem(userobj, "mod")) && tmpobj->type == cJSON_True) {
				svf_mod = 1;
				svf_admin = 0;
			} else {
				svf_admin = 0;
				svf_mod = 0;
			}
		}
		else 
		{
			svf_login = 0;
			svf_user[0] = 0;
			svf_user_id[0] = 0;
			svf_session_id[0] = 0;
			svf_admin = 0;
			svf_mod = 0;
		}
		
		//Read version data
		versionobj = cJSON_GetObjectItem(root, "version");
		if(versionobj){
			if(tmpobj = cJSON_GetObjectItem(versionobj, "major")) last_major = tmpobj->valueint;
			if(tmpobj = cJSON_GetObjectItem(versionobj, "minor")) last_minor = tmpobj->valueint;
			if(tmpobj = cJSON_GetObjectItem(versionobj, "build")) last_build = tmpobj->valueint;
			if((tmpobj = cJSON_GetObjectItem(versionobj, "update")) && tmpobj->type == cJSON_True)
				update_flag = 1;
			else
				update_flag = 0;
		} else {
			last_major = 0;
			last_minor = 0;
			last_build = 0;
			update_flag = 0;
		}
		
		//Read FavMenu/Records
		recobj = cJSON_GetObjectItem(root, "records");
		if (recobj) {
			if(tmpobj = cJSON_GetObjectItem(recobj, "num elements in menu")) locked = tmpobj->valueint;
			for (i = 0; i < locked; i++)
			{
				char eltext[128] = "";
				sprintf(eltext,"element %i",i);
				if(tmpobj = cJSON_GetObjectItem(recobj, eltext)) favMenu[18-i] = tmpobj->valueint;
			}
			if(tmpobj = cJSON_GetObjectItem(recobj, "Total Time Played")) totaltime = (int)((tmpobj->valuedouble)*1000);
			if(tmpobj = cJSON_GetObjectItem(recobj, "Average FPS")) totalfps = tmpobj->valuedouble;
			if(tmpobj = cJSON_GetObjectItem(recobj, "Number of frames")) frames = tmpobj->valueint; totalfps = totalfps * frames;
			if(tmpobj = cJSON_GetObjectItem(recobj, "Max FPS")) maxfps = tmpobj->valuedouble;
			if(tmpobj = cJSON_GetObjectItem(recobj, "Total AFK Time")) prevafktime = (int)((tmpobj->valuedouble)*1000);
			if(tmpobj = cJSON_GetObjectItem(recobj, "Times Played")) timesplayed = tmpobj->valueint;
		}

		//Read display settings
		graphicsobj = cJSON_GetObjectItem(root, "Renderer");
		if(graphicsobj)
		{
			if(tmpobj = cJSON_GetObjectItem(graphicsobj, "ColourMode")) colour_mode = atoi(tmpobj->valuestring);
			if(tmpobj = cJSON_GetObjectItem(graphicsobj, "DisplayModes"))
			{
				char temp[32];
				count = cJSON_GetArraySize(tmpobj);
				free(display_modes);
				display_mode = 0;
				display_modes = (unsigned int*)calloc(count+1, sizeof(unsigned int));
				for(i = 0; i < count; i++)
				{
					unsigned int mode;
					strncpy(temp, cJSON_GetArrayItem(tmpobj, i)->valuestring, 31);
					sscanf(temp, "%x", &mode);
					display_mode |= mode;
					display_modes[i] = mode;
				}
			}
			if(tmpobj = cJSON_GetObjectItem(graphicsobj, "RenderModes"))
			{
				char temp[32];
				count = cJSON_GetArraySize(tmpobj);
				free(render_modes);
				render_mode = 0;
				render_modes = (unsigned int*)calloc(count+1, sizeof(unsigned int));
				for(i = 0; i < count; i++)
				{
					unsigned int mode;
					strncpy(temp, cJSON_GetArrayItem(tmpobj, i)->valuestring, 31);
					sscanf(temp, "%x", &mode);
					render_mode |= mode;
					render_modes[i] = mode;
				}
			}
			if((tmpobj = cJSON_GetObjectItem(graphicsobj, "Decorations")) && tmpobj->type == cJSON_True)
				decorations_enable = tmpobj->valueint;
			if((tmpobj = cJSON_GetObjectItem(graphicsobj, "GravityField")) && tmpobj->type == cJSON_True)
				drawgrav_enable = tmpobj->valueint;
			if((tmpobj = cJSON_GetObjectItem(graphicsobj, "DebugMode")) && tmpobj->type == cJSON_True)
				DEBUG_MODE = tmpobj->valueint;
		}
		else
		{
			graphicsobj = cJSON_GetObjectItem(root, "graphics");
			if (graphicsobj)
			{
				if(tmpobj = cJSON_GetObjectItem(graphicsobj, "colour")) colour_mode = tmpobj->valueint;
				if(tmpobj = cJSON_GetObjectItem(graphicsobj, "display"))
				{
					count = cJSON_GetArraySize(tmpobj);
					free(display_modes);
					display_mode = 0;
					display_modes = (unsigned int*)calloc(count+1, sizeof(unsigned int));
					for(i = 0; i < count; i++)
					{
						display_mode |= cJSON_GetArrayItem(tmpobj, i)->valueint;
						display_modes[i] = cJSON_GetArrayItem(tmpobj, i)->valueint;
					}
				}
				if(tmpobj = cJSON_GetObjectItem(graphicsobj, "render"))
				{
					count = cJSON_GetArraySize(tmpobj);
					free(render_modes);
					render_mode = 0;
					render_modes = (unsigned int*)calloc(count+1, sizeof(unsigned int));
					for(i = 0; i < count; i++)
					{
						render_mode |= cJSON_GetArrayItem(tmpobj, i)->valueint;
						render_modes[i] = cJSON_GetArrayItem(tmpobj, i)->valueint;
					}
				}
			}
		}

		simulationobj = cJSON_GetObjectItem(root, "Simulation");
		if (simulationobj)
		{
			if(tmpobj = cJSON_GetObjectItem(simulationobj, "EdgeMode"))
				edgeMode = tmpobj->valueint;
			if((tmpobj = cJSON_GetObjectItem(simulationobj, "NewtonianGravity")) && tmpobj->valueint == 1)
				start_grav_async();
			if(tmpobj = cJSON_GetObjectItem(simulationobj, "AmbientHeat"))
				aheat_enable = tmpobj->valueint;
			if(tmpobj = cJSON_GetObjectItem(simulationobj, "PrettyPowder"))
				pretty_powder = tmpobj->valueint;
		}

		consoleobj = cJSON_GetObjectItem(root, "Console");
		if (consoleobj)
		{
			int max = 0;
			if(tmpobj = cJSON_GetObjectItem(consoleobj, "History"))
			{
				max = cJSON_GetArraySize(tmpobj);
				if(tmpobj = cJSON_GetObjectItem(consoleobj, "HistoryResults"))
					max = cJSON_GetArraySize(tmpobj)>max?cJSON_GetArraySize(tmpobj):max;
				else
					max = 0;
			}
			if (max)
			{
				load_console_history(cJSON_GetObjectItem(consoleobj, "History"), &last_command, max);
				load_console_history(cJSON_GetObjectItem(consoleobj, "HistoryResults"), &last_command_result, max);
			}
		}

		//Read HUDs
		hudobj = cJSON_GetObjectItem(root, "HUD");
		if(hudobj)
		{
			if(tmpobj = cJSON_GetObjectItem(hudobj, "normal"))
			{
				count = fmin(HUD_OPTIONS,cJSON_GetArraySize(tmpobj));
				for(i = 0; i < count; i++)
				{
					normalHud[i] = cJSON_GetArrayItem(tmpobj, i)->valueint;
				}
			}
			if(tmpobj = cJSON_GetObjectItem(hudobj, "debug"))
			{
				count = fmin(HUD_OPTIONS,cJSON_GetArraySize(tmpobj));
				for(i = 0; i < count; i++)
				{
					debugHud[i] = cJSON_GetArrayItem(tmpobj, i)->valueint;
				}
			}
			if(tmpobj = cJSON_GetObjectItem(hudobj, "modnormal"))
			{
				count = fmin(HUD_OPTIONS,cJSON_GetArraySize(tmpobj));
				for(i = 0; i < count; i++)
				{
					normalHud[i] = cJSON_GetArrayItem(tmpobj, i)->valueint;
				}
			}
			if(tmpobj = cJSON_GetObjectItem(hudobj, "moddebug"))
			{
				count = fmin(HUD_OPTIONS,cJSON_GetArraySize(tmpobj));
				for(i = 0; i < count; i++)
				{
					debugHud[i] = cJSON_GetArrayItem(tmpobj, i)->valueint;
				}
			}
		}
		
		//Read general settings
		if((tmpobj = cJSON_GetObjectItem(root, "proxy")) && tmpobj->type == cJSON_String) strncpy(http_proxy_string, tmpobj->valuestring, 255); else http_proxy_string[0] = 0;
		if(tmpobj = cJSON_GetObjectItem(root, "scale")) sdl_scale = tmpobj->valueint;
		else if(tmpobj = cJSON_GetObjectItem(root, "Scale")) sdl_scale = tmpobj->valueint;
		if(tmpobj = cJSON_GetObjectItem(root, "Debug mode")) DEBUG_MODE = tmpobj->valueint;
		if(tmpobj = cJSON_GetObjectItem(root, "heatmode")) heatmode = tmpobj->valueint;
		//if(tmpobj = cJSON_GetObjectItem(root, "save_as")) save_as = tmpobj->valueint;
		if(tmpobj = cJSON_GetObjectItem(root, "autosave")) autosave = tmpobj->valueint;
		if(tmpobj = cJSON_GetObjectItem(root, "sl")) sl = su = tmpobj->valueint;
		if(tmpobj = cJSON_GetObjectItem(root, "sr")) sr = tmpobj->valueint;
		if(tmpobj = cJSON_GetObjectItem(root, "active_menu")) active_menu = tmpobj->valueint;
		if(tmpobj = cJSON_GetObjectItem(root, "autosave")) autosave = tmpobj->valueint;
		if(tmpobj = cJSON_GetObjectItem(root, "aheat_enable")) aheat_enable = tmpobj->valueint;
		if(tmpobj = cJSON_GetObjectItem(root, "decorations_enable")) decorations_enable = tmpobj->valueint;
		if(tmpobj = cJSON_GetObjectItem(root, "ngrav_enable")) { if (tmpobj->valueint) start_grav_async(); };
		if(tmpobj = cJSON_GetObjectItem(root, "kiosk_enable")) { kiosk_enable = tmpobj->valueint; if (kiosk_enable) set_scale(sdl_scale, kiosk_enable); }
		if(tmpobj = cJSON_GetObjectItem(root, "realistic")) { realistic = tmpobj->valueint; if (realistic) ptypes[PT_FIRE].hconduct = 1; }
		if(tmpobj = cJSON_GetObjectItem(root, "cracker_unlocked")) { unlockedstuff |= 0x01; SC_TOTAL++; }
		if(tmpobj = cJSON_GetObjectItem(root, "show_votes")) unlockedstuff |= 0x08;
		if(tmpobj = cJSON_GetObjectItem(root, "EXPL_unlocked")) { unlockedstuff |= 0x10; ptypes[PT_EXPL].menu = 1; ptypes[PT_EXPL].enabled = 1; }
		if(tmpobj = cJSON_GetObjectItem(root, "old_menu")) old_menu = 1;
		if(tmpobj = cJSON_GetObjectItem(root, "drawgrav_enable")) drawgrav_enable = tmpobj->valueint;
		if(tmpobj = cJSON_GetObjectItem(root, "edgeMode")) edgeMode = tmpobj->valueint;
		if (edgeMode > 3) edgeMode = 0;
		if(tmpobj = cJSON_GetObjectItem(root, "alt_find")) finding |= 0x8;
		if(tmpobj = cJSON_GetObjectItem(root, "dateformat")) dateformat = tmpobj->valueint;
		if(tmpobj = cJSON_GetObjectItem(root, "ShowIDs")) show_ids = tmpobj->valueint;
		if(tmpobj = cJSON_GetObjectItem(root, "decobox_hidden")) decobox_hidden = tmpobj->valueint;
		if(tmpobj = cJSON_GetObjectItem(root, "fastquit")) fastquit = tmpobj->valueint;
		if(tmpobj = cJSON_GetObjectItem(root, "doUpdates")) doUpdates = tmpobj->valueint;
		if(tmpobj = cJSON_GetObjectItem(root, "WindowX")) savedWindowX = tmpobj->valueint;
		if(tmpobj = cJSON_GetObjectItem(root, "WindowY")) savedWindowY = tmpobj->valueint;

		cJSON_Delete(root);
		free(prefdata);
	} else { //Fallback and read from old def file
		FILE *f=fopen("powder.def", "rb");
		unsigned char sig[4], tmp;
		if (!f)
			return;
		fread(sig, 1, 4, f);
		if (sig[0]!=0x50 || sig[1]!=0x44 || sig[2]!=0x65)
		{
			if (sig[0]==0x4D && sig[1]==0x6F && sig[2]==0x46 && sig[3]==0x6F)
			{
				if (fseek(f, -3, SEEK_END))
				{
					remove("powder.def");
					return;
				}
				if (fread(sig, 1, 3, f) != 3)
				{
					remove("powder.def");
					goto fail;
				}
				//last_major = sig[0];
				//last_minor = sig[1];
				last_build = 0;
				update_flag = sig[2];
			}
			fclose(f);
			remove("powder.def");
			return;
		}
		if (sig[3]==0x66) {
			if (load_string(f, svf_user, 63))
				goto fail;
			if (load_string(f, svf_pass, 63))
				goto fail;
		} else {
			if (load_string(f, svf_user, 63))
				goto fail;
			if (load_string(f, svf_user_id, 63))
				goto fail;
			if (load_string(f, svf_session_id, 63))
				goto fail;
		}
		svf_login = !!svf_session_id[0];
		if (fread(&tmp, 1, 1, f) != 1)
			goto fail;
		sdl_scale = (tmp == 2) ? 2 : 1;
		if (fread(&tmp, 1, 1, f) != 1)
			goto fail;
		//TODO: Translate old cmode value into new *_mode values
		//cmode = tmp%CM_COUNT;
		if (fread(&tmp, 1, 1, f) != 1)
			goto fail;
		svf_admin = tmp;
		if (fread(&tmp, 1, 1, f) != 1)
			goto fail;
		svf_mod = tmp;
		if (load_string(f, http_proxy_string, 255))
			goto fail;

		if (sig[3]!=0x68) { //Pre v64 format
			if (fread(sig, 1, 3, f) != 3)
				goto fail;
			last_build = 0;
		} else {
			if (fread(sig, 1, 4, f) != 4)
				goto fail;
			last_build = sig[3];
		}
		last_major = sig[0];
		last_minor = sig[1];
		update_flag = sig[2];
	fail:
		fclose(f);
	}
}

int sregexp(const char *str, char *pattern)
{
	int result;
	regex_t patternc;
	if (regcomp(&patternc, pattern,  0)!=0)
		return 1;
	result = regexec(&patternc, str, 0, NULL, 0);
	regfree(&patternc);
	return result;
}

int load_string(FILE *f, char *str, int max)
{
	int li;
	unsigned char lb[2];
	fread(lb, 2, 1, f);
	li = lb[0] | (lb[1] << 8);
	if (li > max)
	{
		str[0] = 0;
		return 1;
	}
	fread(str, li, 1, f);
	str[li] = 0;
	return 0;
}

void strcaturl(char *dst, char *src)
{
	char *d;
	unsigned char *s;

	for (d=dst; *d; d++) ;

	for (s=(unsigned char *)src; *s; s++)
	{
		if ((*s>='0' && *s<='9') ||
		        (*s>='a' && *s<='z') ||
		        (*s>='A' && *s<='Z'))
			*(d++) = *s;
		else
		{
			*(d++) = '%';
			*(d++) = hex[*s>>4];
			*(d++) = hex[*s&15];
		}
	}
	*d = 0;
}

void strappend(char *dst, char *src)
{
	char *d;
	unsigned char *s;

	for (d=dst; *d; d++) ;

	for (s=(unsigned char *)src; *s; s++)
	{
		*(d++) = *s;
	}
	*d = 0;
}

void *file_load(char *fn, int *size)
{
	FILE *f = fopen(fn, "rb");
	void *s;

	if (!f)
		return NULL;
	fseek(f, 0, SEEK_END);
	*size = ftell(f);
	fseek(f, 0, SEEK_SET);
	s = malloc(*size);
	if (!s)
	{
		fclose(f);
		return NULL;
	}
	fread(s, *size, 1, f);
	fclose(f);
	return s;
}

int file_exists(char *filename)
{
	int exists = 0;
#ifdef WIN
	struct _stat s;
	if(_stat(filename, &s) == 0)
#else
	struct stat s;
	if(stat(filename, &s) == 0)
#endif
	{
		if(s.st_mode & S_IFDIR)
		{
			exists = 1;
		}
		else if(s.st_mode & S_IFREG)
		{
			exists = 1;
		}
		else
		{
			exists = 1;
		}
	}
	else
	{
		exists = 0;
	}
	return exists;
}

int cpu_check(void)
{
#ifdef MACOSX
	return 0;
#else
#ifdef X86
	unsigned af,bf,cf,df;
	x86_cpuid(0, af, bf, cf, df);
	if (bf==0x68747541 && cf==0x444D4163 && df==0x69746E65)
		amd = 1;
	x86_cpuid(1, af, bf, cf, df);
#ifdef X86_SSE
	if (!(df&(1<<25)))
		return 1;
#endif
#ifdef X86_SSE2
	if (!(df&(1<<26)))
		return 1;
#endif
#ifdef X86_SSE3
	if (!(cf&1))
		return 1;
#endif
#endif
#endif
	return 0;
}

matrix2d m2d_multiply_m2d(matrix2d m1, matrix2d m2)
{
	matrix2d result = {
		m1.a*m2.a+m1.b*m2.c, m1.a*m2.b+m1.b*m2.d,
		m1.c*m2.a+m1.d*m2.c, m1.c*m2.b+m1.d*m2.d
	};
	return result;
}
vector2d m2d_multiply_v2d(matrix2d m, vector2d v)
{
	vector2d result = {
		m.a*v.x+m.b*v.y,
		m.c*v.x+m.d*v.y
	};
	return result;
}
matrix2d m2d_multiply_float(matrix2d m, float s)
{
	matrix2d result = {
		m.a*s, m.b*s,
		m.c*s, m.d*s,
	};
	return result;
}

vector2d v2d_multiply_float(vector2d v, float s)
{
	vector2d result = {
		v.x*s,
		v.y*s
	};
	return result;
}

vector2d v2d_add(vector2d v1, vector2d v2)
{
	vector2d result = {
		v1.x+v2.x,
		v1.y+v2.y
	};
	return result;
}
vector2d v2d_sub(vector2d v1, vector2d v2)
{
	vector2d result = {
		v1.x-v2.x,
		v1.y-v2.y
	};
	return result;
}

matrix2d m2d_new(float me0, float me1, float me2, float me3)
{
	matrix2d result = {me0,me1,me2,me3};
	return result;
}
vector2d v2d_new(float x, float y)
{
	vector2d result = {x, y};
	return result;
}

char * clipboardtext = NULL;
void clipboard_push_text(char * text)
{
	if (clipboardtext)
	{
		free(clipboardtext);
		clipboardtext = NULL;
	}
	clipboardtext = mystrdup(text);
#ifdef MACOSX
	PasteboardRef newclipboard;

	if (PasteboardCreate(kPasteboardClipboard, &newclipboard)!=noErr) return;
	if (PasteboardClear(newclipboard)!=noErr) return;
	PasteboardSynchronize(newclipboard);

	CFDataRef data = CFDataCreate(kCFAllocatorDefault, text, strlen(text));
	PasteboardPutItemFlavor(newclipboard, (PasteboardItemID)1, CFSTR("com.apple.traditional-mac-plain-text"), data, 0);
#elif defined WIN32
	if (OpenClipboard(NULL))
	{
		HGLOBAL cbuffer;
		char * glbuffer;

		EmptyClipboard();

		cbuffer = GlobalAlloc(GMEM_DDESHARE, strlen(text)+1);
		glbuffer = (char*)GlobalLock(cbuffer);

		strcpy(glbuffer, text);

		GlobalUnlock(cbuffer);
		SetClipboardData(CF_TEXT, cbuffer);
		CloseClipboard();
	}
#elif (defined(LIN32) || defined(LIN64)) && defined(SDL_VIDEO_DRIVER_X11)
	if (clipboard_text!=NULL) {
		free(clipboard_text);
		clipboard_text = NULL;
	}
	clipboard_text = mystrdup(text);
	sdl_wminfo.info.x11.lock_func();
	XSetSelectionOwner(sdl_wminfo.info.x11.display, XA_CLIPBOARD, sdl_wminfo.info.x11.window, CurrentTime);
	XFlush(sdl_wminfo.info.x11.display);
	sdl_wminfo.info.x11.unlock_func();
#else
	printf("Not implemented: put text on clipboard \"%s\"\n", text);
#endif
}

char * clipboard_pull_text()
{
#ifdef MACOSX
	printf("Not implemented: get text from clipboard\n");
#elif defined WIN32
	if (OpenClipboard(NULL))
	{
		HANDLE cbuffer;
		char * glbuffer;

		cbuffer = GetClipboardData(CF_TEXT);
		glbuffer = (char*)GlobalLock(cbuffer);
		GlobalUnlock(cbuffer);
		CloseClipboard();
		if(glbuffer!=NULL){
			return mystrdup(glbuffer);
		} //else {
		//	return "";
		//}
	}
#elif (defined(LIN32) || defined(LIN64)) && defined(SDL_VIDEO_DRIVER_X11)
	printf("Not implemented: get text from clipboard\n");
#else
	printf("Not implemented: get text from clipboard\n");
#endif
	if (clipboardtext)
		return mystrdup(clipboardtext);
	return "";
}

int register_extension()
{
#if defined WIN32
	int returnval;
	LONG rresult;
	HKEY newkey;
	char *currentfilename = exe_name();
	char *iconname = NULL;
	char *opencommand = NULL;
	char *protocolcommand = NULL;
	//char AppDataPath[MAX_PATH];
	char *AppDataPath = NULL;
	iconname = (char*)malloc(strlen(currentfilename)+6);
	sprintf(iconname, "%s,-102", currentfilename);
	
	//Create Roaming application data folder
	/*if(!SUCCEEDED(SHGetFolderPath(NULL, CSIDL_APPDATA|CSIDL_FLAG_CREATE, NULL, 0, AppDataPath))) 
	{
		returnval = 0;
		goto finalise;
	}*/
	
	AppDataPath = (char*)_getcwd(NULL, 0);

	//Move Game executable into application data folder
	//TODO: Implement
	
	opencommand = (char*)malloc(strlen(currentfilename)+53+strlen(AppDataPath));
	protocolcommand = (char*)malloc(strlen(currentfilename)+55+strlen(AppDataPath));
	/*if((strlen(AppDataPath)+strlen(APPDATA_SUBDIR "\\Powder Toy"))<MAX_PATH)
	{
		strappend(AppDataPath, APPDATA_SUBDIR);
		_mkdir(AppDataPath);
		strappend(AppDataPath, "\\Powder Toy");
		_mkdir(AppDataPath);
	} else {
		returnval = 0;
		goto finalise;
	}*/
	sprintf(opencommand, "\"%s\" open \"%%1\" ddir \"%s\"", currentfilename, AppDataPath);
	sprintf(protocolcommand, "\"%s\" ddir \"%s\" ptsave \"%%1\"", currentfilename, AppDataPath);

	//Create protocol entry
	rresult = RegCreateKeyEx(HKEY_CURRENT_USER, "Software\\Classes\\ptsave", 0, 0, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &newkey, NULL);
	if (rresult != ERROR_SUCCESS) {
		returnval = 0;
		goto finalise;
	}
	rresult = RegSetValueEx(newkey, 0, 0, REG_SZ, (LPBYTE)"Powder Toy Save", strlen("Powder Toy Save")+1);
	if (rresult != ERROR_SUCCESS) {
		RegCloseKey(newkey);
		returnval = 0;
		goto finalise;
	}
	rresult = RegSetValueEx(newkey, (LPCSTR)"URL Protocol", 0, REG_SZ, (LPBYTE)"", strlen("")+1);
	if (rresult != ERROR_SUCCESS) {
		RegCloseKey(newkey);
		returnval = 0;
		goto finalise;
	}
	RegCloseKey(newkey);
	
	
	//Set Protocol DefaultIcon
	rresult = RegCreateKeyEx(HKEY_CURRENT_USER, "Software\\Classes\\ptsave\\DefaultIcon", 0, 0, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &newkey, NULL);
	if (rresult != ERROR_SUCCESS) {
		returnval = 0;
		goto finalise;
	}
	rresult = RegSetValueEx(newkey, 0, 0, REG_SZ, (LPBYTE)iconname, strlen(iconname)+1);
	if (rresult != ERROR_SUCCESS) {
		RegCloseKey(newkey);
		returnval = 0;
		goto finalise;
	}
	RegCloseKey(newkey);	
	
	//Set Protocol Launch command
	rresult = RegCreateKeyEx(HKEY_CURRENT_USER, "Software\\Classes\\ptsave\\shell\\open\\command", 0, 0, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &newkey, NULL);
	if (rresult != ERROR_SUCCESS) {
		returnval = 0;
		goto finalise;
	}
	rresult = RegSetValueEx(newkey, 0, 0, REG_SZ, (LPBYTE)protocolcommand, strlen(protocolcommand)+1);
	if (rresult != ERROR_SUCCESS) {
		RegCloseKey(newkey);
		returnval = 0;
		goto finalise;
	}
	RegCloseKey(newkey);
	
	//Create extension entry
	rresult = RegCreateKeyEx(HKEY_CURRENT_USER, "Software\\Classes\\.cps", 0, 0, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &newkey, NULL);
	if (rresult != ERROR_SUCCESS) {
		returnval = 0;
		goto finalise;
	}
	rresult = RegSetValueEx(newkey, 0, 0, REG_SZ, (LPBYTE)"PowderToySave", strlen("PowderToySave")+1);
	if (rresult != ERROR_SUCCESS) {
		RegCloseKey(newkey);
		returnval = 0;
		goto finalise;
	}
	RegCloseKey(newkey);

	rresult = RegCreateKeyEx(HKEY_CURRENT_USER, "Software\\Classes\\.stm", 0, 0, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &newkey, NULL);
	if (rresult != ERROR_SUCCESS) {
		returnval = 0;
		goto finalise;
	}
	rresult = RegSetValueEx(newkey, 0, 0, REG_SZ, (LPBYTE)"PowderToySave", strlen("PowderToySave")+1);
	if (rresult != ERROR_SUCCESS) {
		RegCloseKey(newkey);
		returnval = 0;
		goto finalise;
	}
	RegCloseKey(newkey);
	
	//Create program entry
	rresult = RegCreateKeyEx(HKEY_CURRENT_USER, "Software\\Classes\\PowderToySave", 0, 0, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &newkey, NULL);
	if (rresult != ERROR_SUCCESS) {
		returnval = 0;
		goto finalise;
	}
	rresult = RegSetValueEx(newkey, 0, 0, REG_SZ, (LPBYTE)"Powder Toy Save", strlen("Powder Toy Save")+1);
	if (rresult != ERROR_SUCCESS) {
		RegCloseKey(newkey);
		returnval = 0;
		goto finalise;
	}
	RegCloseKey(newkey);

	//Set DefaultIcon
	rresult = RegCreateKeyEx(HKEY_CURRENT_USER, "Software\\Classes\\PowderToySave\\DefaultIcon", 0, 0, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &newkey, NULL);
	if (rresult != ERROR_SUCCESS) {
		returnval = 0;
		goto finalise;
	}
	rresult = RegSetValueEx(newkey, 0, 0, REG_SZ, (LPBYTE)iconname, strlen(iconname)+1);
	if (rresult != ERROR_SUCCESS) {
		RegCloseKey(newkey);
		returnval = 0;
		goto finalise;
	}
	RegCloseKey(newkey);

	//Set Launch command
	rresult = RegCreateKeyEx(HKEY_CURRENT_USER, "Software\\Classes\\PowderToySave\\shell\\open\\command", 0, 0, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &newkey, NULL);
	if (rresult != ERROR_SUCCESS) {
		returnval = 0;
		goto finalise;
	}
	rresult = RegSetValueEx(newkey, 0, 0, REG_SZ, (LPBYTE)opencommand, strlen(opencommand)+1);
	if (rresult != ERROR_SUCCESS) {
		RegCloseKey(newkey);
		returnval = 0;
		goto finalise;
	}
	RegCloseKey(newkey);
	
	returnval = 1;
	finalise:

	if(iconname) free(iconname);
	if(opencommand) free(opencommand);
	if(currentfilename) free(currentfilename);
	if(protocolcommand) free(protocolcommand);
	
	return returnval;
#elif defined(LIN32) || defined(LIN64)
	char *currentfilename = exe_name();
	FILE *f;
	char *mimedata =
"<?xml version=\"1.0\"?>\n"
"	<mime-info xmlns='http://www.freedesktop.org/standards/shared-mime-info'>\n"
"	<mime-type type=\"application/vnd.powdertoy.save\">\n"
"		<comment>Powder Toy save</comment>\n"
"		<glob pattern=\"*.cps\"/>\n"
"		<glob pattern=\"*.stm\"/>\n"
"	</mime-type>\n"
"</mime-info>\n";
	f = fopen("powdertoy-save.xml", "wb");
	if (!f)
		return 0;
	fwrite(mimedata, 1, strlen(mimedata), f);
	fclose(f);

	char *protocolfiledata_tmp =
"[Desktop Entry]\n"
"Type=Application\n"
"Name=Powder Toy\n"
"Comment=Physics sandbox game\n"
"MimeType=x-scheme-handler/ptsave;\n"
"NoDisplay=true\n";
	char *protocolfiledata = (char *)malloc(strlen(protocolfiledata_tmp)+strlen(currentfilename)+100);
	strcpy(protocolfiledata, protocolfiledata_tmp);
	strappend(protocolfiledata, "Exec=");
	strappend(protocolfiledata, currentfilename);
	strappend(protocolfiledata, " ptsave %u\n");
	f = fopen("powdertoy-tpt-ptsave.desktop", "wb");
	if (!f)
		return 0;
	fwrite(protocolfiledata, 1, strlen(protocolfiledata), f);
	fclose(f);
	system("xdg-desktop-menu install powdertoy-tpt-ptsave.desktop");

	char *desktopfiledata_tmp =
"[Desktop Entry]\n"
"Type=Application\n"
"Name=Powder Toy\n"
"Comment=Physics sandbox game\n"
"MimeType=application/vnd.powdertoy.save;\n"
"NoDisplay=true\n";
	char *desktopfiledata = (char*)malloc(strlen(desktopfiledata_tmp)+strlen(currentfilename)+100);
	strcpy(desktopfiledata, desktopfiledata_tmp);
	strappend(desktopfiledata, "Exec=");
	strappend(desktopfiledata, currentfilename);
	strappend(desktopfiledata, " open %f\n");
	f = fopen("powdertoy-tpt.desktop", "wb");
	if (!f)
		return 0;
	fwrite(desktopfiledata, 1, strlen(desktopfiledata), f);
	fclose(f);
	system("xdg-mime install powdertoy-save.xml");
	system("xdg-desktop-menu install powdertoy-tpt.desktop");
	f = fopen("powdertoy-save-32.png", "wb");
	if (!f)
		return 0;
	fwrite(icon_doc_32_png, 1, icon_doc_32_png_size, f);
	fclose(f);
	f = fopen("powdertoy-save-16.png", "wb");
	if (!f)
		return 0;
	fwrite(icon_doc_16_png, 1, icon_doc_16_png_size, f);
	fclose(f);
	system("xdg-icon-resource install --noupdate --context mimetypes --size 32 powdertoy-save-32.png application-vnd.powdertoy.save");
	system("xdg-icon-resource install --noupdate --context mimetypes --size 16 powdertoy-save-16.png application-vnd.powdertoy.save");
	system("xdg-icon-resource forceupdate");
	system("xdg-mime default powdertoy-tpt.desktop application/vnd.powdertoy.save");
	system("xdg-mime default powdertoy-tpt-ptsave.desktop x-scheme-handler/ptsave");
	unlink("powdertoy-save-32.png");
	unlink("powdertoy-save-16.png");
	unlink("powdertoy-save.xml");
	unlink("powdertoy-tpt.desktop");
	unlink("powdertoy-tpt-ptsave.desktop");
	return 1;
#elif defined MACOSX
	return 0;
#endif
}

void HSV_to_RGB(int h,int s,int v,int *r,int *g,int *b)//convert 0-255(0-360 for H) HSV values to 0-255 RGB
{
	float hh, ss, vv, c, x;
	int m;
	hh = h/60.0f;//normalize values
	ss = s/255.0f;
	vv = v/255.0f;
	c = vv * ss;
	x = c * ( 1 - fabs(fmod(hh,2.0f) -1) );
	if(hh<1){
		*r = (int)(c*255.0);
		*g = (int)(x*255.0);
		*b = 0;
	}
	else if(hh<2){
		*r = (int)(x*255.0);
		*g = (int)(c*255.0);
		*b = 0;
	}
	else if(hh<3){
		*r = 0;
		*g = (int)(c*255.0);
		*b = (int)(x*255.0);
	}
	else if(hh<4){
		*r = 0;
		*g = (int)(x*255.0);
		*b = (int)(c*255.0);
	}
	else if(hh<5){
		*r = (int)(x*255.0);
		*g = 0;
		*b = (int)(c*255.0);
	}
	else if(hh<6){
		*r = (int)(c*255.0);
		*g = 0;
		*b = (int)(x*255.0);
	}
	m = (int)((vv-c)*255.0);
	*r += m;
	*g += m;
	*b += m;
}

void RGB_to_HSV(int r,int g,int b,int *h,int *s,int *v)//convert 0-255 RGB values to 0-255(0-360 for H) HSV
{
	float rr, gg, bb, a,x,c,d;
	rr = r/255.0f;//normalize values
	gg = g/255.0f;
	bb = b/255.0f;
	a = fmin(rr,gg);
	a = fmin(a,bb);
	x = fmax(rr,gg);
	x = fmax(x,bb);
	if (a==x)//greyscale
	{
		*h = 0;
		*s = 0;
		*v = (int)(a*255.0);
	}
	else
	{
 		c = (rr==a) ? gg-bb : ((bb==a) ? rr-gg : bb-rr);
 		d = (float)((rr==a) ? 3 : ((bb==a) ? 1 : 5));
 		*h = (int)(60.0*(d - c/(x - a)));
 		*s = (int)(255.0*((x - a)/x));
 		*v = (int)(255.0*x);
	}
}

int is_TOOL(int t)
{
	if (t == SPC_AIR || t == SPC_HEAT || t == SPC_COOL || t == SPC_VACUUM || t == SPC_WIND || t == SPC_PGRV || t == SPC_NGRV || t == SPC_PROP)
		return 1;
	return 0;
}

int is_DECOTOOL(int t)
{
	if (t == DECO_DRAW || t == DECO_ERASE || t == DECO_LIGH || t == DECO_DARK || t == DECO_SMDG)
		return 1;
	return 0;
}

void membwand(void * destv, void * srcv, size_t destsize, size_t srcsize)
{
	size_t i;
	unsigned char * dest = (unsigned char*)destv;
	unsigned char * src = (unsigned char*)srcv;
	if (srcsize==destsize)
	{
		for(i = 0; i < destsize; i++){
			dest[i] = dest[i] & src[i];
		}
	}
	else
	{
		for(i = 0; i < destsize; i++){
			dest[i] = dest[i] & src[i%srcsize];
		}
	}
}
vector2d v2d_zero = {0,0};
matrix2d m2d_identity = {1,0,0,1};