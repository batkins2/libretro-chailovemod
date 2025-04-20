#include "chai_shader.h"
#include "math/MathModule.h"

namespace love
{

chai_shader::chai_shader() {
}

void chai_shader::destroy() {
    // instance->setShader();
    // shader->~Shader();
    // instance->unSetMode();
    // fragmentShader->deinitialize();
    // delete instance;
}

chai_shader::~chai_shader() {
    // delete shader;
    // delete fragmentShader;
    // delete instance;
}

chai_shader::chai_shader(const chai_shader &c) {
    shader = c.shader;
    fragmentShader = c.fragmentShader;
    instance = c.instance;
}

chai_shader *chai_shader::clone() const
{
	return new chai_shader(*this);
}

void chai_shader::newVertexShader(love::gfx::Graphics *inst, std::vector<std::string> lines, love::gfx::Shader::CompileOptions options) {
    instance = inst;
    if (instance->isCreated()) {
        shader = instance->newShader(lines, options);
    }
}

void chai_shader::newFragmentShader(love::gfx::Graphics *inst, std::vector<std::string> lines, love::gfx::Shader::CompileOptions options) {
    instance = inst;
    if (instance->isCreated()) {
        fragmentShader = instance->newShader(lines, options);
    }
}

void chai_shader::sendInt(const std::string &uniform, int data) {
    if (instance->isCreated()) {
        const love::gfx::Shader::UniformInfo* info = nullptr;
        try {
            info = shader->getUniformInfo(uniform);
        } catch (std::exception &e) {
            printf("Error: %s\n", e.what());
            return;
        }
        if (info == nullptr || info->baseType == gfx::Shader::UNIFORM_SAMPLER || info->baseType == gfx::Shader::UNIFORM_STORAGETEXTURE
            || info->baseType == gfx::Shader::UNIFORM_TEXELBUFFER || info->baseType == gfx::Shader::UNIFORM_STORAGEBUFFER)
            return;

        std::memcpy(info->ints, &data, sizeof(int));
        shader->updateUniform(info, 1);
    }
}

void chai_shader::sendMap(const std::string &uniform, const std::map<int, glm::mat4> &data, const std::vector<int> &order) {
    if (instance->isCreated()) {
        const love::gfx::Shader::UniformInfo* info = nullptr;
        try {
            info = shader->getUniformInfo(uniform);
        } catch (std::exception &e) {
            printf("Error: %s\n", e.what());
            return;
        }
        if (info == nullptr || info->baseType == gfx::Shader::UNIFORM_SAMPLER || info->baseType == gfx::Shader::UNIFORM_STORAGETEXTURE
            || info->baseType == gfx::Shader::UNIFORM_TEXELBUFFER || info->baseType == gfx::Shader::UNIFORM_STORAGEBUFFER)
            return;


        int startidx = 0;
        std::vector<float> prepD;
        for (auto j : order) {   
            auto m = data.find(j)->second;
            for (int c = 0; c < 4; ++c) {
                prepD.push_back(m[c].x);
                // printf("%f ", m[c].x);
                prepD.push_back(m[c].y);
                // printf("%f ", m[c].y);
                prepD.push_back(m[c].z);
                // printf("%f ", m[c].z);
                prepD.push_back(m[c].w);
                // printf("%f\n", m[c].w);
                startidx+=4;
            }          
        }
        std::memcpy(info->floats, prepD.data(), prepD.size()*sizeof(float));

        shader->updateUniform(info, startidx/(info->matrix.columns*info->matrix.rows));
    }
}

void chai_shader::send(const std::string &uniform, const std::vector<chaiscript::Boxed_Value> &data) {
    if (instance->isCreated()) {
        const love::gfx::Shader::UniformInfo* info = nullptr;
        try {
            info = shader->getUniformInfo(uniform);
        } catch (std::exception &e) {
            printf("Error: %s\n", e.what());
            return;
        }
        if (info == nullptr || info->baseType == gfx::Shader::UNIFORM_SAMPLER || info->baseType == gfx::Shader::UNIFORM_STORAGETEXTURE
            || info->baseType == gfx::Shader::UNIFORM_TEXELBUFFER || info->baseType == gfx::Shader::UNIFORM_STORAGEBUFFER)
            return;

        if (info->baseType == gfx::Shader::UNIFORM_INT) {
            int startidx = 0;
            std::vector<int> prepD;
            for (auto d : data) {
                auto v = chaiscript::boxed_cast<int>(d);
                prepD.push_back(v);
                startidx++;
            }
            std::memcpy(info->ints, prepD.data(), prepD.size()*sizeof(int));

            shader->updateUniform(info, startidx);
        } else if (info->baseType == gfx::Shader::UNIFORM_MATRIX && uniform == "jointMatrix") {
            int startidx = 0;
            
            for (auto d : data) {
                for (auto v : chaiscript::boxed_cast<std::vector<chaiscript::Boxed_Value>>(d)) {
                    auto f = chaiscript::boxed_cast<std::map<chaiscript::Boxed_Value, chaiscript::Boxed_Value>>(v);
                    for (auto value : f) {
                        auto i = chaiscript::boxed_cast<int>(value.first);
                        auto m = chaiscript::boxed_cast<std::vector<chaiscript::Boxed_Value>>(value.second);
                        std::vector<float> prepD;
                        for (auto d : m) {
                            auto v = chaiscript::boxed_cast<float>(d);
                            prepD.push_back(v);
                            startidx++;
                        }
                        std::memcpy(info->data, prepD.data(), prepD.size()*sizeof(float));

                        shader->updateUniform(info, startidx/(info->matrix.columns*info->matrix.rows));
                    }
                }
            }
            
        
        } else {
            int startidx = 0;
            std::vector<float> prepD;
            for (auto d : data) {
                auto v = chaiscript::boxed_cast<float>(d);
                prepD.push_back(v);
                startidx++;
            }
            std::memcpy(info->floats, prepD.data(), prepD.size()*sizeof(float));

            if (info->matrix.rows != 0) {
                shader->updateUniform(info, startidx/(info->matrix.columns*info->matrix.rows));
            } else {
                shader->updateUniform(info, startidx / info->matrix.columns);
            }

        }
        
    }
}

chai_shader *chai_shader::newShader() const {
    return new chai_shader();
} 
}
