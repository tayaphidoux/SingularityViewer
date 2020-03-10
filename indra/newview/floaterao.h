
#ifndef LL_LLFLOATERAO_H
#define LL_LLFLOATERAO_H

#include "llagent.h"
#include "llcontrol.h"
#include "lleventtimer.h"
#include "llfloater.h"


enum AOState : U8
{
	STATE_AGENT_IDLE ,
	STATE_AGENT_WALK,
	STATE_AGENT_RUN,

	STATE_AGENT_PRE_JUMP,
	STATE_AGENT_JUMP,
	STATE_AGENT_TURNLEFT,
	STATE_AGENT_TURNRIGHT,

	STATE_AGENT_SIT,
	STATE_AGENT_SIT_GROUND,

	STATE_AGENT_HOVER,
	STATE_AGENT_HOVER_DOWN,
	STATE_AGENT_HOVER_UP,

	STATE_AGENT_CROUCH,
	STATE_AGENT_CROUCHWALK,
	STATE_AGENT_FALLDOWN,
	STATE_AGENT_STANDUP,
	STATE_AGENT_LAND,

	STATE_AGENT_FLY,
	STATE_AGENT_FLYSLOW,

	STATE_AGENT_TYPE,

	STATE_AGENT_SWIM_DOWN,
	STATE_AGENT_SWIM_UP,
	STATE_AGENT_SWIM,
	STATE_AGENT_FLOAT,
	STATE_AGENT_END
};

class AOStandTimer final : public LLEventTimer
{
	friend class AOSystem;
public:
    AOStandTimer();
    ~AOStandTimer();
    BOOL tick() override;
	void reset();
};

class AOInvTimer final : public LLEventTimer, public LLSingleton<AOInvTimer>
{
	friend class LLSingleton<AOInvTimer>;
	friend class AOSystem;
	AOInvTimer();
	~AOInvTimer();
	static void createIfNeeded();
public:
	static bool needed();
	BOOL tick() override;
};

class LLFloaterAO final : public LLFloater, public LLFloaterSingleton<LLFloaterAO>
{
	friend class AOSystem;
public:

	LLFloaterAO(const LLSD&);
	BOOL postBuild() override;
	void onOpen() override;
	virtual ~LLFloaterAO();
	void updateLayout(bool advanced);

	void onClickCycleStand(bool next) const;
	void onClickReloadCard() const;
	void onClickOpenCard() const;
	void onClickNewCard() const;

private:
	void onSpinnerCommit() const;
	void onComboBoxCommit(LLUICtrl* ctrl) const;
	std::array<class LLComboBox*, STATE_AGENT_END> mCombos;

protected:

	AOState getStateFromCombo(const class LLComboBox* combo) const;
	LLComboBox* getComboFromState(const U8& state) const { return mCombos[state]; }
};

class AOSystem final : public LLSingleton<AOSystem>
{
	friend class LLSingleton<AOSystem>;
	friend class AOInvTimer;
	friend class LLFloaterAO;
	AOSystem();
	~AOSystem();
public:
	static void start(); // Runs the necessary actions to get the AOSystem ready, then initializes it.
	void initSingleton() override;

	static void typing(bool start);

	int stand_iterator;
	bool isStanding() const { return stand().playing; }
	void updateStand();
	int cycleStand(bool next = true, bool random = true);
	void toggleSwim(bool underwater);

	void doMotion(const LLUUID& id, bool start);
	void startMotion(const LLUUID& id) { doMotion(id, true); }
	void stopMotion(const LLUUID& id) { doMotion(id, false); }
	void stopCurrentStand() const { stand().play(false); }
	void stopAllOverrides() const;

protected:
	struct struct_stands
	{
		LLUUID ao_id;
		std::string anim_name;
	};
	typedef std::vector<struct_stands> stands_vec;
	stands_vec mAOStands;

	struct overrides
	{
		virtual bool overrideAnim(bool swimming, const LLUUID& anim) const = 0;
		virtual const LLUUID& getOverride() const { return ao_id; }
		virtual bool play_condition() const; // True if prevented from playing
		virtual bool isLowPriority() const { return false; }
		void play(bool start = true);
		LLUUID ao_id;
		LLPointer<LLControlVariable> setting;
		bool playing;
		virtual ~overrides() {}
	protected:
		overrides(const char* setting_name);
	};
	struct override_low_priority final : public overrides
	{
		override_low_priority(const LLUUID& id, const char* setting_name = nullptr)
			: overrides(setting_name), orig_id(id) {}
		bool overrideAnim(bool swimming, const LLUUID & anim) const override { return orig_id == anim; }
		bool play_condition() const override { return false; }
		bool isLowPriority() const override { return true; }
	private:
		const LLUUID orig_id;
	};
	struct override_single final : public overrides
	{
		override_single(const LLUUID& id, const char* setting_name = nullptr, U8 swim = 2)
			: overrides(setting_name), orig_id(id), swim(swim) {}
		bool overrideAnim(bool swimming, const LLUUID& anim) const override { return (swim == 2 || !!swim == swimming) && orig_id == anim; }
	private:
		const LLUUID orig_id;
		const U8 swim; // 2 = irrelevant, 0 = flying, 1 = swimming
	};
	struct override_stand final : public overrides
	{
		override_stand() : overrides(nullptr) {}
		bool overrideAnim(bool swimming, const LLUUID& anim) const override;
		bool play_condition() const override;
		bool isLowPriority() const override { return true; }
		void update(const stands_vec& stands, const int& iter)
		{
			if (stands.empty()) ao_id.setNull();
			else ao_id = stands[iter].ao_id;
		}
	};
	struct override_sit final : public overrides
	{
		override_sit(const uuid_set_t& ids, const char* setting_name = nullptr)
			: overrides(setting_name)
			, orig_ids(ids)
		{}
		bool overrideAnim(bool swimming, const LLUUID& anim) const override { return orig_ids.find(anim) != orig_ids.end(); }
		bool play_condition() const override;
	private:
		const uuid_set_t orig_ids;
	};
	std::array<overrides*, STATE_AGENT_END> mAOOverrides;

	override_stand& stand() const { return static_cast<override_stand&>(*mAOOverrides[STATE_AGENT_IDLE]); }

private:
	std::array<boost::signals2::scoped_connection, 3> mConnections;

	AOStandTimer mAOStandTimer;

	static void requestConfigNotecard(bool reload = true);
	static void parseNotecard(LLVFS* vfs, const LLUUID& asset_uuid, LLAssetType::EType type, S32 status, bool reload);
};

#endif
