/**
 * Powder Toy - particle simulation (header)
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
#ifndef POWDER_H
#define POWDER_H

#include "defines.h"
#include "misc.h"

#define BRUSH_REPLACEMODE 0x1
#define BRUSH_SPECIFIC_DELETE 0x2

#include "simulation/ElementNumbers.h"
#include "simulation/SimulationData.h"


class Simulation;
#define UPDATE_FUNC_ARGS Simulation *sim, int i, int x, int y, int surround_space, int nt
#define UPDATE_FUNC_SUBCALL_ARGS sim, i, x, y, surround_space, nt
#define GRAPHICS_FUNC_ARGS Simulation *sim, particle *cpart, int nx, int ny, int *pixel_mode, int* cola, int *colr, int *colg, int *colb, int *firea, int *firer, int *fireg, int *fireb
#define GRAPHICS_FUNC_SUBCALL_ARGS sim, cpart, nx, ny, pixel_mode, cola, colr, colg, colb, firea, firer, fireg, fireb

#include "simulation/Particle.h"

void TRON_init_graphics();

void PPIP_flood_trigger(Simulation* sim, int x, int y, int sparkedBy);

struct part_type
{
	char *name;
	int enabled;
	float heat;
	char *descs;
	unsigned int properties;
	int (*graphics_func) (GRAPHICS_FUNC_ARGS);
};
typedef struct part_type part_type;

extern part_type ptypes[PT_NUM];

#define CHANNELS ((int)(MAX_TEMP-73)/100+2)
extern const particle emptyparticle;

extern int airMode;

extern particle *parts;

extern unsigned char bmap[YRES/CELL][XRES/CELL];
extern unsigned char emap[YRES/CELL][XRES/CELL];

extern unsigned pmap[YRES][XRES];
extern int pmap_count[YRES][XRES];

extern unsigned photons[YRES][XRES];

int get_wavelength_bin(int *wm);

void kill_part(int i);

int interactWavelengths(particle* cpart, int origWl);
int getWavelengths(particle* cpart);

void part_change_type(int i, int x, int y, int t);

void get_gravity_field(int x, int y, float particleGrav, float newtonGrav, float *pGravX, float *pGravY);

int get_brush_flags();

int is_wire(int x, int y);

int is_wire_off(int x, int y);

void set_emap(int x, int y);

int parts_avg(int ci, int ni, int t);

int nearest_part(int ci, int t, int max_d);

void decrease_life(int i);

void rotate_area(int area_x, int area_y, int area_w, int area_h, int invert);

void clear_area(int area_x, int area_y, int area_w, int area_h);

int INST_flood_spark(Simulation *sim, int x, int y);

int flood_water(int x, int y, int i, int originaly, int check);

void orbitalparts_get(int block1, int block2, int resblock1[], int resblock2[]);
void orbitalparts_set(int *block1, int *block2, int resblock1[], int resblock2[]);

void draw_bframe();
void erase_bframe();

#endif
