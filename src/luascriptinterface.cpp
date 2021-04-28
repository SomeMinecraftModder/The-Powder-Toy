#ifdef LUACONSOLE

#include <dirent.h>
#include <string>
#ifdef WIN
#include <direct.h>
#else
#include <unistd.h>
#endif
#include <sys/stat.h>

#include "defines.h"
#include "EventLoopSDL.h"
#include "graphics.h"
#include "interface.h"
#include "luascriptinterface.h"
#include "powder.h"
#include "powdergraphics.h"
#include "save_legacy.h"
#include "hud.h"

#include "common/Format.h"
#include "common/Platform.h"
#include "game/Authors.h"
#include "game/Brush.h"
#include "game/Menus.h"
#include "game/Request.h"
#include "game/Save.h"
#include "game/Sign.h"
#include "game/ToolTip.h"
#include "gui/game/PowderToy.h"
#include "graphics/ARGBColour.h"
#include "graphics/Renderer.h"
#include "interface/Engine.h"
#include "lua/LuaSmartRef.h"
#include "simulation/Simulation.h"
#include "simulation/WallNumbers.h"
#include "simulation/Snapshot.h"
#include "simulation/ToolNumbers.h"
#include "simulation/Tool.h"
#include "simulation/elements/FIGH.h"
#include "simulation/elements/LIFE.h"
#include "simulation/elements/STKM.h"

/*

SIMULATION API

*/

int simulation_signIndex(lua_State *l)
{
	std::string key = luaL_checkstring(l, 2);

	//Get Raw Index value for element. Maybe there is a way to get the sign index some other way?
	lua_pushstring(l, "id");
	lua_rawget(l, 1);
	int id = lua_tointeger(l, lua_gettop(l))-1;

	if (id < 0 || id >= MAXSIGNS)
	{
		luaL_error(l, "Invalid sign ID (stop messing with things): %i", id);
		return 0;
	}
	if (id >= (int)signs.size())
	{
		return lua_pushnil(l), 1;
	}

	if (!key.compare("text"))
		return lua_pushstring(l, signs[id]->GetText().c_str()), 1;
	else if (!key.compare("displayText"))
		return lua_pushstring(l, signs[id]->GetDisplayText(luaSim).c_str()), 1;
	else if (!key.compare("linkText"))
		return lua_pushstring(l, signs[id]->GetLinkText().c_str()), 1;
	else if (!key.compare("justification"))
		return lua_pushnumber(l, signs[id]->GetJustification()), 1;
	else if (!key.compare("x"))
		return lua_pushnumber(l, signs[id]->GetRealPos().X), 1;
	else if (!key.compare("y"))
		return lua_pushnumber(l, signs[id]->GetRealPos().Y), 1;
	else if (!key.compare("screenX"))
	{
		int x, y, w, h;
		signs[id]->GetPos(luaSim, x, y, w, h);
		lua_pushnumber(l, x);
		return 1;
	}
	else if (!key.compare("screenY"))
	{
		int x, y, w, h;
		signs[id]->GetPos(luaSim, x, y, w, h);
		lua_pushnumber(l, y);
		return 1;
	}
	else if (!key.compare("width"))
	{
		int x, y, w, h;
		signs[id]->GetPos(luaSim, x, y, w, h);
		lua_pushnumber(l, w);
		return 1;
	}
	else if (!key.compare("height"))
	{
		int x, y, w, h;
		signs[id]->GetPos(luaSim, x, y, w, h);
		lua_pushnumber(l, h);
		return 1;
	}
	else
		return lua_pushnil(l), 1;
}

int simulation_signNewIndex(lua_State *l)
{
	std::string key = luaL_checkstring(l, 2);

	//Get Raw Index value for element. Maybe there is a way to get the sign index some other way?
	lua_pushstring(l, "id");
	lua_rawget(l, 1);
	int id = lua_tointeger(l, lua_gettop(l))-1;

	if (id < 0 || id >= MAXSIGNS)
	{
		luaL_error(l, "Invalid sign ID (stop messing with things)");
		return 0;
	}

	if (!key.compare("text"))
	{
		const char *temp = luaL_checkstring(l, 3);
		std::string cleaned = Format::CleanString(temp, false, true, true).substr(0, 45);
		if (!cleaned.empty())
			signs[id]->SetText(cleaned);
		else
			luaL_error(l, "Text is empty");
		return 1;
	}
	else if (!key.compare("justification"))
	{
		int ju = luaL_checkinteger(l, 3);
		if (ju >= 0 && ju <= 3)
			return signs[id]->SetJustification((Sign::Justification)ju), 1;
		else
			luaL_error(l, "Invalid justification");
		return 0;
	}
	else if (!key.compare("x"))
	{
		int x = luaL_checkinteger(l, 3);
		if (x >= 0 && x < XRES)
			return signs[id]->SetPos(Point(x, signs[id]->GetRealPos().Y)), 1;
		else
			luaL_error(l, "Invalid X coordinate");
		return 0;
	}
	else if (!key.compare("y"))
	{
		int y = luaL_checkinteger(l, 3);
		if (y >= 0 && y < YRES)
			return signs[id]->SetPos(Point(signs[id]->GetRealPos().X, y)), 1;
		else
			luaL_error(l, "Invalid Y coordinate");
		return 0;
	}
	else if (!key.compare("displayText") || !key.compare("linkText")  || !key.compare("screenX") || !key.compare("screenY") || !key.compare("width") || !key.compare("height"))
	{
		luaL_error(l, "That property can't be directly set");
	}
	return 0;
}

// Creates a new sign at the first open index
int simulation_newsign(lua_State *l)
{
	if (signs.size() >= MAXSIGNS)
	{
		lua_pushnumber(l, -1);
		return 1;
	}
	const char* temp = luaL_checkstring(l, 1);
	int x = luaL_checkinteger(l, 2);
	int y = luaL_checkinteger(l, 3);
	int ju = luaL_optinteger(l, 4, 1);
	if (ju < 0 || ju > 3)
		return luaL_error(l, "Invalid justification");
	if (x < 0 || x >= XRES)
		return luaL_error(l, "Invalid X coordinate");
	if (y < 0 || y >= YRES)
		return luaL_error(l, "Invalid Y coordinate");

	std::string cleaned = Format::CleanString(temp, false, true, true).substr(0, 45);
	signs.push_back(new Sign(cleaned, x, y, (Sign::Justification)ju));
	lua_pushnumber(l, signs.size());
	return 1;
}

// Deletes a sign
int simulation_deletesign(lua_State *l)
{
	int signID = luaL_checkinteger(l, 1);
	if (signID <= 0 || signID > (int)signs.size())
		return luaL_error(l, "Sign doesn't exist");

	delete signs[signID-1];
	signs.erase(signs.begin()+signID-1);
	return 1;
}

void initSimulationAPI(lua_State * l)
{
	//Methods
	struct luaL_Reg simulationAPIMethods [] = {
		{"partNeighbors", simulation_partNeighbours},
		{"partNeighbours", simulation_partNeighbours},
		{"partChangeType", simulation_partChangeType},
		{"partCreate", simulation_partCreate},
		{"partID", simulation_partID},
		{"partProperty", simulation_partProperty},
		{"partPosition", simulation_partPosition},
		{"partKill", simulation_partKill},
		{"pressure", simulation_pressure},
		{"ambientHeat", simulation_ambientHeat},
		{"velocityX", simulation_velocityX},
		{"velocityY", simulation_velocityY},
		{"gravMap", simulation_gravMap},
		{"createParts", simulation_createParts},
		{"createLine", simulation_createLine},
		{"createBox", simulation_createBox},
		{"floodParts", simulation_floodParts},
		{"createWalls", simulation_createWalls},
		{"createWallLine", simulation_createWallLine},
		{"createWallBox", simulation_createWallBox},
		{"floodWalls", simulation_floodWalls},
		{"toolBrush", simulation_toolBrush},
		{"toolLine", simulation_toolLine},
		{"toolBox", simulation_toolBox},
		{"decoBrush", simulation_decoBrush},
		{"decoLine", simulation_decoLine},
		{"decoBox", simulation_decoBox},
		{"floodDeco", simulation_floodDeco},
		{"decoColor", simulation_decoColor},
		{"decoColour", simulation_decoColor},
		{"clearSim", simulation_clearSim},
		{"clearRect", simulation_clearRect},
		{"resetTemp", simulation_resetTemp},
		{"resetPressure", simulation_resetPressure},
		{"saveStamp", simulation_saveStamp},
		{"loadStamp", simulation_loadStamp},
		{"deleteStamp", simulation_deleteStamp},
		{"loadSave", simulation_loadSave},
		{"reloadSave", simulation_reloadSave},
		{"getSaveID", simulation_getSaveID},
		{"adjustCoords", simulation_adjustCoords},
		{"prettyPowders", simulation_prettyPowders},
		{"gravityGrid", simulation_gravityGrid},
		{"edgeMode", simulation_edgeMode},
		{"gravityMode", simulation_gravityMode},
		{"airMode", simulation_airMode},
		{"waterEqualization", simulation_waterEqualization},
		{"waterEqualisation", simulation_waterEqualization},
		{"ambientAirTemp", simulation_ambientAirTemp},
		{"elementCount", simulation_elementCount},
		{"can_move", simulation_canMove},
		{"parts", simulation_parts},
		{"brush", simulation_brush},
		{"pmap", simulation_pmap},
		{"photons", simulation_photons},
		{"neighbors", simulation_neighbours},
		{"neighbours", simulation_neighbours},
		{"framerender", simulation_framerender},
		{"gspeed", simulation_gspeed},
		{"takeSnapshot", simulation_takeSnapshot},
		{"stickman", simulation_stickman},
		{NULL, NULL}
	};
	luaL_register(l, "simulation", simulationAPIMethods);

	//Sim shortcut
	lua_getglobal(l, "simulation");
	lua_setglobal(l, "sim");

	//Static values
	SETCONST(l, XRES);
	SETCONST(l, YRES);
	SETCONST(l, CELL);
	SETCONST(l, NT);
	SETCONST(l, ST);
	SETCONST(l, ITH);
	SETCONST(l, ITL);
	SETCONST(l, IPH);
	SETCONST(l, IPL);
	SETCONST(l, PT_NUM);
	lua_pushinteger(l, 0); lua_setfield(l, -2, "NUM_PARTS");
	SETCONST(l, R_TEMP);
	SETCONST(l, MAX_TEMP);
	SETCONST(l, MIN_TEMP);

	SETCONST(l, TOOL_HEAT);
	SETCONST(l, TOOL_COOL);
	SETCONST(l, TOOL_AIR);
	SETCONST(l, TOOL_VAC);
	SETCONST(l, TOOL_PGRV);
	SETCONST(l, TOOL_NGRV);
	SETCONST(l, TOOL_MIX);
	SETCONST(l, TOOL_CYCL);
	SETCONST(l, TOOL_WIND);
	SETCONST(l, TOOL_PROP);
	SETCONST(l, TOOL_SIGN);

	SETCONST(l, DECO_DRAW);
	SETCONST(l, DECO_CLEAR);
	SETCONST(l, DECO_ADD);
	SETCONST(l, DECO_SUBTRACT);
	SETCONST(l, DECO_MULTIPLY);
	SETCONST(l, DECO_DIVIDE);
	SETCONST(l, DECO_LIGHTEN);
	SETCONST(l, DECO_DARKEN);
	SETCONST(l, DECO_SMUDGE);

	SETCONST(l, PMAPBITS);
	SETCONST(l, PMAPMASK);

	//Declare FIELD_BLAH constants
	int particlePropertiesCount = 0;
	for (auto &prop : particle::GetProperties())
	{
		lua_pushinteger(l, particlePropertiesCount++);
		lua_setfield(l, -2, ("FIELD_" + Format::ToUpper(prop.Name)).c_str());
	}

	lua_newtable(l);
	for (int i = 1; i <= MAXSIGNS; i++)
	{
		lua_newtable(l);
		lua_pushinteger(l, i); //set "id" to table index
		lua_setfield(l, -2, "id");
		lua_newtable(l);
		lua_pushcfunction(l, simulation_signIndex);
		lua_setfield(l, -2, "__index");
		lua_pushcfunction(l, simulation_signNewIndex);
		lua_setfield(l, -2, "__newindex");
		lua_setmetatable(l, -2);
		lua_pushinteger(l, i); //table index
		lua_insert(l, -2); //swap k and v
		lua_settable(l, -3); //set metatable to signs[i]
	}
	lua_pushcfunction(l, simulation_newsign);
	lua_setfield(l, -2, "new");
	lua_pushcfunction(l, simulation_deletesign);
	lua_setfield(l, -2, "delete");
	lua_setfield(l, -2, "signs");
}

int simulation_partNeighbours(lua_State * l)
{
	int id = 0;
	lua_newtable(l);
	int x = lua_tointeger(l, 1), y = lua_tointeger(l, 2), r = lua_tointeger(l, 3), rx, ry, n;
	// This is one more than the number of arguments because a table has just been pushed onto the stack with lua_newtable(l);
	if(lua_gettop(l) == 5)
	{
		int t = lua_tointeger(l, 4);
		for (rx = -r; rx <= r; rx++)
			for (ry = -r; ry <= r; ry++)
				if (x+rx >= 0 && y+ry >= 0 && x+rx < XRES && y+ry < YRES && (rx || ry))
				{
					n = pmap[y+ry][x+rx];
					if (!n || TYP(n) != t)
						n = photons[y+ry][x+rx];
					if (n && TYP(n) == t)
					{
						lua_pushinteger(l, ID(n));
						lua_rawseti(l, -2, id++);
					}
				}

	}
	else
	{
		for (rx = -r; rx <= r; rx++)
			for (ry = -r; ry <= r; ry++)
				if (x+rx >= 0 && y+ry >= 0 && x+rx < XRES && y+ry < YRES && (rx || ry))
				{
					n = pmap[y+ry][x+rx];
					if (!n)
						n = photons[y+ry][x+rx];
					if (n)
					{
						lua_pushinteger(l, ID(n));
						lua_rawseti(l, -2, id++);
					}
				}
	}
	return 1;
}

int simulation_partChangeType(lua_State * l)
{
	int partIndex = lua_tointeger(l, 1);
	if (partIndex < 0 || partIndex >= NPART || !parts[partIndex].type)
		return 0;
	part_change_type(partIndex, (int)(parts[partIndex].x+0.5f), (int)(parts[partIndex].y+0.5f), lua_tointeger(l, 2));
	return 0;
}

int simulation_partCreate(lua_State * l)
{
	int newID = lua_tointeger(l, 1);
	if(newID >= NPART || newID < -3)
	{
		lua_pushinteger(l, -1);
		return 1;
	}
	if (newID >= 0 && !parts[newID].type)
	{
		lua_pushinteger(l, -1);
		return 1;
	}
	int type = lua_tointeger(l, 4);
	int v = -1;
	if (type&~PMAPMASK)
	{
		v = ID(type);
		type = TYP(type);
	}
	lua_pushinteger(l, luaSim->part_create(newID, lua_tointeger(l, 2), lua_tointeger(l, 3), type, v));
	return 1;
}

int simulation_partID(lua_State * l)
{
	int x = lua_tointeger(l, 1);
	int y = lua_tointeger(l, 2);
	int amalgam; // "an alloy of mercury with another metal that is solid or liquid at room temperature" What?

	if(x < 0 || x >= XRES || y < 0 || y >= YRES)
	{
		lua_pushnil(l);
		return 1;
	}

	amalgam = pmap[y][x];
	if(!amalgam)
		amalgam = photons[y][x];
	if (!amalgam)
		lua_pushnil(l);
	else
		lua_pushinteger(l, ID(amalgam));
	return 1;
}

int simulation_partPosition(lua_State * l)
{
	int particleID = lua_tointeger(l, 1);
	int argCount = lua_gettop(l);
	if(particleID < 0 || particleID >= NPART || !parts[particleID].type)
	{
		if(argCount == 1)
		{
			lua_pushnil(l);
			lua_pushnil(l);
			return 2;
		} else {
			return 0;
		}
	}
	
	if(argCount == 3)
	{
		parts[particleID].x = (float)lua_tonumber(l, 2);
		parts[particleID].y = (float)lua_tonumber(l, 3);
		return 0;
	}
	else
	{
		lua_pushnumber(l, parts[particleID].x);
		lua_pushnumber(l, parts[particleID].y);
		return 2;
	}
}

int simulation_partProperty(lua_State * l)
{
	int argCount = lua_gettop(l);
	int particleID = luaL_checkinteger(l, 1);

	if (particleID < 0 || particleID >= NPART || !parts[particleID].type)
	{
		if (argCount == 3)
		{
			lua_pushnil(l);
			return 1;
		}
		else
			return 0;
	}

	auto &properties = particle::GetProperties();
	auto prop = properties.end();

	//Get field
	if (lua_type(l, 2) == LUA_TNUMBER)
	{
		int fieldID = lua_tointeger(l, 2);
		if (fieldID < 0 || fieldID >= (int)properties.size())
			return luaL_error(l, "Invalid field ID (%d)", fieldID);
		prop = properties.begin() + fieldID;
	}
	else if (lua_type(l, 2) == LUA_TSTRING)
	{
		const char* fieldName = lua_tostring(l, 2);
		prop = std::find_if(properties.begin(), properties.end(), [&fieldName](StructProperty const &p) {
			return p.Name == fieldName;
		});
		if (prop == properties.end())
			return luaL_error(l, "Unknown field (%s)", fieldName);
	}
	else
	{
		return luaL_error(l, "Field ID must be an name (string) or identifier (integer)");
	}

	//Calculate memory address of property
	auto propertyAddress = reinterpret_cast<intptr_t>((reinterpret_cast<unsigned char*>(&luaSim->parts[particleID])) + prop->Offset);

	if (argCount == 3)
	{
		if (prop == properties.begin() + 0) // i.e. it's .type
			luaSim->part_change_type_force(particleID, luaL_checkinteger(l, 3));
		else
			LuaSetProperty(l, *prop, propertyAddress, 3);
		return 0;
	}
	else
	{
		LuaGetProperty(l, *prop, propertyAddress);
		return 1;
	}
}

int simulation_partKill(lua_State * l)
{
	if (lua_gettop(l) == 2)
		luaSim->part_delete(lua_tointeger(l, 1), lua_tointeger(l, 2));
	else
	{
		int i = lua_tointeger(l, 1);
		if (i>=0 && i<NPART)
			luaSim->part_kill(lua_tointeger(l, 1));
	}
	return 0;
}

int simulation_pressure(lua_State* l)
{
	int argCount = lua_gettop(l), x, y, width = 1, height = 1;
	float value;
	luaL_checktype(l, 1, LUA_TNUMBER);
	luaL_checktype(l, 2, LUA_TNUMBER);
	x = lua_tointeger(l, 1);
	y = lua_tointeger(l, 2);
	if (x*CELL<0 || y*CELL<0 || x*CELL>=XRES || y*CELL>=YRES)
		return luaL_error(l, "coordinates out of range (%d,%d)", x, y);

	if (argCount == 2)
	{
		lua_pushnumber(l, luaSim->air->pv[y][x]);
		return 1;
	}
	luaL_checktype(l, 3, LUA_TNUMBER);
	if (argCount == 3)
		value = (float)lua_tonumber(l, 3);
	else
	{
		luaL_checktype(l, 4, LUA_TNUMBER);
		luaL_checktype(l, 5, LUA_TNUMBER);
		width = lua_tointeger(l, 3);
		height = lua_tointeger(l, 4);
		value = (float)lua_tonumber(l, 5);
	}
	if(value > 256.0f)
		value = 256.0f;
	else if(value < -256.0f)
		value = -256.0f;

	set_map(x, y, width, height, value, 1);
	return 0;
}

int simulation_ambientHeat(lua_State* l)
{
	int argCount = lua_gettop(l), x, y, width = 1, height = 1;
	float value;
	luaL_checktype(l, 1, LUA_TNUMBER);
	luaL_checktype(l, 2, LUA_TNUMBER);
	x = lua_tointeger(l, 1);
	y = lua_tointeger(l, 2);
	if (x*CELL<0 || y*CELL<0 || x*CELL>=XRES || y*CELL>=YRES)
		return luaL_error(l, "coordinates out of range (%d,%d)", x, y);

	if (argCount == 2)
	{
		lua_pushnumber(l, luaSim->air->hv[y][x]);
		return 1;
	}
	luaL_checktype(l, 3, LUA_TNUMBER);
	if (argCount == 3)
		value = (float)lua_tonumber(l, 3);
	else
	{
		luaL_checktype(l, 4, LUA_TNUMBER);
		luaL_checktype(l, 5, LUA_TNUMBER);
		width = lua_tointeger(l, 3);
		height = lua_tointeger(l, 4);
		value = (float)lua_tonumber(l, 5);
	}
	if(value > MAX_TEMP)
		value = MAX_TEMP;
	else if(value < MIN_TEMP)
		value = MIN_TEMP;

	set_map(x, y, width, height, value, 2);
	return 0;
}

int simulation_velocityX(lua_State* l)
{
	int argCount = lua_gettop(l), x, y, width = 1, height = 1;
	float value;
	luaL_checktype(l, 1, LUA_TNUMBER);
	luaL_checktype(l, 2, LUA_TNUMBER);
	x = lua_tointeger(l, 1);
	y = lua_tointeger(l, 2);
	if (x*CELL<0 || y*CELL<0 || x*CELL>=XRES || y*CELL>=YRES)
		return luaL_error(l, "coordinates out of range (%d,%d)", x, y);

	if (argCount == 2)
	{
		lua_pushnumber(l, luaSim->air->vx[y][x]);
		return 1;
	}
	luaL_checktype(l, 3, LUA_TNUMBER);
	if (argCount == 3)
		value = (float)lua_tonumber(l, 3);
	else
	{
		luaL_checktype(l, 4, LUA_TNUMBER);
		luaL_checktype(l, 5, LUA_TNUMBER);
		width = lua_tointeger(l, 3);
		height = lua_tointeger(l, 4);
		value = (float)lua_tonumber(l, 5);
	}
	if(value > 256.0f)
		value = 256.0f;
	else if(value < -256.0f)
		value = -256.0f;

	set_map(x, y, width, height, value, 3);
	return 0;
}

int simulation_velocityY(lua_State* l)
{
	int argCount = lua_gettop(l), x, y, width = 1, height = 1;
	float value;
	luaL_checktype(l, 1, LUA_TNUMBER);
	luaL_checktype(l, 2, LUA_TNUMBER);
	x = lua_tointeger(l, 1);
	y = lua_tointeger(l, 2);
	if (x*CELL<0 || y*CELL<0 || x*CELL>=XRES || y*CELL>=YRES)
		return luaL_error(l, "coordinates out of range (%d,%d)", x, y);

	if (argCount == 2)
	{
		lua_pushnumber(l, luaSim->air->vy[y][x]);
		return 1;
	}
	luaL_checktype(l, 3, LUA_TNUMBER);
	if (argCount == 3)
		value = (float)lua_tonumber(l, 3);
	else
	{
		luaL_checktype(l, 4, LUA_TNUMBER);
		luaL_checktype(l, 5, LUA_TNUMBER);
		width = lua_tointeger(l, 3);
		height = lua_tointeger(l, 4);
		value = (float)lua_tonumber(l, 5);
	}
	if(value > 256.0f)
		value = 256.0f;
	else if(value < -256.0f)
		value = -256.0f;

	set_map(x, y, width, height, value, 4);
	return 0;
}

int simulation_gravMap(lua_State* l)
{
	int argCount = lua_gettop(l), x, y, width = 1, height = 1;
	float value;
	luaL_checktype(l, 1, LUA_TNUMBER);
	luaL_checktype(l, 2, LUA_TNUMBER);
	x = lua_tointeger(l, 1);
	y = lua_tointeger(l, 2);
	if (x*CELL<0 || y*CELL<0 || x*CELL>=XRES || y*CELL>=YRES)
		return luaL_error(l, "coordinates out of range (%d,%d)", x, y);

	if (argCount == 2)
	{
		lua_pushnumber(l, luaSim->grav->gravp[y*XRES/CELL+x]);
		return 1;
	}
	luaL_checktype(l, 3, LUA_TNUMBER);
	if (argCount == 3)
		value = (float)lua_tonumber(l, 3);
	else
	{
		luaL_checktype(l, 4, LUA_TNUMBER);
		luaL_checktype(l, 5, LUA_TNUMBER);
		width = lua_tointeger(l, 3);
		height = lua_tointeger(l, 4);
		value = (float)lua_tonumber(l, 5);
	}

	set_map(x, y, width, height, value, 5);
	return 0;
}

int simulation_createParts(lua_State * l)
{
	int x = luaL_optint(l,1,-1);
	int y = luaL_optint(l,2,-1);
	int rx = luaL_optint(l,3,5);
	int ry = luaL_optint(l,4,5);
	int c = luaL_optint(l,5,((ElementTool*)activeTools[0])->GetID());
	int brush = luaL_optint(l,6,CIRCLE_BRUSH);
	int flags = luaL_optint(l,7,get_brush_flags());
	if (brush < 0 || brush >= BRUSH_NUM)
		return luaL_error(l, "Invalid brush id '%d'", brush);

	Brush* tempBrush = new Brush(Point(rx, ry), brush);
	int ret = luaSim->CreateParts(x, y, c, flags, true, tempBrush);
	delete tempBrush;
	lua_pushinteger(l, ret);
	return 1;
}

int simulation_createLine(lua_State * l)
{
	int x1 = luaL_optint(l,1,-1);
	int y1 = luaL_optint(l,2,-1);
	int x2 = luaL_optint(l,3,-1);
	int y2 = luaL_optint(l,4,-1);
	int rx = luaL_optint(l,5,5);
	int ry = luaL_optint(l,6,5);
	int c = luaL_optint(l,7,((ElementTool*)activeTools[0])->GetID());
	int brush = luaL_optint(l,8,CIRCLE_BRUSH);
	int flags = luaL_optint(l,9,get_brush_flags());
	if (brush < 0 || brush >= BRUSH_NUM)
		return luaL_error(l, "Invalid brush id '%d'", brush);

	Brush* tempBrush = new Brush(Point(rx, ry), brush);
	luaSim->CreateLine(x1, y1, x2, y2, c, flags, tempBrush);
	delete tempBrush;
	return 0;
}

int simulation_createBox(lua_State * l)
{
	int x1 = luaL_optint(l,1,-1);
	int y1 = luaL_optint(l,2,-1);
	int x2 = luaL_optint(l,3,-1);
	int y2 = luaL_optint(l,4,-1);
	int c = luaL_optint(l,5,((ElementTool*)activeTools[0])->GetID());
	int flags = luaL_optint(l,6,get_brush_flags());

	luaSim->CreateBox(x1, y1, x2, y2, c, flags);
	return 0;
}

int simulation_floodParts(lua_State * l)
{
	int x = luaL_optint(l,1,-1);
	int y = luaL_optint(l,2,-1);
	int c = luaL_optint(l,3,((ElementTool*)activeTools[0])->GetID());
	int cm = luaL_optint(l,4,-1);
	int flags = luaL_optint(l,5,get_brush_flags());

	if (x < CELL || x >= XRES-CELL || y < CELL || y >= YRES-CELL)
		return luaL_error(l, "coordinates out of range (%d,%d)", x, y);

	int ret = luaSim->FloodParts(x, y, c, cm, flags);
	lua_pushinteger(l, ret);
	return 1;
}

int simulation_createWalls(lua_State * l)
{
	int x = luaL_optint(l,1,-1)/CELL;
	int y = luaL_optint(l,2,-1)/CELL;
	int rx = luaL_optint(l,3,0)/CELL;
	int ry = luaL_optint(l,4,0)/CELL;
	int c = luaL_optint(l,5,WL_WALL);
	if (c < 0 || c >= WALLCOUNT)
		return luaL_error(l, "Unrecognised wall id '%d'", c);

	luaSim->CreateWallBox(x-rx, y-ry, x+rx, y+ry, c);
	lua_pushinteger(l, 1);
	return 1;
}

int simulation_createWallLine(lua_State * l)
{
	int x1 = luaL_optint(l,1,-1)/CELL;
	int y1 = luaL_optint(l,2,-1)/CELL;
	int x2 = luaL_optint(l,3,-1)/CELL;
	int y2 = luaL_optint(l,4,-1)/CELL;
	int rx = luaL_optint(l,5,0)/CELL;
	int ry = luaL_optint(l,6,0)/CELL;
	int c = luaL_optint(l,7,WL_WALL);
	if (c < 0 || c >= WALLCOUNT)
		return luaL_error(l, "Unrecognised wall id '%d'", c);

	luaSim->CreateWallLine(x1, y1, x2, y2, rx, ry, c);
	return 0;
}

int simulation_createWallBox(lua_State * l)
{
	int x1 = luaL_optint(l,1,-1)/CELL;
	int y1 = luaL_optint(l,2,-1)/CELL;
	int x2 = luaL_optint(l,3,-1)/CELL;
	int y2 = luaL_optint(l,4,-1)/CELL;
	int c = luaL_optint(l,5,WL_WALL);
	if (c < 0 || c >= WALLCOUNT)
		return luaL_error(l, "Unrecognised wall id '%d'", c);

	luaSim->CreateWallBox(x1, y1, x2, y2, c);
	return 0;
}

int simulation_floodWalls(lua_State * l)
{
	int x = luaL_optint(l,1,-1)/CELL;
	int y = luaL_optint(l,2,-1)/CELL;
	int c = luaL_optint(l,3,WL_WALL);
	int bm = luaL_optint(l,4,-1);
	if (c < 0 || c >= WALLCOUNT)
		return luaL_error(l, "Unrecognised wall id '%d'", c);
	if (c == WL_STREAM)
	{
		lua_pushinteger(l, 0);
		return 1;
	}

	int ret = luaSim->FloodWalls(x, y, c, bm);
	lua_pushinteger(l, ret);
	return 1;
}

int simulation_toolBrush(lua_State * l)
{
	int x = luaL_optint(l,1,-1);
	int y = luaL_optint(l,2,-1);
	int rx = luaL_optint(l,3,5);
	int ry = luaL_optint(l,4,5);
	int tool = luaL_optint(l,5,TOOL_HEAT);
	int brush = luaL_optint(l,6,CIRCLE_BRUSH);
	float strength = (float)luaL_optnumber(l, 7, 1.0f);
	if (tool < 0 || tool >= TOOL_PROP)
			return luaL_error(l, "Invalid tool id '%d'", tool);
	if (brush < 0 || brush >= BRUSH_NUM)
		return luaL_error(l, "Invalid brush id '%d'", brush);

	Brush* tempBrush = new Brush(Point(rx, ry), brush);
	luaSim->CreateToolBrush(x, y, tool, strength, tempBrush);
	delete tempBrush;
	lua_pushinteger(l, 0);
	return 1;
}

int simulation_toolLine(lua_State * l)
{
	int x1 = luaL_optint(l,1,-1);
	int y1 = luaL_optint(l,2,-1);
	int x2 = luaL_optint(l,3,-1);
	int y2 = luaL_optint(l,4,-1);
	int rx = luaL_optint(l,5,5);
	int ry = luaL_optint(l,6,5);
	int tool = luaL_optint(l,7,TOOL_HEAT);
	int brush = luaL_optint(l,8,CIRCLE_BRUSH);
	float strength = (float)luaL_optnumber(l, 9, 1.0f);
	if (tool < 0 || tool >= TOOL_PROP)
			return luaL_error(l, "Invalid tool id '%d'", tool);
	if (brush < 0 || brush >= BRUSH_NUM)
		return luaL_error(l, "Invalid brush id '%d'", brush);

	Brush* tempBrush = new Brush(Point(rx, ry), brush);
	luaSim->CreateToolLine(x1, y1, x2, y2, tool, strength, tempBrush);
	delete tempBrush;
	return 0;
}

int simulation_toolBox(lua_State * l)
{
	int x1 = luaL_optint(l,1,-1)/CELL;
	int y1 = luaL_optint(l,2,-1)/CELL;
	int x2 = luaL_optint(l,3,-1)/CELL;
	int y2 = luaL_optint(l,4,-1)/CELL;
	int tool = luaL_optint(l,5,TOOL_HEAT);
	float strength = (float)luaL_optnumber(l, 6, 1.0f);
	if (tool < 0 || tool >= TOOL_PROP)
			return luaL_error(l, "Invalid tool id '%d'", tool);

	luaSim->CreateToolBox(x1, y1, x2, y2, tool, strength);
	return 0;
}

int simulation_decoBrush(lua_State * l)
{
	int x = luaL_optint(l,1,-1);
	int y = luaL_optint(l,2,-1);
	int rx = luaL_optint(l,3,5);
	int ry = luaL_optint(l,4,5);
	int r = luaL_optint(l,5,255);
	int g = luaL_optint(l,6,255);
	int b = luaL_optint(l,7,255);
	int a = luaL_optint(l,8,255);
	int tool = luaL_optint(l,9,DECO_DRAW);
	int brush = luaL_optint(l,10,CIRCLE_BRUSH);
	if (tool < 0 || tool >= DECOCOUNT)
			return luaL_error(l, "Invalid tool id '%d'", tool);
	if (brush < 0 || brush >= BRUSH_NUM)
		return luaL_error(l, "Invalid brush id '%d'", brush);

	unsigned int color = COLARGB(a, r, g, b);
	Brush* tempBrush = new Brush(Point(rx, ry), brush);
	luaSim->CreateDecoBrush(x, y, tool, color, tempBrush);
	delete tempBrush;
	return 0;
}

int simulation_decoLine(lua_State * l)
{
	int x1 = luaL_optint(l,1,-1);
	int y1 = luaL_optint(l,2,-1);
	int x2 = luaL_optint(l,3,-1);
	int y2 = luaL_optint(l,4,-1);
	int rx = luaL_optint(l,5,5);
	int ry = luaL_optint(l,6,5);
	int r = luaL_optint(l,7,255);
	int g = luaL_optint(l,8,255);
	int b = luaL_optint(l,9,255);
	int a = luaL_optint(l,10,255);
	int tool = luaL_optint(l,11,DECO_DRAW);
	int brush = luaL_optint(l,12,CIRCLE_BRUSH);
	if (tool < 0 || tool >= DECOCOUNT)
			return luaL_error(l, "Invalid tool id '%d'", tool);
	if (brush < 0 || brush >= BRUSH_NUM)
		return luaL_error(l, "Invalid brush id '%d'", brush);

	unsigned int color = COLARGB(a, r, g, b);
	Brush* tempBrush = new Brush(Point(rx, ry), brush);
	luaSim->CreateDecoLine(x1, y1, x2, y2, tool, color, tempBrush);
	delete tempBrush;
	return 0;
}

int simulation_decoBox(lua_State * l)
{
	int x1 = luaL_optint(l,1,-1);
	int y1 = luaL_optint(l,2,-1);
	int x2 = luaL_optint(l,3,5);
	int y2 = luaL_optint(l,4,5);
	int r = luaL_optint(l,5,255);
	int g = luaL_optint(l,6,255);
	int b = luaL_optint(l,7,255);
	int a = luaL_optint(l,8,255);
	int tool = luaL_optint(l,9,DECO_DRAW);
	if (tool < 0 || tool >= DECOCOUNT)
		return luaL_error(l, "Invalid tool id '%d'", tool);

	unsigned int color = COLARGB(a, r, g, b);
	luaSim->CreateDecoBox(x1, y1, x2, y2, tool, color);
	return 0;
}

int simulation_floodDeco(lua_State * l)
{
	int x = luaL_optint(l,1,-1);
	int y = luaL_optint(l,2,-1);
	int r = luaL_optint(l,3,255);
	int g = luaL_optint(l,4,255);
	int b = luaL_optint(l,5,255);
	int a = luaL_optint(l,6,255);

	PropertyValue color;
	color.UInteger = COLARGB(a, r, g, b);
	luaSim->FloodProp(x, y, particle::PropertyByName("dcolour"), color);
	return 0;
}

int simulation_decoColor(lua_State * l)
{
	int acount = lua_gettop(l);
	if (acount == 0)
	{
		lua_pushnumber(l, decocolor);
		return 1;
	}
	else if (acount == 1)
		decocolor = (unsigned int)luaL_optnumber(l, 1, 0xFFFF0000);
	else
	{
		int r, g, b, a;
		r = luaL_optint(l, 1, 255);
		g = luaL_optint(l, 2, 255);
		b = luaL_optint(l, 3, 255);
		a = luaL_optint(l, 4, 255);

		if (r < 0) r = 0; else if (r > 255) r = 255;
		if (g < 0) g = 0; else if (g > 255) g = 255;
		if (b < 0) b = 0; else if (b > 255) b = 255;
		if (a < 0) a = 0; else if (a > 255) a = 255;

		decocolor =  COLARGB(a, r, g, b);
	}
	currR = PIXR(decocolor), currG = PIXG(decocolor), currB = PIXB(decocolor), currA = decocolor>>24;
	RGB_to_HSV(currR, currG, currB, &currH, &currS, &currV);
	return 0;
}

int simulation_clearSim(lua_State * l)
{
	clear_sim();
	return 0;
}

int simulation_clearRect(lua_State * l)
{
	int x = luaL_checkint(l,1);
	int y = luaL_checkint(l,2);
	int w = luaL_checkint(l,3);
	int h = luaL_checkint(l,4);
	luaSim->ClearArea(x, y, w, h);
	return 0;
}

int simulation_resetTemp(lua_State * l)
{
	bool onlyConductors = luaL_optint(l, 1, 0) ? true : false;
	for (int i = 0; i < luaSim->parts_lastActiveIndex; i++)
	{
		if (parts[i].type && (luaSim->elements[parts[i].type].HeatConduct || !onlyConductors))
		{
			parts[i].temp = luaSim->elements[parts[i].type].DefaultProperties.temp;
		}
	}
	return 0;
}

int simulation_resetPressure(lua_State * l)
{
	int aCount = lua_gettop(l), width = XRES/CELL, height = YRES/CELL;
	int x1 = abs(luaL_optint(l, 1, 0));
	int y1 = abs(luaL_optint(l, 2, 0));
	if (aCount > 2)
	{
		width = abs(luaL_optint(l, 3, XRES/CELL));
		height = abs(luaL_optint(l, 4, YRES/CELL));
	}
	else if (aCount)
	{
		width = 1;
		height = 1;
	}
	if(x1 > (XRES/CELL)-1)
		x1 = (XRES/CELL)-1;
	if(y1 > (YRES/CELL)-1)
		y1 = (YRES/CELL)-1;
	if(x1+width > (XRES/CELL)-1)
		width = (XRES/CELL)-x1;
	if(y1+height > (YRES/CELL)-1)
		height = (YRES/CELL)-y1;
	for (int nx = x1; nx<x1+width; nx++)
		for (int ny = y1; ny<y1+height; ny++)
		{
			luaSim->air->pv[ny][nx] = 0.0f;
		}
	return 0;
}

int simulation_saveStamp(lua_State* l)
{
	int x = luaL_optint(l,1,0);
	int y = luaL_optint(l,2,0);
	int w = luaL_optint(l,3,XRES);
	int h = luaL_optint(l,4,YRES);
	int includePressure = luaL_optint(l,5,1);
	char *name = stamp_save(x, y, w, h, includePressure);
	lua_pushstring(l, name);
	return 1;
}

int simulation_loadStamp(lua_State* l)
{
	Save *save = NULL;
	int x = luaL_optint(l, 2, 0);
	int y = luaL_optint(l, 3, 0);
	int includePressure = luaL_optint(l, 4, 1);

	// Load from 10 char name, or full filename
	if (lua_isstring(l, 1))
	{
		const char* filename = luaL_optstring(l, 1, "");
		for (int i = 0; i < stamp_count; i++)
			if (!strcmp(stamps[i].name, filename))
			{
				save = stamp_load(i, 0);
				break;
			}
		if (!save)
		{
			int size;
			char *load_data = (char*)file_load(filename, &size);
			if (load_data)
				save = new Save(load_data, size);
			free(load_data);
		}
	}
	if (!save && lua_isnumber(l, 1))
	{
		int i = luaL_optint(l, 1, 0);
		if (i < 0 || i >= stamp_count)
			return luaL_error(l, "Invalid stamp ID: %d", i);
		save = stamp_load(i, 0);
	}
	if (!save)
	{
		lua_pushnil(l);
		return 1;
	}

	int oldPause = sys_pause;
	try
	{
		luaSim->LoadSave(x, y, save, 0, includePressure);
		if (save->authors.size())
		{
			save->authors["type"] = "luastamp";
			MergeStampAuthorInfo(save->authors);
		}
		lua_pushinteger(l, 1);
	}
	catch (ParseException & e)
	{
		lua_pushnil(l);
	}
	delete save;

	// tpt++ doesn't change pause state with this function, so we won't here either
	sys_pause = oldPause;
	return 1;
}

int simulation_deleteStamp(lua_State* l)
{
	int stampNum = -1;

	if (lua_isstring(l, 1))
	{
		const char* filename = luaL_optstring(l, 1, "");
		for (int i = 0; i < stamp_count; i++)
			if (!strcmp(stamps[i].name, filename))
			{
				stampNum = i;
				break;
			}
	}
	if (stampNum < 0)
	{
		luaL_checkint(l, 1);
		stampNum = luaL_optint(l, 1, -1);
		if (stampNum < 0 || stampNum >= stamp_count)
			return luaL_error(l, "Invalid stamp ID: %d", stampNum);
	}

	if (stampNum < 0)
	{
		lua_pushnumber(l, -1);
		return 1;
	}
	else
	{
		del_stamp(stampNum);
		return 0;
	}
}

int simulation_loadSave(lua_State * l)
{
	int saveID = luaL_optint(l,1,1);
	int instant = luaL_optint(l,2,0);
	int history = luaL_optint(l,3,0); //Exact second a previous save was saved
	char save_id[24], save_date[24];
	if (saveID < 0)
		return luaL_error(l, "Invalid save ID");
	sprintf(save_id, "%i", saveID);
	sprintf(save_date, "%i", history);
	
	if (open_ui(lua_vid_buf, save_id, save_date, instant))
	{
		if (console_mode)
			Engine::Ref().CloseTop();
	}
	return 0;
}

int simulation_reloadSave(lua_State * l)
{
	the_game->ReloadSave();
	return 0;
}

int simulation_getSaveID(lua_State *l)
{
	if (svf_open)
	{
		lua_pushinteger(l, atoi(svf_id));
		return 1;
	}
	return 0;
}

int simulation_adjustCoords(lua_State * l)
{
	int x = luaL_optint(l,1,0);
	int y = luaL_optint(l,2,0);
	Point cursor = the_game->AdjustCoordinates(Point(x, y));
	lua_pushinteger(l, cursor.X);
	lua_pushinteger(l, cursor.Y);
	return 2;
}

int simulation_prettyPowders(lua_State * l)
{
	int acount = lua_gettop(l);
	if (acount == 0)
	{
		lua_pushnumber(l, pretty_powder);
		return 1;
	}
	pretty_powder = luaL_optint(l, 1, 0);
	return 0;
}

int simulation_gravityGrid(lua_State * l)
{
	int acount = lua_gettop(l);
	if (acount == 0)
	{
		lua_pushnumber(l, drawgrav_enable);
		return 1;
	}
	drawgrav_enable = luaL_optint(l, 1, 0);
	return 0;
}

int simulation_edgeMode(lua_State * l)
{
	int acount = lua_gettop(l);

	// allow configuring the "temp" edge mode
	bool temp = false;
	if (acount > 1)
	{
		luaL_checktype(l, 2, LUA_TBOOLEAN);
		temp = lua_toboolean(l, 2);
	}

	// get edge mode
	if (acount == 0 || lua_isnil(l, 1))
	{
		if (temp)
			lua_pushnumber(l, luaSim->saveEdgeMode);
		else
			lua_pushnumber(l, luaSim->edgeMode);
		return 1;
	}

	// set edge mode
	int edgeMode = (char)luaL_optint(l, 1, 0);
	if (temp)
		luaSim->saveEdgeMode = edgeMode;
	else
	{
		luaSim->edgeMode = edgeMode;
		luaSim->saveEdgeMode = -1;
	}

	if (luaSim->GetEdgeMode() == 1)
		draw_bframe();
	else
		erase_bframe();

	return 0;
}

int simulation_gravityMode(lua_State * l)
{
	int acount = lua_gettop(l);
	if (acount == 0)
	{
		lua_pushnumber(l, luaSim->gravityMode);
		return 1;
	}
	luaSim->gravityMode = luaL_optint(l, 1, 0);
	return 0;
}

int simulation_airMode(lua_State * l)
{
	int acount = lua_gettop(l);
	if (acount == 0)
	{
		lua_pushnumber(l, airMode);
		return 1;
	}
	airMode = luaL_optint(l, 1, 0);
	return 0;
}

int simulation_waterEqualization(lua_State * l)
{
	int acount = lua_gettop(l);
	if (acount == 0)
	{
		lua_pushnumber(l, water_equal_test);
		return 1;
	}
	water_equal_test = luaL_optint(l, 1, -1);
	return 0;
}

int simulation_ambientAirTemp(lua_State * l)
{
	int acount = lua_gettop(l);
	if (acount == 0)
	{
		lua_pushnumber(l, luaSim->air->outside_temp);
		return 1;
	}
	luaSim->air->outside_temp = (float)luaL_optnumber(l, 1, 295.15f);
	return 0;
}

int simulation_elementCount(lua_State* l)
{
	int element = luaL_checkint(l, 1);
	if (element < 0 || element >= PT_NUM)
		return luaL_error(l, "Invalid element ID (%d)", element);

	lua_pushnumber(l, luaSim->elementCount[element]);
	return 1;
}

int simulation_canMove(lua_State * l)
{
	int movingElement = luaL_checkint(l, 1);
	int destinationElement = luaL_checkint(l, 2);
	if (movingElement < 0 || movingElement >= PT_NUM)
		return luaL_error(l, "Invalid element ID (%d)", movingElement);
	if (destinationElement < 0 || destinationElement >= PT_NUM)
		return luaL_error(l, "Invalid element ID (%d)", destinationElement);
	
	if (lua_gettop(l) < 3)
	{
		lua_pushnumber(l, luaSim->can_move[movingElement][destinationElement]);
		return 1;
	}
	else
	{
		luaSim->can_move[movingElement][destinationElement] = (unsigned char)luaL_checkint(l, 3);
		return 0;
	}
}

int PartsClosure(lua_State * l)
{
	for (int i = lua_tointeger(l, lua_upvalueindex(1)); i <= luaSim->parts_lastActiveIndex; ++i)
	{
		if (luaSim->parts[i].type)
		{
			lua_pushnumber(l, i + 1);
			lua_replace(l, lua_upvalueindex(1));
			lua_pushnumber(l, i);
			return 1;
		}
	}
	return 0;
}

int simulation_parts(lua_State * l)
{
	lua_pushnumber(l, 0); // first value PartsClosure will see = particle 0
	lua_pushcclosure(l, PartsClosure, 1);
	return 1;
}

int BrushClosure(lua_State * l)
{
	// see Simulation::ToolBrush
	int positionX = lua_tointeger(l, lua_upvalueindex(1));
	int positionY = lua_tointeger(l, lua_upvalueindex(2));
	int radiusX = lua_tointeger(l, lua_upvalueindex(3));
	int radiusY = lua_tointeger(l, lua_upvalueindex(4));
	int x = lua_tointeger(l, lua_upvalueindex(5));
	int y = lua_tointeger(l, lua_upvalueindex(6));
	bool *bitmap = (bool *)lua_touserdata(l, lua_upvalueindex(7));


	int yield_x, yield_y;
	while (true)
	{
		if (!(y < radiusY+radiusY+1))
			return 0;
		if (x < radiusX+radiusX+1)
		{
			bool yield_coords = false;
			std::cout << y << "," << x<< "\n";
			if (bitmap[(y*(radiusX+radiusX+1))+x] && (positionX+(x-radiusX) >= 0 && positionY+(y-radiusY) >= 0 && positionX+(x-radiusX) < XRES && positionY+(y-radiusY) < YRES))
			{
				yield_coords = true;
				yield_x = positionX+(x-radiusX);
				yield_y = positionY+(y-radiusY);
			}
			x++;
			if (yield_coords)
				break;
		}
		else
		{
			x = 0;
			y++;
		}
	}

	lua_pushnumber(l, x);
	lua_replace(l, lua_upvalueindex(5));
	lua_pushnumber(l, y);
	lua_replace(l, lua_upvalueindex(6));

	lua_pushnumber(l, yield_x);
	lua_pushnumber(l, yield_y);
	return 2;
}

int simulation_brush(lua_State * l)
{
	int argCount = lua_gettop(l);
	int positionX = luaL_checkint(l, 1);
	int positionY = luaL_checkint(l, 2);
	int brushradiusX, brushradiusY;
	if (argCount >= 4)
	{
		brushradiusX = luaL_checkint(l, 3);
		brushradiusY = luaL_checkint(l, 4);
	}
	else
	{
		Point size = currentBrush->GetRadius();
		brushradiusX = size.X;
		brushradiusY = size.Y;
	}
	int brushID = luaL_optint(l, 5, currentBrush->GetShape());

	if (brushID < 0 || brushID >= BRUSH_NUM)
		return luaL_error(l, "Invalid brush id '%d'", brushID);
	Point tempRadius = currentBrush->GetRadius();
	int tempID = currentBrush->GetShape();
	currentBrush->SetRadius(Point(brushradiusX, brushradiusY));
	currentBrush->SetShape(brushID);

	int radiusX = currentBrush->GetRadius().X, radiusY = currentBrush->GetRadius().Y;
	lua_pushnumber(l, positionX);
	lua_pushnumber(l, positionY);
	lua_pushnumber(l, radiusX);
	lua_pushnumber(l, radiusY);
	lua_pushnumber(l, 0);
	lua_pushnumber(l, 0);
	size_t bitmapSize = (radiusX+radiusX+1) * (radiusY+radiusY+1) * sizeof(unsigned char);
	void *bitmapCopy = lua_newuserdata(l, bitmapSize);
	memcpy(bitmapCopy, currentBrush->GetBitmap(), bitmapSize);
	lua_pushcclosure(l, BrushClosure, 7);

	currentBrush->SetRadius(tempRadius);
	currentBrush->SetShape(tempID);
	return 1;
}

int simulation_pmap(lua_State * l)
{
	int x = luaL_checkint(l, 1);
	int y = luaL_checkint(l, 2);
	if (x < 0 || x >= XRES || y < 0 || y >= YRES)
		return luaL_error(l, "coordinates out of range (%d,%d)", x, y);
	int r = pmap[y][x];
	if (!r)
		return 0;
	lua_pushnumber(l, ID(r));
	return 1;
}

int simulation_photons(lua_State * l)
{
	int x = luaL_checkint(l, 1);
	int y = luaL_checkint(l, 2);
	if (x < 0 || x >= XRES || y < 0 || y >= YRES)
		return luaL_error(l, "coordinates out of range (%d,%d)", x, y);
	int r = photons[y][x];
	if (!r)
		return 0;
	lua_pushnumber(l, ID(r));
	return 1;
}

int NeighboursClosure(lua_State * l)
{
	int rx = lua_tointeger(l, lua_upvalueindex(1));
	int ry = lua_tointeger(l, lua_upvalueindex(2));
	int sx = lua_tointeger(l, lua_upvalueindex(3));
	int sy = lua_tointeger(l, lua_upvalueindex(4));
	int x = lua_tointeger(l, lua_upvalueindex(5));
	int y = lua_tointeger(l, lua_upvalueindex(6));
	int i = 0;
	do
	{
		x++;
		if (x > rx)
		{
			x = -rx;
			y++;
			if (y > ry)
				return 0;
		}
		if (!(x || y) || sx+x<0 || sy+y<0 || sx+x>=XRES*CELL || sy+y>=YRES*CELL)
		{
			continue;
		}
		i = pmap[y+sy][x+sx];
		if (!i)
			i = photons[y+sy][x+sx];
	}
	while (!i);
	lua_pushnumber(l, x);
	lua_replace(l, lua_upvalueindex(5));
	lua_pushnumber(l, y);
	lua_replace(l, lua_upvalueindex(6));
	lua_pushnumber(l, ID(i));
	lua_pushnumber(l, x+sx);
	lua_pushnumber(l, y+sy);
	return 3;
}

int simulation_neighbours(lua_State * l)
{
	int x=luaL_checkint(l, 1);
	int y=luaL_checkint(l, 2);
	int rx=luaL_optint(l, 3, 2);
	int ry=luaL_optint(l, 4, 2);
	lua_pushnumber(l, rx);
	lua_pushnumber(l, ry);
	lua_pushnumber(l, x);
	lua_pushnumber(l, y);
	lua_pushnumber(l, -rx-1);
	lua_pushnumber(l, -ry);
	lua_pushcclosure(l, NeighboursClosure, 6);
	return 1;
}

int simulation_framerender(lua_State * l)
{
	if (lua_gettop(l) == 0)
	{
		lua_pushinteger(l, framerender);
		return 1;
	}
	int frames = luaL_checkinteger(l, 1);
	if (frames < 0)
		return luaL_error(l, "Can't simulate a negative number of frames");
	framerender = frames;
	return 0;
}

int simulation_gspeed(lua_State * l)
{
	if (lua_gettop(l) == 0)
	{
		lua_pushinteger(l, ((LIFE_ElementDataContainer*)luaSim->elementData[PT_LIFE])->golSpeed);
		return 1;
	}
	int gspeed = luaL_checkinteger(l, 1);
	if (gspeed < 1)
		return luaL_error(l, "GSPEED must be at least 1");
	((LIFE_ElementDataContainer*)luaSim->elementData[PT_LIFE])->golSpeed = gspeed;
	return 0;
}

int simulation_takeSnapshot(lua_State * l)
{
	Snapshot::TakeSnapshot(luaSim);
	return 0;
}

//function added only for tptmp really
int simulation_stickman(lua_State *l)
{
	bool set = lua_gettop(l) > 2 && !lua_isnil(l, 3);
	int num = luaL_checkint(l, 1);
	const char* property = luaL_checkstring(l, 2);
	double value = 0, ret = -1;
	int offset = luaL_optint(l, 4, 0);
	if (set)
		value = luaL_checknumber(l, 3);

	if (num < 1 || num > ((FIGH_ElementDataContainer*)luaSim->elementData[PT_FIGH])->MaxFighters()+2)
		return luaL_error(l, "invalid stickmen number %d", num);
	Stickman *stick;
	if (num == 1)
		stick = ((STKM_ElementDataContainer*)luaSim->elementData[PT_STKM])->GetStickman1();
	else if (num == 2)
		stick = ((STKM_ElementDataContainer*)luaSim->elementData[PT_STKM])->GetStickman2();
	else
		stick = ((FIGH_ElementDataContainer*)luaSim->elementData[PT_FIGH])->Get((unsigned char)(num-3));

	if (!strcmp(property, "comm"))
	{
		if (set)
			stick->comm = (char)value;
		else
			ret = stick->comm;
	}
	else if (!strcmp(property, "pcomm"))
	{
		if (set)
			stick->pcomm = (char)value;
		else
			ret = stick->pcomm;
	}
	else if (!strcmp(property, "elem"))
	{
		if (set)
			stick->elem = (int)value;
		else
			ret = stick->elem;
	}
	else if (!strcmp(property, "legs"))
	{
		if (offset >= 0 && offset < 16)
		{
			if (set)
				stick->legs[offset] = (float)value;
			else
				ret = stick->legs[offset];
		}
	}
	else if (!strcmp(property, "accs"))
	{
		if (offset >= 0 && offset < 8)
		{
			if (set)
				stick->accs[offset] = (float)value;
			else
				ret = stick->accs[offset];
		}
	}
	else if (!strcmp(property, "spwn"))
	{
		if (set)
			stick->spwn = value ? 1 : 0;
		else
			ret = stick->spwn;
	}
	else if (!strcmp(property, "frames"))
	{
		if (set)
			stick->frames = (unsigned int)value;
		else
			ret = stick->frames;
	}
	else if (!strcmp(property, "spawnID"))
	{
		if (set)
			stick->spawnID = (int)value;
		else
			ret = stick->spawnID;
	}
	else if (!strcmp(property, "rocketBoots"))
	{
		if (set)
			stick->rocketBoots = value ? 1 : 0;
		else
			ret = stick->rocketBoots;
	}

	if (!set)
	{
		lua_pushnumber(l, ret);
		return 1;
	}
	return 0;
}

/*

RENDERER API

*/

void initRendererAPI(lua_State * l)
{
	//Methods
	struct luaL_Reg rendererAPIMethods [] = {
		{"renderModes", renderer_renderModes},
		{"displayModes", renderer_displayModes},
		{"colorMode", renderer_colorMode},
		{"colourMode", renderer_colorMode},
		{"decorations", renderer_decorations},
		{"grid", renderer_grid},
		{"debugHUD", renderer_debugHUD},
		{"showBrush", renderer_showBrush},
		{"depth3d", renderer_depth3d},
		{"zoomEnabled", renderer_zoomEnabled},
		{"zoomWindow", renderer_zoomWindowInfo},
		{"zoomScope", renderer_zoomScopeInfo},
		{NULL, NULL}
	};
	luaL_register(l, "renderer", rendererAPIMethods);

	//Ren shortcut
	lua_getglobal(l, "renderer");
	lua_setglobal(l, "ren");

	//Static values
	//Particle pixel modes/fire mode/effects
	SETCONST(l, PMODE);
	SETCONST(l, PMODE_NONE);
	SETCONST(l, PMODE_FLAT);
	SETCONST(l, PMODE_BLOB);
	SETCONST(l, PMODE_BLUR);
	SETCONST(l, PMODE_GLOW);
	SETCONST(l, PMODE_SPARK);
	SETCONST(l, PMODE_FLARE);
	SETCONST(l, PMODE_LFLARE);
	SETCONST(l, PMODE_ADD);
	SETCONST(l, PMODE_BLEND);
	SETCONST(l, PSPEC_STICKMAN);
	SETCONST(l, OPTIONS);
	SETCONST(l, NO_DECO);
	SETCONST(l, DECO_FIRE);
	SETCONST(l, FIREMODE);
	SETCONST(l, FIRE_ADD);
	SETCONST(l, FIRE_BLEND);
	SETCONST(l, EFFECT);
	SETCONST(l, EFFECT_GRAVIN);
	SETCONST(l, EFFECT_GRAVOUT);
	SETCONST(l, EFFECT_LINES);
	SETCONST(l, EFFECT_DBGLINES);

	//Display/Render/Colour modes
	SETCONST(l, RENDER_EFFE);
	SETCONST(l, RENDER_FIRE);
	SETCONST(l, RENDER_SPRK);
	SETCONST(l, RENDER_GLOW);
	SETCONST(l, RENDER_BLUR);
	SETCONST(l, RENDER_BLOB);
	SETCONST(l, RENDER_BASC);
	SETCONST(l, RENDER_NONE);
	SETCONST(l, COLOR_HEAT);
	SETCONST(l, COLOR_LIFE);
	SETCONST(l, COLOR_GRAD);
	SETCONST(l, COLOR_BASC);
	SETCONST(l, COLOR_DEFAULT);
	SETCONST(l, DISPLAY_AIRC);
	SETCONST(l, DISPLAY_AIRP);
	SETCONST(l, DISPLAY_AIRV);
	SETCONST(l, DISPLAY_AIRH);
	SETCONST(l, DISPLAY_AIR);
	SETCONST(l, DISPLAY_WARP);
	SETCONST(l, DISPLAY_PERS);
	lua_pushinteger(l, 0);
	lua_setfield(l, -2, "DISPLAY_EFFE");
}

//get/set render modes list
int renderer_renderModes(lua_State * l)
{
	int args = lua_gettop(l);
	if (args)
	{
		int size = 0;
		luaL_checktype(l, 1, LUA_TTABLE);
		size = lua_objlen(l, 1);

		Renderer::Ref().ClearRenderModes();
		for (int i = 1; i <= size; i++)
		{
			lua_rawgeti(l, 1, i);
			Renderer::Ref().AddRenderMode(lua_tointeger(l, -1));
			render_mode = Renderer::Ref().GetRenderModesRaw();
			lua_pop(l, 1);
		}
		return 0;
	}
	else
	{
		int i = 1;
		std::set<unsigned int> renderModes = Renderer::Ref().GetRenderModes();
		lua_newtable(l);
		for (std::set<unsigned int>::iterator it = renderModes.begin(), end = renderModes.end(); it != end; it++)
		{
			lua_pushinteger(l, (*it));
			lua_rawseti(l, -2, i++);
		}
		return 1;
	}
}

int renderer_displayModes(lua_State * l)
{
	int args = lua_gettop(l);
	if (args)
	{
		int size = 0;
		luaL_checktype(l, 1, LUA_TTABLE);
		size = lua_objlen(l, 1);

		Renderer::Ref().ClearDisplayModes();
		for (int i = 1; i <= size; i++)
		{
			lua_rawgeti(l, 1, i);
			Renderer::Ref().AddDisplayMode(lua_tointeger(l, -1));
			display_mode = Renderer::Ref().GetDisplayModesRaw();
			lua_pop(l, 1);
		}
		return 0;
	}
	else
	{
		int i = 1;
		std::set<unsigned int> displayModes = Renderer::Ref().GetDisplayModes();
		lua_newtable(l);
		for (std::set<unsigned int>::iterator it = displayModes.begin(), end = displayModes.end(); it != end; it++)
		{
			lua_pushinteger(l, (*it));
			lua_rawseti(l, -2, i++);
		}
		return 1;
	}
}

int renderer_colorMode(lua_State * l)
{
	int args = lua_gettop(l);
	if(args)
	{
		luaL_checktype(l, 1, LUA_TNUMBER);
		Renderer::Ref().SetColorMode(lua_tointeger(l, 1));
		return 0;
	}
	else
	{
		lua_pushnumber(l, Renderer::Ref().GetColorMode());
		return 1;
	}
}

int renderer_decorations(lua_State * l)
{
	int args = lua_gettop(l);
	if(args)
	{
		decorations_enable = lua_toboolean(l, 1);
		return 0;
	}
	else
	{
		lua_pushboolean(l, decorations_enable);
		return 1;
	}
}

int renderer_grid(lua_State * l)
{
	int acount = lua_gettop(l);
	if (acount == 0)
	{
		lua_pushnumber(l, GRID_MODE);
		return 1;
	}
	GRID_MODE = luaL_optint(l, 1, 0);
	return 0;
}

int renderer_debugHUD(lua_State * l)
{
	int acount = lua_gettop(l);
	if (acount == 0)
	{
		lua_pushnumber(l, DEBUG_MODE);
		return 1;
	}
	DEBUG_MODE = luaL_optint(l, 1, 0);
	SetCurrentHud();
	return 0;
}

int renderer_showBrush(lua_State * l)
{
	int acount = lua_gettop(l);
	if (acount == 0)
	{
		lua_pushnumber(l, the_game->GetBrushEnable());
		return 1;
	}
	int brush = luaL_optint(l, 1, -1);
	the_game->SetBrushEnable(brush);
	return 0;
}

int renderer_depth3d(lua_State * l)
{
	return luaL_error(l, "This feature is no longer supported");
}

int renderer_zoomEnabled(lua_State * l)
{
	if (lua_gettop(l) == 0)
	{
		lua_pushboolean(l, the_game->IsZoomEnabled());
		return 1;
	}
	else
	{
		luaL_checktype(l, -1, LUA_TBOOLEAN);
		the_game->SetZoomEnabled(lua_toboolean(l, -1));
		return 0;
	}
}
int renderer_zoomWindowInfo(lua_State * l)
{
	int zoomScopeSize = the_game->GetZoomScopeSize();
	int zoomFactor = the_game->GetZoomWindowFactor();
	if (lua_gettop(l) == 0)
	{
		Point location = the_game->GetZoomWindowPosition();
		lua_pushnumber(l, location.X);
		lua_pushnumber(l, location.Y);
		lua_pushnumber(l, zoomFactor);
		lua_pushnumber(l, zoomScopeSize * zoomFactor);
		return 4;
	}
	int x = luaL_optint(l, 1, 0);
	int y = luaL_optint(l, 2, 0);
	int f = luaL_optint(l, 3, 0);
	if (f <= 0)
		return luaL_error(l, "Zoom factor must be greater than 0");

	// To prevent crash when zoom window is outside screen
	if (x < 0 || y < 0 || zoomScopeSize * f + x > XRES || zoomScopeSize * f + y > YRES)
		return luaL_error(l, "Zoom window outside of bounds");

	the_game->SetZoomWindowPosition(Point(x, y));
	the_game->SetZoomWindowFactor(f);
	return 0;
}
int renderer_zoomScopeInfo(lua_State * l)
{
	if (lua_gettop(l) == 0)
	{
		Point location = the_game->GetZoomScopePosition();
		lua_pushnumber(l, location.X);
		lua_pushnumber(l, location.Y);
		lua_pushnumber(l, the_game->GetZoomScopeSize());
		return 3;
	}
	int x = luaL_optint(l, 1, 0);
	int y = luaL_optint(l, 2, 0);
	int s = luaL_optint(l, 3, 0);
	if (s <= 0)
		return luaL_error(l, "Zoom scope size must be greater than 0");

	// To prevent crash when zoom or scope window is outside screen
	int windowEdgeRight = the_game->GetZoomWindowFactor() * s + the_game->GetZoomWindowPosition().X;
	int windowEdgeBottom = the_game->GetZoomWindowFactor() * s + the_game->GetZoomWindowPosition().Y;
	if (x < 0 || y < 0 || x + s > XRES || y + s > YRES)
		return luaL_error(l, "Zoom scope outside of bounds");
	if (windowEdgeRight > XRES || windowEdgeBottom > YRES)
		return luaL_error(l, "Zoom window outside of bounds");

	the_game->SetZoomScopePosition(Point(x, y));
	the_game->SetZoomScopeSize(s);
	return 0;
}


/*

FILESYSTEM API

*/

void initFileSystemAPI(lua_State * l)
{
	//Methods
	struct luaL_Reg fileSystemAPIMethods [] = {
		{"list", fileSystem_list},
		{"exists", fileSystem_exists},
		{"isFile", fileSystem_isFile},
		{"isDirectory", fileSystem_isDirectory},
		{"makeDirectory", fileSystem_makeDirectory},
		{"removeDirectory", fileSystem_removeDirectory},
		{"removeFile", fileSystem_removeFile},
		{"move", fileSystem_move},
		{"copy", fileSystem_copy},
		{NULL, NULL}
	};
	luaL_register(l, "fileSystem", fileSystemAPIMethods);

	//elem shortcut
	lua_getglobal(l, "fileSystem");
	lua_setglobal(l, "fs");
}

int fileSystem_list(lua_State * l)
{
	const char * directoryName = luaL_checkstring(l, 1);
	DIR * directory;
	struct dirent * entry;

	int index = 1;
	lua_newtable(l);

	directory = opendir(directoryName);
	if (directory != NULL)
	{
		while ((entry = readdir(directory)))
		{
			if(strncmp(entry->d_name, "..", 3) && strncmp(entry->d_name, ".", 2))
			{
				lua_pushstring(l, entry->d_name);
				lua_rawseti(l, -2, index++);
			}
		}
		closedir(directory);
	}
	else
	{
		lua_pushnil(l);
	}

	return 1;
}

int fileSystem_exists(lua_State * l)
{
	const char * filename = luaL_checkstring(l, 1);

	lua_pushboolean(l, file_exists(filename));
	return 1;
}

int fileSystem_isFile(lua_State * l)
{
	const char * filename = luaL_checkstring(l, 1);

	int isFile = 0;
#ifdef WIN
	struct _stat s;
	if(_stat(filename, &s) == 0)
#else
	struct stat s;
	if(stat(filename, &s) == 0)
#endif
	{
		if(s.st_mode & S_IFREG)
		{
			isFile = 1; //Is file
		}
		else
		{
			isFile = 0; //Is directory or something else
		}
	}
	else
	{
		isFile = 0; //Doesn't exist
	}

	lua_pushboolean(l, isFile);
	return 1;
}

int fileSystem_isDirectory(lua_State * l)
{
	const char * filename = luaL_checkstring(l, 1);

	int isDir = 0;
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
			isDir = 1; //Is directory
		}
		else
		{
			isDir = 0; //Is file or something else
		}
	}
	else
	{
		isDir = 0; //Doesn't exist
	}

	lua_pushboolean(l, isDir);
	return 1;
}

int fileSystem_makeDirectory(lua_State * l)
{
	const char * dirname = luaL_checkstring(l, 1);

	int ret = 0;
#ifdef WIN
	ret = _mkdir(dirname);
#else
	ret = mkdir(dirname, 0755);
#endif
	lua_pushboolean(l, ret == 0);
	return 1;
}

int fileSystem_removeDirectory(lua_State * l)
{
	const char * filename = luaL_checkstring(l, 1);

	int ret = 0;
#ifdef WIN
	ret = _rmdir(filename);
#else
	ret = rmdir(filename);
#endif
	lua_pushboolean(l, ret == 0);
	return 1;
}

int fileSystem_removeFile(lua_State * l)
{
	const char * filename = luaL_checkstring(l, 1);

	int ret = 0;
#ifdef WIN
	ret = _unlink(filename);
#else
	ret = unlink(filename);
#endif
	lua_pushboolean(l, ret == 0);
	return 1;
}

int fileSystem_move(lua_State * l)
{
	const char * filename = luaL_checkstring(l, 1);
	const char * newFilename = luaL_checkstring(l, 2);
	int ret = 0;

	ret = rename(filename, newFilename);

	lua_pushboolean(l, ret == 0);
	return 1;
}

int fileSystem_copy(lua_State * l)
{
	const char * filename = luaL_checkstring(l, 1);
	const char * newFilename = luaL_checkstring(l, 2);
	int ret = 1;

	char buf[BUFSIZ];
	size_t size;

	FILE* source = fopen(filename, "rb");
	if (source)
	{
		FILE* dest = fopen(newFilename, "wb");
		if (dest)
		{
			while ((size = fread(buf, 1, BUFSIZ, source))) {
				fwrite(buf, 1, size, dest);
			}

			fclose(dest);
			ret = 0;
		}
		fclose(source);
	}

	lua_pushboolean(l, ret == 0);
	return 1;
}

/*

GRAPHICS API

*/

void initGraphicsAPI(lua_State * l)
{
	//Methods
	struct luaL_Reg graphicsAPIMethods [] = {
		{"textSize", graphics_textSize},
		{"drawText", graphics_drawText},
		{"drawLine", graphics_drawLine},
		{"drawRect", graphics_drawRect},
		{"fillRect", graphics_fillRect},
		{"drawCircle", graphics_drawCircle},
		{"fillCircle", graphics_fillCircle},
		{"getColors", graphics_getColors},
		{"getHexColor", graphics_getHexColor},
		{"toolTip", graphics_toolTip},
		{NULL, NULL}
	};
	luaL_register(l, "graphics", graphicsAPIMethods);

	//elem shortcut
	lua_getglobal(l, "graphics");
	lua_setglobal(l, "gfx");

	lua_pushinteger(l, XRES+BARSIZE);	lua_setfield(l, -2, "WIDTH");
	lua_pushinteger(l, YRES+MENUSIZE);	lua_setfield(l, -2, "HEIGHT");
}

int graphics_textSize(lua_State * l)
{
    int width, height;
	char* text = (char*)luaL_optstring(l, 1, "");
	textsize(text, &width, &height);

	lua_pushinteger(l, width);
	lua_pushinteger(l, height);
	return 2;
}

int graphics_drawText(lua_State * l)
{
	int x, y, r, g, b, a;
	x = lua_tointeger(l, 1);
	y = lua_tointeger(l, 2);
	const char* text = luaL_optstring(l, 3, "");
	r = luaL_optint(l, 4, 255);
	g = luaL_optint(l, 5, 255);
	b = luaL_optint(l, 6, 255);
	a = luaL_optint(l, 7, 255);
	
	if (r<0) r = 0;
	else if (r>255) r = 255;
	if (g<0) g = 0;
	else if (g>255) g = 255;
	if (b<0) b = 0;
	else if (b>255) b = 255;
	if (a<0) a = 0;
	else if (a>255) a = 255;

	drawtext(lua_vid_buf, x, y, text, r, g, b, a);
	return 0;
}

int graphics_drawLine(lua_State * l)
{
	int x1, y1, x2, y2, r, g, b, a;
	x1 = lua_tointeger(l, 1);
	y1 = lua_tointeger(l, 2);
	x2 = lua_tointeger(l, 3);
	y2 = lua_tointeger(l, 4);
	r = luaL_optint(l, 5, 255);
	g = luaL_optint(l, 6, 255);
	b = luaL_optint(l, 7, 255);
	a = luaL_optint(l, 8, 255);

	if (r<0) r = 0;
	else if (r>255) r = 255;
	if (g<0) g = 0;
	else if (g>255) g = 255;
	if (b<0) b = 0;
	else if (b>255) b = 255;
	if (a<0) a = 0;
	else if (a>255) a = 255;

	blend_line(lua_vid_buf, x1, y1, x2, y2, r, g, b, a);
	return 0;
}

int graphics_drawRect(lua_State * l)
{
	int x, y, w, h, r, g, b, a;
	x = lua_tointeger(l, 1);
	y = lua_tointeger(l, 2);
	w = lua_tointeger(l, 3)-1;
	h = lua_tointeger(l, 4)-1;
	r = luaL_optint(l, 5, 255);
	g = luaL_optint(l, 6, 255);
	b = luaL_optint(l, 7, 255);
	a = luaL_optint(l, 8, 255);

	if (r<0) r = 0;
	else if (r>255) r = 255;
	if (g<0) g = 0;
	else if (g>255) g = 255;
	if (b<0) b = 0;
	else if (b>255) b = 255;
	if (a<0) a = 0;
	else if (a>255) a = 255;

	drawrect(lua_vid_buf, x, y, w, h, r, g, b, a);
	return 0;
}

int graphics_fillRect(lua_State * l)
{
	int x, y, w, h, r, g, b, a;
	x = lua_tointeger(l, 1)-1;
	y = lua_tointeger(l, 2)-1;
	w = lua_tointeger(l, 3)+1;
	h = lua_tointeger(l, 4)+1;
	r = luaL_optint(l, 5, 255);
	g = luaL_optint(l, 6, 255);
	b = luaL_optint(l, 7, 255);
	a = luaL_optint(l, 8, 255);

	if (r<0) r = 0;
	else if (r>255) r = 255;
	if (g<0) g = 0;
	else if (g>255) g = 255;
	if (b<0) b = 0;
	else if (b>255) b = 255;
	if (a<0) a = 0;
	else if (a>255) a = 255;

	fillrect(lua_vid_buf, x, y, w, h, r, g, b, a);
	return 0;
}

int graphics_drawCircle(lua_State * l)
{
	int x, y, w, h, r, g, b, a;
	x = lua_tointeger(l, 1);
	y = lua_tointeger(l, 2);
	w = lua_tointeger(l, 3);
	h = lua_tointeger(l, 4);
	r = luaL_optint(l, 5, 255);
	g = luaL_optint(l, 6, 255);
	b = luaL_optint(l, 7, 255);
	a = luaL_optint(l, 8, 255);

	if (r<0) r = 0;
	else if (r>255) r = 255;
	if (g<0) g = 0;
	else if (g>255) g = 255;
	if (b<0) b = 0;
	else if (b>255) b = 255;
	if (a<0) a = 0;
	else if (a>255) a = 255;

	drawcircle(lua_vid_buf, x, y, w, h, r, g, b, a);
	return 0;
}

int graphics_fillCircle(lua_State * l)
{
	int x, y, w, h, r, g, b, a;
	x = lua_tointeger(l, 1);
	y = lua_tointeger(l, 2);
	w = lua_tointeger(l, 3);
	h = lua_tointeger(l, 4);
	r = luaL_optint(l, 5, 255);
	g = luaL_optint(l, 6, 255);
	b = luaL_optint(l, 7, 255);
	a = luaL_optint(l, 8, 255);

	if (r<0) r = 0;
	else if (r>255) r = 255;
	if (g<0) g = 0;
	else if (g>255) g = 255;
	if (b<0) b = 0;
	else if (b>255) b = 255;
	if (a<0) a = 0;
	else if (a>255) a = 255;

	fillcircle(lua_vid_buf, x, y, w, h, r, g, b, a);
	return 0;
}

int graphics_getColors(lua_State * l)
{
	unsigned int color = lua_tointeger(l, 1);

	int a = color >> 24;
	int r = (color >> 16)&0xFF;
	int g = (color >> 8)&0xFF;
	int b = color&0xFF;

	lua_pushinteger(l, r);
	lua_pushinteger(l, g);
	lua_pushinteger(l, b);
	lua_pushinteger(l, a);
	return 4;
}

int graphics_getHexColor(lua_State * l)
{
	int r = lua_tointeger(l, 1);
	int g = lua_tointeger(l, 2);
	int b = lua_tointeger(l, 3);
	int a = 0;
	if (lua_gettop(l) >= 4)
		a = lua_tointeger(l, 4);
	unsigned int color = (a<<24) + (r<<16) + (g<<8) + b;

	lua_pushinteger(l, color);
	return 1;
}

int graphics_toolTip(lua_State *l)
{
	std::string toolTip = luaL_checklstring(l, 1, NULL);
	int x = luaL_checkinteger(l, 2);
	int y = luaL_checkinteger(l, 3);
	int alpha = luaL_optint(l, 4, 255);
	int ID = luaL_optint(l, 5, LUATIP);

	UpdateToolTip(toolTip, Point(x, y), ID, alpha);
	return 0;
}

/*

ELEMENTS API

*/

void initElementsAPI(lua_State * l)
{
	int i;
	//Methods
	struct luaL_Reg elementsAPIMethods [] = {
		{"allocate", elements_allocate},
		{"element", elements_element},
		{"property", elements_property},
		{"free", elements_free},
		{"loadDefault", elements_loadDefault},
		{NULL, NULL}
	};
	luaL_register(l, "elements", elementsAPIMethods);

	//elem shortcut
	lua_getglobal(l, "elements");
	lua_setglobal(l, "elem");

	//Static values
	//Element types/properties/states
	SETCONST(l, TYPE_PART);
	SETCONST(l, TYPE_LIQUID);
	SETCONST(l, TYPE_SOLID);
	SETCONST(l, TYPE_GAS);
	SETCONST(l, TYPE_ENERGY);
	SETCONST(l, PROP_CONDUCTS);
	SETCONST(l, PROP_BLACK);
	SETCONST(l, PROP_NEUTPENETRATE);
	SETCONST(l, PROP_NEUTABSORB);
	SETCONST(l, PROP_NEUTPASS);
	SETCONST(l, PROP_DEADLY);
	SETCONST(l, PROP_HOT_GLOW);
	SETCONST(l, PROP_LIFE);
	SETCONST(l, PROP_RADIOACTIVE);
	SETCONST(l, PROP_LIFE_DEC);
	SETCONST(l, PROP_LIFE_KILL);
	SETCONST(l, PROP_LIFE_KILL_DEC);
	SETCONST(l, PROP_INDESTRUCTIBLE);
	SETCONST(l, PROP_CLONE);
	SETCONST(l, PROP_BREAKABLECLONE);
	SETCONST(l, PROP_POWERED);
	SETCONST(l, PROP_SPARKSETTLE);
	SETCONST(l, PROP_NOAMBHEAT);
	lua_pushinteger(l, 0); lua_setfield(l, -2, "PROP_DRAWONCTYPE");
	SETCONST(l, PROP_NOCTYPEDRAW);
	SETCONST(l, FLAG_STAGNANT);
	SETCONST(l, FLAG_SKIPMOVE);
	lua_pushinteger(l, 0); lua_setfield(l, -2, "FLAG_WATEREQUAL");
	lua_pushinteger(l, 0); lua_setfield(l, -2, "FLAG_MOVABLE"); //removed this constant, sponge moves again and no reason for other elements to be allowed to
	SETCONST(l, FLAG_PHOTDECO);
	SETCONST(l, FLAG_EXPLODE);
	SETCONST(l, FLAG_DISAPPEAR);
	lua_pushinteger(l, 0);
	lua_setfield(l, -2, "ST_NONE");
	lua_pushinteger(l, 0);
	lua_setfield(l, -2, "ST_SOLID");
	lua_pushinteger(l, 0);
	lua_setfield(l, -2, "ST_LIQUID");
	lua_pushinteger(l, 0);
	lua_setfield(l, -2, "ST_GAS");

	SETCONST(l, SC_WALL);
	SETCONST(l, SC_ELEC);
	SETCONST(l, SC_POWERED);
	SETCONST(l, SC_SENSOR);
	SETCONST(l, SC_FORCE);
	SETCONST(l, SC_EXPLOSIVE);
	SETCONST(l, SC_GAS);
	SETCONST(l, SC_LIQUID);
	SETCONST(l, SC_POWDERS);
	SETCONST(l, SC_SOLIDS);
	SETCONST(l, SC_NUCLEAR);
	SETCONST(l, SC_SPECIAL);
	SETCONST(l, SC_LIFE);
	SETCONST(l, SC_TOOL);
	SETCONST(l, SC_DECO);
	SETCONST(l, SC_FAV);
	SETCONST(l, SC_FAV2);
	SETCONST(l, SC_HUD);
	SETCONST(l, SC_CRACKER);
	SETCONST(l, SC_OTHER);
	SETCONST(l, SC_SEARCH);
	SETCONST(l, SC_TOTAL);

	//Element identifiers
	for(i = 0; i < PT_NUM; i++)
	{
		if(luaSim->elements[i].Enabled)
		{
			lua_pushinteger(l, i);
			lua_setfield(l, -2, luaSim->elements[i].Identifier.c_str());
			std::string realidentifier = "DEFAULT_PT_" + luaSim->elements[i].Name;
			if (i != 0 && i != PT_NBHL && i != PT_NWHL && luaSim->elements[i].Identifier.c_str() != realidentifier)
			{
				lua_pushinteger(l, i);
				lua_setfield(l, -2, realidentifier.c_str());
			}
		}
	}
}

void LuaGetProperty(lua_State* l, StructProperty property, intptr_t propertyAddress)
{
	switch (property.Type)
	{
	case StructProperty::TransitionType:
	case StructProperty::ParticleType:
	case StructProperty::Integer:
		lua_pushnumber(l, *((int*)propertyAddress));
		break;
	case StructProperty::UInteger:
		lua_pushnumber(l, *((unsigned int*)propertyAddress));
		break;
	case StructProperty::Float:
		lua_pushnumber(l, *((float*)propertyAddress));
		break;
	case StructProperty::Char:
		lua_pushnumber(l, *((char*)propertyAddress));
		break;
	case StructProperty::UChar:
		lua_pushnumber(l, *((unsigned char*)propertyAddress));
		break;
	case StructProperty::BString:
	case StructProperty::String:
	{
		lua_pushstring(l, (*((std::string*)propertyAddress)).c_str());
		break;
	}
	case StructProperty::Colour:
#if PIXELSIZE == 4
		lua_pushinteger(l, *((unsigned int*)propertyAddress));
#else
		lua_pushinteger(l, *((unsigned short*)propertyAddress));
#endif
		break;
	case StructProperty::Removed:
		lua_pushnil(l);
	}
}

void LuaSetProperty(lua_State* l, StructProperty property, intptr_t propertyAddress, int stackPos)
{
	switch (property.Type)
	{
	case StructProperty::TransitionType:
	case StructProperty::ParticleType:
	case StructProperty::Integer:
		*((int*)propertyAddress) = luaL_checkinteger(l, stackPos);
		break;
	case StructProperty::UInteger:
		*((unsigned int*)propertyAddress) = luaL_checkinteger(l, stackPos);
		break;
	case StructProperty::Float:
		*((float*)propertyAddress) = luaL_checknumber(l, stackPos);
		break;
	case StructProperty::Char:
		*((char*)propertyAddress) = luaL_checkinteger(l, stackPos);
		break;
	case StructProperty::UChar:
		*((unsigned char*)propertyAddress) = luaL_checkinteger(l, stackPos);
		break;
	case StructProperty::BString:
	case StructProperty::String:
		*((std::string*)((unsigned char*)propertyAddress)) = std::string(luaL_checkstring(l, 3));
		break;
	case StructProperty::Colour:
#if PIXELSIZE == 4
		*((unsigned int*)propertyAddress) = luaL_checkinteger(l, stackPos);
#else
		*((unsigned short*)propertyAddress) = luaL_checkinteger(l, stackPos);
#endif
		break;
	case StructProperty::Removed:
		break;
	}
}

// deprecated
void elements_setProperty(lua_State * l, int id, int format, int offset)
{
	switch(format)
	{
		case 0: //Int
			*((int*)(((unsigned char*)&luaSim->elements[id])+offset)) = luaL_checkinteger(l, 3);
			break;
		case 1: //Float
			*((float*)(((unsigned char*)&luaSim->elements[id])+offset)) = (float)luaL_checknumber(l, 3);
			break;
		case 2: //String
			*((std::string*)(((unsigned char*)&luaSim->elements[id]) + offset)) = std::string(luaL_checkstring(l, 3));
			break;
		case 3: //Unsigned char (HeatConduct)
			*((unsigned char*)(((unsigned char*)&luaSim->elements[id])+offset)) = (unsigned char)luaL_checkinteger(l, 3);
			break;
		case 4: //Color (Color)
		{
#if PIXELSIZE == 4
			unsigned int col = (unsigned int)luaL_checknumber(l, 3);
			*((unsigned int*)(((unsigned char*)&luaSim->elements[id])+offset)) = col;
#else
			*((unsigned short*)(((unsigned char*)&luaSim->elements[id])+offset)) = luaL_checkinteger(l, 3);
#endif
			break;
		}
		case 5: //Unsigned int (Properties, PhotonReflectWavelength, Latent)
			*((unsigned int*)(((unsigned char*)&luaSim->elements[id])+offset)) = luaL_checkinteger(l, 3);
			break;
		case 6: // old state (removed)
		default:
			break;
	}
}

// deprecated
void elements_writeProperty(lua_State *l, int id, int format, int offset)
{
	switch(format)
	{
		case 0: //Int
			lua_pushinteger(l, *((int*)(((unsigned char*)&luaSim->elements[id])+offset)));
			break;
		case 1: //Float
			lua_pushnumber(l, *((float*)(((unsigned char*)&luaSim->elements[id])+offset)));
			break;
		case 2: //String
			lua_pushstring(l, (*((std::string*)(((unsigned char*)&luaSim->elements[id])+offset))).c_str());
			break;
		case 3: //Unsigned char (HeatConduct)
			lua_pushinteger(l, *((unsigned char*)(((unsigned char*)&luaSim->elements[id])+offset)));
			break;
		case 4: //Color (Color)
#if PIXELSIZE == 4
			lua_pushinteger(l, *((unsigned int*)(((unsigned char*)&luaSim->elements[id])+offset)));
#else
			lua_pushinteger(l, *((unsigned short*)(((unsigned char*)&luaSim->elements[id])+offset)));
#endif
			break;
		case 5: //Unsigned int (Properties, PhotonReflectWavelengths, Latent)
			lua_pushinteger(l, *((unsigned int*)(((unsigned char*)&luaSim->elements[id])+offset)));
			break;
		case 6: // old state (removed)
		default:
			lua_pushnil(l);
	}
}

int elements_loadDefault(lua_State * l)
{
	int args = lua_gettop(l);
	if(args)
	{
		int id;
		luaL_checktype(l, 1, LUA_TNUMBER);
		id = lua_tointeger(l, 1);
		if(id < 0 || id >= PT_NUM)
			return luaL_error(l, "Invalid element");

		lua_getglobal(l, "elements");
		lua_pushnil(l);
		lua_setfield(l, -2, luaSim->elements[id].Identifier.c_str());

		if(id < PT_NUM)
		{
			if (luaSim->elements[id].Init)
				luaSim->elements[id].Init(luaSim, &luaSim->elements[id], id);
			else
				luaSim->elements[id] = Element();
		}

		lua_pushinteger(l, id);
		lua_setfield(l, -2, luaSim->elements[id].Identifier.c_str());
		lua_pop(l, 1);
	}
	else
	{
		for (int i = 0; i < PT_NUM; i++)
			if (luaSim->elements[i].Init)
				luaSim->elements[i].Init(luaSim, &luaSim->elements[i], i);
		lua_pushnil(l);
		lua_setglobal(l, "elements");
		lua_pushnil(l);
		lua_setglobal(l, "elem");

		lua_getglobal(l, "package");
		lua_getfield(l, -1, "loaded");
		lua_pushnil(l);
		lua_setfield(l, -2, "elements");

		initElementsAPI(l);
	}

	FillMenus();
	luaSim->InitCanMove();
	memset(graphicscache, 0, sizeof(gcache_item)*PT_NUM);
	return 0;
}

int elements_allocate(lua_State * l)
{
	luaL_checktype(l, 1, LUA_TSTRING);
	luaL_checktype(l, 2, LUA_TSTRING);
	std::string group = std::string(lua_tostring(l, 1));
	std::string id = std::string(lua_tostring(l, 2));
	
	std::transform(group.begin(), group.end(), group.begin(), ::toupper);
	std::transform(id.begin(), id.end(), id.begin(), ::toupper);

	if (id.find('_') != id.npos)
		return luaL_error(l, "The element name may not contain '_'.");
	if (group.find('_') != id.npos)
		return luaL_error(l, "The group name may not contain '_'.");
	if (group == "DEFAULT")
		return luaL_error(l, "You cannot create elements in the 'DEFAULT' group.");

	std::stringstream identifierStream;
	identifierStream << group << "_PT_" << id;
	std::string identifier = identifierStream.str();

	for (int i = 0; i < PT_NUM; i++)
	{
		if (luaSim->elements[i].Enabled &&luaSim->elements[i].Identifier == identifier)
			return luaL_error(l, "Element identifier already in use");
	}

	int newID = -1;
	// Start out at 255 so that lua element IDs are still one byte (better save compatibility)
	for (int i = PT_NUM >= 255 ? 255 : PT_NUM; i >= 0; i--)
	{
		if (!luaSim->elements[i].Enabled)
		{
			newID = i;
			break;
		}
	}
	// If not enough space, then we start with the new maimum ID
	if (newID == -1)
	{
		for (int i = PT_NUM-1; i >= 255; i--)
		{
			if (!luaSim->elements[i].Enabled)
			{
				newID = i;
				break;
			}
		}
	}

	if (newID != -1)
	{
		luaSim->elements[newID] = Element();
		luaSim->elements[newID].Enabled = true;
		luaSim->elements[newID].Identifier = identifier;
		luaSim->elements[newID].MenuSection = SC_OTHER;
		menuSections[SC_OTHER]->AddTool(new Tool(INVALID_TOOL, identifier, ""));

		lua_getglobal(l, "elements");
		lua_pushinteger(l, newID);
		lua_setfield(l, -2, identifier.c_str());
		lua_pop(l, 1);
	}

	lua_pushinteger(l, newID);
	return 1;
}

int elements_element(lua_State * l)
{
	int id = luaL_checkinteger(l, 1);
	if (!luaSim->IsElementOrNone(id))
		return luaL_error(l, "Invalid element");

	if (lua_gettop(l) > 1)
	{
		luaL_checktype(l, 2, LUA_TTABLE);
		// Write values from native data to a table
		for (auto &prop : Element::GetProperties())
		{
			lua_getfield(l, -1, prop.Name.c_str());
			if (lua_type(l, -1) != LUA_TNIL)
			{
				auto propertyAddress = reinterpret_cast<intptr_t>((reinterpret_cast<unsigned char*>(&luaSim->elements[id])) + prop.Offset);
				LuaSetProperty(l, prop, propertyAddress, -1);
			}
			lua_pop(l, 1);
		}

		lua_getfield(l, -1, "Update");
		if (lua_type(l, -1) == LUA_TFUNCTION)
		{
			lua_el_func[id].Assign(-1);
			lua_el_mode[id] = 1;
		}
		else if (lua_type(l, -1) == LUA_TBOOLEAN && !lua_toboolean(l, -1))
		{
			lua_el_func[id].Clear();
			lua_el_mode[id] = 0;
		}
		lua_pop(l, 1);

		lua_getfield(l, -1, "Graphics");
		if (lua_type(l, -1) == LUA_TFUNCTION)
		{
			lua_gr_func[id].Assign(-1);
		}
		else if (lua_type(l, -1) == LUA_TBOOLEAN && !lua_toboolean(l, -1))
		{
			lua_gr_func[id].Clear();
			luaSim->elements[id].Graphics = nullptr;
		}
		lua_pop(l, 1);

		lua_getfield(l, -1, "CtypeDraw");
		if (lua_type(l, -1) == LUA_TFUNCTION)
		{
			luaCtypeDrawHandlers[id].Assign(-1);
			luaSim->elements[id].CtypeDraw = luaCtypeDrawWrapper;
		}
		else if (lua_type(l, -1) == LUA_TBOOLEAN && !lua_toboolean(l, -1))
		{
			luaCtypeDrawHandlers[id].Clear();
			luaSim->elements[id].CtypeDraw = nullptr;
		}
		lua_pop(l, 1);

		lua_getfield(l, -1, "Create");
		if (lua_type(l, -1) == LUA_TFUNCTION)
		{
			luaCreateHandlers[id].Assign(-1);
			luaSim->elements[id].Func_Create = luaCreateWrapper;
		}
		else if (lua_type(l, -1) == LUA_TBOOLEAN && !lua_toboolean(l, -1))
		{
			luaCreateHandlers[id].Clear();
			luaSim->elements[id].Func_Create = nullptr;
		}
		lua_pop(l, 1);

		lua_getfield(l, -1, "CreateAllowed");
		if (lua_type(l, -1) == LUA_TFUNCTION)
		{
			luaCreateAllowedHandlers[id].Assign(-1);
			luaSim->elements[id].Func_Create_Allowed = luaCreateAllowedWrapper;
		}
		else if (lua_type(l, -1) == LUA_TBOOLEAN && !lua_toboolean(l, -1))
		{
			luaCreateAllowedHandlers[id].Clear();
			luaSim->elements[id].Func_Create_Allowed = nullptr;
		}
		lua_pop(l, 1);

		lua_getfield(l, -1, "ChangeType");
		if (lua_type(l, -1) == LUA_TFUNCTION)
		{
			luaChangeTypeHandlers[id].Assign(-1);
			luaSim->elements[id].Func_ChangeType = luaChangeTypeWrapper;
		}
		else if (lua_type(l, -1) == LUA_TBOOLEAN && !lua_toboolean(l, -1))
		{
			luaChangeTypeHandlers[id].Clear();
			luaSim->elements[id].Func_ChangeType = nullptr;
		}
		lua_pop(l, 1);

		lua_getfield(l, -1, "DefaultProperties");
		if (lua_type(l, -1) == LUA_TTABLE)
		{
			for (auto &prop : particle::GetProperties())
			{
				lua_getfield(l, -1, prop.Name.c_str());
				if (lua_type(l, -1) != LUA_TNIL)
				{
					auto propertyAddress = reinterpret_cast<intptr_t>((reinterpret_cast<unsigned char*>(&luaSim->elements[id].DefaultProperties)) + prop.Offset);
					LuaSetProperty(l, prop, propertyAddress, -1);
				}
				lua_pop(l, 1);
			}
		}
		lua_pop(l, 1);

		FillMenus();
		luaSim->InitCanMove();
		graphicscache[id].isready = 0;

		return 0;
	}
	else
	{
		// Write values from native data to a table
		lua_newtable(l);
		for (auto &prop : Element::GetProperties())
		{
			auto propertyAddress = reinterpret_cast<intptr_t>((reinterpret_cast<unsigned char*>(&luaSim->elements[id])) + prop.Offset);
			LuaGetProperty(l, prop, propertyAddress);
			lua_setfield(l, -2, prop.Name.c_str());
		}

		lua_pushstring(l, luaSim->elements[id].Identifier.c_str());
		lua_setfield(l, -2, "Identifier");

		lua_newtable(l);
		int tableIdx = lua_gettop(l);
		for (auto &prop : particle::GetProperties())
		{
			auto propertyAddress = reinterpret_cast<intptr_t>((reinterpret_cast<unsigned char*>(&luaSim->elements[id].DefaultProperties)) + prop.Offset);
			LuaGetProperty(l, prop, propertyAddress);
			lua_setfield(l, tableIdx, prop.Name.c_str());
		}
		lua_setfield(l, -2, "DefaultProperties");

		return 1;
	}
}

int elements_property(lua_State * l)
{
	int id = luaL_checkinteger(l, 1);
	if (!luaSim->IsElementOrNone(id))
		return luaL_error(l, "Invalid element");

	std::string propertyName = luaL_checkstring(l, 2);

	auto &properties = Element::GetProperties();
	auto prop = std::find_if(properties.begin(), properties.end(), [&propertyName](StructProperty const &p) {
		return p.Name == propertyName;
	});

	if (lua_gettop(l) > 2)
	{
		if (prop != properties.end())
		{
			if (lua_type(l, 3) != LUA_TNIL)
			{
				if (prop->Type == StructProperty::TransitionType)
				{
					int type = luaL_checkinteger(l, 3);
					if (!luaSim->IsElementOrNone(type) && type != NT && type != ST)
					{
						return luaL_error(l, "Invalid element");
					}
				}

				auto propertyAddress = reinterpret_cast<intptr_t>((reinterpret_cast<unsigned char*>(&luaSim->elements[id])) + (*prop).Offset);
				LuaSetProperty(l, *prop, propertyAddress, 3);
			}

			FillMenus();
			luaSim->InitCanMove();
			graphicscache[id].isready = 0;

			return 0;
		}
		else if (propertyName == "Update")
		{
			if (lua_type(l, 3) == LUA_TFUNCTION)
			{
				switch (luaL_optint(l, 4, 0))
				{
				case 2:
					lua_el_mode[id] = 3; //update before
					break;
				case 1:
					lua_el_mode[id] = 2; //replace
					break;
				default:
					lua_el_mode[id] = 1; //update after
					break;
				}

				lua_el_func[id].Assign(3);
			}
			else if (lua_type(l, 3) == LUA_TBOOLEAN && !lua_toboolean(l, 3))
			{
				lua_el_func[id].Clear();
				lua_el_mode[id] = 0;
			}
		}
		else if (propertyName == "Graphics")
		{
			if (lua_type(l, 3) == LUA_TFUNCTION)
			{
				lua_gr_func[id].Assign(3);
				graphicscache[id].isready = 0;
			}
			else if (lua_type(l, 3) == LUA_TBOOLEAN && !lua_toboolean(l, 3))
			{
				lua_gr_func[id].Clear();
				luaSim->elements[id].Graphics = nullptr;
			}
			graphicscache[id].isready = 0;
		}
		else if (propertyName == "CtypeDraw")
		{
			if (lua_type(l, 3) == LUA_TFUNCTION)
			{
				luaCtypeDrawHandlers[id].Assign(3);
				luaSim->elements[id].CtypeDraw = luaCtypeDrawWrapper;
			}
			else if (lua_type(l, 3) == LUA_TBOOLEAN && !lua_toboolean(l, 3))
			{
				luaCtypeDrawHandlers[id].Clear();
				luaSim->elements[id].CtypeDraw = nullptr;
			}
			return 0;
		}
		else if (propertyName == "Create")
		{
			if (lua_type(l, 3) == LUA_TFUNCTION)
			{
				luaCreateHandlers[id].Assign(3);
				luaSim->elements[id].Func_Create = luaCreateWrapper;
			}
			else if (lua_type(l, 3) == LUA_TBOOLEAN && !lua_toboolean(l, 3))
			{
				luaCreateHandlers[id].Clear();
				luaSim->elements[id].Func_Create = nullptr;
			}
			return 0;
		}
		else if (propertyName == "CreateAllowed")
		{
			if (lua_type(l, 3) == LUA_TFUNCTION)
			{
				luaCreateAllowedHandlers[id].Assign(3);
				luaSim->elements[id].Func_Create_Allowed = luaCreateAllowedWrapper;
			}
			else if (lua_type(l, 3) == LUA_TBOOLEAN && !lua_toboolean(l, 3))
			{
				luaCreateAllowedHandlers[id].Clear();
				luaSim->elements[id].Func_Create_Allowed = nullptr;
			}
		}
		else if (propertyName == "ChangeType")
		{
			if (lua_type(l, 3) == LUA_TFUNCTION)
			{
				luaChangeTypeHandlers[id].Assign(3);
				luaSim->elements[id].Func_ChangeType = luaChangeTypeWrapper;
			}
			else if (lua_type(l, 3) == LUA_TBOOLEAN && !lua_toboolean(l, 3))
			{
				luaChangeTypeHandlers[id].Clear();
				luaSim->elements[id].Func_ChangeType = nullptr;
			}
		}
		else if (propertyName == "DefaultProperties")
		{
			luaL_checktype(l, 3, LUA_TTABLE);
			for (auto &prop : particle::GetProperties())
			{
				lua_getfield(l, -1, prop.Name.c_str());
				if (lua_type(l, -1) != LUA_TNIL)
				{
					auto propertyAddress = reinterpret_cast<intptr_t>((reinterpret_cast<unsigned char*>(&luaSim->elements[id].DefaultProperties)) + prop.Offset);
					LuaSetProperty(l, prop, propertyAddress, -1);
				}
				lua_pop(l, 1);
			}
		}
		else
			return luaL_error(l, "Invalid element property");
	}
	else
	{
		if (prop != properties.end())
		{
			auto propertyAddress = reinterpret_cast<intptr_t>((reinterpret_cast<unsigned char*>(&luaSim->elements[id])) + (*prop).Offset);
			LuaGetProperty(l, *prop, propertyAddress);
			return 1;
		}
		else if (propertyName == "Identifier")
		{
			lua_pushstring(l, luaSim->elements[id].Identifier.c_str());
			return 1;
		}
		else if (propertyName == "DefaultProperties")
		{
			lua_newtable(l);
			int tableIdx = lua_gettop(l);
			for (auto &prop : particle::GetProperties())
			{
				auto propertyAddress = reinterpret_cast<intptr_t>((reinterpret_cast<unsigned char*>(&luaSim->elements[id].DefaultProperties)) + prop.Offset);
				LuaGetProperty(l, prop, propertyAddress);
				lua_setfield(l, tableIdx, prop.Name.c_str());
			}
			return 1;
		}
		else
			return luaL_error(l, "Invalid element property");
	}
	return 0;
}

int elements_free(lua_State * l)
{
	int id = luaL_checkinteger(l, 1);
	if (!luaSim->IsElementOrNone(id))
		return luaL_error(l, "Invalid element");

	if (luaSim->elements[id].Identifier.find("DEFAULT_PT_") != luaSim->elements[id].Identifier.npos)
		return luaL_error(l, "Cannot free default elements");

	luaSim->elements[id].Enabled = 0;
	FillMenus();

	lua_getglobal(l, "elements");
	lua_pushnil(l);
	lua_setfield(l, -2, luaSim->elements[id].Identifier.c_str());
	lua_pop(l, 1);

	return 0;
}


void initPlatformAPI(lua_State * l)
{
	struct luaL_Reg platformAPIMethods [] = {
		{"platform", platform_platform},
		{"build", platform_build},
		{"releaseType", platform_releaseType},
		{"exeName", platform_exeName},
		{"restart", platform_restart},
		{"openLink", platform_openLink},
		{"clipboardCopy", platform_clipboardCopy},
		{"clipboardPaste", platform_clipboardPaste},
		{"showOnScreenKeyboard", platform_showOnScreenKeyboard},
		{"getOnScreenKeyboardInput", platform_getOnScreenKeyboardInput},
		{NULL, NULL}
	};
	luaL_register(l, "platform", platformAPIMethods);

	lua_getglobal(l, "platform");
	lua_setglobal(l, "plat");
}

int platform_platform(lua_State * l)
{
	lua_pushstring(l, IDENT_PLATFORM);
	return 1;
}

int platform_build(lua_State * l)
{
	lua_pushstring(l, IDENT_BUILD);
	return 1;
}

int platform_releaseType(lua_State * l)
{
	lua_pushstring(l, IDENT_RELTYPE);
	return 1;
}

int platform_exeName(lua_State * l)
{
	char *name = Platform::ExecutableName();
	if (name)
		lua_pushstring(l, name);
	else
		luaL_error(l, "Error, could not get executable name");
	free(name);
	return 1;
}

int platform_restart(lua_State * l)
{
	int saveTab = luaL_optinteger(l, 1, 0);
	Platform::DoRestart(saveTab ? true : false, false);
	return 0;
}

int platform_openLink(lua_State * l)
{
	const char * uri = luaL_checkstring(l, 1);
	Platform::OpenLink(uri);
	return 0;
}

int platform_clipboardCopy(lua_State * l)
{
	std::string text = Engine::Ref().ClipboardPull();
	lua_pushstring(l, text.c_str());
	return 1;
}

int platform_clipboardPaste(lua_State * l)
{
	luaL_checktype(l, 1, LUA_TSTRING);
	Engine::Ref().ClipboardPush(luaL_optstring(l, 1, ""));
	return 0;
}

int platform_showOnScreenKeyboard(lua_State * l)
{
	const char *startText = luaL_optstring(l, 1, "");
	int acount = lua_gettop(l);
	bool autoCorrect = false;
	if (acount > 1)
	{
		luaL_checktype(l, 2, LUA_TBOOLEAN);
		autoCorrect = lua_toboolean(l, 2);
	}
	Platform::ShowOnScreenKeyboard(startText, autoCorrect);
	return 0;
}

int platform_getOnScreenKeyboardInput(lua_State * l)
{
	int acount = lua_gettop(l);
	if (acount)
		luaL_checktype(l, 1, LUA_TSTRING);
	int limit = luaL_optint(l, 2, 1024);
	if (limit < 0 || limit > 2048)
		luaL_error(l, "Error, string size too long");
	const char *startText = luaL_optstring(l, 1, "");
	char *buff = (char*)calloc(limit+1, sizeof(char));
	strncpy(buff, startText, limit);
	bool autoCorrect = false;
	if (acount > 2)
	{
		luaL_checktype(l, 3, LUA_TBOOLEAN);
		autoCorrect = lua_toboolean(l, 3);
	}
	Platform::GetOnScreenKeyboardInput(buff, limit, autoCorrect);
	lua_pushstring(l, buff);
	free(buff);
	return 1;
}

void initEventAPI(lua_State * l)
{
	struct luaL_Reg eventAPIMethods [] = {
		{"register", event_register},
		{"unregister", event_unregister},
		{"getmodifiers", event_getmodifiers},
		{NULL, NULL}
	};
	luaL_register(l, "event", eventAPIMethods);

	lua_getglobal(l, "event");
	lua_setglobal(l, "evt");

	lua_pushinteger(l, LuaEvents::keypress); lua_setfield(l, -2, "keypress");
	lua_pushinteger(l, LuaEvents::keyrelease); lua_setfield(l, -2, "keyrelease");
	lua_pushinteger(l, LuaEvents::textinput); lua_setfield(l, -2, "textinput");
	lua_pushinteger(l, LuaEvents::mousedown); lua_setfield(l, -2, "mousedown");
	lua_pushinteger(l, LuaEvents::mouseup); lua_setfield(l, -2, "mouseup");
	lua_pushinteger(l, LuaEvents::mousemove); lua_setfield(l, -2, "mousemove");
	lua_pushinteger(l, LuaEvents::mousewheel); lua_setfield(l, -2, "mousewheel");
	lua_pushinteger(l, LuaEvents::tick); lua_setfield(l, -2, "tick");
	lua_pushinteger(l, LuaEvents::blur); lua_setfield(l, -2, "blur");
	lua_pushinteger(l, LuaEvents::close); lua_setfield(l, -2, "close");
}

int event_register(lua_State * l)
{
	int eventName = luaL_checkinteger(l, 1);
	luaL_checktype(l, 2, LUA_TFUNCTION);
	return LuaEvents::RegisterEventHook(l, "tptevents-" + Format::NumberToString<int>(eventName));
}

int event_unregister(lua_State * l)
{
	int eventName = luaL_checkinteger(l, 1);
	luaL_checktype(l, 2, LUA_TFUNCTION);
	return LuaEvents::UnregisterEventHook(l, "tptevents-" + Format::NumberToString<int>(eventName));
}

int event_getmodifiers(lua_State * l)
{
	lua_pushnumber(l, Engine::Ref().GetModifiers());
	return 1;
}

class RequestHandle
{
	Request *request;
	bool dead;

public:
	RequestHandle(std::string &uri, std::map<std::string, std::string> &post_data, std::map<std::string, std::string> &headers)
	{
		dead = false;
		request = new Request(uri);
		for (auto &header : headers)
		{
			request->AddHeader(header.first, header.second);
		}
		request->AddPostData(post_data);
		request->Start();
	}

	~RequestHandle()
	{
		if (!Dead())
		{
			Cancel();
		}
	}

	bool Dead() const
	{
		return dead;
	}

	bool Done() const
	{
		return dead || request->CheckDone();
	}

	void Progress(int *total, int *done)
	{
		if (!dead)
		{
			request->CheckProgress(total, done);
		}
	}

	void Cancel()
	{
		if (!dead)
		{
			request->Cancel();
			dead = true;
		}
	}

	std::string Finish(int *status_out)
	{
		std::string data;
		if (!dead)
		{
			if (request->CheckDone())
			{
				data = request->Finish(status_out);
				dead = true;
			}
		}
		return data;
	}
};

int http_request_gc(lua_State *l)
{
	auto *rh = (RequestHandle *)luaL_checkudata(l, 1, "HTTPRequest");
	rh->~RequestHandle();
	return 0;
}

int http_request_status(lua_State *l)
{
	auto *rh = (RequestHandle *)luaL_checkudata(l, 1, "HTTPRequest");
	if (rh->Dead())
	{
		lua_pushliteral(l, "dead");
	}
	else if (rh->Done())
	{
		lua_pushliteral(l, "done");
	}
	else
	{
		lua_pushliteral(l, "running");
	}
	return 1;
}

int http_request_progress(lua_State *l)
{
	auto *rh = (RequestHandle *)luaL_checkudata(l, 1, "HTTPRequest");
	if (!rh->Dead())
	{
		int total, done;
		rh->Progress(&total, &done);
		lua_pushinteger(l, total);
		lua_pushinteger(l, done);
		return 2;
	}
	return 0;
}

int http_request_cancel(lua_State *l)
{
	auto *rh = (RequestHandle *)luaL_checkudata(l, 1, "HTTPRequest");
	if (!rh->Dead())
	{
		rh->Cancel();
	}
	return 0;
}

int http_request_finish(lua_State *l)
{
	auto *rh = (RequestHandle *)luaL_checkudata(l, 1, "HTTPRequest");
	if (!rh->Dead())
	{
		int status_out;
		std::string data = rh->Finish(&status_out);
		lua_pushlstring(l, data.c_str(), data.size());
		lua_pushinteger(l, status_out);
		return 2;
	}
	return 0;
}

int http_request(lua_State *l, bool isPost)
{
	std::string uri(luaL_checkstring(l, 1));

	std::map<std::string, std::string> post_data;
	if (isPost)
	{
		if (lua_istable(l, 2))
		{
			lua_pushnil(l);
			while (lua_next(l, 2))
			{
				lua_pushvalue(l, -2);
				post_data.emplace(lua_tostring(l, -1), lua_tostring(l, -2));
				lua_pop(l, 2);
			}
		}
	}

	std::map<std::string, std::string> headers;
	if (lua_istable(l, isPost ? 3 : 2))
	{
		lua_pushnil(l);
		while (lua_next(l, isPost ? 3 : 2))
		{
			lua_pushvalue(l, -2);
			headers.emplace(lua_tostring(l, -1), lua_tostring(l, -2));
			lua_pop(l, 2);
		}
	}
	auto *rh = (RequestHandle *)lua_newuserdata(l, sizeof(RequestHandle));
	if (!rh)
	{
		return 0;
	}
	new(rh) RequestHandle(uri, post_data, headers);
	luaL_newmetatable(l, "HTTPRequest");
	lua_setmetatable(l, -2);
	return 1;
}

int http_get(lua_State *l)
{
	return http_request(l, false);
}

int http_post(lua_State *l)
{
	return http_request(l, true);
}

void initHttpAPI(lua_State *l)
{
	luaL_newmetatable(l, "HTTPRequest");
	lua_pushcfunction(l, http_request_gc);
	lua_setfield(l, -2, "__gc");
	lua_newtable(l);
	lua_pushcfunction(l, http_request_status);
	lua_setfield(l, -2, "status");
	lua_pushcfunction(l, http_request_progress);
	lua_setfield(l, -2, "progress");
	lua_pushcfunction(l, http_request_cancel);
	lua_setfield(l, -2, "cancel");
	lua_pushcfunction(l, http_request_finish);
	lua_setfield(l, -2, "finish");
	lua_setfield(l, -2, "__index");
	struct luaL_Reg httpAPIMethods [] = {
		{"get", http_get},
		{"post", http_post},
		{NULL, NULL}
	};
	luaL_register(l, "http", httpAPIMethods);
}

#endif
