#include "llviewerprecompiledheaders.h"

#include "llviewerwindow.h"
#include "llwindow.h"
#include "llpanelgeneral.h"
#include "llappviewer.h"
#include "llbutton.h"
#include "llviewercontrol.h"
#include "llnotificationsutil.h"
#include "llstartup.h"
#include "llviewerwindow.h"			// to link into child list
#include "llnotify.h"
#include "lluictrlfactory.h"
#include "llhttpclient.h"
#include "llversioninfo.h"
#include "llbufferstream.h"
#include "llweb.h"

#include <nlohmann/json.hpp>

extern AIHTTPTimeoutPolicy getUpdateInfoResponder_timeout;
///////////////////////////////////////////////////////////////////////////////
// GetUpdateInfoResponder
class GetUpdateInfoResponder : public LLHTTPClient::ResponderWithCompleted
{
	LOG_CLASS(GetUpdateInfoResponder);

public:
	GetUpdateInfoResponder(std::string type)
		: mType(type)
	{}
	void onNotifyButtonPress(const LLSD& notification, const LLSD& response, std::string name, std::string url)
	{
		S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
		if (option == 0) // URL
		{
			std::string escaped_url = LLWeb::escapeURL(url);
			if (gViewerWindow)
			{
				gViewerWindow->getWindow()->spawnWebBrowser(escaped_url, true);
			}
		}
		if (option == 1) // Later
		{}
	}
	/*virtual*/ void completedRaw(LLChannelDescriptors const& channels, buffer_ptr_t const& buffer)
	{
		LLBufferStream istr(channels, buffer.get());
		std::stringstream strstrm;
		strstrm << istr.rdbuf();
		const std::string body = strstrm.str();

		if (mStatus != HTTP_OK)
		{
			LL_WARNS() << "Failed to get update info (" << mStatus << ")" << LL_ENDL;
			return;
		}

		auto root = nlohmann::json::parse(body);

		std::string viewer_version = llformat("%s (%i)", LLVersionInfo::getShortVersion().c_str(), LLVersionInfo::getBuild());

		const auto data = root[mType];
#if LL_WINDOWS
		constexpr auto platform = "windows";
#elif LL_LINUX
		constexpr auto platform = "linux";
#elif LL_DARWIN
		constexpr auto platform = "apple";
#endif
		std::string recommended_version = data["recommended"][platform];
		std::string minimum_version = data["minimum"][platform];
		
		S32 minimum_build, recommended_build;
		sscanf(recommended_version.c_str(), "%*i.%*i.%*i (%i)", &recommended_build);
		sscanf(minimum_version.c_str(), "%*i.%*i.%*i (%i)", &minimum_build);

		LL_INFOS() << LLVersionInfo::getBuild() << LL_ENDL;
		LLSD args;
		args["CURRENT_VER"] = viewer_version;
		args["RECOMMENDED_VER"] = recommended_version;
		args["MINIMUM_VER"] = minimum_version;
		args["URL"] = data["url"].get<std::string>();
		args["TYPE"] = mType == "release" ? "Viewer" : "Alpha";

		static LLCachedControl<S32> sLastKnownReleaseBuild("SinguLastKnownReleaseBuild", 0);
		static LLCachedControl<S32> sLastKnownAlphaBuild("SinguLastKnownAlphaBuild", 0);

		LLCachedControl<S32>& lastver = mType == "release" ? sLastKnownReleaseBuild : sLastKnownAlphaBuild;

		if (LLVersionInfo::getBuild() < minimum_build || LLVersionInfo::getBuild() < recommended_build)
		{
			if (lastver.get() < recommended_build)
			{
				lastver = recommended_build;
				LLUI::sIgnoresGroup->setWarning("UrgentUpdateModal", true);
				LLUI::sIgnoresGroup->setWarning("UrgentUpdate", true);
				LLUI::sIgnoresGroup->setWarning("RecommendedUpdate", true);
			}
			std::string notificaiton;
			if (LLVersionInfo::getBuild() < minimum_build)
			{
				if (LLUI::sIgnoresGroup->getWarning("UrgentUpdateModal"))
				{
					notificaiton = "UrgentUpdateModal";
				}
				else
				{
					notificaiton = "UrgentUpdate";
				}
			}
			else if (LLVersionInfo::getBuild() < recommended_build)
			{
				notificaiton = "RecommendedUpdate";
			}
			if (!notificaiton.empty())
			{
				LLNotificationsUtil::add(notificaiton, args, LLSD(), boost::bind(&GetUpdateInfoResponder::onNotifyButtonPress, this, _1, _2, notificaiton, data["url"]));
			}
		}	
	}

protected:
	/*virtual*/ AIHTTPTimeoutPolicy const& getHTTPTimeoutPolicy(void) const { return getUpdateInfoResponder_timeout; }
	/*virtual*/ char const* getName(void) const { return "GetUpdateInfoResponder"; }

private:
	std::string mType;
};

void check_for_updates()
{
	// Hard-code the update url for now.
	std::string url = "http://singularity-viewer.github.io/pages/api/get_update_info.json";//gSavedSettings.getString("SHUpdateCheckURL");
	if (!url.empty())
	{
		std::string type;
		auto& channel = LLVersionInfo::getChannel();
		if (channel == std::string("Singularity"))
		{
			type = "release";
		}
		else if (channel == std::string("Singularity Test") || channel == std::string("Singularity Alpha"))
		{
			type = "alpha";
		}
		else
		{
			return;
		}
		LLHTTPClient::get(url, new GetUpdateInfoResponder(type));
	}
}
