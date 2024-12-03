#include "chai_gfx.h"
#include <sstream>
// #include "filesystem/Filesystem.h"
#include "../ChaiLove.h"
#include "filesystem.h"
namespace love
{

chai_gfx::chai_gfx() {
    instance = new gfx::opengl::Graphics();
    win = new windowmod::sdl::Window();
    win->setGraphics(instance);
    auto winset = new windowmod::WindowSettings();
    winset->displayindex = 0;
    win->setWindow(800, 600, winset);
}

chai_gfx::~chai_gfx() {
    // delete i;
    // delete fs;
}

bool chai_gfx::init() {
    // instance = new gfx::opengl::Graphics();
    // love::window::WindowSettings *ws;
    // ws->fullscreen = true;
    // win = new windowmod::sdl::Window();
    // win->setGraphics(instance);
    // win->setWindow();
    // auto cl = ChaiLove::getInstance();
    // cl->win = (SDL_Window *) win->getHandle();
    // cl->videoBuffer = (uint32_t *) cl->win;
    return true;
}

chai_shader *chai_gfx::wrap_newShader(const std::string *FileName) {
    // delete win;
    // delete instance;
    // init();
    if (instance->isCreated()) {
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
                    lines.push_back(line);

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
                    lines.push_back(shaderFunc);
                    shaderFunc = "";
                }
            } else {
                int delim = line.find_last_of(";");
                if (delim > 0 && delim < line.length()) {
                    std::string trimmed = line.substr(0, delim+1);
                    lines.push_back(trimmed);
                    // delim = trimmed.find_last_of(" ");
                    // std::string last = trimmed.substr(0, delim);
                    // std::string first = trimmed.substr(delim+1,trimmed.length()-1);
                    // options.defines.emplace(first, last);
                }
            }
        }

        std::string a;
        for (const auto &piece : lines) a += piece+'\n';

        std::vector<std::string> code;
        code.push_back(a);

        auto shader = new chai_shader();
        shader->newShader(instance, code, options);
        return shader;
        // return instance->newShader(lines, options);
    }
    return nullptr;
}

void chai_gfx::wrap_setShader(chai_shader *s) {
    if (instance->isCreated()) {
        instance->setShader(s->shader);
    }
}

chai_mesh *chai_gfx::wrap_newMesh(const std::vector<chaiscript::Boxed_Value> &vertexFormat, const std::vector<chaiscript::Boxed_Value> &data, const std::string &type) {
    if (instance->isCreated()) {
        auto m = new chai_mesh();
        m->newMesh(instance, vertexFormat, data, type);
        return m;
    }
    return nullptr;
}

void chai_gfx::createCanvas() {
    gfx::Texture::Settings settings;
    settings.width = 800;
    settings.height = 600;
    settings.renderTarget = true;
    settings.format = PIXELFORMAT_RGBA8_UINT;
    auto slices = new gfx::Texture::Slices(gfx::TextureType::TEXTURE_2D);
    canvas = instance->newTexture(settings, slices);
}

void chai_gfx::draw(chai_mesh *m) {
    if (instance->isCreated()) {
        auto matrix = new Matrix4();
        // auto tex = m->mesh->getTexture();

        instance->setRenderTarget(canvas, 0);
        instance->draw(m->mesh, *matrix);
        instance->setRenderTarget();
        // instance->draw(canvas, *matrix);

        instance->setShader();
    }
}

void chai_gfx::drawCanvas() {
    auto img = new imagemod::ImageData(canvas->getWidth(), canvas->getHeight(), PIXELFORMAT_RGBA8_UNORM);

    Rect rect = Rect();
    rect.w = canvas->getWidth();
    rect.h = canvas->getHeight();
    instance->readbackTexture(canvas, 0, 0, rect, img, 0, 0);
    auto d = img->getData();
    auto cl = ChaiLove::getInstance();
    auto surf = SDL_CreateRGBSurfaceFrom(d, canvas->getWidth(), canvas->getHeight(), 32, 4*canvas->getWidth(), 0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF);
    SDL_Rect dstrect;
    dstrect.x = 0;
    dstrect.y = 0;
    SDL_BlitSurface(surf, NULL, cl->screen, &dstrect);

}

void readFBOIntoVideoBuffer() {
    GLuint fbo = getInternalBackbufferFBO(); // or getSystemBackbufferFBO()
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    int width = 800; // Set appropriate width
    int height = 600; // Set appropriate height
    std::vector<uint8_t> buffer(width * height * 4); // Assuming RGBA format

    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, buffer.data());

    // Copy to video buffer
    auto cl = ChaiLove::getInstance();
    memcpy(cl->videoBuffer, buffer.data(), buffer.size());

    glBindFramebuffer(GL_FRAMEBUFFER, 0); // Unbind the FBO
}

}
