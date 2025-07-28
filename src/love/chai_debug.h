#define __HAVE_CHAI_DEBUG__

#include <windows.h>
#include <glm/glm.hpp>

#ifdef __HAVE_CHAISCRIPT__
#include <chaiscript/chaiscript.hpp>
#endif

namespace love
{
class chai_debug
{
    public:
    chai_debug();
    ~chai_debug();
    chai_debug *clone() const {
        chai_debug *copy;
        copy->hdc = hdc;
        copy->hglrc = hglrc;
        return copy;
    };
    chai_debug(const chai_debug &c) {
        hdc = c.hdc;
        hglrc = c.hglrc;
    };
    chai_debug &operator=(const chai_debug &c) {
        if (this != &c) {
            hdc = c.hdc;
            hglrc = c.hglrc;
        }
        return *this;
    };
    chai_debug *newDebug() const {
        return new chai_debug();
    };
    void init();
    void update(float dt, std::vector<chaiscript::Boxed_Value> viewMatrix);
    void pushDebugMessage(const std::string &message);
    void pushDebugMessagef(const char* fmt, ...);
    void displayDebugMessages();
    void visualizeMatrix(const glm::mat4 &matrix);
    void displayMatrix();
    std::vector<std::string> messages;
    glm::mat4 matrix = glm::mat4(1.0f);
    HDC hdc = nullptr;
    HGLRC hglrc = nullptr;
};
}