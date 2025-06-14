/*
	Copyright(C) 2023-2024 Kirollos Nashaat

	This program is free software : you can redistribute it and /or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.If not, see < https://www.gnu.org/licenses/>.
*/
#include "stdafx.h"
#include "config.h"
#include "DiscordEuroscopeExt.h"
#include "ConfigManager.h"
#include "MessageFormatter.h"

// Helper function to convert callsign to display name
static std::string GetDisplayName(const std::string& callsign) {
    if (callsign == "CYHZ_GND") return "Halifax Ground";
    if (callsign == "CYHZ_TWR") return "Halifax Tower";
    if (callsign == "CYHZ_APP") return "Halifax Terminal";
    if (callsign == "CZQM_CTR") return "Moncton Center";
    if (callsign == "CYYT_APP") return "St. John's Terminal";
    if (callsign == "CYYT_TWR") return "St. John's Tower";
    if (callsign == "CYYT_GND") return "St. John's Ground";
    // Add more mappings as needed
    return callsign; // Return original if no mapping
}

DiscordEuroscopeExt::DiscordEuroscopeExt() : EuroScopePlugIn::CPlugIn(EuroScopePlugIn::COMPATIBILITY_CODE, "DiscordEuroscope", PLUGIN_VERSION, "Kirollos Nashaat", "GNU GPLv3")
{
	char DllPathFile[_MAX_PATH];
	std::string RCPath;
	GetModuleFileNameA(HINSTANCE(&__ImageBase), DllPathFile, sizeof(DllPathFile));
	RCPath = DllPathFile;
	RCPath = RCPath.substr(0, RCPath.find_last_of('\\') + 1);
	RCPath += CONFIG_FILENAME;
	
	try {
		config.Init(RCPath);
	}
	catch (std::exception& e)
	{
		if (std::string(e.what()).find("Failed to open") != std::string::npos)
		{
			// File cannot be opened, try to generate
			std::string error_msg = "Error: ";
			error_msg += e.what();
			DisplayUserMessage("Message", "DiscordEuroscope", error_msg.c_str(), true, true, false, true, false);
			DisplayUserMessage("Message", "DiscordEuroscope", "Attempting to generate " CONFIG_FILENAME, true, true, false, true, false);
			config.GenerateConfigFile(RCPath);
			try {
				config.Init(RCPath);
			}
			catch (std::exception& e2)
			{
				std::string error_msg = "Error: ";
				error_msg += e2.what();
				error_msg += ".. Halting..";
				DisplayUserMessage("Message", "DiscordEuroscope", error_msg.c_str(), true, true, false, true, false);
				return;
			}
		}
		else
		{
			std::string error_msg = "Error: ";
			error_msg += e.what();
			DisplayUserMessage("Message", "DiscordEuroscope", error_msg.c_str(), true, true, false, true, false);
			return;
		}
	}
	DiscordEventHandlers handlers;
	memset(&handlers, 0, sizeof(handlers));
	// handlers
	Discord_Initialize(static_cast<const char*>(config.Data().discord_appid.c_str()), &handlers, 1, NULL);
	this->EuroInittime = (int)time(NULL);
	config.LoadRadioCallsigns();

	char dmsg[40] = { 0 };
	int count = config.Data().RadioCallsigns.size();
	sprintf_s(dmsg, 40, "Successfully parsed %i callsign%s", count, count == 1 ? "!" : "s!");
	DisplayUserMessage("Message", "DiscordEuroscope", dmsg, true, true, false, true, false);
}


DiscordEuroscopeExt::~DiscordEuroscopeExt()
{
	config.Cleanup();
}

VOID CALLBACK DiscordTimer(_In_ HWND hwnd, _In_ UINT uMsg, _In_ UINT_PTR idEvent, _In_ DWORD dwTime)
{
	using namespace DiscordEuroScope_Configuration;
	if (pMyPlugIn == nullptr)
		return;
	if (pMyPlugIn->EuroInittime == 0)
		return;
	if (!pMyPlugIn->config.isReady())
	{
		Discord_ClearPresence();
		Discord_RunCallbacks();
		return;
	}
	DiscordRichPresence discordPresence;
	memset(&discordPresence, 0, sizeof(discordPresence));
	const ConfigData data = pMyPlugIn->config.Data();
	discordPresence.largeImageKey = data.discord_presence_large_image_key.c_str();
	discordPresence.smallImageKey = data.discord_presence_small_image_key.c_str();
	discordPresence.startTimestamp = pMyPlugIn->EuroInittime;
	
	DiscordButton buttons[2] = { nullptr, nullptr };
	Button configButtons[2];

	switch (pMyPlugIn->GetConnectionType())
	{
		using namespace EuroScopePlugIn;
	case CONNECTION_TYPE_NO:
    	discordPresence.details = "Moncton/Gander FIR";
		discordPresence.largeImageKey = ConfigData::LocalOrGlobal(data.states[State_Idle].presence_large_image_key, data.discord_presence_large_image_key).c_str();
		discordPresence.largeImageText = "Moncton/Gander FIR";
		discordPresence.smallImageKey = ConfigData::LocalOrGlobal(data.states[State_Idle].presence_small_image_key, data.discord_presence_small_image_key).c_str();
		discordPresence.smallImageText = data.states[State_Idle].presence_small_image_text.c_str();
		discordPresence.state = "Chiling";
		configButtons[0] = Button(
			ConfigData::LocalOrGlobal(data.states[State_Idle].buttons[0].label, data.buttons[0].label),
			ConfigData::LocalOrGlobal(data.states[State_Idle].buttons[0].url, data.buttons[0].url)
		);
		configButtons[1] = Button(
			ConfigData::LocalOrGlobal(data.states[State_Idle].buttons[1].label, data.buttons[1].label),
			ConfigData::LocalOrGlobal(data.states[State_Idle].buttons[1].url, data.buttons[1].url)
		);

		if (configButtons[0].IsValid())
		{
			configButtons[0].FillStruct(buttons);
			// Nested condition because there will never be a single button in index 1.
			if (configButtons[1].IsValid())
			{
				configButtons[1].FillStruct(buttons+1);
			}
			else {
				buttons[1] = { "" , "" };
			}
			discordPresence.buttons = buttons;
		}
		Discord_UpdatePresence(&discordPresence);
		Discord_RunCallbacks();
		return;
	case CONNECTION_TYPE_PLAYBACK:
		discordPresence.details = data.states[State_Playback].details.c_str();
		discordPresence.largeImageKey = ConfigData::LocalOrGlobal(data.states[State_Playback].presence_large_image_key, data.discord_presence_large_image_key).c_str();
    	discordPresence.largeImageText = "Moncton/Gander FIR";			
		discordPresence.smallImageKey = ConfigData::LocalOrGlobal(data.states[State_Playback].presence_small_image_key, data.discord_presence_small_image_key).c_str();
		discordPresence.smallImageText = data.states[State_Playback].presence_small_image_text.c_str();
		discordPresence.state = "Looking back...";
		configButtons[0] = Button(
			ConfigData::LocalOrGlobal(data.states[State_Playback].buttons[0].label, data.buttons[0].label),
			ConfigData::LocalOrGlobal(data.states[State_Playback].buttons[0].url, data.buttons[0].url)
		);
		configButtons[1] = Button(
			ConfigData::LocalOrGlobal(data.states[State_Playback].buttons[1].label, data.buttons[1].label),
			ConfigData::LocalOrGlobal(data.states[State_Playback].buttons[1].url, data.buttons[1].url)
		);

		if (configButtons[0].IsValid())
		{
			configButtons[0].FillStruct(buttons);
			// Nested condition because there will never be a single button in index 1.
			if (configButtons[1].IsValid())
			{
				configButtons[1].FillStruct(buttons+1);
			}
			else {
				buttons[1] = { "" , "" };
			}
			discordPresence.buttons = buttons;
		}
		Discord_UpdatePresence(&discordPresence);
		Discord_RunCallbacks();
		return;
	case CONNECTION_TYPE_SWEATBOX:
		if (data.sweatbox_bypass == true)
			break;
		discordPresence.details = "Moncton/Gander FIR";
    	discordPresence.largeImageKey = "czqm_logo";
    	discordPresence.largeImageText = "Moncton/Gander FIR";
		discordPresence.smallImageKey = ConfigData::LocalOrGlobal(data.states[State_Sweatbox].presence_small_image_key, data.discord_presence_small_image_key).c_str();
		discordPresence.smallImageText = data.states[State_Sweatbox].presence_small_image_text.c_str();
		discordPresence.state = "Making Someone Sweat...";
		configButtons[0] = Button(
			ConfigData::LocalOrGlobal(data.states[State_Sweatbox].buttons[0].label, data.buttons[0].label),
			ConfigData::LocalOrGlobal(data.states[State_Sweatbox].buttons[0].url, data.buttons[0].url)
		);
		configButtons[1] = Button(
			ConfigData::LocalOrGlobal(data.states[State_Sweatbox].buttons[1].label, data.buttons[1].label),
			ConfigData::LocalOrGlobal(data.states[State_Sweatbox].buttons[1].url, data.buttons[1].url)
		);

		if (configButtons[0].IsValid())
		{
			configButtons[0].FillStruct(buttons);
			// Nested condition because there will never be a single button in index 1.
			if (configButtons[1].IsValid())
			{
				configButtons[1].FillStruct(buttons+1);
			}
			else {
				buttons[1] = { "" , "" };
			}
			discordPresence.buttons = buttons;
		}
		Discord_UpdatePresence(&discordPresence);
		Discord_RunCallbacks();	
		return;
	case CONNECTION_TYPE_DIRECT:
		break;
	default:
		Discord_ClearPresence();
		Discord_RunCallbacks();
		return;
	}
	EuroScopePlugIn::CController controller = pMyPlugIn->ControllerMyself();
	const char* callsign = controller.GetCallsign();
	double frequency = controller.GetPrimaryFrequency();

	int use_state = -1;

	if (data.states[State_Direct_SUP].used && controller.GetRating() >= 11)
	{
		// Supervisor format
		use_state = State_Direct_SUP;
	}
	else if (data.states[State_Direct_OBS].used && !controller.IsController())
	{
		// Observer format
		use_state = State_Direct_OBS;
	}
	else if (data.states[State_Direct_CON].used)
	{
		// Controller format
		use_state = State_Direct_CON;
	}
	else
	{
		// Global format
		use_state = State_Direct;
	}
	
	std::string presence_small_image_key, presence_large_image_key, state, details, presence_small_image_text, presence_large_image_text;

    char freq_buffer[20];
    std::snprintf(freq_buffer, sizeof(freq_buffer), "%.3f MHz", frequency);
    std::string freq_str = freq_buffer;
    std::string radio_callsign;
    pMyPlugIn->config.FindRadioCallsign(callsign, freq_str, radio_callsign);
    std::string display_name = GetDisplayName(radio_callsign);
const std::map<std::string, std::string> Dictionary =
    {
        {"callsign", callsign},          // Original controller callsign
        {"rcallsign", radio_callsign},   // Radio callsign (might be modified)
        {"raw_callsign", callsign},      // Always the original callsign
        {"display_name", display_name},  // Friendly station name
        {"frequency", freq_str}          // Formatted frequency
    };

	presence_small_image_key = ConfigData::LocalOrGlobal(ConfigData::LocalOrGlobal(data.states[use_state].presence_small_image_key, data.states[State_Direct].presence_small_image_key), data.discord_presence_small_image_key);
	presence_large_image_key = ConfigData::LocalOrGlobal(ConfigData::LocalOrGlobal(data.states[use_state].presence_large_image_key, data.states[State_Direct].presence_large_image_key), data.discord_presence_large_image_key);
    details = "{display_name}";  // Top line: Station Name
    state = "{raw_callsign} on {frequency}";  // Bottom line
	presence_small_image_text = ConfigData::LocalOrGlobal(data.states[use_state].presence_small_image_text, data.states[State_Direct].presence_small_image_text);
	presence_large_image_text = ConfigData::LocalOrGlobal(data.states[use_state].presence_large_image_text, data.states[State_Direct].presence_large_image_text);

	std::string button1_label = ConfigData::LocalOrGlobal(ConfigData::LocalOrGlobal(data.states[use_state].buttons[0].label, data.states[State_Direct].buttons[0].label), data.buttons[0].label);
	MessageFormatter::formatmap(button1_label, Dictionary);
	std::string button1_url = ConfigData::LocalOrGlobal(ConfigData::LocalOrGlobal(data.states[use_state].buttons[0].url, data.states[State_Direct].buttons[0].url), data.buttons[0].url);
	MessageFormatter::formatmap(button1_url, Dictionary);
	configButtons[0] = Button(
		button1_label,
		button1_url
	);			
	std::string button2_label = ConfigData::LocalOrGlobal(ConfigData::LocalOrGlobal(data.states[use_state].buttons[1].label, data.states[State_Direct].buttons[1].label), data.buttons[1].label);
	MessageFormatter::formatmap(button2_label, Dictionary);
	std::string button2_url = ConfigData::LocalOrGlobal(ConfigData::LocalOrGlobal(data.states[use_state].buttons[1].url, data.states[State_Direct].buttons[1].url), data.buttons[1].url);
	MessageFormatter::formatmap(button2_url, Dictionary);
	configButtons[1] = Button(
		button2_label.c_str(),
		button2_url.c_str()
	);

	if (configButtons[0].IsValid())
	{
		configButtons[0].FillStruct(buttons);
		// Nested condition because there will never be a single button in index 1.
		if (configButtons[1].IsValid())
		{
			configButtons[1].FillStruct(buttons+1);
		}
		else {
			buttons[1] = { "" , "" };
		}
		discordPresence.buttons = buttons;
	}

    MessageFormatter::formatmap(details, Dictionary);
    MessageFormatter::formatmap(state, Dictionary);
	MessageFormatter::formatmap(presence_small_image_text, Dictionary);
	MessageFormatter::formatmap(presence_large_image_text, Dictionary);

	discordPresence.smallImageKey = presence_small_image_key.c_str();
	discordPresence.largeImageKey = presence_large_image_key.c_str();
	discordPresence.state = state.c_str();
	discordPresence.details = details.c_str();
	discordPresence.smallImageText = presence_small_image_text.c_str();
	discordPresence.largeImageText = presence_large_image_text.c_str();

	discordPresence.startTimestamp = pMyPlugIn->EuroInittime;
	Discord_UpdatePresence(&discordPresence);
	Discord_RunCallbacks();
}


int DiscordEuroscopeExt::CountACinRange()
{
	int count = 0;
	EuroScopePlugIn::CRadarTarget ac = RadarTargetSelectFirst();
	while (ac.IsValid())
	{
		count++;
		ac = RadarTargetSelectNext(ac);
	}
	return count;
}

int DiscordEuroscopeExt::CountTrackedAC()
{
	int count = 0;
	EuroScopePlugIn::CRadarTarget ac = RadarTargetSelectFirst();
	while (ac.IsValid())
	{
		if (ac.GetCorrelatedFlightPlan().GetTrackingControllerIsMe() == true)
		{
			count++;
			if (std::find(pMyPlugIn->tracklist.begin(), pMyPlugIn->tracklist.end(), ac.GetCallsign()) == pMyPlugIn->tracklist.end())
				pMyPlugIn->tracklist.push_back(ac.GetCallsign());
		}
		ac = RadarTargetSelectNext(ac);
	}
	return count;
}

bool DiscordEuroscopeExt::OnCompileCommand(const char* sCommandLine)
{
	if (strcmp(sCommandLine, ".discord hotreload") == 0)
	{
		DisplayUserMessage("Message", "DiscordEuroscope", "Resetting...", true, true, false, true, false);
		config.Cleanup();
		DisplayUserMessage("Message", "DiscordEuroscope", "Reloading...", true, true, false, true, false);
		try {
			config.Init();
		}
		catch (std::exception& e)
		{
			std::string error_msg = "Error: ";
			error_msg += e.what();
			DisplayUserMessage("Message", "DiscordEuroscope", error_msg.c_str(), true, true, false, true, false);
			return true;
		}

		this->EuroInittime = (int)time(NULL);
		config.LoadRadioCallsigns();

		char dmsg[40] = { 0 };
		int count = config.Data().RadioCallsigns.size();
		sprintf_s(dmsg, 40, "Successfully parsed %i callsign%s", count, count == 1 ? "!" : "s!");
		DisplayUserMessage("Message", "DiscordEuroscope", dmsg, true, true, false, true, false);
		return true;
	}
	return false;
}