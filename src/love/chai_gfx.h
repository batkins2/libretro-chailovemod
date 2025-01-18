#define __HAVE_CHAI_GFX__

#include <iostream>

#include "opengl/Graphics.h"
#include "gfx.h"

#ifndef __HAVE_CHAI_SHADER__
#include "chai_shader.h"
#endif
#ifndef __HAVE_CHAI_MESH__
#include "chai_mesh.h"
#endif
#include "window/Window.h"
#include "window/sdl/Window.h"
#include <vector>
#include "scene_mesh.h"


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
    bool destroy();
    chai_shader *wrap_newShader(const std::string *FileName);
    void wrap_setShader(chai_shader *s);
    chai_mesh *wrap_newMesh(const std::vector<chaiscript::Boxed_Value> &vertexFormat, const std::vector<chaiscript::Boxed_Value> &data, const std::string &type);
    chai_mesh *wrap_newMeshFromFile(const std::vector<chaiscript::Boxed_Value> &vertexFormat, const std::string *FileName, const std::string &type);
    void draw(chai_mesh *m);
    void drawScene();
    void createCanvas();
    void drawCanvas();
    love::gfx::Graphics *instance; // = gfx::Graphics::createInstance();
    love::windowmod::Window *win;
    love::gfx::Texture *canvas;
    inline static struct retro_hw_render_callback hw_render;
    unsigned int FRAMEBUFFER;
    unsigned int COLORATTACH;
    chai_shader *shader;
    std::vector<chai_mesh *> meshes;
    int width = 1440;
    int height = 1080;
    SceneMesh *scene = nullptr;
};
}
