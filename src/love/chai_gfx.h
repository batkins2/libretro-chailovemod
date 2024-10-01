#pragma once

#include "gfx.h"
#include "shader.h"

namespace love
{
namespace gfx
{
    love::gfx::graphics *instance = (Module::getInstance<love::gfx::graphics>(Module::M_GRAPHICS));
    Shader *wrap_newShader(const char *FileName);
}
}