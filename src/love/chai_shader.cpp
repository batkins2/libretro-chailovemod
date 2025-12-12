#include "chai_shader.h"
#include "math/MathModule.h"

namespace love
{

chai_shader::chai_shader() {
    // Pre-reserve cache vectors to reduce allocations (typical scene uses ~200 entries)
    m_floatCache.reserve(512);
    m_intCache.reserve(512);
    m_mat4Cache.reserve(128);
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
        
        m_floatCache.resize(order.size() * 16);  // Reuse allocated memory
        size_t idx = 0;
        for (const auto& j : order) {   
            auto m = data.find(j)->second;
            for (int c = 0; c < 4; ++c) {
                m_floatCache[idx++] = m[c].x;
                // printf("%f ", m[c].x);
                m_floatCache[idx++] = m[c].y;
                // printf("%f ", m[c].y);
                m_floatCache[idx++] = m[c].z;
                // printf("%f ", m[c].z);
                m_floatCache[idx++] = m[c].w;
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
        m_intCache.resize(data.size());  // Reuse allocated memory
        for (size_t i = 0; i < data.size(); i++) {
            m_intCache[i] = chaiscript::boxed_cast<int>(data[i]);
            startidx++;
        }
        shader->setPushConstant(info, m_intCache.data(), m_intCache.size() * sizeof(int));
    } else if (info->baseType == gfx::Shader::UNIFORM_FLOAT) {
        m_floatCache.resize(data.size());  // Reuse allocated memory
        for (size_t i = 0; i < data.size(); i++) {
            m_floatCache[i] = chaiscript::boxed_cast<float>(data[i]);
            startidx++;
        }
        shader->setPushConstant(info, m_floatCache.data(), m_floatCache.size() * sizeof(float));
    } else if (info->baseType == gfx::Shader::UNIFORM_MATRIX && uniform == "jointMatrix") {
        size_t totalSize = 0;
        for (const auto& d : data) {
            const auto& outerVec = chaiscript::boxed_cast<const std::vector<chaiscript::Boxed_Value>&>(d);
            for (const auto& v : outerVec) {
                const auto& innerVec = chaiscript::boxed_cast<const std::vector<chaiscript::Boxed_Value>&>(v);
                totalSize += innerVec.size();
            }
        }
        m_floatCache.resize(totalSize);  // Reuse allocated memory
        size_t idx = 0;
        for (const auto& d : data) {
            // Cache the outer vector once
            const auto& outerVec = chaiscript::boxed_cast<const std::vector<chaiscript::Boxed_Value>&>(d);
            for (const auto& v : outerVec) {
                // Cache the inner vector once
                const auto& innerVec = chaiscript::boxed_cast<const std::vector<chaiscript::Boxed_Value>&>(v);
                for (const auto& f : innerVec) {
                    m_floatCache[idx++] = chaiscript::boxed_cast<float>(f);
                    startidx++;
                }
            }
        }
        shader->setPushConstant(info, m_floatCache.data(), m_floatCache.size() * sizeof(float));
    } else if (info->baseType == gfx::Shader::UNIFORM_MATRIX) {
        m_floatCache.resize(data.size());  // Reuse allocated memory
        for (size_t i = 0; i < data.size(); i++) {
            m_floatCache[i] = chaiscript::boxed_cast<float>(data[i]);
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
    m_floatCache.resize(data.size() * 4);  // Reuse allocated memory
    size_t idx = 0;
    for (const auto& d : data) {
        m_floatCache[idx++] = d.x;
        m_floatCache[idx++] = d.y;
        m_floatCache[idx++] = d.z;
        m_floatCache[idx++] = d.w;
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
            
            static int intCacheUseCount = 0;
            static size_t maxIntCacheCapacity = 0;
            if (++intCacheUseCount % 1000 == 0 && m_intCache.capacity() > maxIntCacheCapacity) {
                maxIntCacheCapacity = m_intCache.capacity();
                printf("[SHADER LEAK] m_intCache capacity: %zu (size=%zu)\n", 
                       m_intCache.capacity(), data.size());
            }
            
            m_intCache.resize(data.size());  // Reuse allocated memory instead of clear+reserve
            for (size_t i = 0; i < data.size(); i++) {
                m_intCache[i] = chaiscript::boxed_cast<int>(data[i]);
                startidx++;
            }
            std::memcpy(info->ints, m_intCache.data(), m_intCache.size()*sizeof(int));
            shader->updateUniform(info, startidx);
        } else if (info->baseType == gfx::Shader::UNIFORM_MATRIX && uniform == "jointMatrix") {
            int startidx = 0;
            
            // REMOVED: Unused vectors that were causing allocator churn
            // std::vector<std::reference_wrapper<const std::vector<chaiscript::Boxed_Value>>> outerVecs;
            // std::vector<std::reference_wrapper<const std::map<chaiscript::Boxed_Value, chaiscript::Boxed_Value>>> maps;
            // std::vector<std::reference_wrapper<const std::vector<chaiscript::Boxed_Value>>> innerVecs;
            
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
            
            m_floatCache.resize(totalSize);  // Reuse allocated memory
            size_t idx = 0;
            
            for (const auto& d : data) {
                const auto& outerVec = chaiscript::boxed_cast<const std::vector<chaiscript::Boxed_Value>&>(d);
                for (const auto& v : outerVec) {
                    const auto& mapRef = chaiscript::boxed_cast<const std::map<chaiscript::Boxed_Value, chaiscript::Boxed_Value>&>(v);
                    for (const auto& value : mapRef) {
                        const auto& innerVec = chaiscript::boxed_cast<const std::vector<chaiscript::Boxed_Value>&>(value.second);
                        for (const auto& elem : innerVec) {
                            m_floatCache[idx++] = chaiscript::boxed_cast<float>(elem);
                            startidx++;
                        }
                    }
                }
            }

            std::memcpy(info->data, m_floatCache.data(), m_floatCache.size()*sizeof(float));
            shader->updateUniform(info, startidx/(info->matrix.columns*info->matrix.rows));
            
        
        } else {
            int startidx = 0;
            m_floatCache.resize(data.size());  // Reuse allocated memory
            for (size_t i = 0; i < data.size(); i++) {
                m_floatCache[i] = chaiscript::boxed_cast<float>(data[i]);  // Direct use, no temporary binding
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

        static int sendCount = 0;
        static size_t maxCapacity = 0;
        sendCount++;
        
        int startidx = 0;
        size_t requiredSize = data.size() * 16;
        
        // Track cache growth
        if (m_floatCache.capacity() > maxCapacity) {
            maxCapacity = m_floatCache.capacity();
            if (sendCount % 100 == 0) {
                printf("[SHADER LEAK] m_floatCache capacity grew to %zu (size=%zu, required=%zu)\n", 
                       maxCapacity, m_floatCache.size(), requiredSize);
            }
        }
        
        // Note: Cannot shrink cache safely - may cause crashes if GPU is still using the data
        
        m_floatCache.resize(requiredSize);  // Reuse allocated memory
        size_t idx = 0;
        for (const auto& d : data) {
            for (int c = 0; c < 4; ++c) {
                m_floatCache[idx++] = d[c].x;
                m_floatCache[idx++] = d[c].y;
                m_floatCache[idx++] = d[c].z;
                m_floatCache[idx++] = d[c].w;
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
        m_intCache.resize(data.size());  // Reuse allocated memory
        for (size_t i = 0; i < data.size(); i++) {
            m_intCache[i] = data[i];
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

        // LEAK FIX: Use matrix pool instead of resizing m_floatCache
        // Avoid vector resize() operations that cause capacity growth
        size_t slot = cacheMatrix(data);
        const float* matrixData = getCachedMatrix(slot);
        
        std::memcpy(info->floats, matrixData, 16 * sizeof(float));
        shader->updateUniform(info, 1);
        return 0;      
    }
    return -1;
}

chai_shader *chai_shader::newShader() const {
    return new chai_shader();
} 
}
