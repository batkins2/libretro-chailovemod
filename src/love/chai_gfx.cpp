#include "chai_gfx.h"
#include <sstream>
// #include "filesystem/Filesystem.h"
#include "../ChaiLove.h"
#include "filesystem.h"
namespace love
{

chai_gfx::chai_gfx() {
    // instance = new gfx::opengl::Graphics();
    // win = new windowmod::sdl::Window();
    // win->setGraphics(instance);
    // auto winset = new windowmod::WindowSettings();
    // winset->displayindex = 0;
    // win->setWindow(800, 600, winset);
}

chai_gfx::~chai_gfx() {
    // delete i;
    // delete fs;
}

bool chai_gfx::init() {
    instance = new gfx::opengl::Graphics();
    instance->hw_render = hw_render;
    instance->FRAMEBUFFER = FRAMEBUFFER;
    instance->COLORATTACH = COLORATTACH;
    // instance->setProjection(Matrix4::perspective(120000.0f, 800.0f/600.0f, 0.1f, 100.0f));
    win = new windowmod::sdl::Window();
    
    auto winset = new windowmod::WindowSettings();
    winset->displayindex = 0;
    winset->depth = 16;
    
    width = 1440;
    height = 1080;

    win->setWindow(width, height, winset);
  
    win->setGraphics(instance);

    // instance->setMode(nullptr, 1920, 1080, 1920, 1080, true, 16, 0);
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

bool chai_gfx::destroy() {
    delete win;
    instance->setShader();
    shader->destroy();
    instance->unSetMode();
    for_each(meshes.begin(), meshes.end(), [](chai_mesh *m) { m->destroy(); });
    // delete instance;
    return true;
}

chai_shader *chai_gfx::wrap_newShader(const std::string *FileName) {
    // delete win;
    // delete instance;
    // init();

   

    if (instance->isCreated()) {
        instance->bindVAO();
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
                if (!shaderFound || 
                    strstr(line.c_str(), "for (") != NULL || 
                    strstr(line.c_str(), "if (") != NULL || 
                    strstr(line.c_str(), "else {") != NULL || 
                    strstr(line.c_str(), "else if (") != NULL) {

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
        std::string c = "uniform sampler2D shadowMap;";
            c += "uniform int shadow;";
            c += "varying vec3 lighting;";
            c += "varying vec4 fragPosLightSpace;";
            c += "float grid (vec2 vBC, float width) {";
            c += "vec3 bary = vec3(vBC.x, vBC.y, 1.0 - vBC.x - vBC.y);";
            c += "vec3 d = fwidth(bary);";
            c += "vec3 a3 = smoothstep(d * (width - 0.5), d * (width + 0.5), bary);";
            c += "return min(min(a3.x, a3.y), a3.z);";
            c += "}";
            c += "float ShadowCalculation() {";
            c += "vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;";
            c += "projCoords = projCoords * 0.5 + 0.5;";
            // c += "projCoords.z = 1.0 - projCoords.z;";
            // c += "projCoords.y = 1.0 - projCoords.y;";
            c += "float closestDepth = texture2D(shadowMap, projCoords.xy).r;";
            c += "float currentDepth = projCoords.z;";
            c += "float shadow = currentDepth > closestDepth ? 0.5 : 0.0;";
            c += "return shadow;";
            c += "}";
            c += "vec4 effect( vec4 color, Image texture, vec2 texture_coords, vec2 screen_coords ){ ";
            c += "vec4 pixel = Texel(texture, texture_coords );";
            // c += "return vec4(1.0,.0,.0,1.0);";
            c += "float shadows = 0.0;";
            c += "if (shadow == 1) {";
            c += "shadows = ShadowCalculation();";
            c += "}"; 
            c += "vec3 finalColor = (1.0 - shadows) * pixel.rgb * lighting;";
            c += "return vec4(finalColor, pixel.a) * color;";
            // c += "return vec4(vec3(grid(vec2(finalColor.x, finalColor.y), 10.0)), 1);";
            c += "}";

        code.push_back(a);
        code.push_back(c);

        shader = new chai_shader();
        shader->newShader(instance, code, options);

        // if (!shader->fragmentShader) {
        //     shader->fragmentShader = new love::gfx::ShaderStage(
        //         instance,
        //         love::gfx::SHADERSTAGE_PIXEL,
        //         c,
        //         false, // Assuming not GLES
        //         "GL_FRAGMENT_SHADER"
        //     );
        // }
        return shader;
        // return instance->newShader(lines, options);
    }
    return nullptr;
}

void chai_gfx::wrap_setShader(chai_shader *s) {
    if (instance->isCreated()) {
        
        // instance->bindVAO();
        instance->setShader(0);
        instance->setShader(s->shader);
    }
}

chai_mesh *chai_gfx::wrap_newMesh(const std::vector<chaiscript::Boxed_Value> &vertexFormat, const std::vector<chaiscript::Boxed_Value> &data, const std::string &type) {
    if (instance->isCreated()) {
        auto m = new chai_mesh();
        
        // instance->bindVAO();
        m->newMesh(instance, vertexFormat, data, type);
        meshes.push_back(m);
        return m;
    }
    return nullptr;
}

chai_mesh *chai_gfx::wrap_newMeshFromFile(const std::vector<chaiscript::Boxed_Value> &vertexFormat, const std::string *FileName, const std::string &type) {
    if (instance->isCreated()) {
        auto m = new chai_mesh();
        
        // instance->bindVAO();
        m->newMeshFromFile(instance, vertexFormat, FileName, type);
        meshes.push_back(m);
        return m;
    }
    return nullptr;
}

void chai_gfx::createCanvas() {
    gfx::Texture::Settings settings;
    settings.width = 800;
    settings.height = 600;
    settings.renderTarget = true;
    settings.debugName = "RARCH_TEXTURE_2";
    settings.format = PIXELFORMAT_RGBA8_UNORM;

    auto slices = gfx::Texture::Slices(gfx::TextureType::TEXTURE_2D);
    slices.clear();
    canvas = instance->newTexture(settings, &slices);

}

void readFBOIntoVideoBuffer(love::gfx::Graphics *instance, love::gfx::Mesh *mesh) {
    // gfx::Buffer::Settings settings(gfx::BufferUsageFlags::BUFFERUSAGEFLAG_VERTEX, gfx::BufferDataUsage::BUFFERDATAUSAGE_STATIC);
    // gfx::Buffer *buffer = new gfx::Buffer(instance, settings, mesh->getVertexFormat(), mesh->getVertexCount() * mesh->getVertexStride(), 0);
    // // Create a destination ByteData object
    // love::datamod::ByteData *dest = new love::datamod::ByteData(800*600*4); // Assuming RGBA format

    // // Call readbackBuffer
    // love::datamod::ByteData *result = graphics->readbackBuffer(buffer, 0, size, dest, 0);

    // glad::glBindFramebuffer(GL_FRAMEBUFFER, result->getData());

    // int width = 800; // Set appropriate width
    // int height = 600; // Set appropriate height
    // std::vector<uint8_t> buffer(width * height * 4); // Assuming RGBA format

    // glad::glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, buffer.data());

    // // Copy to video buffer
    // auto cl = ChaiLove::getInstance();
    // memcpy(cl->videoBuffer, buffer.data(), buffer.size());

    // glad::glBindFramebuffer(GL_FRAMEBUFFER, 0); // Unbind the FBO

    // // Clean up
    // result->release();
    // dest->release();
}


void chai_gfx::drawScene() {
    if (instance->isCreated()) {
        // instance->setRenderTarget();
        // auto matrix = Matrix4(new float[16] {
        //     1.0f, 0.0f, 0.0f, 0.0f,
        //     0.0f, 1.0f, 0.0f, 0.0f,
        //     0.0f, 0.0f, 1.0f, 0.0f,
        //     0.0f, 0.0f, 0.0f, 1.0f});
        // instance->draw(canvas, matrix);
        instance->setShader();
        scene = nullptr;
    }
}

chai_scene *chai_gfx::wrap_newScene() {
    if (instance->isCreated()) {
        return new chai_scene();
    }
    return nullptr;
}

void chai_gfx::draw(chai_mesh *m) {
    if (instance->isCreated()) { 
        if (scene == nullptr) {
            // scene = new SceneMesh();
            instance->setDepthMode(gfx::CompareMode::COMPARE_LEQUAL, true);
            gfx::OptionalColorD clearcolor;
            OptionalInt clearstencil(0);
            OptionalDouble cleardepth(1.0);
            instance->clear(clearcolor, clearstencil, cleardepth);
            // createCanvas();
            // auto rt = gfx::Graphics::RenderTarget(canvas, 0, 0);
            // instance->setRenderTarget(rt, 0);        
        }    
        auto matrix = Matrix4(new float[16] {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f});  
        m->draw(instance, matrix, shader);
    }

//     scene->addMesh(m);
}

void chai_gfx::drawCanvas() {
    auto img = new imagemod::ImageData(canvas->getWidth(), canvas->getHeight(), PIXELFORMAT_RGBA8_UNORM);

    Rect rect = Rect();
    rect.w = canvas->getWidth();
    rect.h = canvas->getHeight();
    instance->readbackTexture(canvas, 0, 0, rect, img, 0, 0);
    auto d = img->getData();
    auto cl = ChaiLove::getInstance();
    // auto surf = SDL_CreateRGBSurfaceFrom(d, canvas->getWidth(), canvas->getHeight(), 32, 4*canvas->getWidth(), 0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF);
    // SDL_Rect dstrect;
    // dstrect.x = 0;
    // dstrect.y = 0;
    // SDL_BlitSurface(surf, NULL, cl->screen, &dstrect);
    memcpy(cl->videoBuffer, img->getData(), img->getSize());
}

}
