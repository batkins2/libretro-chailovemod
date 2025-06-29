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
    };
    

    chai_gui();
    ~chai_gui();
    chai_gui *clone() const;
    chai_gui *newGui() const;
    int addElement(std::string type, std::string image, std::string text, const std::vector<std::string> options);
    void draw(chai_shader *shader);
    std::vector<chai_guiData*> guiElements;
};
}