#include "window.h"
#include <string>
#include "../ChaiLove.h"
#include "../LibretroLog.h"

#include "SDL2/SDL.h"
#include "config.h"
#include "libretro.h"
#include "Types/Graphics/Point.h"
#include <SDL2/SDL_video.h>

using ::ChaiLove;
using love::Types::Graphics::Point;

namespace love {

bool window::load(const config& conf) {
	ChaiLove* app = ChaiLove::getInstance();

	// Initialize SDL.
	if (SDL_Init(SDL_INIT_VIDEO) == -1) {
		const char* errorChar = SDL_GetError();
		std::string errString("");
		if (errorChar != NULL) {
			errString = errorChar;
		}
		LibretroLog::log(RETRO_LOG_ERROR) << "[game] Unable to initialize SDL " << errString << std::endl;
		return false;
	}

	// Build the Screen.
	// Uint32 flags;
	// if (conf.window.hwsurface) {
	// 	flags = SDL_HWSURFACE;
	// } else {
	// 	flags = SDL_SWSURFACE;
	// }
	// if (conf.window.asyncblit) {
	// 	flags |= SDL_ASYNCBLIT;
	// }
	// if (conf.window.doublebuffering) {
	// 	// flags |= SDL_DOUBLEBUF;
	// }
	SDL_CreateWindowAndRenderer(conf.window.width, conf.window.height, SDL_WINDOW_FULLSCREEN, &app->win, &app->renderer);
	if (app->win == NULL) {
		const char* errorChar = SDL_GetError();
		std::string errString("");
		if (errorChar != NULL) {
			errString = errorChar;
		}
		LibretroLog::log(RETRO_LOG_ERROR) << "[game] Unable to initialize SDL" << errString << std::endl;
		SDL_Quit();
		return false;
	}

	auto surf = SDL_GetWindowSurface(app->win);
	// Enable video buffering.
	app->videoBuffer = (uint32_t *)surf->pixels;

	// Set the title.
	setTitle(conf.window.title);
	return true;
}

bool window::unload() {
	// Destroy the screen.
	ChaiLove* app = ChaiLove::getInstance();
	if (app->screen != NULL) {
		SDL_FreeSurface(app->screen);
		app->screen = NULL;
	}

	// Close SDL.
	SDL_Quit();
	return true;
}

std::string window::getTitle() {
	char* titleChar;
	// SDL_WM_GetCaption(&titleChar, NULL);
	return std::string(titleChar);
}

window& window::setTitle(const std::string& title) {
	ChaiLove::getInstance()->config.window.title = title;
	// SDL_WM_SetCaption(title.c_str(), 0);
	return *this;
}

window& window::showMessageBox(const std::string& msg, int frames) {
	retro_message retroMessage;
	retroMessage.msg = msg.c_str();
	retroMessage.frames = frames;
	ChaiLove::environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE, &retroMessage);
	return *this;
}

window& window::showMessageBox(const std::string& msg) {
	showMessageBox(msg, 1000);
	return *this;
}

}  // namespace love
