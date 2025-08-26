#define __HAVE_CHAI_GUI__
#include <string>
#include <vector>

#include "gfx.h"

#ifndef __HAVE_CHAI_SHADER__
#include "chai_shader.h"
#endif

namespace love
{
class chai_gui
{
public:
    struct chai_guiData
    {
        /* data */
        int id;
        std::string type;
        std::string image;
        std::string text;
        std::vector<std::string> options;
        gfx::Texture *texture = nullptr;
        bool visible = true; // Added visibility flag
    };
    

    chai_gui();
    ~chai_gui();
    chai_gui *clone() const;
    chai_gui *newGui() const;
    int addElement(std::string type, std::string image, std::string text, const std::vector<std::string> options);
    void draw(chai_shader *shader, std::vector<chaiscript::Boxed_Value> viewMatrix1, bool debug);
    void setElementVisible(int id, bool visible);
    void setElementText(int id, const std::string &text);
    void writeConsole(std::string s);
    void executeConsole();
    void initConsole();
    std::vector<chai_guiData*> guiElements;
    gfx::Texture *overlayTexture = nullptr;
    chai_guiData* console = nullptr;
};
}