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
    // Clear caches to ensure proper cleanup
    m_floatCache.clear();
    m_intCache.clear();
    m_mat4Cache.clear();
    
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

size_t chai_shader::sendMap(const std::string &uniform, const std::map<int, glm::mat4> &data, const std::vector<int> &order) {
    if (instance->isCreated()) {
        const love::gfx::Shader::UniformInfo* info = nullptr;
        try {
            info = shader->getUniformInfo(uniform);
        } catch (std::exception &e) {
            printf("Error: %s\n", e.what());
            return 0;
        }
        if (info == nullptr || info->baseType == gfx::Shader::UNIFORM_SAMPLER || info->baseType == gfx::Shader::UNIFORM_STORAGETEXTURE
            || info->baseType == gfx::Shader::UNIFORM_TEXELBUFFER || info->baseType == gfx::Shader::UNIFORM_STORAGEBUFFER)
            return 0;

        int startidx = 0;
        if (uniform == "jointMatrix") {
            // this->sendConstant("jointOffset", std::vector<chaiscript::Boxed_Value>({ chaiscript::Boxed_Value(modelJointOffset) }));
            startidx = modelJointOffset;
        }
        
        m_floatCache.clear();
        m_floatCache.reserve(order.size() * 16);
        for (const auto& j : order) {   
            auto m = data.find(j)->second;
            for (int c = 0; c < 4; ++c) {
                m_floatCache.emplace_back(m[c].x);
                // printf("%f ", m[c].x);
                m_floatCache.emplace_back(m[c].y);
                // printf("%f ", m[c].y);
                m_floatCache.emplace_back(m[c].z);
                // printf("%f ", m[c].z);
                m_floatCache.emplace_back(m[c].w);
                // printf("%f\n", m[c].w);
                startidx+=4;
            }          
        }
        auto returnOffset = modelJointOffset;
        if (uniform == "jointMatrix") {
            modelJointOffset += order.size();
        }
        std::memcpy(info->floats + returnOffset * info->matrix.columns * info->matrix.rows, m_floatCache.data(), m_floatCache.size()*sizeof(float));
        shader->updateUniform(info, startidx/(info->matrix.columns*info->matrix.rows) + (uniform == "jointMatrix" ? returnOffset : 0));
        return returnOffset;
        // shader->setPushConstant(info, prepD.data(), startidx/(info->matrix.columns*info->matrix.rows));
    }
    return 0;
}

void chai_shader::sendConstant(const std::string &uniform, const std::vector<chaiscript::Boxed_Value> &data) {
    if (!instance || !instance->isCreated())
        return;

    const love::gfx::Shader::UniformInfo* info = nullptr;
    try {
        info = shader->getUniformInfo(uniform);
    } catch (std::exception &e) {
        printf("Error: %s\n", e.what());
        return;
    }
    if (!info || info->baseType == gfx::Shader::UNIFORM_SAMPLER ||
        info->baseType == gfx::Shader::UNIFORM_STORAGETEXTURE ||
        info->baseType == gfx::Shader::UNIFORM_TEXELBUFFER ||
        info->baseType == gfx::Shader::UNIFORM_STORAGEBUFFER)
        return;
 
    int startidx = 0;
    if (info->baseType == gfx::Shader::UNIFORM_INT) {
        m_intCache.clear();
        m_intCache.reserve(data.size());
        for (const auto& d : data) {
            m_intCache.emplace_back(chaiscript::boxed_cast<int>(d));
            startidx++;
        }
        shader->setPushConstant(info, m_intCache.data(), m_intCache.size() * sizeof(int));
    } else if (info->baseType == gfx::Shader::UNIFORM_FLOAT) {
        m_floatCache.clear();
        m_floatCache.reserve(data.size());
        for (const auto& d : data) {
            m_floatCache.emplace_back(chaiscript::boxed_cast<float>(d));
            startidx++;
        }
        shader->setPushConstant(info, m_floatCache.data(), m_floatCache.size() * sizeof(float));
    } else if (info->baseType == gfx::Shader::UNIFORM_MATRIX && uniform == "jointMatrix") {
        m_floatCache.clear();
        size_t totalSize = 0;
        for (const auto& d : data) {
            const auto& outerVec = chaiscript::boxed_cast<const std::vector<chaiscript::Boxed_Value>&>(d);
            for (const auto& v : outerVec) {
                const auto& innerVec = chaiscript::boxed_cast<const std::vector<chaiscript::Boxed_Value>&>(v);
                totalSize += innerVec.size();
            }
        }
        m_floatCache.reserve(totalSize);
        for (const auto& d : data) {
            // Cache the outer vector once
            const auto& outerVec = chaiscript::boxed_cast<const std::vector<chaiscript::Boxed_Value>&>(d);
            for (const auto& v : outerVec) {
                // Cache the inner vector once
                const auto& innerVec = chaiscript::boxed_cast<const std::vector<chaiscript::Boxed_Value>&>(v);
                for (const auto& f : innerVec) {
                    m_floatCache.emplace_back(chaiscript::boxed_cast<float>(f));
                    startidx++;
                }
            }
        }
        shader->setPushConstant(info, m_floatCache.data(), m_floatCache.size() * sizeof(float));
    } else if (info->baseType == gfx::Shader::UNIFORM_MATRIX) {
        m_floatCache.clear();
        m_floatCache.reserve(data.size());
        for (const auto& d : data) {
            m_floatCache.emplace_back(chaiscript::boxed_cast<float>(d));
            startidx++;
        }
        int count = startidx / (info->matrix.columns * info->matrix.rows);
        shader->setPushConstant(info, m_floatCache.data(), m_floatCache.size() * sizeof(float));
    }
}

void chai_shader::sendConstant(const std::string &uniform, const std::vector<glm::vec4> &data) {
    if (!instance || !instance->isCreated())
        return;

    const love::gfx::Shader::UniformInfo* info = nullptr;
    try {
        info = shader->getUniformInfo(uniform);
    } catch (std::exception &e) {
        printf("Error: %s\n", e.what());
        return;
    }
    if (!info || info->baseType == gfx::Shader::UNIFORM_SAMPLER ||
        info->baseType == gfx::Shader::UNIFORM_STORAGETEXTURE ||
        info->baseType == gfx::Shader::UNIFORM_TEXELBUFFER ||
        info->baseType == gfx::Shader::UNIFORM_STORAGEBUFFER)
        return;
 
    int startidx = 0;
    m_floatCache.clear();
    m_floatCache.reserve(data.size() * 4);
    for (const auto& d : data) {
        m_floatCache.emplace_back(d.x);
        m_floatCache.emplace_back(d.y);
        m_floatCache.emplace_back(d.z);
        m_floatCache.emplace_back(d.w);
        startidx += 4;
    }
    shader->setPushConstant(info, m_floatCache.data(), m_floatCache.size() * sizeof(float));
}

int chai_shader::send(const std::string &uniform, const std::vector<chaiscript::Boxed_Value> &data) {
    if (instance->isCreated()) {
        const love::gfx::Shader::UniformInfo* info = nullptr;
        try {
            info = shader->getUniformInfo(uniform);
        } catch (std::exception &e) {
            printf("Error: %s\n", e.what());
            return -1;
        }
        if (info == nullptr || info->baseType == gfx::Shader::UNIFORM_SAMPLER || info->baseType == gfx::Shader::UNIFORM_STORAGETEXTURE
            || info->baseType == gfx::Shader::UNIFORM_TEXELBUFFER || info->baseType == gfx::Shader::UNIFORM_STORAGEBUFFER)
            return -1;
        if (info->baseType == gfx::Shader::UNIFORM_INT) {
            int startidx = 0;
            m_intCache.clear();
            m_intCache.reserve(data.size());
            for (const auto& d : data) {
                m_intCache.emplace_back(chaiscript::boxed_cast<int>(d));  // Direct use, no temporary binding
                startidx++;
            }
            std::memcpy(info->ints, m_intCache.data(), m_intCache.size()*sizeof(int));
            shader->updateUniform(info, startidx);
        } else if (info->baseType == gfx::Shader::UNIFORM_MATRIX && uniform == "jointMatrix") {
            int startidx = 0;
            
            std::vector<std::reference_wrapper<const std::vector<chaiscript::Boxed_Value>>> outerVecs;
            std::vector<std::reference_wrapper<const std::map<chaiscript::Boxed_Value, chaiscript::Boxed_Value>>> maps;
            std::vector<std::reference_wrapper<const std::vector<chaiscript::Boxed_Value>>> innerVecs;
            
            size_t totalSize = 0;
            for (const auto& d : data) {
                const auto& outerVec = chaiscript::boxed_cast<const std::vector<chaiscript::Boxed_Value>&>(d);
                for (const auto& v : outerVec) {
                    const auto& mapRef = chaiscript::boxed_cast<const std::map<chaiscript::Boxed_Value, chaiscript::Boxed_Value>&>(v);
                    for (const auto& value : mapRef) {
                        const auto& innerVec = chaiscript::boxed_cast<const std::vector<chaiscript::Boxed_Value>&>(value.second);
                        totalSize += innerVec.size();
                    }
                }
            }
            
            m_floatCache.clear();
            m_floatCache.reserve(totalSize);
            
            for (const auto& d : data) {
                const auto& outerVec = chaiscript::boxed_cast<const std::vector<chaiscript::Boxed_Value>&>(d);
                for (const auto& v : outerVec) {
                    const auto& mapRef = chaiscript::boxed_cast<const std::map<chaiscript::Boxed_Value, chaiscript::Boxed_Value>&>(v);
                    for (const auto& value : mapRef) {
                        const auto& innerVec = chaiscript::boxed_cast<const std::vector<chaiscript::Boxed_Value>&>(value.second);
                        for (const auto& elem : innerVec) {
                            m_floatCache.emplace_back(chaiscript::boxed_cast<float>(elem));
                            startidx++;
                        }
                    }
                }
            }

            std::memcpy(info->data, m_floatCache.data(), m_floatCache.size()*sizeof(float));
            shader->updateUniform(info, startidx/(info->matrix.columns*info->matrix.rows));
            
        
        } else {
            int startidx = 0;
            m_floatCache.clear();
            m_floatCache.reserve(data.size());
            for (const auto& d : data) {
                m_floatCache.emplace_back(chaiscript::boxed_cast<float>(d));  // Direct use, no temporary binding
                startidx++;
            }
            if (uniform == "modelMatrix") {
                std::memcpy(info->floats + modelCount * (info->matrix.columns * info->matrix.rows), m_floatCache.data(), m_floatCache.size()*sizeof(float));
            } else {
                std::memcpy(info->floats, m_floatCache.data(), m_floatCache.size()*sizeof(float));
            }
            if (info->matrix.rows != 0) {
                shader->updateUniform(info, startidx/(info->matrix.columns*info->matrix.rows) * (uniform == "modelMatrix" ? modelCount + 1 : 1));
            } else {
                shader->updateUniform(info, startidx / info->matrix.columns);
            }
            auto returnIdx = modelCount;
            if (uniform == "modelMatrix") {
                modelCount++;
            }
            return returnIdx;
        }       
    }
    return -1;
}

int chai_shader::send(const std::string &uniform, const std::vector<glm::mat4> &data) {
    if (instance->isCreated()) {
        const love::gfx::Shader::UniformInfo* info = nullptr;
        try {
            info = shader->getUniformInfo(uniform);
        } catch (std::exception &e) {
            printf("Error: %s\n", e.what());
            return -1;
        }
        if (info == nullptr || info->baseType == gfx::Shader::UNIFORM_SAMPLER || info->baseType == gfx::Shader::UNIFORM_STORAGETEXTURE
            || info->baseType == gfx::Shader::UNIFORM_TEXELBUFFER || info->baseType == gfx::Shader::UNIFORM_STORAGEBUFFER)
            return -1;

        int startidx = 0;
        m_floatCache.clear();
        m_floatCache.reserve(data.size() * 16);
        for (const auto& d : data) {
            for (int c = 0; c < 4; ++c) {
                m_floatCache.emplace_back(d[c].x);
                m_floatCache.emplace_back(d[c].y);
                m_floatCache.emplace_back(d[c].z);
                m_floatCache.emplace_back(d[c].w);
                startidx += 4;
            }          
        }
        if (uniform == "modelMatrix") {
            std::memcpy(info->floats + modelCount * (info->matrix.columns * info->matrix.rows), m_floatCache.data(), m_floatCache.size()*sizeof(float));
        } else {
            std::memcpy(info->floats, m_floatCache.data(), m_floatCache.size()*sizeof(float));
        }
        if (info->matrix.rows != 0) {
            shader->updateUniform(info, startidx/(info->matrix.columns*info->matrix.rows) * (uniform == "modelMatrix" ? modelCount + 1 : 1));
        } else {
            shader->updateUniform(info, startidx / info->matrix.columns);
        }
        auto returnIdx = modelCount;
        if (uniform == "modelMatrix") {
            modelCount++;
        }
        return returnIdx;      
    }
    return -1;
}

void chai_shader::send(const std::string &uniform, float data) {
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

        info->floats[0] = data;
        shader->updateUniform(info, 1);
    }
}

void chai_shader::send(const std::string &uniform, const glm::vec3 &data) {
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

        info->floats[0] = data.x;
        info->floats[1] = data.y;
        info->floats[2] = data.z;
        shader->updateUniform(info, 1);
    }
}

void chai_shader::send(const std::string &uniform, const std::vector<glm::vec3> &data) {
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

        int idx = 0;
        for (const auto& vec : data) {
            info->floats[idx++] = vec.x;
            info->floats[idx++] = vec.y;
            info->floats[idx++] = vec.z;
        }
        shader->updateUniform(info, data.size());
    }
}

int chai_shader::send(const std::string &uniform, const std::vector<int> &data) {
    if (instance->isCreated()) {
        const love::gfx::Shader::UniformInfo* info = nullptr;
        try {
            info = shader->getUniformInfo(uniform);
        } catch (std::exception &e) {
            printf("Error: %s\n", e.what());
            return -1;
        }
        if (info == nullptr || info->baseType == gfx::Shader::UNIFORM_SAMPLER || info->baseType == gfx::Shader::UNIFORM_STORAGETEXTURE
            || info->baseType == gfx::Shader::UNIFORM_TEXELBUFFER || info->baseType == gfx::Shader::UNIFORM_STORAGEBUFFER)
            return -1;

        int startidx = 0;
        m_intCache.clear();
        m_intCache.reserve(data.size());
        for (const auto& d : data) {
            m_intCache.emplace_back(d);
            startidx++;
        }
        std::memcpy(info->ints, m_intCache.data(), m_intCache.size()*sizeof(int));
        shader->updateUniform(info, startidx);
        return 0;      
    }
    return -1;
}

int chai_shader::send(const std::string &uniform, const glm::mat4 &data) {
    if (instance->isCreated()) {
        const love::gfx::Shader::UniformInfo* info = nullptr;
        try {
            info = shader->getUniformInfo(uniform);
        } catch (std::exception &e) {
            printf("Error: %s\n", e.what());
            return -1;
        }
        if (info == nullptr || info->baseType == gfx::Shader::UNIFORM_SAMPLER || info->baseType == gfx::Shader::UNIFORM_STORAGETEXTURE
            || info->baseType == gfx::Shader::UNIFORM_TEXELBUFFER || info->baseType == gfx::Shader::UNIFORM_STORAGEBUFFER)
            return -1;

        m_floatCache.clear();
        m_floatCache.reserve(16);
        for (int c = 0; c < 4; ++c) {
            m_floatCache.emplace_back(data[c].x);
            m_floatCache.emplace_back(data[c].y);
            m_floatCache.emplace_back(data[c].z);
            m_floatCache.emplace_back(data[c].w);
        }          

        std::memcpy(info->floats, m_floatCache.data(), m_floatCache.size()*sizeof(float));
        shader->updateUniform(info, 1);
        return 0;      
    }
    return -1;
}

chai_shader *chai_shader::newShader() const {
    return new chai_shader();
} 
}
