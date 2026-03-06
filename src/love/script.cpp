#include "script.h"
#include "../ChaiLove.h"
#include "../LibretroLog.h"
#include <algorithm>
#include <ctime>
#include <chrono>
#include <dbghelp.h>

#ifdef __HAVE_CHAISCRIPT__
#include "chaiscript/extras/math.hpp"
#include "chaiscript/extras/string_methods.hpp"
using namespace chaiscript;
#endif

using ::ChaiLove;
using love::Types::Graphics::Point;
using love::Types::Graphics::Image;
// using love::Types::Graphics::Font;
using love::Types::Graphics::Point;
using love::Types::Graphics::Color;
using love::Types::Input::Joystick;
using love::Types::Config::WindowConfig;
using love::Types::Config::ModuleConfig;
using love::Types::Audio::SoundData;
using love::Types::FileSystem::FileData;
using love::graphics;
using love::chai_mesh;

namespace love {

std::string script::findModule(const std::string& filename) {
	ChaiLove* app = ChaiLove::getInstance();
	std::string possibilities[5] = {
		filename,
		filename + ".chai",
		// Allow loading lua files as ChaiScript?
		// filename + ".lua",
		// Attempt to load a directory's init.chai, if available.
		filename + "/init.chai"
        // Allow loading .lua files?
		// filename + "/init.lua"
	};
	for (const std::string& possibility : possibilities) {
		// Make sure the file exists and is a file.
		if (app->filesystem.exists(possibility) && app->filesystem.isFile(possibility)) {
			return possibility;
		}
	}
	return "";
}

bool script::loadModule(const std::string& moduleName) {
	#ifdef __HAVE_CHAISCRIPT__
	ChaiLove* app = ChaiLove::getInstance();

	// Ensure we're loading a valid module name.
	if (moduleName.empty()) {
		// LibretroLog::log(RETRO_LOG_ERROR) << "[ChaiLove] [script] loadModule was called with an empty moduleName." << std::endl;
		return false;
	}

	// Store a filename for the module.
	std::string filename = findModule(moduleName);
	if (filename.empty()) {
		// LibretroLog::log(RETRO_LOG_INFO) << "[ChaiLove] [script] Module " << moduleName << " not found." << std::endl;
		return false;
	}

	// Load the contents of the file.
	std::string contents = app->filesystem.read(filename);

	// Make sure it was not empty.
	if (contents.empty()) {
		// LibretroLog::log(RETRO_LOG_ERROR) << "[ChaiLove] [script] Module " << filename << " was loaded, but empty." << std::endl;
		return false;
	}

	// Run the script.
	eval(contents, filename);
	return true;
	#endif
	return false;
}

bool script::require(const std::string& moduleName) {
	// Find what the cleansed module name is.
	std::string noExtension = replaceString(replaceString(moduleName, ".chai", ""), ".lua", "");
	std::string filename = replaceString(noExtension, ".", "/");

	// Ensure we only load the script once.
	if (std::find(m_requiremodules.begin(), m_requiremodules.end(), filename) != m_requiremodules.end()) {
		return true;
	}

	// Attempt to load the module.
	bool loaded = loadModule(filename);
	if (loaded) {
		m_requiremodules.push_back(filename);
	}

	return loaded;
}

chaiscript::Boxed_Value script::eval(const std::string& code, const std::string& filename) {
	// Replace possible problematic tabs, and evaluate the script.
	std::string contents = replaceString(code, "\t", "  ");
	return chai.eval(contents, Exception_Handler(), filename);
}

std::string script::evalString(const std::string& code, const std::string& filename) {
	// Replace possible problematic tabs, and evaluate the script.
	std::string contents = replaceString(code, "\t", "  ");
	return chai.eval<std::string>(contents, Exception_Handler(), filename);
}

void script::debugbreak() {
	#ifdef _WIN32
	__debugbreak();
	#else
	raise(SIGTRAP);
	#endif
}

script::script(const std::string& file) {
	#ifdef __HAVE_CHAISCRIPT__
	// LEAK FIX: Initialize Boxed_Value object pools and scalar caches at startup
	#ifdef CHAISCRIPT_POOLING_ENABLED
	chaiscript::initializeBoxedValuePools();
	chaiscript::ScalarCache::initialize();
	std::printf("[ChaiScript] LEAK FIX: Thread-local pooling ENABLED (64-object per-thread pool)\n");
	#else
	#ifdef CHAISCRIPT_NO_THREADS
	std::printf("[ChaiScript] WARNING: Boxed_Value pooling DISABLED (NO_THREADS build - pooling requires threading)\n");
	#else
	std::printf("[ChaiScript] WARNING: Boxed_Value pooling DISABLED (standard allocator active)\n");
	#endif
	#endif
	
	   ChaiLove* app = ChaiLove::getInstance();
	   // Ensure chai_async is initialized before binding
	   if (!app->chai_async) {
		   app->chai_async = new love::chai_async();
	   }

	// Debug: Track vector allocations
	static int vectorAllocCount = 0;
	auto vectorFloatDebugWrapper = []() -> std::vector<float> {
		vectorAllocCount++;
		if (vectorAllocCount % 1000 == 0) {
			std::printf("[ChaiScript] VectorFloat allocated %d times\n", vectorAllocCount);
		}
		return std::vector<float>();
	};

	// ChaiScript Standard Library Additions
	// This adds some basic type definitions to ChaiScript.
	auto stl_module = std::make_shared<chaiscript::Module>();
	bootstrap::standard_library::vector_type<std::vector<int>>("VectorInt", *stl_module);
	bootstrap::standard_library::vector_type<std::vector<float>>("VectorFloat", *stl_module);
	bootstrap::standard_library::vector_type<std::vector<std::string>>("StringVector", *stl_module);
	bootstrap::standard_library::map_type<std::map<std::string, bool>>("StringBoolMap", *stl_module);
	bootstrap::standard_library::map_type<std::map<std::string, int>>("StringIntMap", *stl_module);
	bootstrap::standard_library::map_type<std::map<std::string, float>>("StringFloatMap", *stl_module);
	bootstrap::standard_library::map_type<std::map<std::string, std::vector<float>>>("StringFloatVectorMap", *stl_module);
	chai.add(stl_module);
	chai.add(fun(vectorFloatDebugWrapper), "VectorFloat"); // Debug wrapper

	// GLM Types
	chai.add(user_type<glm::vec3>(), "vec3");
	chai.add(constructor<glm::vec3()>(), "vec3");
	chai.add(constructor<glm::vec3(float)>(), "vec3");
	chai.add(constructor<glm::vec3(float, float, float)>(), "vec3");
	chai.add(constructor<glm::vec3(const glm::vec3&)>(), "vec3");
	chai.add(fun(static_cast<glm::vec3& (glm::vec3::*)(const glm::vec3&)>(&glm::vec3::operator=)), "=");
	chai.add(fun(&glm::vec3::x), "x");
	chai.add(fun(&glm::vec3::y), "y");
	chai.add(fun(&glm::vec3::z), "z");
	
	chai.add(user_type<glm::mat4>(), "mat4");
	chai.add(constructor<glm::mat4()>(), "mat4");
	chai.add(constructor<glm::mat4(float)>(), "mat4");
	chai.add(constructor<glm::mat4(const glm::mat4&)>(), "mat4");
	chai.add(fun(static_cast<glm::mat4& (glm::mat4::*)(const glm::mat4&)>(&glm::mat4::operator=)), "=");
	
	bootstrap::standard_library::vector_type<std::vector<glm::vec3>>("VectorVec3", *stl_module);
	bootstrap::standard_library::vector_type<std::vector<glm::mat4>>("VectorMat4", *stl_module);

	// ChaiScript_Extras: String Methods
	auto stringmethods = chaiscript::extras::string_methods::bootstrap();
	chai.add(stringmethods);

	// List
	auto listModule = std::make_shared<chaiscript::Module>();
	chaiscript::bootstrap::standard_library::list_type<std::list<chaiscript::Boxed_Value> >("List", *listModule);
	chai.add(listModule);

	// Add the "love" namespace.
	chai.register_namespace([](chaiscript::Namespace& love) {
		ChaiLove* app = ChaiLove::getInstance();
		love["audio"] = var(std::ref(app->audio));
		love["config"] = var(std::ref(app->config));
		love["console"] = var(std::ref(app->console));
		love["data"] = var(std::ref(app->data));
		love["event"] = var(std::ref(app->event));
		love["filesystem"] = var(std::ref(app->filesystem));
		// love["font"] = var(std::ref(app->font));
		love["graphics"] = var(std::ref(app->graphics));
		love["chai_gfx"] = var(std::ref(app->chai_gfx));
		love["chai_mesh"] = var(std::ref(app->chai_mesh));
		love["chai_scene"] = var(std::ref(app->chai_scene));		
		love["chai_shader"] = var(std::ref(app->chai_shader));
		love["chai_matrices"] = var(std::ref(app->chai_matrices));
		love["chai_collisions"] = var(std::ref(app->chai_collisions));
		love["chai_particles"] = var(std::ref(app->chai_particles));
		love["chai_gui"] = var(std::ref(app->chai_gui));
		love["chai_debug"] = var(std::ref(app->chai_debug));
		love["chai_editor"] = var(std::ref(app->chai_editor));
		love["image"] = var(std::ref(app->image));
		love["joystick"] = var(std::ref(app->joystick));
		love["keyboard"] = var(std::ref(app->keyboard));
		love["math"] = var(std::ref(app->math));
		love["mouse"] = var(std::ref(app->mouse));
		love["sound"] = var(std::ref(app->sound));
		love["system"] = var(std::ref(app->system));
		love["timer"] = var(std::ref(app->timer));
		love["window"] = var(std::ref(app->window));
	},
	"love");

	   if (app->chai_async) {
		   app->chai_async->bindToChaiScript(chai);
	   }

	// Safe sleep function
	chai.add(fun([](double milliseconds) {
		if (milliseconds > 0 && milliseconds < 60000) { // Limit to 60 seconds max
			std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<long long>(milliseconds)));
		}
	}), "sleep");

	// Time functions
	chai.add(fun([]() -> double {
		auto now = std::chrono::high_resolution_clock::now();
		auto duration = now.time_since_epoch();
		auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
		return static_cast<double>(millis);
	}), "time");

	// Calculate difference in milliseconds
	chai.add(fun([](double end, double start) -> double {
		return end - start; // Already in milliseconds
	}), "difftime");

	// Quad Object.
	chai.add(user_type<Quad>(), "Quad");
	chai.add(constructor<Quad()>(), "Quad");
	chai.add(fun(&Quad::x), "x");
	chai.add(fun(&Quad::y), "y");
	chai.add(fun(&Quad::width), "width");
	chai.add(fun(&Quad::height), "height");
	chai.add(fun(&Quad::sw), "sw");
	chai.add(fun(&Quad::sh), "sh");

	// Point Object.
	chai.add(user_type<Point>(), "Point");
	chai.add(fun(&Point::x), "x");
	chai.add(fun(&Point::y), "y");
	chai.add(constructor<Point()>(), "Point");
	chai.add(constructor<Point(float)>(), "Point");
	chai.add(constructor<Point(float, float)>(), "Point");

	// FileInfo Object.
	chai.add(user_type<FileInfo>(), "FileInfo");
	chai.add(fun(&FileInfo::type), "type");
	chai.add(fun(&FileInfo::size), "size");
	chai.add(fun(&FileInfo::modtime), "modtime");
	chai.add(constructor<FileInfo()>(), "FileInfo");
	chai.add(constructor<FileInfo(const std::string&)>(), "FileInfo");
	chai.add(constructor<FileInfo(const std::string&, int)>(), "FileInfo");
	chai.add(constructor<FileInfo(const std::string&, int, int)>(), "FileInfo");

	// FileData Object.
	chai.add(user_type<FileData>(), "FileData");
	chai.add(fun(&FileData::getSize), "getSize");
	chai.add(fun(&FileData::getFilename), "getFilename");
	chai.add(fun(&FileData::getString), "getString");
	chai.add(fun(&FileData::getExtension), "getExtension");
	chai.add(constructor<FileData(const std::string&)>(), "FileData");

	// Color Object.
	chai.add(user_type<Color>(), "Color");
	chai.add(fun(&Color::r), "r");
	chai.add(fun(&Color::g), "g");
	chai.add(fun(&Color::b), "b");
	chai.add(fun(&Color::a), "a");
	chai.add(constructor<Color()>(), "Color");
	chai.add(constructor<Color(int, int, int)>(), "Color");
	chai.add(constructor<Color(int, int, int, int)>(), "Color");

	// Image Object.
	chai.add(user_type<Image>(), "Image");
	chai.add(constructor<Image(const Image &)>(), "Image");
	chai.add(fun(&Image::operator=), "=");
	chai.add(fun(&Image::getWidth), "getWidth");
	chai.add(fun(&Image::getHeight), "getHeight");

	// SoundData Object.
	chai.add(user_type<SoundData>(), "SoundData");
	chai.add(fun(&SoundData::isLooping), "isLooping");
	chai.add(fun(&SoundData::isPlaying), "isPlaying");
	chai.add(fun(&SoundData::setLooping), "setLooping");
	chai.add(fun(&SoundData::stop), "stop");
	chai.add(fun(&SoundData::play), "play");
	chai.add(fun(&SoundData::setVolume), "setVolume");
	chai.add(fun(&SoundData::getVolume), "getVolume");

	// Font.
	// chai.add(user_type<Font>(), "Font");
	// chai.add(fun(&Font::loaded), "loaded");
	// chai.add(fun<int, Font>(&Font::getHeight), "getHeight");
	// chai.add(fun<int, Font, const std::string&>(&Font::getHeight), "getHeight");
	// chai.add(fun<int, Font, const std::string&>(&Font::getWidth), "getWidth");

	// Config
	chai.add(user_type<WindowConfig>(), "WindowConfig");
	chai.add(fun(&WindowConfig::width), "width");
	chai.add(fun(&WindowConfig::height), "height");
	chai.add(fun(&WindowConfig::bbp), "bbp");
	chai.add(fun(&WindowConfig::title), "title");
	chai.add(fun(&WindowConfig::asyncblit), "asyncblit");
	chai.add(fun(&WindowConfig::hwsurface), "hwsurface");
	chai.add(fun(&WindowConfig::doublebuffering), "doublebuffering");
	chai.add(user_type<ModuleConfig>(), "ModuleConfig");
	chai.add(fun(&ModuleConfig::sound), "sound");
	chai.add(user_type<config>(), "Config");
	chai.add(fun(&config::identity), "identity");
	chai.add(fun(&config::version), "version");
	chai.add(fun(&config::window), "window");
	chai.add(fun(&config::modules), "modules");
	chai.add(fun(&config::options), "options");
	chai.add(fun(&config::console), "console");

	// Debug
	chai.add(fun(&script::debugbreak, this), "debugbreak");

	// Console
	chai.add(fun(&console::isEnabled), "isEnabled");
	chai.add(fun(&console::setEnabled), "setEnabled");
	chai.add(fun(&console::isShown), "isShown");

	// Joystick
	chai.add(user_type<Joystick>(), "Joystick");
	chai.add(fun(static_cast<bool (Joystick::*)(const std::string&)>(&Joystick::isDown)), "isDown");
	chai.add(fun(static_cast<bool (Joystick::*)(int)>(&Joystick::isDown)), "isDown");
	chai.add(fun(static_cast<float (Joystick::*)(int)>(&Joystick::getAxis)), "getAxis");
	chai.add(fun(static_cast<float (Joystick::*)(const std::string&)>(&Joystick::getAxis)), "getAxis");
	chai.add(fun(&Joystick::getName), "getName");
	chai.add(fun(&Joystick::isConnected), "isConnected");
	chai.add(fun(&Joystick::getID), "getID");

	// Graphics
	chai.add(fun(&graphics::rectangle), "rectangle");
	chai.add(fun(&graphics::newImage), "newImage");
	chai.add(fun(static_cast<love::graphics& (graphics::*)(const std::string&, int, int)>(&graphics::print)), "print");
	chai.add(fun(static_cast<love::graphics& (graphics::*)(const std::string&)>(&graphics::print)), "print");
	chai.add(fun(static_cast<love::graphics& (graphics::*)(int, int)>(&graphics::point)), "point");
	chai.add(fun(static_cast<love::graphics& (graphics::*)(Point*)>(&graphics::point)), "point");
	// chai.add(bootstrap::standard_library::vector_type<std::vector<Point*>>("VectorPointPointer"));
	// chai.add(bootstrap::standard_library::vector_type<std::vector<Point>>("VectorPoint"));
	// chai.add(fun<void, graphics, std::vector<Point*>>(&graphics::points), "points");
	// chai.add(fun<void, graphics, std::vector<Point>>(&graphics::points), "points");
	chai.add(fun(&graphics::arc), "arc");
	chai.add(fun(&graphics::ellipse), "ellipse");
	chai.add(fun(&graphics::getWidth), "getWidth");
	chai.add(fun(&graphics::getHeight), "getHeight");
	chai.add(fun(&graphics::getDimensions), "getDimensions");
	chai.add(fun(&graphics::circle), "circle");
	chai.add(fun(&graphics::line), "line");
	chai.add(fun(&graphics::newQuad), "newQuad");
	chai.add(fun(&graphics::setDefaultFilter), "setDefaultFilter");
	chai.add(fun(&graphics::getDefaultFilter), "getDefaultFilter");
	// chai.add(fun<Font*, graphics, const std::string&, int>(&graphics::newFont), "newFont");
	// chai.add(fun<Font*, graphics, const std::string&>(&graphics::newFont), "newFont");
	// chai.add(fun<Font*, graphics, const std::string&, int, int, const std::string&>(&graphics::newFont), "newFont");
	// chai.add(fun<Font*, graphics>(&graphics::newFont), "newFont");
	// chai.add(fun<love::graphics&, graphics, Font*>(&graphics::setFont), "setFont");
	// chai.add(fun<love::graphics&, graphics>(&graphics::setFont), "setFont");
	// chai.add(fun<Font*, graphics>(&graphics::getFont), "getFont");
	chai.add(fun(static_cast<love::graphics& (graphics::*)(int, int, int, int)>(&graphics::setColor)), "setColor");
	chai.add(fun(static_cast<love::graphics& (graphics::*)(int, int, int)>(&graphics::setColor)), "setColor");
	chai.add(fun(static_cast<love::graphics& (graphics::*)(int, int, int, int)>(&graphics::setBackgroundColor)), "setBackgroundColor");
	chai.add(fun(static_cast<love::graphics& (graphics::*)(int, int, int)>(&graphics::setBackgroundColor)), "setBackgroundColor");

	chai.add(fun(static_cast<love::graphics& (graphics::*)(Image*, int, int)>(&graphics::draw)), "draw");
	chai.add(fun(static_cast<love::graphics& (graphics::*)(Image*)>(&graphics::draw)), "draw");
	chai.add(fun(static_cast<love::graphics& (graphics::*)(Image*, int, int, float, float, float, float, float)>(&graphics::draw)), "draw");
	chai.add(fun(static_cast<love::graphics& (graphics::*)(Image*, int, int, float, float, float, float)>(&graphics::draw)), "draw");
	chai.add(fun(static_cast<love::graphics& (graphics::*)(Image*, int, int, float, float, float)>(&graphics::draw)), "draw");
	chai.add(fun(static_cast<love::graphics& (graphics::*)(Image*, int, int, float, float)>(&graphics::draw)), "draw");
	chai.add(fun(static_cast<love::graphics& (graphics::*)(Image*, int, int, float)>(&graphics::draw)), "draw");

	chai.add(fun(static_cast<love::graphics& (graphics::*)(Image*, Quad, int, int)>(&graphics::draw)), "draw");
	chai.add(fun(static_cast<love::graphics& (graphics::*)(Image*, Quad)>(&graphics::draw)), "draw");
	chai.add(fun(static_cast<love::graphics& (graphics::*)(int, int, int, int)>(&graphics::clear)), "clear");
	chai.add(fun(static_cast<love::graphics& (graphics::*)(int, int, int)>(&graphics::clear)), "clear");
	chai.add(fun(static_cast<love::graphics& (graphics::*)()>(&graphics::clear)), "clear");

	// Gfx
	chai.add(fun(&chai_gfx::wrap_newShader), "newShader");
	// chai.add(fun(&chai_gfx::wrap_setShader), "setShader");
	// chai.add(fun(&chai_gfx::wrap_newMesh), "newMesh");
	chai.add(fun(&chai_gfx::wrap_newMeshFromFile), "newMeshFromFile");
	// chai.add(fun(&chai_gfx::wrap_newScene), "newScene");
	// chai.add(fun(&chai_gfx::draw), "draw");
	// chai.add(fun(&chai_gfx::drawScene), "drawScene");
	// chai.add(fun(&chai_gfx::createCanvas), "createCanvas");
	// chai.add(fun(&chai_gfx::drawCanvas), "drawCanvas");
	chai.add(fun(&chai_gfx::hasReinit), "hasReinit");
	chai.add(fun(&chai_gfx::print), "print");
	// chai.add(fun(&chai_gfx::getShader), "getShader");
	chai.add(user_type<chai_shader>(), "chai_shader");
	chai.add(constructor<chai_shader(const chai_shader &)>(), "chai_shader");
	chai.add(fun(&chai_shader::operator=), "=");
	chai.add(fun(&chai_shader::newShader), "newShader");
	// Expose all send overloads - prefer direct types over Boxed_Value for performance
	chai.add(fun(static_cast<void (chai_shader::*)(const std::string&, float)>(&chai_shader::send)), "send");
	chai.add(fun(static_cast<void (chai_shader::*)(const std::string&, const glm::vec3&)>(&chai_shader::send)), "send");
	chai.add(fun(static_cast<void (chai_shader::*)(const std::string&, const std::vector<glm::vec3>&)>(&chai_shader::send)), "send");
	chai.add(fun(static_cast<int (chai_shader::*)(const std::string&, const std::vector<int>&)>(&chai_shader::send)), "send");
	chai.add(fun(static_cast<int (chai_shader::*)(const std::string&, const glm::mat4&)>(&chai_shader::send)), "send");
	chai.add(fun(static_cast<int (chai_shader::*)(const std::string&, const std::vector<glm::mat4>&)>(&chai_shader::send)), "send");
	chai.add(fun(static_cast<int (chai_shader::*)(const std::string&, const std::vector<chaiscript::Boxed_Value>&)>(&chai_shader::send)), "send");
	chai.add(fun(&chai_shader::sendInt), "sendInt");
	chai.add(user_type<chai_particles>(), "chai_particles");
	chai.add(constructor<chai_particles(const chai_particles &)>(), "chai_particles");
	chai.add(fun(&chai_particles::operator=), "=");	
	chai.add(fun(&chai_particles::clone), "clone");
	chai.add(fun(&chai_particles::newParticles), "newParticles");
	chai.add(fun(&chai_particles::setParticleSystem), "setParticleSystem");
	chai.add(fun(&chai_particles::updateParticleSystem), "updateParticleSystem");
	chai.add(fun(&chai_particles::setParameter), "setParameter");
	chai.add(fun(&chai_particles::setParent), "setParent");
	chai.add(fun(&chai_particles::setParticleDetails), "setParticleDetails");
	chai.add(fun(&chai_particles::draw), "draw");
	chai.add(user_type<chai_mesh>(), "chai_mesh");
	chai.add(constructor<chai_mesh(const chai_mesh &)>(), "chai_mesh");
	chai.add(fun(&chai_mesh::operator=), "=");	
	chai.add(fun(&chai_mesh::newMesh), "newMesh");
	chai.add(fun(&chai_mesh::newMeshWithData), "newMeshWithData");
	chai.add(fun(&chai_mesh::loadMeshFromFile), "loadMeshFromFile");
	chai.add(fun(&chai_mesh::wrap_setTexture), "setTexture");
	chai.add(fun(&chai_mesh::getCameraParams), "getCameraParams");
	chai.add(fun(&chai_mesh::getLightParams), "getLightParams");
	chai.add(fun(&chai_mesh::setLightParams), "setLightParams");
	chai.add(fun(&chai_mesh::playAnimation), "playAnimation");
	chai.add(fun(&chai_mesh::endAnimation), "endAnimation");
	chai.add(fun(&chai_mesh::stopAnimations), "stopAnimations");
	chai.add(fun(&chai_mesh::isAnimationPlaying), "isAnimationPlaying");
	chai.add(fun(&chai_mesh::getAnimationPercent), "getAnimationPercent");
	chai.add(fun(&chai_mesh::reloadMesh), "reloadMesh");
	chai.add(fun(&chai_mesh::destroy), "destroy");
	chai.add(fun(&chai_mesh::getId), "getId");
	chai.add(fun(&chai_mesh::getMeshBoundingBox), "getMeshBoundingBox");
	chai.add(fun(&chai_mesh::isVisible), "isVisible");
	chai.add(fun(&chai_mesh::update), "update");
	chai.add(user_type<chai_meshData>(), "chai_meshData");
	chai.add(fun(&chai_meshData::clone), "clone");
	
	
	static int drawCallCount = 0;
	auto chai_scene_draw_wrapper = [](chai_scene* scene, 
		const std::vector<float>& vm1,
		const std::vector<float>& vm2,
		const std::vector<float>& vm3,
		const std::vector<float>& vm4,
		int viewCount) {
		drawCallCount++;
		// if (drawCallCount % 60 == 0) {
		// 	std::printf("[chai_scene] draw called %d times (60 FPS check)\n", drawCallCount);
		// }
		auto convertToMat4 = [](const std::vector<float>& vec) -> glm::mat4 {
			glm::mat4 mat;
			float* ptr = glm::value_ptr(mat);
			for (int i = 0; i < 16 && i < vec.size(); i++) {
				ptr[i] = vec[i];
			}
			return mat;
		};
		scene->draw(convertToMat4(vm1), convertToMat4(vm2), convertToMat4(vm3), convertToMat4(vm4), viewCount);
	};
	
	chai.add(user_type<chai_scene>(), "chai_scene");
	chai.add(constructor<chai_scene(const chai_scene &)>(), "chai_scene");
	chai.add(fun(&chai_scene::operator=), "=");	
	chai.add(fun(&chai_scene::addMesh), "addMesh");
	chai.add(fun(&chai_scene::addParticleSystem), "addParticleSystem");
	chai.add(fun(&chai_scene::hideMesh), "hideMesh");
	chai.add(fun(&chai_scene::showMesh), "showMesh");
	chai.add(fun(&chai_scene::setShader), "setShader");
	chai.add(fun(&chai_scene::setMatrix), "setMatrix");	
	chai.add(fun(chai_scene_draw_wrapper), "draw");
	chai.add(fun(&chai_scene::newScene), "newScene");
	chai.add(fun(&chai_scene::destroy), "destroy");
	chai.add(fun(&chai_scene::prepareScreen), "prepareScreen");
	chai.add(fun(&chai_scene::update), "update");
	chai.add(fun(&chai_scene::removeChildMesh), "removeChildMesh");
	chai.add(fun(&chai_scene::addChildMesh), "addChildMesh");
	chai.add(fun(&chai_scene::getFramerate), "getFramerate");
	chai.add(user_type<chai_collisions>(), "chai_collisions");
	chai.add(constructor<chai_collisions(const chai_collisions &)>(), "chai_collisions");
	chai.add(fun(&chai_collisions::operator=), "=");
	chai.add(fun(&chai_collisions::test), "test");
	chai.add(fun(&chai_collisions::init), "init");
	chai.add(fun(&chai_collisions::destroy), "destroy");
	chai.add(fun(&chai_collisions::process), "process");
	chai.add(fun(&chai_collisions::addRigidMesh), "addRigidMesh");
	chai.add(fun(&chai_collisions::addCharacterController), "addCharacterController");
	chai.add(fun(&chai_collisions::applyForceToCharacter), "applyForceToCharacter");
	chai.add(fun(&chai_collisions::applyForceToRigidMesh), "applyForceToRigidMesh");
	chai.add(fun(static_cast<void (chai_collisions::*)(int, int, int, int, int, float, float, float, float, float, float, float, float, float, float, float, float, float)>(&chai_collisions::createVehicle)), "createVehicle");
	chai.add(fun(&chai_collisions::setCharacterControllerPosition), "setCharacterControllerPosition");
	chai.add(fun(&chai_collisions::setRigidMeshPosition), "setRigidMeshPosition");
	chai.add(fun(&chai_collisions::getCharacterController), "getCharacterController");
	chai.add(fun(&chai_collisions::getRigidMesh), "getRigidMesh");
	chai.add(fun(&chai_collisions::addBox), "addBox");	
	chai.add(fun(&chai_collisions::togglePhysics), "togglePhysics");
	chai.add(fun(&chai_collisions::portalCollide), "portalCollide");
	chai.add(fun(&chai_collisions::teleportCharacter), "teleportCharacter");
	chai.add(fun(&chai_collisions::teleportRigidMesh), "teleportRigidMesh");
	chai.add(fun(&chai_collisions::setVehicleControl), "setVehicleControls");
	chai.add(user_type<chai_gui>(), "chai_gui");
	chai.add(constructor<chai_gui(const chai_gui &)>(), "chai_gui");
	chai.add(fun(&chai_gui::operator=), "=");
	chai.add(fun(&chai_gui::addElement), "addElement");
	chai.add(fun(&chai_gui::draw), "draw");
	chai.add(fun(&chai_gui::setElementVisible), "setElementVisible");
	chai.add(fun(&chai_gui::setElementText), "setElementText");
	chai.add(fun(&chai_gui::writeConsole), "writeConsole");
	chai.add(fun(&chai_gui::executeConsole), "executeConsole");
	chai.add(user_type<chai_debug>(), "chai_debug");
	chai.add(constructor<chai_debug(const chai_debug &)>(), "chai_debug");
	chai.add(fun(&chai_debug::operator=), "=");
	chai.add(fun(&chai_debug::clone), "clone");
	chai.add(fun(&chai_debug::newDebug), "newDebug");
	chai.add(fun(&chai_debug::init), "init");
	chai.add(fun(&chai_debug::update), "update");
	chai.add(fun(&chai_debug::pushDebugMessage), "pushDebugMessage");
	chai.add(fun(&chai_debug::displayDebugMessages), "displayDebugMessages");
	chai.add(fun(&chai_debug::displayMatrix), "displayMatrix");
	chai.add(user_type<chai_editor>(), "chai_editor");
	chai.add(constructor<chai_editor(const chai_editor &)>(), "chai_editor");
	chai.add(fun(&chai_editor::isEditMode), "isEditMode");

	// Matrices - OPTIMIZED versions with thread-local storage reuse (ACTIVE - fixes memory leak)
	// chai.add(fun(&chai_matrices::setProjectionMatrixOptimized), "setProjectionMatrix");
	// chai.add(fun(&chai_matrices::setViewMatrixOptimized), "setViewMatrix");
	// chai.add(fun(&chai_matrices::setOrthographicMatrixOptimized), "setOrthographicMatrix");
	// chai.add(fun(&chai_matrices::setTransformationMatrixOptimized), "setTransformationMatrix");
	
	// Matrices - IN-PLACE versions (ChaiScript creates new vector each call, causing leak!)
	chai.add(fun(&chai_matrices::setProjectionMatrixInPlace), "setProjectionMatrix");
	chai.add(fun(&chai_matrices::setViewMatrixInPlace), "setViewMatrix");
	chai.add(fun(&chai_matrices::setOrthographicMatrixInPlace), "setOrthographicMatrix");
	chai.add(fun(&chai_matrices::setTransformationMatrixInPlace), "setTransformationMatrix");
	chai.add(fun(&chai_matrices::mat4ToVectorInPlace), "mat4ToVector");
	
	// Matrices - OLD return-by-value versions (kept for compatibility but cause memory leaks)
	// chai.add(fun<std::vector<float>, chai_matrices, const std::vector<float>&, const std::vector<float>&, const std::vector<float>&, const std::vector<float>&>(&chai_matrices::setTransformationMatrix), "setTransformationMatrix");
	// chai.add(fun(static_cast<std::vector<float> (chai_matrices::*)(const std::vector<float>&)>(&chai_matrices::setProjectionMatrix)), "setProjectionMatrix");
	// chai.add(fun(static_cast<std::vector<float> (chai_matrices::*)(float, float, float, float)>(&chai_matrices::setProjectionMatrix)), "setProjectionMatrix");
	// chai.add(fun(static_cast<std::vector<float> (chai_matrices::*)(const std::vector<float>&, const std::vector<float>&, const std::vector<float>&)>(&chai_matrices::setViewMatrix)), "setViewMatrix");
	// chai.add(fun(static_cast<std::vector<float> (chai_matrices::*)(const std::vector<float>&)>(&chai_matrices::setOrthographicMatrix)), "setOrthographicMatrix");
	// chai.add(fun(static_cast<std::vector<float> (chai_matrices::*)(float, float, float, float, float, float)>(&chai_matrices::setOrthographicMatrix)), "setOrthographicMatrix");
	// chai.add(fun(&chai_matrices::mat4ToVector), "mat4ToVector");
	chai.add(fun(&chai_matrices::vectorToMat4), "vectorToMat4");
	
	// Font
	// chai.add(fun(&font::isOpen), "isOpen");

	// Keyboard
	chai.add(fun(static_cast<bool (keyboard::*)(const std::string&)>(&keyboard::isDown)), "isDown");
	chai.add(fun(static_cast<bool (keyboard::*)(int)>(&keyboard::isDown)), "isDown");
	chai.add(fun(&keyboard::isScancodeDown), "isScancodeDown");
	chai.add(fun(&keyboard::getKeyFromScancode), "getKeyFromScancode");
	chai.add(fun(&keyboard::getScancodeFromKey), "getScancodeFromKey");

	// Event
	chai.add(fun(&event::quit), "quit");
	chai.add(fun(&event::pause), "pause");
	chai.add(fun(&event::isrenderlocked), "isrenderlocked");

	// Image
	chai.add(fun(&image::newImageData), "newImageData");

	// Filesystem
	chai.add(fun(&filesystem::unmount), "unmount");
	chai.add(fun(&filesystem::read), "read");
	chai.add(fun(&filesystem::isDirectory), "isDirectory");
	chai.add(fun(&filesystem::createDirectory), "createDirectory");
	chai.add(fun(&filesystem::isSymlink), "isSymlink");
	chai.add(fun(&filesystem::isFile), "isFile");
	chai.add(fun(&filesystem::write), "write");
	chai.add(fun(&filesystem::exists), "exists");
	chai.add(fun(&filesystem::getExecutablePath), "getExecutablePath");
	chai.add(fun(&filesystem::getSaveDirectory), "getSaveDirectory");
	chai.add(fun(&filesystem::getInfo), "getInfo");
	chai.add(fun(static_cast<FileData (filesystem::*)(const std::string&)>(&filesystem::newFileData)), "newFileData");
	chai.add(fun(static_cast<FileData (filesystem::*)(const std::string&, const std::string&)>(&filesystem::newFileData)), "newFileData");
	chai.add(fun(&filesystem::getDirectoryItems), "getDirectoryItems");
	chai.add(fun(static_cast<bool (filesystem::*)(const std::string&, const std::string&, bool)>(&filesystem::mount)), "mount");
	chai.add(fun(static_cast<bool (filesystem::*)(const std::string&, const std::string&)>(&filesystem::mount)), "mount");
	chai.add(fun(static_cast<int (filesystem::*)(const std::string&)>(&filesystem::getSize)), "getSize");
	chai.add(fun(static_cast<std::vector<std::string> (filesystem::*)(const std::string&)>(&filesystem::lines)), "lines");
	chai.add(fun(static_cast<std::vector<std::string> (filesystem::*)(const std::string&, const std::string&)>(&filesystem::lines)), "lines");
	chai.add(fun(&filesystem::load), "load");
	chai.add(fun(&script::require, this), "require");
	chai.add(fun(&filesystem::getFileExtension), "getFileExtension");
	chai.add(fun(&filesystem::getBasename), "getBasename");
	chai.add(fun(&filesystem::getParentDirectory), "getParentDirectory");

	// System
	chai.add(fun(&system::getOS), "getOS");
	chai.add(fun(&system::getVersion), "getVersion");
	chai.add(fun(&system::getVersionString), "getVersionString");
	chai.add(fun(&system::getUsername), "getUsername");
	chai.add(fun(&system::getClipboardText), "getClipboardText");
	chai.add(fun(&system::setClipboardText), "setClipboardText");
	chai.add(fun(&system::execute), "execute");

	// Mouse
	chai.add(fun(&mouse::getX), "getX");
	chai.add(fun(&mouse::getY), "getY");
	chai.add(fun(&mouse::getPosition), "getPosition");
	chai.add(fun(static_cast<bool (mouse::*)(const std::string&)>(&mouse::isDown)), "isDown");
	chai.add(fun(static_cast<bool (mouse::*)(int)>(&mouse::isDown)), "isDown");

	// Sound
	chai.add(fun(static_cast<SoundData* (sound::*)(const std::string&)>(&sound::newSoundData)), "newSoundData");
	// Audio
	chai.add(fun(&audio::play), "play");
	chai.add(fun(static_cast<SoundData* (audio::*)(const std::string&, const std::string&)>(&audio::newSource)), "newSource");
	chai.add(fun(static_cast<SoundData* (audio::*)(const std::string&)>(&audio::newSource)), "newSource");
	chai.add(fun(&audio::getVolume), "getVolume");
	chai.add(fun(&audio::setVolume), "setVolume");

	// Window
	chai.add(fun(&window::setTitle), "setTitle");
	chai.add(fun(&window::getTitle), "getTitle");
	chai.add(fun(static_cast<love::window& (window::*)(const std::string&, int)>(&window::showMessageBox)), "showMessageBox");
	chai.add(fun(static_cast<love::window& (window::*)(const std::string&)>(&window::showMessageBox)), "showMessageBox");

	// Timer
	chai.add(fun(&timer::getDelta), "getDelta");
	chai.add(fun(&timer::getFPS), "getFPS");
	chai.add(fun(&timer::step), "step");
	chai.add(fun(&timer::getTime), "getTime");

	// Joystick
	chai.add(fun(&joystick::getJoysticks), "getJoysticks");
	chai.add(fun(&joystick::getJoystickCount), "getJoystickCount");
	chai.add(fun(static_cast<bool (joystick::*)(int, const std::string&)>(&joystick::isDown)), "isDown");
	chai.add(fun(static_cast<bool (joystick::*)(int, int)>(&joystick::isDown)), "isDown");
	chai.add(fun(static_cast<float (joystick::*)(int, int)>(&joystick::getAxis)), "getAxis");
	chai.add(fun(static_cast<float (joystick::*)(int, const std::string&)>(&joystick::getAxis)), "getAxis");
	chai.add(fun(&joystick::operator[]), "[]");

	// Math
	auto mathlib = chaiscript::extras::math::bootstrap();
	chai.add(mathlib);
	chai.add(fun(&math::pi), "pi");
	chai.add(fun(&math::e), "e");
	chai.add(fun(&math::rad), "rad");
	chai.add(fun(&math::degrees), "degrees");
	chai.add(fun(static_cast<float (math::*)()>(&math::random)), "random");
	chai.add(fun(static_cast<float (math::*)(float)>(&math::random)), "random");
	chai.add(fun(static_cast<float (math::*)(float, float)>(&math::random)), "random");
	chai.add(fun(static_cast<int (math::*)(int)>(&math::random)), "random");
	chai.add(fun(static_cast<int (math::*)(int, int)>(&math::random)), "random");
	chai.add(fun(static_cast<double (math::*)(double)>(&math::random)), "random");
	chai.add(fun(static_cast<double (math::*)(double, double)>(&math::random)), "random");
	chai.add(fun(static_cast<math& (math::*)(int, int)>(&math::setRandomSeed)), "setRandomSeed");
	chai.add(fun(static_cast<math& (math::*)(int)>(&math::setRandomSeed)), "setRandomSeed");
	chai.add(fun(&math::getRandomSeed), "getRandomSeed");

	// Data
	chai.add(fun(static_cast<std::string (data::*)(const std::string&)>(&data::compress)), "compress");
	chai.add(fun(static_cast<std::string (data::*)(const std::string&, int)>(&data::compress)), "compress");
	chai.add(fun(&data::decompress), "decompress");
	chai.add(fun(&data::hash), "hash");
	chai.add(fun(&data::encode), "encode");
	chai.add(fun(&data::decode), "decode");

	// Ensure the love namespace is imported and ready.
	chai.import("love");

	// Load the main.chai file.
	require("conf");

	std::string extension(app->filesystem.getFileExtension(file));
	if (extension == "chailove" || extension == "chaigame") {
		mainLoaded = require("main");
	} else {
		// Otherwise, load the actual file.
		std::string filename(app->filesystem.getBasename(file));
		mainLoaded = require(filename);
	}

	// Find the game functions.
	try {
		chaiload = chai.eval<std::function<void()> >("load");
	}
	catch (const std::exception& e) {
		// LibretroLog::log(RETRO_LOG_ERROR) << "[ChaiLove] [script] load() " << e.what() << std::endl;
		hasload = false;
	}
	try {
		chaireset = chai.eval<std::function<void()> >("reset");
	}
	catch (const std::exception& e) {
		// LibretroLog::log(RETRO_LOG_INFO) << "[ChaiLove] [script] reset() " << e.what() << std::endl;
		hasreset = false;
	}
	try {
		chaiupdate = chai.eval<std::function<void(float)> >("update");
	}
	catch (const std::exception& e) {
		// LibretroLog::log(RETRO_LOG_INFO) << "[ChaiLove] [script] update(delta) " << e.what() << std::endl;
		hasUpdate = false;
	}
	try {
		chaiconf = chai.eval<std::function<void(config&)> >("conf");
	}
	catch (const std::exception& e) {
		// LibretroLog::log(RETRO_LOG_INFO) << "[ChaiLove] [script] conf(t) " << e.what() << std::endl;
		hasconf = false;
	}
	try {
		chaidraw = chai.eval<std::function<void()> >("draw");
	}
	catch (const std::exception& e) {
		// LibretroLog::log(RETRO_LOG_INFO) << "[ChaiLove] [script] draw() " << e.what() << std::endl;
		hasDraw = false;
	}
	try {
		chaijoystickpressed = chai.eval<std::function<void(int, const std::string&)> >("joystickpressed");
	}
	catch (const std::exception& e) {
		// LibretroLog::log(RETRO_LOG_INFO) << "[ChaiLove] [script] joystickpressed() " << e.what() << std::endl;
		hasjoystickpressed = false;
	}
	try {
		chaijoystickreleased = chai.eval<std::function<void(int, const std::string&)> >("joystickreleased");
	}
	catch (const std::exception& e) {
		// LibretroLog::log(RETRO_LOG_INFO) << "[ChaiLove] [script] joystickreleased() " << e.what() << std::endl;
		hasjoystickreleased = false;
	}
	try {
		chaimousepressed = chai.eval<std::function<void(int, int, const std::string&)> >("mousepressed");
	}
	catch (const std::exception& e) {
		// LibretroLog::log(RETRO_LOG_INFO) << "[ChaiLove] [script] mousepressed() " << e.what() << std::endl;
		hasmousepressed = false;
	}
	try {
		chaimousereleased = chai.eval<std::function<void(int, int, const std::string&)> >("mousereleased");
	}
	catch (const std::exception& e) {
		// LibretroLog::log(RETRO_LOG_INFO) << "[ChaiLove] [script] mousereleased() " << e.what() << std::endl;
		hasmousereleased = false;
	}
	try {
		chaimousemoved = chai.eval<std::function<void(int, int, int, int)> >("mousemoved");
	}
	catch (const std::exception& e) {
		// LibretroLog::log(RETRO_LOG_INFO) << "[ChaiLove] [script] mousemoved() " << e.what() << std::endl;
		hasmousemoved = false;
	}
	try {
		chaikeypressed = chai.eval<std::function<void(const std::string&, int)> >("keypressed");
	}
	catch (const std::exception& e) {
		// LibretroLog::log(RETRO_LOG_INFO) << "[ChaiLove] [script] keypressed() " << e.what() << std::endl;
		haskeypressed = false;
	}
	try {
		chaikeyreleased = chai.eval<std::function<void(const std::string&, int)> >("keyreleased");
	}
	catch (const std::exception& e) {
		// LibretroLog::log(RETRO_LOG_INFO) << "[ChaiLove] [script] keyreleased() " << e.what() << std::endl;
		haskeyreleased = false;
	}
	try {
		chailoadstate = chai.eval<std::function<bool(const std::string&)> >("loadstate");
	}
	catch (const std::exception& e) {
		// LibretroLog::log(RETRO_LOG_INFO) << "[ChaiLove] [script] loadstate() " << e.what() << std::endl;
		hasloadstate = false;
	}
	try {
		chaisavestate = chai.eval<std::function<std::string()> >("savestate");
	}
	catch (const std::exception& e) {
		// LibretroLog::log(RETRO_LOG_INFO) << "[ChaiLove] [script] savestate() " << e.what() << std::endl;
		hassavestate = false;
	}
	try {
		chaicheatreset = chai.eval<std::function<void()> >("cheatreset");
	}
	catch (const std::exception& e) {
		// LibretroLog::log(RETRO_LOG_INFO) << "[ChaiLove] [script] cheatreset() Warning: " << e.what() << std::endl;
		hascheatreset = false;
	}
	try {
		chaicheatset = chai.eval<std::function<void(int, bool, const std::string&)> >("cheatset");
	}
	catch (const std::exception& e) {
		// LibretroLog::log(RETRO_LOG_INFO) << "[ChaiLove] [script] cheatset() Warning: " << e.what() << std::endl;
		hascheatset = false;
	}
	try {
		chaiexit = chai.eval<std::function<void()> >("exit");
	}
	catch (const std::exception& e) {
		// LibretroLog::log(RETRO_LOG_INFO) << "[ChaiLove] [script] exit() Warning: " << e.what() << std::endl;
		hasexit = false;
	}
	#endif
}

void script::conf(config& t) {
	#ifdef __HAVE_CHAISCRIPT__
	if (hasconf) {
		try {
			chaiconf(t);
		}
		catch (const std::exception& e) {
			LibretroLog::log(RETRO_LOG_ERROR) << "[ChaiLove] [script] Failed to invoke conf(t): " << e.what() << std::endl;
			hasconf = false;
		}
	}
	#endif
}

void script::load() {
	#ifdef __HAVE_CHAISCRIPT__
	if (hasload) {
		try {
			chaiload();
		}
		catch (const std::exception& e) {
			LibretroLog::log(RETRO_LOG_ERROR) << "[ChaiLove] [script] Failed to call load(): " << e.what() << std::endl;
			hasload = false;
		}
	}
	#endif
}

void script::reset() {
	#ifdef __HAVE_CHAISCRIPT__
	if (hasreset) {
		try {
			chaireset();
		}
		catch (const std::exception& e) {
			LibretroLog::log(RETRO_LOG_ERROR) << "[ChaiLove] [script] Failed to call reset(): " << e.what() << std::endl;
			hasreset = false;
		}
	}
	#endif
}

void script::update(float delta) {
	#ifdef __HAVE_CHAISCRIPT__
	if (hasUpdate) {
		try {			
			chaiupdate(delta);
			
			// Note: ChaiScript internal memory cannot be easily cleaned up
			// The RAM leak is likely from Boxed_Value wrappers that accumulate
			// inside ChaiScript's internal state. This is a known limitation.
		}
		catch (const std::exception& e) {
			hasUpdate = false;
			LibretroLog::log(RETRO_LOG_ERROR) << "[ChaiLove] [script] Failed to call update(delta): " << e.what() << std::endl;
		}
	}
	#endif
}

void script::draw() {
	#ifdef __HAVE_CHAISCRIPT__
	if (hasDraw) {
		try {			
			chaidraw();
		}
		catch (const std::exception& e) {
			LibretroLog::log(RETRO_LOG_ERROR) << "[ChaiLove] [script] Failed to call draw(): " << e.what() << std::endl;

			void* stack[100];
			WORD frames = RtlCaptureStackBackTrace(0, 100, stack, NULL);
			HANDLE process = GetCurrentProcess();
			SymInitialize(process, NULL, TRUE);
			char symbolBuffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
			SYMBOL_INFO* symbol = reinterpret_cast<SYMBOL_INFO*>(symbolBuffer);
			symbol->MaxNameLen = MAX_SYM_NAME;
			symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
			IMAGEHLP_LINE64 line = {};
			line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
			DWORD displacement = 0;
			for (WORD i = 0; i < frames; i++) {
				DWORD64 address = reinterpret_cast<DWORD64>(stack[i]);
				if (SymFromAddr(process, address, nullptr, symbol)) {
					if (SymGetLineFromAddr64(process, address, &displacement, &line)) {
						LibretroLog::log(RETRO_LOG_ERROR) << "  Frame " << i << ": " << symbol->Name << " (" << line.FileName << ":" << line.LineNumber << ")" << std::endl;
					} else {
						LibretroLog::log(RETRO_LOG_ERROR) << "  Frame " << i << ": " << symbol->Name << " (0x" << std::hex << address << std::dec << ")" << std::endl;
					}
				} else {
					LibretroLog::log(RETRO_LOG_ERROR) << "  Frame " << i << ": 0x" << std::hex << address << std::dec << std::endl;
				}
			}

			hasDraw = false;
		}
	} else {
		ChaiLove::getInstance()->graphics.print("ChaiLove: def draw() not found.", 100, 100);
	}
	
	// Draw physics debug geometry AFTER user draw code but BEFORE render pass ends
	#ifdef JPH_DEBUG_RENDERER
	#if ENABLE_DEBUG_GEOMETRY_RENDERING
	auto app = ChaiLove::getInstance();
	try {
		// Get line vertices from the default world group (0)
		auto lineVertices = app->chai_collisions.getDebugRendererLineVertices(0);
		size_t lineCount = app->chai_collisions.getDebugRendererLineCount(0);
		
		// Draw line endpoints as points (stays in same render pass)
		if (lineCount > 0 && lineVertices.size() >= lineCount * 14) {
			auto vulkanGraphics = dynamic_cast<love::gfx::vulkan::Graphics*>(app->chai_gfx.instance);
			if (vulkanGraphics) {
				// Set larger point size so they're visible
				vulkanGraphics->setPointSize(3.0f);
				
				// Collect all point positions and colors
				std::vector<love::Vector2> positions;
				std::vector<love::Colorf> colors;
				positions.reserve(lineCount * 2);
				colors.reserve(lineCount * 2);
				
				for (size_t i = 0; i < lineCount; i++) {
					size_t idx = i * 14;
					
					// First vertex
					positions.push_back(love::Vector2(lineVertices[idx + 0], lineVertices[idx + 1]));
					colors.push_back(love::Colorf(lineVertices[idx + 3], lineVertices[idx + 4], 
					                               lineVertices[idx + 5], lineVertices[idx + 6]));
					
					// Second vertex
					positions.push_back(love::Vector2(lineVertices[idx + 7], lineVertices[idx + 8]));
					colors.push_back(love::Colorf(lineVertices[idx + 10], lineVertices[idx + 11], 
					                               lineVertices[idx + 12], lineVertices[idx + 13]));
				}
				
				// Draw all points in one call
				if (!positions.empty()) {
					vulkanGraphics->points(positions.data(), colors.data(), positions.size());
				}
			}
		}
		
		// Clear debug geometry for next frame
		app->chai_collisions.clearDebugRendererGeometry(0);
	} catch (const std::exception& e) {
		// Silently ignore errors to avoid spamming logs
	}
	#endif // ENABLE_DEBUG_GEOMETRY_RENDERING
	#endif // JPH_DEBUG_RENDERER
	#endif
}

void script::joystickpressed(int joystick, const std::string& button) {
	#ifdef __HAVE_CHAISCRIPT__
	if (hasjoystickpressed) {
		try {
			chaijoystickpressed(joystick, button);
		}
		catch (const std::exception& e) {
			LibretroLog::log(RETRO_LOG_ERROR) << "[ChaiLove] [script] Failed to call joystickpressed(): " << e.what() << std::endl;
			hasjoystickpressed = false;
		}
	}
	#endif
}

void script::joystickreleased(int joystick, const std::string& button) {
	#ifdef __HAVE_CHAISCRIPT__
	if (hasjoystickreleased) {
		try {
			chaijoystickreleased(joystick, button);
		}
		catch (const std::exception& e) {
			LibretroLog::log(RETRO_LOG_ERROR) << "[ChaiLove] [script] Failed to call joystickreleased(): " << e.what() << std::endl;
			hasjoystickreleased = false;
		}
	}
	#endif
}

void script::mousepressed(int x, int y, const std::string& button) {
	#ifdef __HAVE_CHAISCRIPT__
	if (hasmousepressed) {
		try {
			chaimousepressed(x, y, button);
		}
		catch (const std::exception& e) {
			LibretroLog::log(RETRO_LOG_ERROR) << "[ChaiLove] [script] Failed to call mousepressed(): " << e.what() << std::endl;
			hasmousepressed = false;
		}
	}
	#endif
}

void script::mousereleased(int x, int y, const std::string& button) {
	#ifdef __HAVE_CHAISCRIPT__
	if (hasmousereleased) {
		try {
			chaimousereleased(x, y, button);
		}
		catch (const std::exception& e) {
			LibretroLog::log(RETRO_LOG_ERROR) << "[ChaiLove] [script] Failed to call mousereleased(): " << e.what() << std::endl;
			hasmousereleased = false;
		}
	}
	#endif
}

void script::mousemoved(int x, int y, int dx, int dy) {
	#ifdef __HAVE_CHAISCRIPT__
	if (hasmousemoved) {
		try {
			chaimousemoved(x, y, dx, dy);
		}
		catch (const std::exception& e) {
			LibretroLog::log(RETRO_LOG_ERROR) << "[ChaiLove] [script] Failed to call mousemoved(): " << e.what() << std::endl;
			hasmousemoved = false;
		}
	}
	#endif
}

void script::keypressed(const std::string& key, int scancode) {
	#ifdef __HAVE_CHAISCRIPT__
	if (haskeypressed) {
		try {
			chaikeypressed(key, scancode);
		}
		catch (const std::exception& e) {
			LibretroLog::log(RETRO_LOG_ERROR) << "[ChaiLove] [script] Failed to call keypressed(): " << e.what() << std::endl;
			haskeypressed = false;
		}
	}
	#endif
}

void script::keyreleased(const std::string& key, int scancode) {
	#ifdef __HAVE_CHAISCRIPT__
	if (haskeyreleased) {
		try {
			chaikeyreleased(key, scancode);
		}
		catch (const std::exception& e) {
			LibretroLog::log(RETRO_LOG_ERROR) << "[ChaiLove] [script] Failed to call keyreleased(): " << e.what() << std::endl;
			haskeyreleased = false;
		}
	}
	#endif
}

/**
 * Call the script's savestate() function and retrieve a string.
 */
std::string script::savestate() {
	#ifdef __HAVE_CHAISCRIPT__
	if (hassavestate) {
		try {
			return chaisavestate();
		}
		catch (const std::exception& e) {
			LibretroLog::log(RETRO_LOG_ERROR) << "[ChaiLove] [script] Failed to call savestate(): " << e.what() << std::endl;
			hassavestate = false;
		}
	}
	#endif

	// If there is no state data, return an empty string.
	return std::string("");
}

/**
 * Pass the state data string over to the script, asking it to load the state.
 */
bool script::loadstate(const std::string& data) {
	#ifdef __HAVE_CHAISCRIPT__
	if (hasloadstate) {
		try {
			return chailoadstate(data);
		}
		catch (const std::exception& e) {
			LibretroLog::log(RETRO_LOG_ERROR) << "[ChaiLove] [script] Failed to call loadstate(): " << e.what() << std::endl;
			hasloadstate = false;
		}
	}
	#endif

	// If there is an error in loading the state, return false.
	return false;
}

void script::cheatreset() {
	#ifdef __HAVE_CHAISCRIPT__
	if (hascheatreset) {
		try {
			chaicheatreset();
		}
		catch (const std::exception& e) {
			LibretroLog::log(RETRO_LOG_ERROR) << "[ChaiLove] [script] Failed to call cheatreset(): " << e.what() << std::endl;
			hascheatreset = false;
		}
	}
	#endif
}

void script::cheatset(int index, bool enabled, const std::string& code) {
	#ifdef __HAVE_CHAISCRIPT__
	if (hascheatreset) {
		try {
			chaicheatset(index, enabled, code);
		}
		catch (const std::exception& e) {
			LibretroLog::log(RETRO_LOG_ERROR) << "[ChaiLove] [script] Failed to call cheatset(): " << e.what() << std::endl;
			hascheatset = false;
		}
	}
	#endif
}

void script::exit() {
	#ifdef __HAVE_CHAISCRIPT__
	if (hasexit) {
		try {
			chaiexit();
		}
		catch (const std::exception& e) {
			LibretroLog::log(RETRO_LOG_ERROR) << "[ChaiLove] [script] Failed to call exit(): " << e.what() << std::endl;
			hasexit = false;
		}
	}
	#endif
}

/**
 * Replaces all instances of a string in a given subject string.
 */
std::string script::replaceString(std::string subject, const std::string& search, const std::string& replace) {
	size_t pos = 0;
	while ((pos = subject.find(search, pos)) != std::string::npos) {
		 subject.replace(pos, search.length(), replace);
		 pos += replace.length();
	}
	return subject;
}

}  // namespace love
