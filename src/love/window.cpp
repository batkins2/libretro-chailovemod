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

	// SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
	// SDL_RenderSetLogicalSize(app->renderer, conf.window.width, conf.window.height);

	app->texture = SDL_CreateTexture(app->renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, conf.window.width, conf.window.height);

	app->screen = SDL_GetWindowSurface(app->win);
	// app->screen = SDL_CreateRGBSurface(0, conf.window.width, conf.window.height, 32, 0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF);

	// Enable video buffering.
	app->videoBuffer = (uint32_t *)app->screen->pixels;

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
