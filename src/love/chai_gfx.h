
#include <iostream>

#include "opengl/Graphics.h"
#include "gfx.h"
namespace love
{
// class Graphics;

// auto instance = Graphics::createInstance();

class chai_gfx {
    public:
    chai_gfx();
    bool wrap_newShader(const std::string *FileName);
    love::gfx::Graphics *instance; // = gfx::Graphics::createInstance();
};
}
