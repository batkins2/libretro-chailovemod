#include "chai_gfx.h"
#include <sstream>
#include "filesystem/Filesystem.h"
#include "opengl/Graphics.h"
#include "gfx.h"

namespace love
{
#define instance() (Module::getInstance<gfx::Graphics>(Module::M_GRAPHICS))

chai_gfx::chai_gfx() {
    instance()->createInstance();
}

chai_gfx& chai_gfx::wrap_newShader(const std::string *FileName) {
    // 
    if (instance()->isCreated()) {
        auto file = Module::getInstance<filesystemmod::Filesystem>(Module::M_FILESYSTEM);
        auto fn = FileName->c_str();
        auto fd = file->read(fn);
        const char *d = (const char *)fd->getData();
        std::string data = d;
        std::stringstream s(data);
        std::vector<std::string> lines;
        std::string line;
        while (std::getline(s, line)) {
            lines.push_back(line);
        }
        love::gfx::Shader::CompileOptions options;
        instance()->newShader(lines, options);
    }
    return *this;
}

}
