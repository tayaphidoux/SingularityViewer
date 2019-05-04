/* @file lffloaterinvpanel.cpp
 * @brief Simple floater displaying an inventory panel with any category as its root
 *
 * Copyright (C) 2013 Liru Færs
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

#include "llviewerprecompiledheaders.h"

#include "lffloaterinvpanel.h"

#include <boost/algorithm/string/erase.hpp>

#include "llinventorypanel.h"
#include "lluictrlfactory.h"


LFFloaterInvPanel::LFFloaterInvPanel(const LLSD& cat, const std::string& name, LLInventoryModel* model)
: LLInstanceTracker<LFFloaterInvPanel, LLSD>(cat)
{
	mCommitCallbackRegistrar.add("InvPanel.Search", boost::bind(&LLInventoryPanel::setFilterSubString, boost::ref(mPanel), _2));
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_inv_panel.xml");
	LLPanel* panel = getChild<LLPanel>("placeholder_panel");
	mPanel = new LLInventoryPanel("inv_panel", LLInventoryPanel::DEFAULT_SORT_ORDER, cat, panel->getRect(), model ? model : &gInventory, true);
	mPanel->postBuild();
	mPanel->setFollows(FOLLOWS_ALL);
	mPanel->setEnabled(true);
	addChild(mPanel);
	removeChild(panel);
	const auto& title = name.empty() ? mPanel->getRootFolder()->getName() : name;
	setTitle(title);

	// Figure out a unique name for our rect control
	const auto rect_control = llformat("FloaterInv%sRect",
		cat.has("name") ? boost::algorithm::erase_all_copy(cat["name"].asStringRef(), " ").data() :
		cat.has("id") ? cat["id"].asStringRef().data() :
		cat["type"].asStringRef().data());

	if (gSavedSettings.controlExists(rect_control)) // Set our initial rect to the stored control
		setRect(gSavedSettings.getRect(rect_control));
	else // Or create the rect control if it doesn't exist
		gSavedSettings.declareRect(rect_control, getRect(), "Rectangle for " + title + " window");

	setRectControl(rect_control);
}

LFFloaterInvPanel::~LFFloaterInvPanel()
{
	delete mPanel;
}

// static
void LFFloaterInvPanel::show(const LLSD& cat, const std::string& name, LLInventoryModel* model)
{
	LFFloaterInvPanel* floater = LFFloaterInvPanel::getInstance(cat);
	if (!floater) floater = new LFFloaterInvPanel(cat, name, model);
	floater->open();
}

// static
void LFFloaterInvPanel::closeAll()
{
	// We must make a copy first, because LLInstanceTracker doesn't allow destruction while having iterators to it.
	std::vector<LFFloaterInvPanel*> cache;
	for (instance_iter i = beginInstances(), end(endInstances()); i != end; ++i)
	{
		cache.push_back(&*i);
	}
	// Now close all, without using instance_iter iterators.
	for (auto& floater : cache)
	{
		floater->close();
	}
}

// virtual
BOOL LFFloaterInvPanel::handleKeyHere(KEY key, MASK mask)
{
	if (!mPanel->hasFocus() && mask == MASK_NONE && (key == KEY_RETURN || key == KEY_DOWN))
	{
		mPanel->setFocus(true);
		if (LLFolderView* root = mPanel->getRootFolder())
			root->scrollToShowSelection();
		return true;
	}

	return LLFloater::handleKeyHere(key, mask);
}
