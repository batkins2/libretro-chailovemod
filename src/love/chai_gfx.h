#define __HAVE_CHAI_GFX__

#include <iostream>

#include "opengl/Graphics.h"
#include "gfx.h"
#ifndef __HAVE_CHAI_MESH__
#include "chai_mesh.h"
#endif
#ifndef __HAVE_CHAI_SHADER__
#include "chai_shader.h"
#endif
#include "window/Window.h"
#include "window/sdl/Window.h"
#include <vector>


#ifdef __HAVE_CHAISCRIPT__
#include <chaiscript/chaiscript.hpp>
#endif
namespace love
{
// class Graphics;

// auto instance = Graphics::createInstance();

class chai_gfx {
    public:
    chai_gfx();
    ~chai_gfx();
    bool init();
    chai_shader *wrap_newShader(const std::string *FileName);
    void wrap_setShader(chai_shader *s);
    chai_mesh *wrap_newMesh(const std::vector<chaiscript::Boxed_Value> &vertexFormat, const std::vector<chaiscript::Boxed_Value> &data, const std::string &type);
    void draw(chai_mesh *m);
    void createCanvas();
    void drawCanvas();
    love::gfx::Graphics *instance; // = gfx::Graphics::createInstance();
    love::windowmod::Window *win;
    love::gfx::Texture *canvas;
    // imagemod::Image *i = NULL;
    // filesystem *fs = NULL;
};
}
