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
#include "graphics.h"

int FIRE_update_legacy(UPDATE_FUNC_ARGS)
{
	int t = parts[i].type;
	for (int rx = -2; rx <= 2; rx++)
		for (int ry = -2; ry <= 2; ry++)
			if (BOUNDS_CHECK && (rx || ry))
			{
				int r = pmap[y+ry][x+rx];
				if (!r)
					continue;
				if (bmap[(y+ry)/CELL][(x+rx)/CELL] && bmap[(y+ry)/CELL][(x+rx)/CELL]!=WL_STREAM)
					continue;
				int rt = TYP(r);
				int lpv = (int)sim->air->pv[(y+ry)/CELL][(x+rx)/CELL];
				if (lpv < 1) lpv = 1;
				if (sim->elements[rt].Meltable && ((rt!=PT_RBDM && rt!=PT_LRBD) || t!=PT_SPRK) &&
				        ((t!=PT_FIRE&&t!=PT_PLSM) || (rt!=PT_METL && rt!=PT_IRON && rt!=PT_ETRD && rt!=PT_PSCN &&
				        rt!=PT_NSCN && rt!=PT_NTCT && rt!=PT_PTCT && rt!=PT_BMTL && rt!=PT_BRMT && rt!=PT_SALT && rt!=PT_INWR))
				        && RNG::Ref().chance(sim->elements[rt].Meltable * lpv, 1000))
				{
					if (t!=PT_LAVA || parts[i].life>0)
					{
						if (rt==PT_BRMT)
							parts[ID(r)].ctype = PT_BMTL;
						else if (rt==PT_SAND)
							parts[ID(r)].ctype = PT_GLAS;
						else
							parts[ID(r)].ctype = rt;
						sim->part_change_type(ID(r),x+rx,y+ry,PT_LAVA);
						parts[ID(r)].life = RNG::Ref().between(240, 359);
					}
					else
					{
						parts[i].life = 0;
						parts[i].ctype = PT_NONE;//rt;
						sim->part_change_type(i,x,y,(parts[i].ctype)?parts[i].ctype:PT_STNE);
						return 1;
					}
				}
				if (rt==PT_ICEI || rt==PT_SNOW)
				{
					sim->part_change_type(ID(r), x+rx, y+ry, PT_WATR);
					if (t==PT_FIRE)
					{
						sim->part_kill(i);
						return 1;
					}
					if (t==PT_LAVA)
					{
						parts[i].life = 0;
						sim->part_change_type(i,x,y,PT_STNE);
					}
				}
				if (rt==PT_WATR || rt==PT_DSTW || rt==PT_SLTW)
				{
					sim->part_kill(ID(r));
					if (t==PT_FIRE)
					{
						sim->part_kill(i);
						return 1;
					}
					if (t==PT_LAVA)
					{
						parts[i].life = 0;
						parts[i].ctype = PT_NONE;
						sim->part_change_type(i,x,y,(parts[i].ctype)?parts[i].ctype:PT_STNE);
					}
				}
			}
	return 0;
}

int FIRE_update(UPDATE_FUNC_ARGS)
{
	int t = parts[i].type;
	switch (t)
	{
	case PT_PLSM:
		if (parts[i].life <= 1)
		{
			if (parts[i].ctype == PT_NBLE)
			{
				t = PT_NBLE;
				sim->part_change_type(i, x, y, t);
				parts[i].life = 0;
			}
			else if ((parts[i].tmp&0x3) == 3)
			{
				sim->part_change_type(i, x, y, PT_DSTW);
				parts[i].life = 0;
				parts[i].ctype = PT_FIRE;
			}
		}
		break;
	case PT_FIRE:
		if (parts[i].life <=1)
		{
			if ((parts[i].tmp&0x3) == 3)
			{
				sim->part_change_type(i, x, y, PT_DSTW);
				parts[i].life = 0;
				parts[i].ctype = PT_FIRE;
			}
			else if (parts[i].temp<625)
			{
				sim->part_change_type(i, x, y, PT_SMKE);
				parts[i].life = RNG::Ref().between(250, 269);
			}
		}
		break;
	case PT_LAVA:
		if (parts[i].ctype == PT_ROCK)
		{
			float pres = sim->air->pv[y / CELL][x / CELL];
			if (pres <= -9)
			{
				parts[i].ctype = PT_STNE;
				break;
			}
			
			if (pres >= 25 && RNG::Ref().chance(1, 12500))
			{
				if (pres <= 50)
				{
					if (RNG::Ref().chance(1, 2))
						parts[i].ctype = PT_BRMT;
					else
						parts[i].ctype = PT_CNCT;
				}
				else if (pres <= 75)
				{
					if (pres >= 73 || RNG::Ref().chance(1, 8))
						parts[i].ctype = PT_GOLD;
					else
						parts[i].ctype = PT_QRTZ;
				}
				else if (pres <= 100 && parts[i].temp >= 5000)
				{
					if (RNG::Ref().chance(1, 5)) // 1 in 5 chance IRON to TTAN
						parts[i].ctype = PT_TTAN;
					else
						parts[i].ctype = PT_IRON;
				}
				else if (parts[i].temp >= 5000 && RNG::Ref().chance(1, 5))
				{
					if (RNG::Ref().chance(1, 5))
						parts[i].ctype = PT_URAN;
					else if (RNG::Ref().chance(1, 5))
						parts[i].ctype = PT_PLUT;
					else
						parts[i].ctype = PT_TUNG;
				}
			}
		}
		else if ((parts[i].ctype == PT_STNE || !parts[i].ctype) && sim->air->pv[y / CELL][x / CELL] >= 30.0f && (parts[i].temp > 1943.15f || sim->air->pv[y / CELL][x / CELL] < 120.0f)) // Form ROCK with pressure, if it will stay molten or not immediately break
		{
			parts[i].tmp2 = RNG::Ref().between(0, 10); // Provide tmp2 for color noise
			parts[i].ctype = PT_ROCK;
		}
		break;
	default:
		break;
	}
	for (int rx = -2; rx <= 2; rx++)
		for (int ry = -2; ry <= 2; ry++)
			if (BOUNDS_CHECK && (rx || ry))
			{
				int r = pmap[y+ry][x+rx];
				if (!r)
					continue;
				int rt = TYP(r);
				//THRM burning
				if (rt==PT_THRM && (t==PT_FIRE || t==PT_PLSM || t==PT_LAVA))
				{
					if (RNG::Ref().chance(1, 500))
					{
						sim->part_change_type(ID(r),x+rx,y+ry,PT_LAVA);
						parts[ID(r)].ctype = PT_BMTL;
						parts[ID(r)].temp = 3500.0f;
						sim->air->pv[(y+ry)/CELL][(x+rx)/CELL] += 50.0f;
					}
					else
					{
						sim->part_change_type(ID(r),x+rx,y+ry,PT_LAVA);
						parts[ID(r)].life = 400;
						parts[ID(r)].ctype = PT_THRM;
						parts[ID(r)].temp = 3500.0f;
						parts[ID(r)].tmp = 20;
					}
					continue;
				}

				if ((rt==PT_COAL) || (rt==PT_BCOL))
				{
					if ((t==PT_FIRE || t==PT_PLSM))
					{
						if (parts[ID(r)].life>100 && RNG::Ref().chance(1, 500))
						{
							parts[ID(r)].life = 99;
						}
					}
					else if (t==PT_LAVA)
					{
						if (parts[i].ctype == PT_IRON && RNG::Ref().chance(1, 500))
						{
							parts[i].ctype = PT_METL;
							sim->part_kill(ID(r));
							continue;
						}
						if ((parts[i].ctype == PT_STNE || parts[i].ctype == PT_NONE) && RNG::Ref().chance(1, 60))
						{
							parts[i].ctype = PT_SLCN;
							sim->part_kill(ID(r));
							continue;
						}
					}
				}


				if (t == PT_LAVA)
				{
					// LAVA(CLST) + LAVA(PQRT) + high enough temp = LAVA(CRMC) + LAVA(CRMC)
					if (parts[i].ctype == PT_QRTZ && rt == PT_LAVA && parts[ID(r)].ctype == PT_CLST)
					{
						float pres = std::max(sim->air->pv[y/CELL][x/CELL]*10.0f, 0.0f);
						if (parts[i].temp >= pres+sim->elements[PT_CRMC].HighTemperatureTransitionThreshold+50.0f)
						{
							parts[i].ctype = PT_CRMC;
							parts[ID(r)].ctype = PT_CRMC;
						}
					}
					else if (rt == PT_O2 && parts[i].ctype == PT_SLCN)
					{
						switch (RNG::Ref().between(0, 2))
						{
							case 0:
								parts[i].ctype = PT_SAND;
								break;
								
							case 1:
								parts[i].ctype = PT_CLST;
								// avoid creating CRMC.
								if (parts[i].temp >= sim->elements[PT_PQRT].HighTemperatureTransitionThreshold * 3)
								{
									parts[i].ctype = PT_PQRT;
								}
								break;
								
							case 2:
								parts[i].ctype = PT_STNE;
								break;
						}
						parts[i].tmp = 0;
						sim->part_kill(ID(r));
						continue;
					}
					else if (rt == PT_LAVA && (parts[ID(r)].ctype == PT_METL || parts[ID(r)].ctype == PT_BMTL) && parts[i].ctype == PT_SLCN)
					{
						parts[i].tmp = 0;
						parts[i].ctype = PT_NSCN;
						parts[ID(r)].ctype = PT_PSCN;
					}
					else if (rt == PT_HEAC && parts[i].ctype == PT_HEAC)
					{
						if (parts[ID(r)].temp > sim->elements[PT_HEAC].HighTemperatureTransitionThreshold)
						{
							sim->part_change_type(ID(r), x+rx, y+ry, PT_LAVA);
							parts[ID(r)].ctype = PT_HEAC;
						}
					}
					else if (parts[i].ctype == PT_ROCK && rt == PT_LAVA && parts[ID(r)].ctype == PT_GOLD && parts[ID(r)].tmp == 0 &&
						sim->air->pv[y / CELL][x / CELL] >= 50 && RNG::Ref().chance(1, 10000)) // Produce GOLD veins/clusters
					{
						parts[i].ctype = PT_GOLD;
						if (rx > 1 || rx < -1) // Trend veins vertical
							parts[i].tmp = 1;
					}
				}

				if ((surround_space || sim->elements[rt].Explosive) &&
					sim->elements[rt].Flammable && RNG::Ref().chance(sim->elements[rt].Flammable + sim->air->pv[(y+ry)/CELL][(x+rx)/CELL] * 10.0f, 1000) &&
					//exceptions, t is the thing causing the flame and rt is what's burning
					(t != PT_SPRK || (rt != PT_RBDM && rt != PT_LRBD && rt != PT_INSL)) &&
					(t != PT_PHOT || rt != PT_INSL) &&
					(rt != PT_SPNG || parts[ID(r)].life == 0))
				{
					sim->part_change_type(ID(r), x+rx, y+ry, PT_FIRE);
					parts[ID(r)].temp = restrict_flt(sim->elements[PT_FIRE].DefaultProperties.temp + (sim->elements[rt].Flammable/2), MIN_TEMP, MAX_TEMP);
					parts[ID(r)].life = RNG::Ref().between(180, 259);
					parts[ID(r)].tmp = parts[ID(r)].ctype = 0;
					if (sim->elements[rt].Explosive)
						sim->air->pv[y/CELL][x/CELL] += 0.25f * CFDS;
				}
			}
	if (legacy_enable && t!=PT_SPRK) // SPRK has no legacy reactions
		FIRE_update_legacy(UPDATE_FUNC_SUBCALL_ARGS);
	return 0;
}

int FIRE_graphics(GRAPHICS_FUNC_ARGS)
{
	int caddress = (int)restrict_flt(restrict_flt((float)cpart->life, 0.0f, 200.0f)*3, 0.0f, (200.0f*3)-3);
	*colr = (unsigned char)flm_data[caddress];
	*colg = (unsigned char)flm_data[caddress+1];
	*colb = (unsigned char)flm_data[caddress+2];
	
	*firea = 255;
	*firer = *colr;
	*fireg = *colg;
	*fireb = *colb;
	
	*pixel_mode = PMODE_NONE; //Clear default, don't draw pixel
	*pixel_mode |= FIRE_ADD;
	//Returning 0 means dynamic, do not cache
	return 0;
}

void FIRE_create(ELEMENT_CREATE_FUNC_ARGS)
{
	sim->parts[i].life = RNG::Ref().between(120, 169);
}

void FIRE_init_element(ELEMENT_INIT_FUNC_ARGS)
{
	elem->Identifier = "DEFAULT_PT_FIRE";
	elem->Name = "FIRE";
	elem->Colour = COLPACK(0xFF1000);
	elem->MenuVisible = 1;
	elem->MenuSection = SC_EXPLOSIVE;
	elem->Enabled = 1;

	elem->Advection = 0.9f;
	elem->AirDrag = 0.04f * CFDS;
	elem->AirLoss = 0.97f;
	elem->Loss = 0.20f;
	elem->Collision = 0.0f;
	elem->Gravity = -0.1f;
	elem->Diffusion = 0.00f;
	elem->HotAir = 0.001f	* CFDS;
	elem->Falldown = 1;

	elem->Flammable = 0;
	elem->Explosive = 0;
	elem->Meltable = 0;
	elem->Hardness = 1;

	elem->Weight = 2;

	elem->DefaultProperties.temp = R_TEMP + 400.0f + 273.15f;
	elem->HeatConduct = 88;
	elem->Latent = 0;
	elem->Description = "Ignites flammable materials. Heats air.";

	elem->Properties = TYPE_GAS|PROP_LIFE_DEC|PROP_LIFE_KILL;

	elem->LowPressureTransitionThreshold = IPL;
	elem->LowPressureTransitionElement = NT;
	elem->HighPressureTransitionThreshold = IPH;
	elem->HighPressureTransitionElement = NT;
	elem->LowTemperatureTransitionThreshold = ITL;
	elem->LowTemperatureTransitionElement = NT;
	elem->HighTemperatureTransitionThreshold = 2773.0f;
	elem->HighTemperatureTransitionElement = PT_PLSM;

	elem->Update = &FIRE_update;
	elem->Graphics = &FIRE_graphics;
	elem->Func_Create = &FIRE_create;
	elem->Init = &FIRE_init_element;
}
