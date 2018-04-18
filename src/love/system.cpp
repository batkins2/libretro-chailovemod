#include "system.h"
#include "../ChaiLove.h"

#include <string>
#include <iostream>
#include "semver.h"

namespace love {

std::string system::getOS() {
	// TODO(RobLoach): Replace Macros with something more complex?
	#if (defined __HAIKU__)
	return "Linux";
	#elif((defined __BEOS__) ||(defined __beos__))
	return "Linux";
	#elif(defined _WIN32_WCE) || (defined _WIN64_WCE)
	return "Windows";
	#elif((defined WINAPI_FAMILY) && WINAPI_FAMILY == WINAPI_FAMILY_APP)
	return "Windows";
	#elif(((defined _WIN32) || (defined _WIN64)) && (!defined __CYGWIN__))
	return "Windows";
	#elif(defined OS2)
	return "OS X";
	#elif((defined __MACH__) && (defined __APPLE__))
	return "OS X";
	#elif defined(macintosh)
	return "OS X";
	#elif defined(ANDROID)
	return "Android";
	#elif defined(__linux)
	return "Linux";
	#elif defined(__sun) || defined(sun)
	return "Linux";
	#elif defined(__FreeBSD__) || defined(__DragonFly__)
	return "Linux";
	#elif defined(__NetBSD__) || defined(__OpenBSD__) || defined(__bsdi__)
	return "Linux";
	#elif defined(unix) || defined(__unix__)
	return "Linux";
	#else
	return "Other";
	#endif
}

std::vector<int> system::getVersion() {
	std::vector<int> version;
	version.push_back(CHAILOVE_VERSION_MAJOR);
	version.push_back(CHAILOVE_VERSION_MINOR);
	version.push_back(CHAILOVE_VERSION_PATCH);
	return version;
}

std::string system::getVersionString() {
	return CHAILOVE_VERSION_STRING;
}

bool system::load(config& t) {
	// Load the semantic version string.
	semver_t chailoveVersion = {};
	semver_t coreVersion = {};
	bool showWarning = false;

	if (semver_parse(getVersionString().c_str(), &chailoveVersion)
		|| semver_parse(t.version.c_str(), &coreVersion)) {
		std::cout << "[ChaiLove] [system] Error: Invalid t.version string " << t.version << std::endl;
		return true;
	}
	std::cout << "[ChaiLove] [system] Version current:  " << getVersionString() << std::endl;
	std::cout << "[ChaiLove] [system] Version targeted: " << t.version << std::endl;

	// Compare the version to the version of ChaiLove.
	int resolution = semver_compare(coreVersion, chailoveVersion);
	if (resolution == 0) {
		std::cout << "[ChaiLove] [system] Version " << getVersionString() << " == " << t.version << std::endl;
	} else if (resolution == -1) {
		std::cout << "[ChaiLove] [system] Version " << getVersionString() << " > " << t.version << std::endl;
	} else {
		std::cout << "[ChaiLove] [system] Version " << getVersionString() << " < " << t.version << std::endl;
	}

	// Check the ~= satisfaction
	resolution = semver_satisfies(chailoveVersion, coreVersion, "~");
	if (resolution == 1) {
		std::cout << "[ChaiLove] [system] Version " << getVersionString() << " ~= " << t.version << std::endl;
	} else {
		std::cout << "[ChaiLove] [system] Version " << getVersionString() << " !~= " << t.version << std::endl;
	}

	// Check the ^= satisfaction
	resolution = semver_satisfies(chailoveVersion, coreVersion, "^");
	if (resolution == 1) {
		std::cout << "[ChaiLove] [system] Version " << getVersionString() << " ^= " << t.version << std::endl;
	} else {
		std::cout << "[ChaiLove] [system] Version " << getVersionString() << " !^= " << t.version << std::endl;
		showWarning = true;
	}

	// Display a warning in the On-Screen Display.
	if (showWarning) {
		std::string message = "Warning: Expected ChaiLove " + t.version + ", running " + getVersionString();
		std::cout << "[ChaiLove] [system] " << warning << std::endl;
		ChaiLove::getInstance()->window.showMessageBox(message);
	}

	return true;
}

}  // namespace love
