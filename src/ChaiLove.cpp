#include "ChaiLove.h"
#include "LibretroLog.h"
#include <libretro.h>
#include <SDL2/SDL.h>
#include <string>
#include <chrono>

ChaiLove* ChaiLove::m_instance = NULL;
retro_input_state_t ChaiLove::input_state_cb = NULL;
retro_input_poll_t ChaiLove::input_poll_cb = NULL;
retro_environment_t ChaiLove::environ_cb = NULL;

ChaiLove::ChaiLove() {
	chai_async = nullptr;
}

void ChaiLove::destroy() {
	if (log_cb) log_cb(RETRO_LOG_INFO, "[ChaiLove] Attempting to destroy ChaiLove\n");
	if (hasInstance()) {
			   if (log_cb) log_cb(RETRO_LOG_INFO, "[ChaiLove] Destroying ChaiLove\n");
		m_instance->quit();
		if (m_instance->chai_async) {
			delete m_instance->chai_async;
			m_instance->chai_async = nullptr;
		}
		delete m_instance;
		m_instance = NULL;
	}
	if (log_cb) log_cb(RETRO_LOG_INFO, "[ChaiLove] Destroyed ChaiLove\n");
}

ChaiLove* ChaiLove::getInstance() {
	if (!hasInstance()) {
		// if (log_cb) log_cb(RETRO_LOG_INFO, "[ChaiLove] Initializing ChaiLove\n");
		m_instance = new ChaiLove;
	}
	return m_instance;
}

bool ChaiLove::hasInstance() {
	return m_instance != NULL;
}

ChaiLove::~ChaiLove() {
	quit();
	if (chai_async) {
		delete chai_async;
		chai_async = nullptr;
	}
}

void ChaiLove::quit(void) {
	// Enforce the should close flag.
	event.m_shouldclose = true;

	// Disengage the scripting language.
	if (script) {
		script->exit();
		delete script;
		script = NULL;
	}

	// Unload all the other sub-systems.
	joystick.unload();
	// font.unload();
	// delete fm;
	// freetype_font.~FontMod();
	image.unload();
	sound.unload();
	filesystem.unload();
	window.unload();
}

bool ChaiLove::load(const std::string& file, const void* data) {
	// Display a welcome message from ChaiLove.
#ifndef GIT_VERSION
#define GIT_VERSION ""
#endif
	std::string version = CHAILOVE_VERSION_STRING GIT_VERSION;
	// if (log_cb) log_cb(RETRO_LOG_INFO, "[ChaiLove] ChaiLove %s\n", version.c_str());

	// Iniitalize some of the initial subsystems.
	sound.load();

	// Initalize the file system.
	bool loaded = filesystem.init(file, data);
	if (!loaded) {
		if (log_cb) log_cb(RETRO_LOG_ERROR, "[ChaiLove] [filesystem] Error loading %s\n", file.c_str());
		return false;
	}

	filesystem.mountlibretro();

	// Initialize the scripting system.
	script = new love::script(file);
	if (!script->mainLoaded) {
		if (log_cb) log_cb(RETRO_LOG_ERROR, "[ChaiLove] [script] Error loading %s\n", file.c_str());
		return false;
	}
	script->conf(config);
	system.load(config);

	// Load up the window dimensions.
	window.load(config);
	console.load(config);
	graphics.load();
	// chai_gfx.init();
	image.load();
	keyboard.load();
	joystick.load();
	math.load();
	mouse.load();
	// font.load();

	// Now that all subsystems are loaded, create chai_async
	if (!chai_async) {
		chai_async = new love::chai_async();
	}

	// Now that all subsystems are loaded, start the script.
	script->load();

	return true;
}

void ChaiLove::update() {
	// Update and poll all the events.
	event.update();

	// Update the input systems.
	mouse.update();
	joystick.update();
	keyboard.update();

	// Step forward the timer, and update the game.
	if (script != NULL) {
		script->update(timer.getDelta());
	}
}

/**
 * Reset the current game.
 */
void ChaiLove::reset() {
	// Tell the script that we are to reset the game.
	if (log_cb) log_cb(RETRO_LOG_INFO, "[ChaiLove] Reset\n");
	if (script != NULL) {
		script->reset();
	}
}

/**
 * Render the ChaiLove.
 */
void ChaiLove::draw() {
	if (event.m_shouldclose || event.m_pauserendering) {
		return;
	}

	// Clear the screen.
	// graphics.clear();

	// SDL_LockTexture(texture, NULL, &screen->pixels, &screen->pitch);
	// SDL_SetRenderTarget(renderer, texture);
	// SDL_RenderClear(renderer);

	// Render the game.
	if (script != NULL) {
		// PERF: Time ChaiScript execution vs Vulkan recording
		static int drawCounter = 0;
		static double totalScriptTime = 0.0;
		static double totalVulkanTime = 0.0;
		auto scriptStart = std::chrono::high_resolution_clock::now();
		
		script->draw();
		
		auto scriptEnd = std::chrono::high_resolution_clock::now();
		auto scriptMs = std::chrono::duration<double, std::milli>(scriptEnd - scriptStart).count();
		totalScriptTime += scriptMs;
		
		if (++drawCounter % 60 == 0) {
			// std::printf("[PERF CHAISCRIPT] 60 draw() calls | Last: %.2f ms | Avg: %.2f ms | Total: %.2f ms\n",
			// 	scriptMs, totalScriptTime / drawCounter, totalScriptTime);
			// fflush(stdout);
		}
	}

	// Render the in-game console.
	// console.draw();

	// Flip the buffer.
	// if (SDL_blit(screen)) {
	// 	std::string out("[ChaiLove] Failed to swap the buffers: ");
	// 	LibretroLog::log(RETRO_LOG_ERROR) << out << SDL_GetError() << std::endl;
	// }
	// SDL_UnlockTexture(texture);
	// SDL_SetRenderTarget(renderer, NULL);
	// SDL_RenderCopy(renderer, texture, NULL, NULL);
	// SDL_RenderPresent(renderer);
}

/**
 * Tell the script to return a string representing the game data.
 */
std::string ChaiLove::savestate() {
	if (script != NULL) {
		return script->savestate();
	}
	return "";
}

/**
 * Ask the script to load the given string.
 */
bool ChaiLove::loadstate(const std::string& data) {
	if (script != NULL) {
		return script->loadstate(data);
	}
	return false;
}

/**
 * Invoke the script cheatreset hook.
 */
void ChaiLove::cheatreset() {
	if (script != NULL) {
		script->cheatreset();
	}
}

/**
 * Invoke the script cheatset hook.
 */
void ChaiLove::cheatset(int index, bool enabled, const std::string& code) {
	if (script != NULL) {
		script->cheatset(index, enabled, code);
	}
}
