#pragma once

#include "LuaLuna.h"
#include "LuaComponent.h"

class Checkbox;

class LuaCheckbox: public LuaComponent
{
	Checkbox * checkbox;
	LuaComponentCallback actionFunction;
	void triggerAction();
	int action(lua_State * l);
	int checked(lua_State * l);
	int text(lua_State * l);
public:
	static const char className[];
	static Luna<LuaCheckbox>::RegType methods[];

	LuaCheckbox(lua_State * l);
};
