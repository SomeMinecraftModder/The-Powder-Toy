#ifndef LIFE_H
#define LIFE_H

#include "simulation/ElementDataContainer.h"
#include "simulation/GolNumbers.h"
#include "simulation/Simulation.h"

class LIFE_ElementDataContainer : public ElementDataContainer
{
	unsigned char gol[YRES][XRES];
	short gol2[YRES][XRES][9];
	int golSpeedCounter;
public:
	int golSpeed;
	int golGeneration;
	LIFE_ElementDataContainer()
	{
		std::fill_n(&gol2[0][0][0], YRES*XRES*9, 0);
		golSpeed = 1;
		golSpeedCounter = 0;
		golGeneration = 0;
	}

	virtual ElementDataContainer * Clone() { return new LIFE_ElementDataContainer(*this); }

	virtual void Simulation_Cleared(Simulation *sim)
	{
		std::fill_n(&gol2[0][0][0], YRES*XRES*9, 0);
		golSpeedCounter = 0;
		golGeneration = 0;
	}

	virtual void Simulation_BeforeUpdate(Simulation *sim)
	{
		//golSpeed is frames per generation
		if (sim->elementCount[PT_LIFE] <= 0 || ++golSpeedCounter < golSpeed)
			return;

		bool createdSomething = false;
		golSpeedCounter = 0;

		//go through every particle and set neighbor map
		for (int ny = CELL; ny < YRES-CELL; ny++)
		{
			for (int nx = CELL; nx < XRES-CELL; nx++)
			{
				int r = pmap[ny][nx];
				if (!r)
				{
					gol[ny][nx] = 0;
					continue;
				}
				if ((r&0xFF) == PT_LIFE)
				{
					unsigned char golnum = (unsigned char)(parts[r>>8].ctype+1);
					if (golnum <= 0 || golnum > NGOL)
					{
						sim->part_kill(r>>8);
						continue;
					}
					gol[ny][nx] = golnum;
					if (parts[r>>8].tmp == grule[golnum][9]-1)
					{
						for (int nnx = -1; nnx <= 1; nnx++)
						{
							//it will count itself as its own neighbor, which is needed, but will have 1 extra for delete check
							for (int nny = -1; nny <= 1; nny++)
							{
								int adx = ((nx+nnx+XRES-3*CELL)%(XRES-2*CELL))+CELL;
								int ady = ((ny+nny+YRES-3*CELL)%(YRES-2*CELL))+CELL;
								int rt = pmap[ady][adx];
								if (!rt || (rt&0xFF) == PT_LIFE)
								{
									//the total neighbor count is in 0
									gol2[ady][adx][0]++;
									//insert golnum into neighbor table
									for (int i = 1; i < 9; i++)
									{
										if (!gol2[ady][adx][i])
										{
											gol2[ady][adx][i] = (golnum<<4)+1;
											break;
										}
										else if( (gol2[ady][adx][i]>>4) == golnum)
										{
											gol2[ady][adx][i]++;
											break;
										}
									}
								}
							}
						}
					}
					else
					{
						parts[r>>8].tmp--;
					}
				}
			}
		}
		//go through every particle again, but check neighbor map, then update particles
		for (int ny = CELL; ny < YRES-CELL; ny++)
		{
			for (int nx = CELL; nx < XRES-CELL; nx++)
			{
				int r = pmap[ny][nx];
				if (r && (r&0xFF) != PT_LIFE)
					continue;
				int neighbors = gol2[ny][nx][0];
				if (neighbors)
				{
					int golnum = gol[ny][nx];
					if (!r)
					{
						//Find which type we can try and create
						int creategol = 0xFF;
						for (int i = 1; i < 9; i++)
						{
							if (!gol2[ny][nx][i])
								break;
							golnum = (gol2[ny][nx][i]>>4);
							if (grule[golnum][neighbors] >= 2 && (gol2[ny][nx][i]&0xF) >= (neighbors%2)+neighbors/2)
							{
								if (golnum < creategol)
									creategol = golnum;
							}
						}
						if (creategol < 0xFF)
							if (sim->part_create(-1, nx, ny, PT_LIFE, creategol-1) > -1)
								createdSomething = true;
					}
					//subtract 1 because it counted itself
					else if (grule[golnum][neighbors-1] == 0 || grule[golnum][neighbors-1] == 2)
					{
						if (parts[r>>8].tmp == grule[golnum][9]-1)
							parts[r>>8].tmp--;
					}
					//this improves performance A LOT compared to the memset, i was getting ~23 more fps with this.
					for (int z = 0; z < 9; z++)
						gol2[ny][nx][z] = 0;
				}
				//we still need to kill things with 0 neighbors (higher state life)
				if (r && parts[r>>8].tmp <= 0)
					sim->part_kill(r>>8);
			}
		}
		if (createdSomething)
			golGeneration++;
	}
};

#endif
