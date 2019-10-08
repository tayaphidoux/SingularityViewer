/* Copyright (C) 2019 Liru Færs
 *
 * LFIDBearer is a class that holds an ID or IDs that menus can use
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA */

#pragma once

#include "lluuid.h"

class LLMenuGL;
class LLView;

struct LFIDBearer
{
	virtual ~LFIDBearer() { if (sActive == this) sActive = nullptr; }
	virtual LLUUID getStringUUIDSelectedItem() const = 0;
	virtual uuid_vec_t getSelectedIDs() const = 0;
	virtual S32 getNumSelected() const = 0;

	template<typename T> static T* getActive() { return static_cast<T*>(sActive); }
	static LLUUID getActiveSelectedID() { return sActive->getStringUUIDSelectedItem(); }
	static uuid_vec_t getActiveSelectedIDs() { return sActive->getSelectedIDs(); }
	static S32 getActiveNumSelected() { return sActive->getNumSelected(); }

	void showMenu(LLView* self, LLMenuGL* menu, S32 x, S32 y);
	static void addCommonMenu(LLMenuGL* menu) { sMenus.push_back(menu); }

protected:
	static std::vector<LLMenuGL*> sMenus; // Menus that recur, such as general avatars or groups menus
	static LFIDBearer* sActive;
};
