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

#include "common/Probability.h"
#include "simulation/ElementsCommon.h"
#include "EMP.h"

class DeltaTempGenerator
{
protected:
	float stepSize;
	unsigned int maxStepCount;
	Probability::SmallKBinomialGenerator binom;
public:
	DeltaTempGenerator(int n, float p, float tempStep) :
		stepSize(tempStep),
		// hardcoded limit of 10, to avoid massive lag if someone adds a few zeroes to MAX_TEMP
		maxStepCount((MAX_TEMP/stepSize < 10) ? ((unsigned int)(MAX_TEMP/stepSize)+1) : 10),
		binom(n, p, maxStepCount)
	{}
	float getDelta(float randFloat)
	{
		// randFloat should be a random float between 0 and 1
		return binom.calc(randFloat) * stepSize;
	}
	void apply(Simulation *sim, particle &p)
	{
		p.temp = restrict_flt(p.temp+getDelta(RNG::Ref().uniform01()), MIN_TEMP, MAX_TEMP);
	}
};

void EMP_ElementDataContainer::Simulation_AfterUpdate(Simulation *sim)
{
	/* Known differences from original one-particle-at-a-time version:
	 * - SPRK that disappears during a frame (such as SPRK with life==0 on that frame) will not cause destruction around it.
	 * - SPRK neighbour effects are calculated assuming the SPRK exists and causes destruction around it for the entire frame (so was not turned into BREL/NTCT partway through). This means mass EMP will be more destructive.
	 * - The chance of a METL particle near sparked semiconductor turning into BRMT within 1 frame is different if triggerCount>2. See comment for prob_breakMETLMore.
	 * - Probability of centre isElec particle breaking is slightly different (1/48 instead of 1-(1-1/80)*(1-1/120) = just under 1/48).
	 */

	particle *parts = sim->parts;
	int triggerCount = static_cast<EMP_ElementDataContainer&>(*sim->elementData[PT_EMP]).emp_trigger_count;
	if (triggerCount == 0)
		return;
	static_cast<EMP_ElementDataContainer&>(*sim->elementData[PT_EMP]).emp_trigger_count = 0;

	float prob_changeCenter = Probability::binomial_gte1(triggerCount, 1.0f/48);
	DeltaTempGenerator temp_center(triggerCount, 1.0f/100, 3000.0f);

	float prob_breakMETL = Probability::binomial_gte1(triggerCount, 1.0f/300);
	float prob_breakBMTL = Probability::binomial_gte1(triggerCount, 1.0f/160);
	DeltaTempGenerator temp_metal(triggerCount, 1.0f/280, 3000.0f);
	/* Probability of breaking from BMTL to BRMT, given that the particle has just broken from METL to BMTL. There is no mathematical reasoning for the numbers used, other than:
	 * - larger triggerCount should make this more likely, so it should depend on triggerCount instead of being a constant probability
	 * - triggerCount==1 should make this a chance of 0 (matching previous behaviour)
	 * - triggerCount==2 should make this a chance of 1/160 (matching previous behaviour)
	 */
	// TODO: work out in a more mathematical way what this should be?
	float prob_breakMETLMore = Probability::binomial_gte1(triggerCount/2, 1.0f/160);

	float prob_randWIFI = Probability::binomial_gte1(triggerCount, 1.0f/8);
	float prob_breakWIFI = Probability::binomial_gte1(triggerCount, 1.0f/16);

	float prob_breakSWCH = Probability::binomial_gte1(triggerCount, 1.0f/100);
	DeltaTempGenerator temp_SWCH(triggerCount, 1.0f/100, 2000.0f);

	float prob_breakARAY = Probability::binomial_gte1(triggerCount, 1.0f/60);

	float prob_randDLAY = Probability::binomial_gte1(triggerCount, 1.0f/70);

	for (int r = 0; r <=sim->parts_lastActiveIndex; r++)
	{
		int t = parts[r].type;
		int rx = (int)parts[r].x;
		int ry = (int)parts[r].y;
		if (t==PT_SPRK || (t==PT_SWCH && parts[r].life!=0 && parts[r].life!=10) || (t==PT_WIRE && parts[r].ctype>0))
		{
			bool is_elec = false;
			if (parts[r].ctype==PT_PSCN || parts[r].ctype==PT_NSCN || parts[r].ctype==PT_PTCT ||
			    parts[r].ctype==PT_NTCT || parts[r].ctype==PT_INST || parts[r].ctype==PT_SWCH || t==PT_WIRE || t==PT_SWCH)
			{
				is_elec = true;
				temp_center.apply(sim, parts[r]);
				if (RNG::Ref().uniform01() < prob_changeCenter)
				{
					if (RNG::Ref().chance(2, 5))
						sim->part_change_type(r, rx, ry, PT_BREL);
					else
						sim->part_change_type(r, rx, ry, PT_NTCT);
				}
			}
			for (int nx =-2; nx <= 3; nx++)
				for (int ny =-2; ny <= 2; ny++)
					if (rx+nx>=0 && ry+ny>=0 && rx+nx<XRES && ry+ny<YRES && (rx || ry))
					{
						int n = pmap[ry+ny][rx+nx];
						if (!n)
							continue;
						int ntype = TYP(n);
						n = ID(n);
						//Some elements should only be affected by wire/swch, or by a spark on inst/semiconductor
						//So not affected by spark on metl, watr etc
						if (is_elec)
						{
							switch (ntype)
							{
							case PT_METL:
								temp_metal.apply(sim, parts[n]);
								if (RNG::Ref().uniform01() < prob_breakMETL)
								{
									sim->part_change_type(n, rx+nx, ry+ny, PT_BMTL);
									if (RNG::Ref().uniform01() < prob_breakMETLMore)
									{
										sim->part_change_type(n, rx+nx, ry+ny, PT_BRMT);
										parts[n].temp = restrict_flt(parts[n].temp+1000.0f, MIN_TEMP, MAX_TEMP);
									}
								}
								break;
							case PT_BMTL:
								temp_metal.apply(sim, parts[n]);
								if (RNG::Ref().uniform01() < prob_breakBMTL)
								{
									sim->part_change_type(n, rx+nx, ry+ny, PT_BRMT);
									parts[n].temp = restrict_flt(parts[n].temp+1000.0f, MIN_TEMP, MAX_TEMP);
								}
								break;
							case PT_WIFI:
								if (RNG::Ref().uniform01() < prob_randWIFI)
								{
									// Randomize channel
									parts[n].temp = RNG::Ref().between(0, MAX_TEMP);
								}
								if (RNG::Ref().uniform01() < prob_breakWIFI)
								{
									sim->part_create(n, rx+nx, ry+ny, PT_BREL);
									parts[n].temp = restrict_flt(parts[n].temp+1000.0f, MIN_TEMP, MAX_TEMP);
								}
								continue;
							default:
								break;
							}
						}
						switch (ntype)
						{
						case PT_SWCH:
							if (RNG::Ref().uniform01() < prob_breakSWCH)
								sim->part_change_type(n, rx+nx, ry+ny, PT_BREL);
							temp_SWCH.apply(sim, parts[n]);
							break;
						case PT_ARAY:
							if (RNG::Ref().uniform01() < prob_breakARAY)
							{
								sim->part_create(n, rx+nx, ry+ny, PT_BREL);
								parts[n].temp = restrict_flt(parts[n].temp+1000.0f, MIN_TEMP, MAX_TEMP);
							}
							break;
						case PT_DLAY:
							if (RNG::Ref().uniform01() < prob_randDLAY)
							{
								// Randomize delay
								parts[n].temp = RNG::Ref().between(0, 255) + 273.15f;
							}
							break;
						default:
							break;
						}
					}
		}
	}
}

int EMP_graphics(GRAPHICS_FUNC_ARGS)
{
	if(cpart->life)
	{
		*colr = (int)(cpart->life*1.5);
		*colg = (int)(cpart->life*1.5);
		*colb = 200-(cpart->life);
	}
	return 0;
}

void EMP_init_element(ELEMENT_INIT_FUNC_ARGS)
{
	elem->Identifier = "DEFAULT_PT_EMP";
	elem->Name = "EMP";
	elem->Colour = COLPACK(0x66AAFF);
	elem->MenuVisible = 1;
	elem->MenuSection = SC_ELEC;
	elem->Enabled = 1;

	elem->Advection = 0.0f;
	elem->AirDrag = 0.00f * CFDS;
	elem->AirLoss = 0.90f;
	elem->Loss = 0.00f;
	elem->Collision = 0.0f;
	elem->Gravity = 0.0f;
	elem->Diffusion = 0.0f;
	elem->HotAir = 0.0f	* CFDS;
	elem->Falldown = 0;

	elem->Flammable = 0;
	elem->Explosive = 0;
	elem->Meltable = 0;
	elem->Hardness = 3;

	elem->Weight = 100;

	elem->HeatConduct = 121;
	elem->Latent = 0;
	elem->Description = "Electromagnetic pulse. Breaks activated electronics.";

	elem->Properties = TYPE_SOLID|PROP_LIFE_DEC;

	elem->LowPressureTransitionThreshold = IPL;
	elem->LowPressureTransitionElement = NT;
	elem->HighPressureTransitionThreshold = IPH;
	elem->HighPressureTransitionElement = NT;
	elem->LowTemperatureTransitionThreshold = ITL;
	elem->LowTemperatureTransitionElement = NT;
	elem->HighTemperatureTransitionThreshold = ITH;
	elem->HighTemperatureTransitionElement = NT;

	elem->Update = NULL;
	elem->Graphics = &EMP_graphics;
	elem->Init = &EMP_init_element;

	sim->elementData[t].reset(new EMP_ElementDataContainer);
}
