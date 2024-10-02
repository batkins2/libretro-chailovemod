#include "gfx.h"
#include "shader.h"
#include "common/Module.h"
#include "filesystem/Filesystem.h"


namespace love
{

class Module;
class Filesystemmod;
class File;
class Shader;

namespace gfx
{
    love::gfx::graphics *instance = (Module::getInstance<love::gfx::graphics>(Module::M_GRAPHICS));
    Shader *wrap_newShader(const char *FileName);
}
}