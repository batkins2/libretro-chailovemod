#include "chai_gfx.h"
#include <sstream>
// #include "filesystem/Filesystem.h"
#include "filesystem.h"
namespace love
{
#define instance() (Module::getInstance<gfx::Graphics>(Module::M_GRAPHICS))

chai_gfx::chai_gfx() {
    instance = instance()->createInstance();
}

bool chai_gfx::wrap_newShader(const std::string *FileName) {
    // instance->isCreated()
    if (true) {
        auto file = new filesystem();
        std::string data = file->read(FileName->c_str());
        // auto file = Module::getInstance<filesystemmod::Filesystem>(Module::M_FILESYSTEM);
        // auto fn = FileName->c_str();
        // auto fd = file->read(fn);
        // const char *d = (const char *)fd->getData();
        // std::string data = d;
        std::stringstream s(data);
        std::vector<std::string> lines;
        std::string line;
        love::gfx::Shader::CompileOptions options;
        bool shaderFound = false;
        while (std::getline(s, line)) {
            if (!line.length() || (line[0] == '/' && line[1] == '/')) {
                continue;
            }

            int delim = line.find_last_of(" //");
            if (delim) {
                line.erase(delim, line.length() - 1);
            }

            if (shaderFound || strstr(line.c_str(), "vec4 position(") != NULL) {
                lines.push_back(line);
                shaderFound = true;
            } else {
                delim = line.find_last_of(' ');
                std::string first = line.substr(0, delim-1);
                std::string last = line.substr(delim+1,line.size()-1);
                options.defines.emplace(first, last);
            }
        }
        instance->newShader(lines, options);
        return true;
    }
    return false;
}

}
