#include "chai_gfx.h"
#include <sstream>
// #include "filesystem/Filesystem.h"
#include "filesystem.h"
#include "window/Window.h"
namespace love
{
#define instance() (Module::getInstance<gfx::Graphics>(Module::M_GRAPHICS))
#define window() (Module::getInstance<window::Window>(Module::M_WINDOW))

chai_gfx::chai_gfx() {
    // instance = instance()->createInstance();
    love::window::WindowSettings ws;
    ws.fullscreen = true;
    window()->setWindow(800, 600, &ws);
}

bool chai_gfx::wrap_newShader(const std::string *FileName) {
    if (instance()->isCreated()) {
        auto file = new filesystem();
        std::string data = file->read(FileName->c_str());
        // auto file = Module::getInstance<filesystemmod::Filesystem>(Module::M_FILESYSTEM);
        // auto fn = FileName->c_str();
        // auto fd = file->read(fn);
        // const char *d = (const char *)fd->getData();
        // std::string data = d;
        std::stringstream s(data);
        std::vector<std::string> lines;
        std::string shaderFunc = "";
        std::string line;
        love::gfx::Shader::CompileOptions options;
        bool shaderFound = false;
        while (std::getline(s, line)) {
            if (!line.length() || (line[0] == '/' && line[1] == '/')) {
                continue;
            }

            if (shaderFound || strstr(line.c_str(), "vec4 position(") != NULL) {
                if (!shaderFound) {
                    shaderFunc.append(line);
                    shaderFound = true;
                } else {
                    int delim = line.find_last_of(";");
                    if (delim > 0 && delim < line.length()) {
                        std::string trimmed = line.substr(0, delim+1);
                        shaderFunc.append(trimmed);
                    }
                    delim = line.find_last_of("{");
                    if (delim > 0 && delim < line.length()) {
                        std::string trimmed = line.substr(0, delim+1);
                        shaderFunc.append(trimmed);
                    }
                    delim = line.find_last_of("}");
                    if (delim >= 0 && delim < line.length()) {
                        std::string trimmed = line.substr(0, delim+1);
                        shaderFunc.append(trimmed);
                    }
                }
            } else {
                int delim = line.find_last_of(";");
                if (delim > 0 && delim < line.length()) {
                    std::string trimmed = line.substr(0, delim);
                    delim = trimmed.find_last_of(" ");
                    std::string last = trimmed.substr(0, delim);
                    std::string first = trimmed.substr(delim+1,trimmed.length()-1);
                    options.defines.emplace(first, last);
                }
            }
        }
        lines.push_back(shaderFunc);
        instance()->newShader(lines, options);
        return true;
    }
    return false;
}

}
