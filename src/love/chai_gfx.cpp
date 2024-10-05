#include "chai_gfx.h"
#include <sstream>
#include "filesystem/Filesystem.h"
#include "opengl/Graphics.h"
namespace love
{
namespace gfx
{
// #define instance() (Module::getInstance<gfx::opengl::Graphics>(Module::M_GRAPHICS))

chai_gfx::chai_gfx() {
    instance = Graphics::createInstance();
}

chai_gfx& chai_gfx::wrap_newShader(const std::string *FileName) {
    if (instance->isCreated()) {
        auto file = Module::getInstance<filesystemmod::Filesystem>(Module::M_FILESYSTEM);
        std::string data = (const char *) file->read(FileName->c_str())->getData();
        std::stringstream s(data);
        std::vector<std::string> lines;
        std::string line;
        while (std::getline(s, line)) {
            lines.push_back(line);
        }
        love::gfx::Shader::CompileOptions options;
        instance->newShader(lines, options);
    }
    return *this;
}

void load() {
}

}
}