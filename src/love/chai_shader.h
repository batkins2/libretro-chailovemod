#define __HAVE_CHAI_SHADER__

#include "gfx.h"
#include <glm/glm.hpp>

#ifdef __HAVE_CHAISCRIPT__
#include <chaiscript/chaiscript.hpp>
#endif


namespace love {
class chai_shader {
    public:
    chai_shader();
    chai_shader(const chai_shader &c);
    ~chai_shader();
    void destroy();
    chai_shader *clone() const;
    chai_shader& operator=(const chai_shader& m) {
		return *this;
	};
    chai_shader *newShader() const;
    void newVertexShader(love::gfx::Graphics *inst, std::vector<std::string> lines, love::gfx::Shader::CompileOptions options);
    void newFragmentShader(love::gfx::Graphics *inst, std::vector<std::string> lines, love::gfx::Shader::CompileOptions options);
    void sendInt(const std::string &uniform, int data);
    void sendMap(const std::string &uniform, const std::map<int, glm::mat4> &data, const std::vector<int> &order);
    void send(const std::string &uniform, const std::vector<chaiscript::Boxed_Value> &data);
    love::gfx::Graphics *instance;
    gfx::Shader *shader = nullptr;
    gfx::Shader *fragmentShader;
};
}