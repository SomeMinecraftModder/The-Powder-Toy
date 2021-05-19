/*
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

#include "simulation/ElementsCommon.h"
#include "simulation/GolNumbers.h"
#include "LIFE.h"

int LIFE_update(UPDATE_FUNC_ARGS)
{
	sim->parts[i].temp = restrict_flt(sim->parts[i].temp - 50.0f, MIN_TEMP, MAX_TEMP);
	return 0;
}

int LIFE_graphics(GRAPHICS_FUNC_ARGS)
{
	auto color1 = cpart->dcolour;
	auto color2 = cpart->tmp;
	if (!color1)
	{
		color1 = PIXPACK(0xFFFFFF);
	}
	auto ruleset = cpart->ctype;
	bool renderDeco = true; //!ren->blackDecorations;
	if (ruleset >= 0 && ruleset < NGOL)
	{
		if (!renderDeco)
		{
			color1 = builtinGol[ruleset].color;
			color2 = builtinGol[ruleset].color2;
			renderDeco = true;
		}
		ruleset = builtinGol[ruleset].ruleset;
	}
	if (renderDeco)
	{
		auto states = ((ruleset >> 17) & 0xF) + 2;
		if (states == 2)
		{
			*colr = PIXR(color1);
			*colg = PIXG(color1);
			*colb = PIXB(color1);
		}
		else
		{
			auto mul = (cpart->tmp2 - 1) / float(states - 2);
			*colr = int(PIXR(color1) * mul + PIXR(color2) * (1.f - mul));
			*colg = int(PIXG(color1) * mul + PIXG(color2) * (1.f - mul));
			*colb = int(PIXB(color1) * mul + PIXB(color2) * (1.f - mul));
		}
	}
	*pixel_mode |= NO_DECO;
	return 0;
}

void LIFE_create(ELEMENT_CREATE_FUNC_ARGS)
{
	// * 0x200000: No need to look for colours, they'll be set later anyway.
	bool skipLookup = v & 0x200000;
	v &= 0x1FFFFF;
	sim->parts[i].ctype = v;
	if (v < NGOL)
	{
		sim->parts[i].dcolour = builtinGol[v].color;
		sim->parts[i].tmp = builtinGol[v].color2;
		v = builtinGol[v].ruleset;
	}
	else if (!skipLookup)
	{
		auto *cgol = ((LIFE_ElementDataContainer*)sim->elementData[PT_LIFE])->GetCustomGOLByRule(v);
		if (cgol)
		{
			sim->parts[i].dcolour = cgol->color1;
			sim->parts[i].tmp = cgol->color2;
		}
	}
	sim->parts[i].tmp2 = ((v >> 17) & 0xF) + 1;
}

void LIFE_init_element(ELEMENT_INIT_FUNC_ARGS)
{
	elem->Identifier = "DEFAULT_PT_LIFE";
	elem->Name = "LIFE";
	elem->Colour = COLPACK(0x0CAC00);
	elem->MenuVisible = 0;
	elem->MenuSection = SC_LIFE;
	elem->Enabled = 1;

	elem->Advection = 0.0f;
	elem->AirDrag = 0.00f * CFDS;
	elem->AirLoss = 0.90f;
	elem->Loss = 0.00f;
	elem->Collision = 0.0f;
	elem->Gravity = 0.0f;
	elem->Diffusion = 0.00f;
	elem->HotAir = 0.000f	* CFDS;
	elem->Falldown = 0;

	elem->Flammable = 0;
	elem->Explosive = 0;
	elem->Meltable = 0;
	elem->Hardness = 0;

	elem->Weight = 100;

	elem->DefaultProperties.temp = 9000.0f;
	elem->HeatConduct = 40;
	elem->Latent = 0;
	elem->Description = "Game Of Life: Begin 3/Stay 23";

	elem->Properties = TYPE_SOLID|PROP_LIFE;

	elem->LowPressureTransitionThreshold = IPL;
	elem->LowPressureTransitionElement = NT;
	elem->HighPressureTransitionThreshold = IPH;
	elem->HighPressureTransitionElement = NT;
	elem->LowTemperatureTransitionThreshold = ITL;
	elem->LowTemperatureTransitionElement = NT;
	elem->HighTemperatureTransitionThreshold = ITH;
	elem->HighTemperatureTransitionElement = NT;

	elem->Update = &LIFE_update;
	elem->Graphics = &LIFE_graphics;
	elem->Func_Create = &LIFE_create;
	elem->Init = &LIFE_init_element;

	if (sim->elementData[t])
	{
		delete sim->elementData[t];
	}
	sim->elementData[t] = new LIFE_ElementDataContainer;
}
