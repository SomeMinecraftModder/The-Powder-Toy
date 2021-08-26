
#include "Snapshot.h"
#include "common/tpt-compat.h"
#include "elements/STKM.h"
#include "elements/PRTI.h"
#include "game/Authors.h"
#include "game/Sign.h"
#include "simulation/ElementDataContainer.h"
#include "simulation/Simulation.h"

std::unique_ptr<Snapshot> Snapshot::Create(Simulation * sim)
{
	auto snap = std::make_unique<Snapshot>();
	snap->AirPressure  .insert(snap->AirPressure.begin(),   &sim->air->pv[0][0],    &sim->air->pv   [0][0] + ((XRES / CELL) * (YRES / CELL)));
	snap->AirVelocityX .insert(snap->AirVelocityX.begin(),  &sim->air->vx[0][0],    &sim->air->vx   [0][0] + ((XRES / CELL) * (YRES / CELL)));
	snap->AirVelocityY .insert(snap->AirVelocityY.begin(),  &sim->air->vy[0][0],    &sim->air->vy   [0][0] + ((XRES / CELL) * (YRES / CELL)));
	snap->AmbientHeat  .insert(snap->AmbientHeat.begin(),   &sim->air->hv[0][0],    &sim->air->hv   [0][0] + ((XRES / CELL) * (YRES / CELL)));
	snap->GravVelocityX.insert(snap->GravVelocityX.begin(), &sim->grav->gravx[0],   &sim->grav->gravx  [0] + ((XRES / CELL) * (YRES / CELL)));
	snap->GravVelocityY.insert(snap->GravVelocityY.begin(), &sim->grav->gravy[0],   &sim->grav->gravy  [0] + ((XRES / CELL) * (YRES / CELL)));
	snap->GravValue    .insert(snap->GravValue.begin(),     &sim->grav->gravp[0],   &sim->grav->gravp  [0] + ((XRES / CELL) * (YRES / CELL)));
	snap->GravMap      .insert(snap->GravMap.begin(),       &sim->grav->gravmap[0], &sim->grav->gravmap[0] + ((XRES / CELL) * (YRES / CELL)));
	snap->BlockMap     .insert(snap->BlockMap.begin(),      &bmap[0][0],            &bmap           [0][0] + ((XRES / CELL) * (YRES / CELL)));
	snap->ElecMap      .insert(snap->ElecMap.begin(),       &emap[0][0],            &emap           [0][0] + ((XRES / CELL) * (YRES / CELL)));
	snap->FanVelocityX .insert(snap->FanVelocityX.begin(),  &sim->air->fvx[0][0],   &sim->air->fvx  [0][0] + ((XRES / CELL) * (YRES / CELL)));
	snap->FanVelocityY .insert(snap->FanVelocityY.begin(),  &sim->air->fvy[0][0],   &sim->air->fvy  [0][0] + ((XRES / CELL) * (YRES / CELL)));
	snap->Particles    .insert(snap->Particles.begin(),     &parts[0],              &parts[sim->parts_lastActiveIndex + 1]);
	snap->Signs = signs;
	snap->Authors = authors;

	sim->RecountElements();
	for (int i = 0; i < PT_NUM; i++)
	{
		if (sim->elementData[i])
			snap->elementData[i] = sim->elementData[i]->Clone();
		else
			snap->elementData[i] = nullptr;
	}
	return snap;
}

void Snapshot::Restore(Simulation * sim, const Snapshot &snap)
{
	for (int i = 0; i < NPART; i++)
		parts[i].type = 0;

	std::copy(snap.AirPressure .begin(), snap.AirPressure .end(), &sim->air->pv [0][0]);
	std::copy(snap.AirVelocityX.begin(), snap.AirVelocityX.end(), &sim->air->vx [0][0]);
	std::copy(snap.AirVelocityY.begin(), snap.AirVelocityY.end(), &sim->air->vy [0][0]);
	std::copy(snap.AmbientHeat .begin(), snap.AmbientHeat .end(), &sim->air->hv [0][0]);
	std::copy(snap.BlockMap    .begin(), snap.BlockMap    .end(), &bmap         [0][0]);
	std::copy(snap.ElecMap     .begin(), snap.ElecMap     .end(), &emap         [0][0]);
	std::copy(snap.FanVelocityX.begin(), snap.FanVelocityX.end(), &sim->air->fvx[0][0]);
	std::copy(snap.FanVelocityY.begin(), snap.FanVelocityY.end(), &sim->air->fvy[0][0]);
	std::copy(snap.Particles   .begin(), snap.Particles   .end(), &parts        [0]);

	if (sim->grav->IsEnabled())
	{
		std::copy(snap.GravVelocityX.begin(), snap.GravVelocityX.end(), &sim->grav->gravx  [0]);
		std::copy(snap.GravVelocityY.begin(), snap.GravVelocityY.end(), &sim->grav->gravy  [0]);
		std::copy(snap.GravValue    .begin(), snap.GravValue    .end(), &sim->grav->gravp  [0]);
		std::copy(snap.GravMap      .begin(), snap.GravMap      .end(), &sim->grav->gravmap[0]);
	}

	ClearSigns();
	signs = snap.Signs;
	authors = snap.Authors;

	for (int i = 0; i < PT_NUM; i++)
	{
		if (snap.elementData[i])
			sim->elementData[i] = snap.elementData[i]->Clone();
	}

	sim->air->RecalculateBlockAirMaps(sim);
	sim->parts_lastActiveIndex = NPART - 1;
	sim->RecalcFreeParticles(false);
	sim->forceStackingCheck = true;
	sim->grav->gravWallChanged = true;
	sim->RecountElements();
}
