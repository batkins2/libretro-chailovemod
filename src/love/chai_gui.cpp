#include "chai_gui.h"
#include "../ChaiLove.h"

namespace love
{
chai_gui::chai_gui()
{
}

chai_gui::~chai_gui()
{
}

chai_gui *chai_gui::clone() const
{
    return new chai_gui(*this);
}

chai_gui *chai_gui::newGui() const
{
    return new chai_gui();
}

int chai_gui::addElement(std::string type, std::string image, std::string text, const std::vector<std::string> options)
{
    chai_guiData *data = new chai_guiData();
    data->id = guiElements.size();
    data->type = type;
    data->image = image;
    data->text = text;
    data->options = options;
    guiElements.push_back(data);
    return data->id;
}

void chai_gui::draw(chai_shader *shader)
{
    glDisable(GL_DEPTH_TEST);
    auto cg = ChaiLove::getInstance()->chai_gfx;
    
    for (const auto &element : guiElements)
    {
        // Example: Render each element based on its type, image, text, etc.
        // This is where you would integrate with your graphics library to draw the GUI.
        if (element->type == "text")
        {
            shader->send("scale", std::vector<chaiscript::Boxed_Value>({ chaiscript::Boxed_Value(stof(element->options[6])) }));
            ChaiLove::getInstance()->printNew(element->text, 
                stoi(element->options[0]), 
                stoi(element->options[1]),
                stoi(element->options[2]), 
                stoi(element->options[3]), 
                stoi(element->options[4]),
                stoi(element->options[5]));          
        } else if (element->type == "image")
        {
            shader->send("scale", std::vector<chaiscript::Boxed_Value>({ chaiscript::Boxed_Value(1.0f) }));
           
            if (element->texture)
            {
                // Draw the image using the texture
                auto cg = ChaiLove::getInstance()->chai_gfx;
                auto x = stof(element->options[0]);
                auto y = stof(element->options[1]);
                auto w = stof(element->options[2]);
                auto h = stof(element->options[3]);
                auto scaleX = stof(element->options[4]);
                auto scaleY = stof(element->options[5]);
                Matrix4 m = Matrix4(new float[16]{
                    scaleX, 0.0f, 0.0f, 0.0f,
                    0.0f, scaleY, 0.0f, 0.0f,
                    0.0f, 0.0f, 1.0f, 0.0f,
                    x, y, 0.0f, 1.0f
                });
                cg.instance->draw(element->texture, m);                
            } else
            {
                auto texture = element->image;
                SDL_RWops* rw = ChaiLove::getInstance()->filesystem.openRW(texture);
                auto img = new Image(rw);

                auto w = img->getWidth();
                auto h = img->getHeight();
                gfx::Texture::Settings settings;
                settings.width = w;
                settings.height = h;
                settings.format = PIXELFORMAT_RGBA8_UNORM;
                auto slices = gfx::Texture::Slices(gfx::TextureType::TEXTURE_2D);

                size_t dataSize = w*h;

                SDL_LockSurface(img->surface);
                // Assuming pixelData is a byte array containing the ARGB data.
                uint8_t* pixelData = static_cast<uint8_t*>(img->surface->pixels);
                uint8_t* copyOfPixelData = new uint8_t[dataSize * 4];
                for (size_t i = 0; i < dataSize * 4; i += 4) {
                    uint8_t alpha = pixelData[i];        // ARGB - Alpha at index 0
                    uint8_t blue = pixelData[i + 1];      // ARGB - Red at index 1
                    uint8_t green = pixelData[i + 2];    // ARGB - Green at index 2
                    uint8_t red = pixelData[i + 3];     // ARGB - Blue at index 3

                    // Swap to RGBA format
                    copyOfPixelData[i] = red;                 // RGBA - Red at index 0
                    copyOfPixelData[i + 1] = green;           // RGBA - Green at index 1
                    copyOfPixelData[i + 2] = blue;            // RGBA - Blue at index 2
                    copyOfPixelData[i + 3] = alpha;           // RGBA - Alpha at index 3
                }

                SDL_UnlockSurface(img->surface);

                img->~Image();

                img = NULL;

                Rect rect = Rect();
                rect.w = w;
                rect.h = h;

                element->texture = cg.instance->newTexture(settings, &slices);
                element->texture->replacePixels(copyOfPixelData, dataSize*4, 0, 0, rect, false);
            }
        }
    }
    glEnable(GL_DEPTH_TEST);
    cg.instance->setShader();
}
} // namespace love