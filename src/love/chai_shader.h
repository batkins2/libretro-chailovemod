#include "gfx.h"

#define __HAVE_CHAI_SHADER__

#ifdef __HAVE_CHAISCRIPT__
#include <chaiscript/chaiscript.hpp>
#endif


namespace love {
class chai_shader {
    public:
    chai_shader();
    chai_shader(const chai_shader &c);
    ~chai_shader();
    chai_shader *clone() const;
    chai_shader& operator=(const chai_shader& m) {
		return *this;
	};
    void newShader(love::gfx::Graphics *inst, std::vector<std::string> lines, love::gfx::Shader::CompileOptions options);
    void newFragmentShader(love::gfx::Graphics *inst, std::vector<std::string> lines, love::gfx::Shader::CompileOptions options);
    void send(const std::string &uniform, const std::vector<chaiscript::Boxed_Value> &data);
    love::gfx::Graphics *instance;
    gfx::Shader *shader;
    gfx::Shader *fragmentShader;
};
}