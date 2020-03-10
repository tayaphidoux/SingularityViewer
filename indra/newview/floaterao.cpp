/**
 * @file llfloaterao.cpp
 * @brief clientside animation overrider
 * by Skills Hak & Liru Færs
 */

#include "llviewerprecompiledheaders.h"

#include "floaterao.h"

#include "llagent.h"
#include "llagentcamera.h"
#include "llanimationstates.h"
#include "llcombobox.h"
#include "llfirstuse.h"
#include "llinventory.h"
#include "llinventorybridge.h"
#include "llinventoryfunctions.h"
#include "llinventorypanel.h"
#include "llinventorymodelbackgroundfetch.h"
#include "llmemorystream.h"
#include "llnotecard.h"
#include "llpreviewnotecard.h"
#include "llstartup.h"
#include "lltexteditor.h"
#include "lluictrlfactory.h"
#include "llvoavatarself.h"
#include "llviewerregion.h"
#include "roles_constants.h"

#include <boost/regex.hpp>

// Uncomment and use instead if we ever add the chatbar as a command line - MC
void cmdline_printchat(const std::string& message);

namespace
{
	bool sSwimming = false;
	bool enable_swim()
	{
		static const LLCachedControl<bool> swim(gSavedSettings, "AOSwimEnabled", false);
		return swim;
	}
	bool is_underwater() { return enable_swim() && gAgentAvatarp && gAgentAvatarp->mBelowWater; }
}

class AONotecardCallback final : public LLInventoryCallback
{
public:
	AONotecardCallback(const std::string& filename) : mFileName(filename) {}

	void fire(const LLUUID &inv_item) override
	{
		if (!mFileName.empty())
		{ 
			LLPreviewNotecard* nc = (LLPreviewNotecard*)LLPreview::show(inv_item);
			if (!nc)
			{
				auto item = gInventory.getItem(inv_item);
				open_notecard(item, "Note: " + item->getName(), LLUUID::null, false);
				nc = (LLPreviewNotecard*)LLPreview::find(inv_item);
			}

			if (nc)
			{
				if (LLTextEditor *text = nc->getEditor())
				{
					text->clear();
					text->makePristine();

					std::ifstream file(mFileName.c_str());

					std::string line;
					while (!file.eof())
					{
						getline(file, line);
						text->insertText(line + '\n');
					}
					file.close();

					nc->saveIfNeeded();
				}
			}
		}
	}

private:
	std::string mFileName;
};


// -------------------------------------------------------

AOStandTimer::AOStandTimer() : LLEventTimer(F32_MAX)
{
}
AOStandTimer::~AOStandTimer()
{
//	LL_INFOS() << "dead" << LL_ENDL;
}
void AOStandTimer::reset()
{
	mPeriod = gSavedSettings.getF32("AOStandInterval");
	mEventTimer.reset();
//	LL_INFOS() << "reset" << LL_ENDL;
}
BOOL AOStandTimer::tick()
{
//	LL_INFOS() << "tick" << LL_ENDL;
	if (auto ao = AOSystem::getIfExists())
		ao->cycleStand();
	return false;
}

// -------------------------------------------------------

AOInvTimer::AOInvTimer() : LLEventTimer( 1.0f )
{
}
AOInvTimer::~AOInvTimer()
{
}
bool AOInvTimer::needed()
{
	return LLStartUp::getStartupState() < STATE_INVENTORY_SEND // Haven't done inventory transfer yet
		|| !LLInventoryModelBackgroundFetch::instance().isEverythingFetched(); // Everything hasn't been fetched yet
}
void AOInvTimer::createIfNeeded()
{
	if (needed()) LLSingleton::getInstance();
	else AOSystem::getInstance();
}
BOOL AOInvTimer::tick()
{
	if (!gSavedSettings.getBOOL("AOEnabled")) return true; // If disabled on a tick, we don't care anymore
	if (!needed())
	{
//		cmdline_printchat("Inventory fetched, loading AO.");
		AOSystem::getInstance(); // Initializes everything
		return true;
	}
	return false;
}

class ObjectNameMatches final : public LLInventoryCollectFunctor
{
public:
	ObjectNameMatches(const std::string& name, const LLUUID& folder) : mName(name), mFolder(folder) {}
	virtual ~ObjectNameMatches() {}
	bool operator()(LLInventoryCategory* cat, LLInventoryItem* item) override
	{
		return item && item->getParentUUID() == mFolder && item->getName() == mName;
	}
private:
	const std::string& mName;
	const LLUUID& mFolder;
};

static LLUUID invfolderid;
static LLUUID getAssetIDByName(const std::string& name)
{
	if (name.empty()) return LLUUID::null;

	LLViewerInventoryCategory::cat_array_t cats;
	LLViewerInventoryItem::item_array_t items;
	ObjectNameMatches matches(name, invfolderid);
	gInventory.collectDescendentsIf(LLUUID::null, cats, items, false, matches, true);

	return items.empty() ? LLUUID(name) : items[0]->getAssetUUID();
};

void AOSystem::start()
{
	llassert(!instanceExists() && !AOInvTimer::instanceExists()); // This should only be run once!

	auto control(gSavedSettings.getControl("AOEnabled"));
	if (control->get()) AOInvTimer::createIfNeeded(); // Start the clock

	control->getSignal()->connect([](LLControlVariable*, const LLSD& enabled) {
		if (enabled.asBoolean()) AOInvTimer::createIfNeeded(); // Start the clock
		else deleteSingleton();
	});
}

// STUFF -------------------------------------------------------

LLFloaterAO::LLFloaterAO(const LLSD&) : LLFloater("floater_ao")
, mCombos({})
{
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_ao.xml", nullptr, false);
}

void LLFloaterAO::onOpen()
{
	LLFirstUse::useAO();
}

LLFloaterAO::~LLFloaterAO()
{
//	LL_INFOS() << "floater destroyed" << LL_ENDL;
}

BOOL LLFloaterAO::postBuild()
{
	gSavedSettings.getControl("AOAdvanced")->getSignal()->connect(boost::bind(&LLFloaterAO::updateLayout, this, _2));

	getChild<LLUICtrl>("reloadcard")->setCommitCallback(boost::bind(&LLFloaterAO::onClickReloadCard, this));
	getChild<LLUICtrl>("opencard")->setCommitCallback(boost::bind(&LLFloaterAO::onClickOpenCard, this));
	getChild<LLUICtrl>("newcard")->setCommitCallback(boost::bind(&LLFloaterAO::onClickNewCard, this));
	getChild<LLUICtrl>("prevstand")->setCommitCallback(boost::bind(&LLFloaterAO::onClickCycleStand, this, false));
	getChild<LLUICtrl>("nextstand")->setCommitCallback(boost::bind(&LLFloaterAO::onClickCycleStand, this, true));
	getChild<LLUICtrl>("standtime")->setCommitCallback(boost::bind(&LLFloaterAO::onSpinnerCommit, this));

	auto ao = AOSystem::getIfExists();
	const auto& cb = boost::bind(&LLFloaterAO::onComboBoxCommit, this, _1);
	const auto& setup_combo = [&](const std::string& name, const AOState& state) {
		if (auto combo = findChild<LLComboBox>(name))
		{
			combo->setCommitCallback(cb);
			LLControlVariablePtr setting = nullptr;
			if (auto aop = ao ? ao->mAOOverrides[state] : nullptr)
				setting = aop->setting;
			else // AO is down or missing override struct(why?), try getting from UI
				setting = combo->getControl(combo->getControlName());
			if (setting) combo->add(setting->get().asStringRef(), ADD_BOTTOM, true);
			mCombos[state] = combo;
		}
	};
	setup_combo("stands", STATE_AGENT_IDLE);
	setup_combo("walks", STATE_AGENT_WALK);
	setup_combo("runs", STATE_AGENT_RUN);
	setup_combo("prejumps", STATE_AGENT_PRE_JUMP);
	setup_combo("jumps", STATE_AGENT_JUMP);
	setup_combo("turnlefts", STATE_AGENT_TURNLEFT);
	setup_combo("turnrights", STATE_AGENT_TURNRIGHT);
	setup_combo("sits", STATE_AGENT_SIT);
	setup_combo("gsits", STATE_AGENT_SIT_GROUND);
	setup_combo("hovers", STATE_AGENT_HOVER);
	setup_combo("flydowns", STATE_AGENT_HOVER_DOWN);
	setup_combo("flyups", STATE_AGENT_HOVER_UP);
	setup_combo("crouchs", STATE_AGENT_CROUCH);
	setup_combo("cwalks", STATE_AGENT_CROUCHWALK);
	setup_combo("falls", STATE_AGENT_FALLDOWN);
	setup_combo("standups", STATE_AGENT_STANDUP);
	setup_combo("lands", STATE_AGENT_LAND);
	setup_combo("flys", STATE_AGENT_FLY);
	setup_combo("flyslows", STATE_AGENT_FLYSLOW);
	setup_combo("typings", STATE_AGENT_TYPE);
	setup_combo("swimdowns", STATE_AGENT_SWIM_DOWN);
	setup_combo("swimups", STATE_AGENT_SWIM_UP);
	setup_combo("swims", STATE_AGENT_SWIM);
	setup_combo("floats", STATE_AGENT_FLOAT);

	updateLayout(gSavedSettings.getBOOL("AOAdvanced"));
	AOSystem::requestConfigNotecard(false);

	return true;
}

void LLFloaterAO::updateLayout(bool advanced)
{
	reshape(advanced ? 800 : 200, getRect().getHeight());
	childSetVisible("tabcontainer", advanced);
}

void LLFloaterAO::onSpinnerCommit() const
{
	if (auto ao = AOSystem::getIfExists())
		ao->mAOStandTimer.reset();
}

void LLFloaterAO::onComboBoxCommit(LLUICtrl* ctrl) const
{
	if (LLComboBox* box = (LLComboBox*)ctrl)
	{
		std::string stranim = box->getValue().asString();
		if (auto ao = AOSystem::getIfExists())
		{
			if (box == mCombos[STATE_AGENT_IDLE])
			{
				ao->stand_iterator = box->getCurrentIndex();
				llassert(ao->stand_iterator < ao->mAOStands.size());
				const auto& name = ao->mAOStands[ao->stand_iterator].anim_name;
				llassert(name == box->getValue().asStringRef());
				cmdline_printchat("Changing stand to " + name + '.');
				ao->updateStand();
				ao->mAOStandTimer.reset();
			}
			else
			{
				AOState state = getStateFromCombo(box);
				llassert(state != STATE_AGENT_END);
				const LLUUID& anim = getAssetIDByName(stranim);
				if (auto aop = ao->mAOOverrides[state])
				{
//					LL_INFOS() << "state " << state << " - " << aop->playing() << LL_ENDL;
					bool was_playing = aop->playing;
					if (was_playing) aop->play(false);
					aop->ao_id = anim;
					if (was_playing) aop->play();
				}
			}
		}
		gSavedPerAccountSettings.setString(ctrl->getControlName(), stranim);
	}
}

void LLFloaterAO::onClickCycleStand(bool next) const
{
	auto ao = AOSystem::getIfExists();
	if (!ao) return;
	auto stand = ao->cycleStand(next, false);
	ao->mAOStandTimer.reset();
	if (stand < 0) return;
	cmdline_printchat("Changed stand to " + ao->mAOStands[stand].anim_name + '.');
}

void LLFloaterAO::onClickReloadCard() const
{
	AOSystem::instance().initSingleton();
}

void LLFloaterAO::onClickOpenCard() const
{
	if (!AOInvTimer::needed())
	{
		LLUUID config_nc_id = (LLUUID)gSavedPerAccountSettings.getString("AOConfigNotecardID");
		if (config_nc_id.notNull())
			if (LLViewerInventoryItem* item = gInventory.getItem(config_nc_id))
				if (gAgent.allowOperation(PERM_COPY, item->getPermissions(), GP_OBJECT_MANIPULATE) || gAgent.isGodlike())
					if(!item->getAssetUUID().isNull())
						open_notecard(item, "Note: " + item->getName(), LLUUID::null, false);
	}
}

void LLFloaterAO::onClickNewCard() const
{
	// load the template file from app_settings/ao_template.ini then
	// create a new properly-formatted notecard in the user's inventory
	std::string ao_template = gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "ao_template.ini");
	if (ao_template.empty())
	{
		ao_template = "#Can't find ao_template.ini in app_settings!";
		LL_WARNS() << ao_template << LL_ENDL;
		ao_template += "\n#ZHAO II Style Notecards are supported.";
	}

	create_inventory_item(gAgentID, gAgentSessionID,
						LLUUID::null, LLTransactionID::tnull, "New AO Notecard", 
						"Drop this notecard in your AO window to use", LLAssetType::AT_NOTECARD,
						LLInventoryType::IT_NOTECARD, NOT_WEARABLE, PERM_ALL, new AONotecardCallback(ao_template));
}

AOState LLFloaterAO::getStateFromCombo(const LLComboBox* combo) const
{
	if (combo)
	{
		const auto& end = mCombos.end();
		const auto it = std::find(mCombos.begin(), end, combo);
		if (it != end) return (AOState)std::distance(mCombos.begin(), it);
	}
	return STATE_AGENT_END;
}

AOSystem::overrides::overrides(const char* setting_name)
	: setting(setting_name ? gSavedPerAccountSettings.getControl("AODefault" + std::string(setting_name)) : nullptr)
	, playing(false)
{
}

bool AOSystem::overrides::play_condition() const
{
	// Stop stand first then play
	instance().stopCurrentStand();
	return false;
}

void AOSystem::overrides::play(bool start)
{
	if (playing == start) return;
//	LL_INFOS() << "st" << (start ? "art" : "op") << " override: " << aop->getOverride() << LL_ENDL;
	// We can always stop, but starting is particular
	if (start)
	{
		if (play_condition())
			return;
	}
	else if (playing && !isLowPriority()) // Stands were turned off if this isn't a low priority overrides
	{
		if (!isAgentAvatarValid() && !gAgentAvatarp->isSitting() && !gAgent.getFlying()) // check if sitting or hovering
			instance().stand().play();
	}
	playing = start;
	gAgent.sendAnimationRequest(ao_id, start ? ANIM_REQUEST_START : ANIM_REQUEST_STOP);
}

bool AOSystem::override_stand::play_condition() const
{
	// Do not call base, stands do their own thing
	// stands have lowest priority
	return (!isAgentAvatarValid() || gAgentAvatarp->isSitting())
	&& (gAgentCamera.cameraMouselook() && gSavedSettings.getBOOL("AONoStandsInMouselook"));
}

bool AOSystem::override_sit::play_condition() const
{
	overrides::play_condition();
	return !gSavedSettings.getBOOL("AOSitsEnabled");
}

bool AOSystem::override_stand::overrideAnim(bool swimming, const LLUUID& anim) const
{
	return anim == ANIM_AGENT_STAND;
}

static AOState getSitType()
{
	return gAgentAvatarp->getParent() ? STATE_AGENT_SIT : STATE_AGENT_SIT_GROUND;
}

AOSystem::AOSystem()
: stand_iterator(0)
, mAOOverrides({})
{
// TODO: When we move to C++20 and when GCC and MSVC support it (at the time of writing, neither fully do), use __VA_OPT__ here instead.
#define ANY_OVERRIDE(which, state, ...) mAOOverrides[STATE_AGENT_##state] = new which(ANIM_AGENT_##state, ##__VA_ARGS__)
#define BASIC_OVERRIDE(state, ...) ANY_OVERRIDE(override_single, ##state, ##__VA_ARGS__)
	mAOOverrides[STATE_AGENT_IDLE] = new override_stand();
	BASIC_OVERRIDE(WALK, "Walk");
	BASIC_OVERRIDE(RUN, "Run");
	BASIC_OVERRIDE(PRE_JUMP, "PreJump");
	BASIC_OVERRIDE(JUMP, "Jump");
	BASIC_OVERRIDE(TURNLEFT);
	BASIC_OVERRIDE(TURNRIGHT);

#define SIT_OVERRIDE(control, state, ...) mAOOverrides[STATE_AGENT_##state] = new override_sit(uuid_set_t{ANIM_AGENT_##state, __VA_ARGS__}, control)
	SIT_OVERRIDE("Sit", SIT, ANIM_AGENT_SIT_FEMALE, ANIM_AGENT_SIT_GENERIC);
	SIT_OVERRIDE("GroundSit", SIT_GROUND, ANIM_AGENT_SIT_GROUND_CONSTRAINED);
#undef SIT_OVERRIDE

	BASIC_OVERRIDE(HOVER, "Hover", false);
	BASIC_OVERRIDE(HOVER_DOWN, "FlyDown", false);
	BASIC_OVERRIDE(HOVER_UP, "FlyUp", false);

	BASIC_OVERRIDE(CROUCH, "Crouch");
	BASIC_OVERRIDE(CROUCHWALK, "CrouchWalk");
	BASIC_OVERRIDE(FALLDOWN, "Fall");
	BASIC_OVERRIDE(STANDUP, "StandUp");
	BASIC_OVERRIDE(LAND, "Land");

	BASIC_OVERRIDE(FLY, "Fly", false);
	BASIC_OVERRIDE(FLYSLOW, "FlySlow", false);

	ANY_OVERRIDE(override_low_priority, TYPE, "Typing");

	mAOOverrides[STATE_AGENT_SWIM_DOWN] = new override_single(ANIM_AGENT_HOVER_DOWN, "SwimDown", true);
	mAOOverrides[STATE_AGENT_SWIM_UP] = new override_single(ANIM_AGENT_HOVER_UP, "SwimUp", true);
	mAOOverrides[STATE_AGENT_SWIM] = new override_single(ANIM_AGENT_FLY, "Swim", true);
	mAOOverrides[STATE_AGENT_FLOAT] = new override_single(ANIM_AGENT_HOVER, "Float", true);
#undef BASIC_OVERRIDE
#undef ANY_OVERRIDE

	sSwimming = is_underwater();
	mConnections[0] = gSavedSettings.getControl("AOSitsEnabled")->getSignal()->connect([this](LLControlVariable*, const LLSD& val) {
		if (!isAgentAvatarValid() || !gAgentAvatarp->isSitting()) return;
		gAgent.sendAnimationRequest(mAOOverrides[getSitType()]->ao_id, val.asBoolean() ? ANIM_REQUEST_START : ANIM_REQUEST_STOP);
	});
	mConnections[1] = gSavedSettings.getControl("AOSwimEnabled")->getSignal()->connect(boost::bind(&AOSystem::toggleSwim, this, boost::bind(is_underwater)));
}

AOSystem::~AOSystem()
{
	stopAllOverrides();
	for (auto& ao : mAOOverrides)
	{
		if (ao)
		{
			delete ao;
			ao = nullptr;
		}
	}
}

void AOSystem::requestConfigNotecard(bool reload)
{
	LLUUID configncitem = (LLUUID)gSavedPerAccountSettings.getString("AOConfigNotecardID");
	if (configncitem.notNull())
	{
		bool success = false;
		if (const LLInventoryItem* item = gInventory.getItem(configncitem))
		{
			if (gAgent.allowOperation(PERM_COPY, item->getPermissions(), GP_OBJECT_MANIPULATE) || gAgent.isGodlike())
			{
				if (item->getAssetUUID().notNull())
				{
					invfolderid = item->getParentUUID();
					gAssetStorage->getInvItemAsset(LLHost::invalid,
						gAgentID,
						gAgentSessionID,
						item->getPermissions().getOwner(),
						LLUUID::null,
						item->getUUID(),
						item->getAssetUUID(),
						item->getType(),
						[reload](LLVFS* vfs, const LLUUID& asset_uuid, LLAssetType::EType type, void* user_data, S32 status, LLExtStat ext_status)
						{
							parseNotecard(vfs, asset_uuid, type, status, reload);
						},
						nullptr,
						true);
					success = true;
				}
			}
		}
		if (!success) cmdline_printchat("Could not read the specified Config Notecard");
	}
}

void AOSystem::initSingleton()
{
	mAOStands.clear();
	requestConfigNotecard();
}

void AOSystem::typing(bool start)
{
	uuid_vec_t anims;
	// If we're stopping, stop regardless, just in case the setting was toggled during (e.g.: keyboard shortcut)
	if (!start || gSavedSettings.getBOOL("PlayTypingAnim")) // Linden typing
		anims.push_back(ANIM_AGENT_TYPE);
	if (const auto& ao = getIfExists()) // Typing override
		anims.push_back(ao->mAOOverrides[STATE_AGENT_TYPE]->getOverride());
	if (anims.empty()) return;
	gAgent.sendAnimationRequests(anims, start ? ANIM_REQUEST_START : ANIM_REQUEST_STOP);
}

AOState GetStateFromToken(const std::string& strtoken)
{
	if (strtoken == "[ Sitting On Ground ]") return STATE_AGENT_SIT_GROUND;
	if (strtoken == "[ Sitting ]") return STATE_AGENT_SIT;
	if (strtoken == "[ Crouching ]") return STATE_AGENT_CROUCH;
	if (strtoken == "[ Crouch Walking ]") return STATE_AGENT_CROUCHWALK;
	if (strtoken == "[ Standing Up ]") return STATE_AGENT_STANDUP;
	if (strtoken == "[ Falling ]") return STATE_AGENT_FALLDOWN;
	if (strtoken == "[ Flying Down ]") return STATE_AGENT_HOVER_DOWN;
	if (strtoken == "[ Flying Up ]") return STATE_AGENT_HOVER_UP;
	if (strtoken == "[ Flying Slow ]") return STATE_AGENT_FLYSLOW;
	if (strtoken == "[ Flying ]") return STATE_AGENT_FLY;
	if (strtoken == "[ Hovering ]") return STATE_AGENT_HOVER;
	if (strtoken == "[ Jumping ]") return STATE_AGENT_JUMP;
	if (strtoken == "[ Pre Jumping ]") return STATE_AGENT_PRE_JUMP;
	if (strtoken == "[ Running ]") return STATE_AGENT_RUN;
	if (strtoken == "[ Turning Right ]") return STATE_AGENT_TURNRIGHT;
	if (strtoken == "[ Turning Left ]") return STATE_AGENT_TURNLEFT;
	if (strtoken == "[ Walking ]") return STATE_AGENT_WALK;
	if (strtoken == "[ Landing ]") return STATE_AGENT_LAND;
	if (strtoken == "[ Standing ]") return STATE_AGENT_IDLE;
	if (strtoken == "[ Swimming Down ]") return STATE_AGENT_SWIM_DOWN;
	if (strtoken == "[ Swimming Up ]") return STATE_AGENT_SWIM_UP;
	if (strtoken == "[ Swimming Forward ]") return STATE_AGENT_SWIM;
	if (strtoken == "[ Floating ]") return STATE_AGENT_FLOAT;
	if (strtoken == "[ Typing ]") return STATE_AGENT_TYPE;
	return STATE_AGENT_END;
}

void AOSystem::updateStand()
{
	auto& stand_ao = stand();
	bool is_standing = stand_ao.playing;
	if (is_standing) stand_ao.play(false); // Stop stand first
	stand_ao.update(mAOStands, stand_iterator); // Now update stand
	if (is_standing && !mAOStands.empty()) // Play stand if needed
		stand_ao.play();
}

int AOSystem::cycleStand(bool next, bool random)
{
	if (mAOStands.empty()) return -1;
	const int size = mAOStands.size();
	if (size > 1)
	{
		if (random && gSavedSettings.getBOOL("AOStandRandomize"))
			for (auto previous = stand_iterator; previous == stand_iterator;
				stand_iterator = ll_rand(size));
		else
		{
			stand_iterator += next ? 1 : -1;
			// Wrap around
			if (stand_iterator == size) stand_iterator = 0;
			else if (stand_iterator == -1) stand_iterator = size - 1;
		}
		if (auto floater = LLFloaterAO::findInstance())
			if (auto combo = floater->getComboFromState(STATE_AGENT_IDLE))
				combo->selectNthItem(stand_iterator);
//		LL_INFOS() << "changing stand to " << mAOStands[stand_iterator].anim_name << LL_ENDL;
	}
	updateStand();
	return stand_iterator;
}

void AOSystem::toggleSwim(bool underwater)
{
	sSwimming = underwater && enable_swim();

	if (isStanding()) return; // Don't bother if we're just standing (Who pushed us?!)

	typedef std::array<overrides*, 4> flies_t;
	// Stop all of the previous states
#define AOO(state) mAOOverrides[STATE_AGENT_##state]
	const flies_t swims = { AOO(FLOAT), AOO(SWIM), AOO(SWIM_UP), AOO(SWIM_DOWN) };
	const flies_t flies = { AOO(HOVER), AOO(FLY), AOO(HOVER_UP), AOO(HOVER_DOWN) };
#undef AOO
	const auto& oldaos = sSwimming ? flies : swims;
	const auto& newaos = sSwimming ? swims : flies;

	for (auto i = 0; i < oldaos.size(); ++i)
	{
		auto& old = *oldaos[i];
		if (old.playing)
		{
			old.play(false);
			newaos[i]->play();
		}
	}
}

void AOSystem::doMotion(const LLUUID& id, bool start)
{
	if (id.isNull() || !gAgentAvatarp) return;

	overrides* aop = nullptr;
	AOState state = STATE_AGENT_IDLE;
	for (U8 i = STATE_AGENT_IDLE; !aop && i < STATE_AGENT_END; ++i)
	{
		auto& ao = mAOOverrides[i];
		if (ao && ao->overrideAnim(sSwimming, id))
		{
			aop = ao;
			state = (AOState)i;
		}
	}
	if (aop)
	{
//		LL_INFOS() << "st" << (start ? "art" : "op") << " anim " << id << " state " << state << LL_ENDL;
		aop->play(start);
	}
}

void AOSystem::stopAllOverrides() const
{
	if (!isAgentAvatarValid()) return;
	uuid_vec_t anims;
	for (auto& ao : mAOOverrides)
	{
		if (ao->playing)
		{
			anims.push_back(ao->getOverride());
			ao->playing = false;
		}
	}
	for (const auto& stand : mAOStands)
		anims.push_back(stand.ao_id);
	gAgent.sendAnimationRequests(anims, ANIM_REQUEST_STOP);
}

void AOSystem::parseNotecard(LLVFS *vfs, const LLUUID& asset_uuid, LLAssetType::EType type, S32 status, bool reload)
{
	if (status == LL_ERR_NOERR)
	{
		if (type == LLAssetType::AT_NOTECARD)
		{
			AOSystem* self = getIfExists();
			S32 size = vfs->getSize(asset_uuid, LLAssetType::AT_NOTECARD);
			U8* buffer = new U8[size];
			vfs->getData(asset_uuid, type, buffer, 0, size);
			LLMemoryStream str(buffer, size);
			LLNotecard nc;
			if (nc.importStream(str))
			{
				LL_INFOS() << "ao nc decode success" << LL_ENDL;

				if (self && reload) self->stopAllOverrides();

				auto floater = LLFloaterAO::findInstance();
				if (floater)
				for (auto& combo : floater->mCombos)
				{
					if (combo)
					{
						combo->clear();
						combo->removeall();
					}
				}

				typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
				boost::char_separator<char> sep("\n");
				tokenizer tokline(nc.getText(), sep);

				for (const auto& strline : tokline)
				{
//					LL_INFOS() << "uncommented line: " << strline << LL_ENDL;
					boost::regex type("^(\\s*)(\\[ )(.*)( \\])");
					boost::smatch what; 
					if (boost::regex_search(strline, what, type)) 
					{
						const std::string strtoken(what[0]);
						const AOState state = GetStateFromToken(strtoken);
						if (state == STATE_AGENT_END)
						{
							LL_WARNS() << "Invalid token: " << strtoken << LL_ENDL;
							continue;
						}
//						LL_INFOS() << "type: " << strtoken << LL_ENDL;
						LLComboBox* combo = floater ? floater->getComboFromState(state) : nullptr;
						auto aop = (reload && self) ? self->mAOOverrides[state] : nullptr;
//						LL_INFOS() << "anims in type: " << boost::regex_replace(strline, type, "") << LL_ENDL;

						boost::char_separator<char> sep("|,");
						std::string stranimnames(boost::regex_replace(strline, type, ""));
						tokenizer tokanimnames(stranimnames, sep);
						for (const auto& stranim : tokanimnames)
						{
							if (stranim.empty()) continue;
							const auto& animid(getAssetIDByName(stranim));

//							LL_INFOS() << invfolderid.asString().c_str() << LL_ENDL;
//							LL_INFOS() << "anim: " << stranim.c_str() << " assetid: " << animid << LL_ENDL;
							if (animid.notNull())
							{
								if (aop) // If we're reloading
								{
									if (state == STATE_AGENT_IDLE)
										self->mAOStands.push_back({ animid, stranim });
									else
										aop->ao_id = animid;
								}

								if (combo && !combo->selectByValue(stranim)) // check if exists
									combo->add(stranim, ADD_BOTTOM, true);
							}
							else cmdline_printchat("Warning: animation '" + stranim + "' could not be found (Section: " + strtoken + ").");
						}
					} 
				}
				LL_INFOS() << "ao nc read sucess" << LL_ENDL;

				if (self)
				{
					if (auto combo = floater ? floater->getComboFromState(STATE_AGENT_IDLE) : nullptr)
						combo->selectNthItem(self->stand_iterator);

					for (U8 i = STATE_AGENT_IDLE+1; i < STATE_AGENT_END; ++i)
					{
						auto& aop = self->mAOOverrides[i];
						if (!aop) continue;
						auto& ao = *aop;
						const auto& setting = ao.setting;
						if (!setting && ao.ao_id.isNull()) continue;
						const auto& defaultanim = setting ? setting->get().asStringRef() : ao.ao_id.asString();
						if (defaultanim.empty()) continue;
						const LLUUID& ao_id = getAssetIDByName(defaultanim);
						if (reload && ao_id.notNull()) ao.ao_id = ao_id;
						if (LLComboBox* combo = floater ? floater->getComboFromState(i) : nullptr)
							if (!combo->selectByValue(defaultanim))
								combo->add(defaultanim, ADD_BOTTOM, true);
					}

					if (reload && isAgentAvatarValid())
					{
						// Fix the stand iter, container size may have changed
						auto& iter = self->stand_iterator;
						const auto& stands = self->mAOStands;
						iter = llmin(iter, (int)stands.size()-1);
						// Update the current stand
						self->stand().update(stands, iter);
						self->mAOStandTimer.reset();

						const auto& anims = gAgentAvatarp->mPlayingAnimations;
						bool playing = false;
						for (auto& aop : self->mAOOverrides)
						{
							for (const auto& anim : anims)
							{
								if (aop && aop->overrideAnim(sSwimming, anim.first))
								{
									if (!aop->isLowPriority()) playing = true;
									aop->play();
									break;
								}
							}
						}

						if (!playing) self->stand().play(); // Play stand if nothing was being played, this sometimes happens

						// Toggle typing AO the moment we toggle AO
						typing(gAgent.getRenderState() & AGENT_STATE_TYPING);
					}
				}
			}
			else LL_INFOS() << "ao nc decode error" << LL_ENDL;
			delete[] buffer;
		}
	}
	else LL_INFOS() << "ao nc read error" << LL_ENDL;
}
