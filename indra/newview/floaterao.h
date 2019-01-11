
#ifndef LL_LLFLOATERAO_H
#define LL_LLFLOATERAO_H

#include "llfloater.h"
#include "llviewercontrol.h"
#include "llagent.h"
#include "lleventtimer.h"


enum AOState
{
	STATE_AGENT_IDLE = 0,
	STATE_AGENT_WALK = 1,
	STATE_AGENT_RUN = 2,
	STATE_AGENT_STAND = 3,

	STATE_AGENT_PRE_JUMP = 4,
	STATE_AGENT_JUMP = 5,
	STATE_AGENT_TURNLEFT = 6,
	STATE_AGENT_TURNRIGHT = 7,

	STATE_AGENT_SIT = 8,
	STATE_AGENT_GROUNDSIT = 9,

	STATE_AGENT_HOVER = 10,
	STATE_AGENT_HOVER_DOWN = 11,
	STATE_AGENT_HOVER_UP = 12,

	STATE_AGENT_CROUCH = 13,
	STATE_AGENT_CROUCHWALK = 14,
	STATE_AGENT_FALLDOWN = 15,
	STATE_AGENT_STANDUP = 16,
	STATE_AGENT_LAND = 17,

	STATE_AGENT_FLY = 18,
	STATE_AGENT_FLYSLOW = 19,

	STATE_AGENT_TYPING = 20,

	STATE_AGENT_SWIM_DOWN = 21,
	STATE_AGENT_SWIM_UP = 22,
	STATE_AGENT_SWIM = 23,
	STATE_AGENT_FLOAT = 24,
	STATE_AGENT_END
};




class AOStandTimer : public LLEventTimer
{
public:
    AOStandTimer();
    ~AOStandTimer();
    BOOL tick() override;
	void reset();
};

class AOInvTimer : public LLEventTimer
{
public:
	AOInvTimer();
	~AOInvTimer();
	BOOL tick() override;
};

class LLFloaterAO : public LLFloater, public LLFloaterSingleton<LLFloaterAO>
{
public:

    LLFloaterAO(const LLSD&);
	BOOL postBuild() override;
	void onOpen() override;
    virtual ~LLFloaterAO();

	static void init();

	static void run();
	void updateLayout(bool advanced);

	//static bool loadAnims();

	static void typing(bool start);
	static AOState flyToSwimState(const AOState& state);
	static AOState swimToFlyState(const AOState& state);
	static AOState getAnimationState();
	static void setAnimationState(const AOState& state);

	static LLUUID getCurrentStandId();
	static void setCurrentStandId(const LLUUID& id);
	static int stand_iterator;
	static void ChangeStand();
	static void toggleSwim(bool underwater);

	static void doMotion(const LLUUID& id, bool start, bool stand = false);
	static void startMotion(const LLUUID& id, bool stand = false) { doMotion(id, true, stand); }
	static void stopMotion(const LLUUID& id, bool stand = false) { doMotion(id, false, stand); }

	static bool swimCheck(const AOState& state);
	static LLUUID GetAnimID(const LLUUID& id);

	static AOState GetStateFromAnimID(const LLUUID& id);
	static LLUUID GetAnimIDFromState(const AOState& state);
	static AOState GetStateFromToken(std::string strtoken);

	static void onClickCycleStand(bool next);
	static void onClickReloadCard();
	static void onClickOpenCard();
	static void onClickNewCard();

	static LLUUID invfolderid;
	static const LLUUID& getAssetIDByName(const std::string& name);
	
private:

	static AOState mAnimationState;
	static LLUUID mCurrentStandId;

	static void onSpinnerCommit();
	static void onComboBoxCommit(LLUICtrl* ctrl);

protected:

	static AOState getStateFromCombo(const class LLComboBox* combo);
	static LLComboBox* getComboFromState(const AOState& state);

	static void onNotecardLoadComplete(LLVFS *vfs,const LLUUID& asset_uuid,LLAssetType::EType type,void* user_data, S32 status, LLExtStat ext_status);

};

extern AOInvTimer* gAOInvTimer;

#endif
