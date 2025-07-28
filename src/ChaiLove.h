/**
 * \mainpage ChaiLove API
 *
 * [ChaiLove](https://github.com/libretro/libretro-chailove) is a framework for making 2D games in the [ChaiScript scripting language](http://chaiscript.com/). ChaiLove is totally free, and can be used in anything from friendly open-source hobby projects, to evil, closed-source commercial ones.
 *
 * Some interesting places to go from here:
 * - \link gettingstarted Getting Started \endlink
 * - \link ::love love \endlink (the module)
 * - \link love::script Callbacks \endlink All the callbacks that are executed.
 * - \link games Games \endlink
 * - [Examples](https://git.io/vxPmn)
 * - [Building ChaiLove](https://github.com/libretro/libretro-chailove#development)
 *
 * ## ChaiScript
 *
 * Never used ChaiScript before? It's a really cool language! This manual won't teach you ChaiScript, but fortunately there are other good resources for that.
 *
 * - [ChaiScript Cheatsheet](https://git.io/vxPqI)
 * - [ChaiScript Documentation](http://chaiscript.com/docs.html)
 *
 * ## Hello World
 *
 * This is the full source for "hello world" in ChaiLove, using a `main.chai` file. Running this code will cause an 640 by 480 window to appear, and display white text on a black background.
 *
 * @code
 * def load() {
 * 	 global logo = love.graphics.newImage("logo.png")
 *   global x = 10.0f
 * }
 *
 * def draw() {
 *   love.graphics.print("Hello World!", 400, 300)
 *   love.graphics.draw(logo, x, 100)
 * }
 *
 * def update(dt) {
 *   x = x + 20.0f * dt
 * }
 * @endcode
 *
 * To run the above, execute it with `retroarch -L chailove_libretro.so main.chai` .
 */

#ifndef SRC_CHAILOVE_H_
#define SRC_CHAILOVE_H_

#define CHAILOVE_VERSION_MAJOR 1
#define CHAILOVE_VERSION_MINOR 3
#define CHAILOVE_VERSION_PATCH 0
#define CHAILOVE_VERSION_STRING "1.3.0"

#include "SDL2/SDL.h"
#include "libretro.h"
#include "love/common/Module.h"
#include "love/chai_matrices.h"
#ifndef __HAVE_CHAI_GFX__
#include "love/chai_gfx.h"
#endif
#ifndef __HAVE_CHAI_MESH__
#include "love/chai_mesh.h"
#endif
#ifndef __HAVE_CHAI_SHADER__
#include "love/chai_shader.h"
#endif
#ifndef __HAVE_CHAI_SCENE__
#include "love/chai_scene.h"
#endif
#ifndef __HAVE_CHAI_COLLISIONS__
#include "love/chai_collisions.h"
#endif
#ifndef __HAVE_CHAI_PARTICLES__
#include "love/chai_particles.h"
#endif
#ifndef __HAVE_CHAI_GUI__
#include "love/chai_gui.h"
#endif
#ifndef __HAVE_CHAI_DEBUG__
#include "love/chai_debug.h"
#endif
#ifndef __HAVE_CHAI_EDITOR__
#include "love/chai_editor.h"
#endif
#include "love/keyboard.h"
#include "love/config.h"
#include "love/data.h"
#include "love/script.h"
#include "love/filesystem.h"
#include "love/filesystem/Filesystem.h"
#include "love/graphics.h"
#include "love/image.h"
#include "love/system.h"
#include "love/sound.h"
// #include "love/font.h"
#include "love/fontmod.h"
#include "love/font/freetype/Font.h"
#include "love/timer.h"
#include "love/audio.h"
#include "love/joystick.h"
#include "love/mouse.h"
#include "love/window.h"
#include "love/math.h"
#include "love/event.h"
#include "love/console.h"

class ChaiLove {
	public:
	static ChaiLove* getInstance();
	static ChaiLove* m_instance;
	static void destroy();
	static bool hasInstance();

	static retro_input_state_t input_state_cb;
	static retro_input_poll_t input_poll_cb;
	static retro_environment_t environ_cb;

	love::config config;
	love::console console;
	love::keyboard keyboard;
	love::script* script = NULL;
	love::data data;
	love::filesystem filesystem;
	love::graphics graphics;
	love::image image;
	love::system system;
	love::sound sound;
	// love::font font;
	love::gfx::FontMod *fm = nullptr;
	// love::fontmod::Rasterizer rasterizer;
	// love::fontmod::freetype::FontMod freetype_font;
	love::timer timer;
	love::audio audio;
	love::joystick joystick;
	love::mouse mouse;
	love::math math;
	love::window window;
	love::event event;
	love::chai_matrices chai_matrices;
	// #ifndef __HAVE_CHAI_GFX__
	love::chai_gfx chai_gfx;
	// #endif
	// #ifndef __HAVE_CHAI_MESH__
	love::chai_mesh chai_mesh;
	// #endif
	// #ifndef __HAVE_CHAI_SHADER__
	love::chai_shader chai_shader;
	// #endif
	love::chai_scene chai_scene;
	love::chai_collisions chai_collisions;
	love::chai_particles chai_particles;
	love::chai_gui chai_gui;
	love::chai_debug chai_debug;
	love::chai_editor chai_editor;
	
	ChaiLove();
	~ChaiLove();
	void quit(void);
	bool load(const std::string& file, const void* data);
	void update();
	void draw();
	void reset();
	std::string savestate();
	bool loadstate(const std::string& data);
	void cheatreset();
	void cheatset(int index, bool enabled, const std::string& code);

	uint32_t *videoBuffer = NULL;
	SDL_Surface* screen = NULL;
	SDL_Renderer* renderer = NULL;
	SDL_Window* win = NULL;
	SDL_Texture* texture = NULL;
	love::imagemod::Image *getImageModule() { return love::Module::getInstance<love::imagemod::Image>(love::Module::M_IMAGE); };
	love::filesystem getFSModule() { return filesystem; };
	void printNew(const std::string& text, int x, int y, int r, int g, int b, int a, int size = 24) {
		auto t = std::vector<love::fontmod::ColoredString>();
		auto cs = love::fontmod::ColoredString();
		cs.str = text;
		cs.color = love::toColorf(love::Color32(r, g, b, a));
		t.push_back(cs);
		
		auto i = love::Module::getInstance<love::gfx::Graphics>(love::Module::M_GRAPHICS);
		if (fm == nullptr) {	
			love::fontmod::TrueTypeRasterizer::Settings settings;
			settings.hinting = love::fontmod::TrueTypeRasterizer::HINTING_NONE; // Set hinting to normal
			fm = i->newDefaultFont(size, settings);
		}
		
		love::Matrix4 m;
		m.setTranslation((float)x, (float)y);
		auto vcs = std::vector<love::fontmod::ColoredString>({cs});
		float wrap = 0.0f; // No wrapping
		love::gfx::FontMod::AlignMode align = love::gfx::FontMod::AlignMode::ALIGN_LEFT;
		i->print(vcs, fm, m);
	}
};

#endif  // SRC_CHAILOVE_H_
