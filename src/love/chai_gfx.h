
#include <iostream>
#include "gfx.h"

namespace love
{
class chai_gfx {
    public:
    chai_gfx();
    chai_gfx& wrap_newShader(const std::string *FileName);
    void load();
    static love::gfx::graphics *instance;
};
}