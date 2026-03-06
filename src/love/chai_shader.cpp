#include "chai_shader.h"
#include "math/MathModule.h"

namespace love
{

chai_shader::chai_shader() {
    // Pre-reserve cache vectors to reduce allocations (typical scene uses ~200 entries)
    m_floatCache.reserve(512);
    m_intCache.reserve(512);
    m_mat4Cache.reserve(128);
    
    // static int constructorCount = 0;
    // fprintf(stderr, "[MODELCOUNT] Default constructor #%d called: new shader at %p with modelCount=0\n",
    //         ++constructorCount, (void*)this);
    // fflush(stderr);
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
    
    // Release shader objects (they're reference counted via Object)
    if (shader != nullptr) {
        shader->release();
        shader = nullptr;
    }
    if (fragmentShader != nullptr) {
        fragmentShader->release();
        fragmentShader = nullptr;
    }
}

chai_shader::chai_shader(const chai_shader &c) {
    shader = c.shader;
    fragmentShader = c.fragmentShader;
    instance = c.instance;
    modelCount = c.modelCount;
    modelJointOffset = c.modelJointOffset;
    
    // Retain the shader objects when copying to prevent double-release
    if (shader != nullptr) {
        shader->retain();
    }
    if (fragmentShader != nullptr) {
        fragmentShader->retain();
    }
    
    // fprintf(stderr, "[MODELCOUNT] Copy constructor called: copying modelCount=%d from %p to %p\n",
    //         c.modelCount, (void*)&c, (void*)this);
    // fflush(stderr);
}

chai_shader *chai_shader::clone() const
{
	return new chai_shader(*this);
}

void chai_shader::newVertexShader(love::gfx::Graphics *inst, std::vector<std::string> lines, love::gfx::Shader::CompileOptions options) {
    instance = inst;
    printf("[CHAI_SHADER] newVertexShader called with %zu code stages\n", lines.size());
    fflush(stdout);
    
    if (instance->isCreated()) {
        shader = instance->newShader(lines, options);
        printf("[CHAI_SHADER] newShader result: %p\n", shader);
        fflush(stdout);
    } else {
        printf("[CHAI_SHADER] Graphics instance not created!\n");
        fflush(stdout);
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
        
        m_floatCache.resize(order.size() * 16);  // Reuse allocated memory
        size_t idx = 0;
        for (const auto& j : order) {   
            auto m = data.find(j)->second;
            for (int c = 0; c < 4; ++c) {
                m_floatCache[idx++] = m[c].x;
                m_floatCache[idx++] = m[c].y;
                m_floatCache[idx++] = m[c].z;
                m_floatCache[idx++] = m[c].w;
                startidx+=4;
            }          
        }
        auto returnOffset = modelJointOffset;
        if (uniform == "jointMatrix") {
            modelJointOffset += order.size();
        }

        if (info->floats == nullptr) {
            printf("[SENDMAP] ERROR: info->floats is null for uniform '%s' — skipping memcpy\n", uniform.c_str());
            return returnOffset;
        }
        std::memcpy(info->floats + returnOffset * info->matrix.columns * info->matrix.rows, m_floatCache.data(), m_floatCache.size()*sizeof(float));
        shader->updateUniform(info, startidx/(info->matrix.columns*info->matrix.rows) + (uniform == "jointMatrix" ? returnOffset : 0));
        return returnOffset;
    }
    return 0;
}

void chai_shader::sendConstant(const std::string &uniform, const std::vector<chaiscript::Boxed_Value> &data) {
    // Push constants are already efficient - don't batch them, send immediately
    // (Batching is only needed for descriptor set updates, not push constants)
    
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
        // Use updateUniform instead of setPushConstant - uses uniform buffers
        memcpy((void*)info->data, m_intCache.data(), m_intCache.size() * sizeof(int));
        shader->updateUniform(info, data.size());
    } else if (info->baseType == gfx::Shader::UNIFORM_FLOAT) {
        m_floatCache.resize(data.size());  // Reuse allocated memory
        for (size_t i = 0; i < data.size(); i++) {
            m_floatCache[i] = chaiscript::boxed_cast<float>(data[i]);
            startidx++;
        }
        memcpy((void*)info->data, m_floatCache.data(), m_floatCache.size() * sizeof(float));
        shader->updateUniform(info, data.size());
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
        memcpy((void*)info->data, m_floatCache.data(), m_floatCache.size() * sizeof(float));
        shader->updateUniform(info, startidx);
    } else if (info->baseType == gfx::Shader::UNIFORM_MATRIX) {
        m_floatCache.resize(data.size());  // Reuse allocated memory
        for (size_t i = 0; i < data.size(); i++) {
            m_floatCache[i] = chaiscript::boxed_cast<float>(data[i]);
            startidx++;
        }
        int count = startidx / (info->matrix.columns * info->matrix.rows);
        memcpy((void*)info->data, m_floatCache.data(), m_floatCache.size() * sizeof(float));
        shader->updateUniform(info, count);
    }
}

void chai_shader::sendConstant(const std::string &uniform, const std::vector<glm::vec4> &data) {
    // Push constants are already efficient - don't batch them, send immediately
    // (Batching is only needed for descriptor set updates, not push constants)
    
    if (!instance || !instance->isCreated()) {
        // printf("[SENDCONSTANT] Instance not created for uniform '%s'\n", uniform.c_str());
        return;
    }

    const love::gfx::Shader::UniformInfo* info = nullptr;
    try {
        info = shader->getUniformInfo(uniform);
    } catch (std::exception &e) {
        // printf("[SENDCONSTANT] Error getting uniform '%s': %s\n", uniform.c_str(), e.what());
        return;
    }
    if (!info || info->baseType == gfx::Shader::UNIFORM_SAMPLER ||
        info->baseType == gfx::Shader::UNIFORM_STORAGETEXTURE ||
        info->baseType == gfx::Shader::UNIFORM_TEXELBUFFER ||
        info->baseType == gfx::Shader::UNIFORM_STORAGEBUFFER) {
        // printf("[SENDCONSTANT] Invalid uniform type for '%s' (info=%p, baseType=%d)\n", 
        //        uniform.c_str(), (void*)info, info ? info->baseType : -1);
        return;
    }
 
    
    size_t idx = 0;
    size_t count = 4;
    if (uniform == "miscInfo") {
        count = 8;
    }
    
    m_floatCache.resize(data.size() * count);  // Reuse allocated memory
    
    if (uniform == "miscInfo") {
        m_floatCache[idx++] = 0.0f;
        m_floatCache[idx++] = 0.0f;
        m_floatCache[idx++] = 0.0f;
        m_floatCache[idx++] = 0.0f;
    }

    for (const auto& d : data) {
        m_floatCache[idx++] = d.x;
        m_floatCache[idx++] = d.y;
        m_floatCache[idx++] = d.z;
        m_floatCache[idx++] = d.w;
    }
    // printf("[SENDCONSTANT] Sending %zu vec4 to uniform '%s'\n", data.size(), uniform.c_str());
    // printf("[SENDCONSTANT] Data: ");
    // for (size_t i = 0; i < m_floatCache.size(); i++) {
    //     printf("%f ", m_floatCache[i]);
    // }
    // printf("\n");
    shader->setPushConstant(info, (float*)m_floatCache.data(), (int)(count * sizeof(float)));
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
            
            // static int intCacheUseCount = 0;
            // static size_t maxIntCacheCapacity = 0;
            // if (++intCacheUseCount % 1000 == 0 && m_intCache.capacity() > maxIntCacheCapacity) {
            //     maxIntCacheCapacity = m_intCache.capacity();
            //     printf("[SHADER LEAK] m_intCache capacity: %zu (size=%zu)\n", 
            //            m_intCache.capacity(), data.size());
            // }
            
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
                int matrixCount = startidx / (info->matrix.columns * info->matrix.rows);
                std::memcpy(info->floats + modelCount * (info->matrix.columns * info->matrix.rows), m_floatCache.data(), m_floatCache.size()*sizeof(float));
                shader->updateUniform(info, modelCount + matrixCount);
                auto returnIdx = modelCount;
                modelCount += matrixCount;
                return returnIdx;
            } else {
                std::memcpy(info->floats, m_floatCache.data(), m_floatCache.size()*sizeof(float));
                if (info->matrix.rows != 0) {
                    shader->updateUniform(info, startidx/(info->matrix.columns*info->matrix.rows));
                } else {
                    shader->updateUniform(info, startidx / info->matrix.columns);
                }
                return 0;
            }
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
        // if (m_floatCache.capacity() > maxCapacity) {
        //     maxCapacity = m_floatCache.capacity();
        //     if (sendCount % 100 == 0) {
        //         printf("[SHADER LEAK] m_floatCache capacity grew to %zu (size=%zu, required=%zu)\n", 
        //                maxCapacity, m_floatCache.size(), requiredSize);
        //     }
        // }
        
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
            shader->updateUniform(info, modelCount + data.size());
            auto returnIdx = modelCount;
            modelCount += data.size();
            return returnIdx;
        } else {
            std::memcpy(info->floats, m_floatCache.data(), m_floatCache.size()*sizeof(float));
            if (info->matrix.rows != 0) {
                shader->updateUniform(info, startidx/(info->matrix.columns*info->matrix.rows));
            } else {
                shader->updateUniform(info, startidx / info->matrix.columns);
            }
            return 0;
        }      
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

void chai_shader::send(const std::string &uniform, const glm::vec4 &data) {
    if (instance->isCreated()) {
        const love::gfx::Shader::UniformInfo* info = nullptr;
        try {
            info = shader->getUniformInfo(uniform);
        } catch (std::exception &e) {
            return;
        }
        if (info == nullptr || info->data == nullptr ||
            info->baseType == gfx::Shader::UNIFORM_SAMPLER ||
            info->baseType == gfx::Shader::UNIFORM_STORAGETEXTURE ||
            info->baseType == gfx::Shader::UNIFORM_TEXELBUFFER ||
            info->baseType == gfx::Shader::UNIFORM_STORAGEBUFFER)
            return;

        info->floats[0] = data.x;
        info->floats[1] = data.y;
        info->floats[2] = data.z;
        info->floats[3] = data.w;
        shader->updateUniform(info, 1);
    }
}

void chai_shader::sendTexture(const std::string &uniform, gfx::Texture *tex) {
    if (!instance || !instance->isCreated() || shader == nullptr || tex == nullptr)
        return;
    const love::gfx::Shader::UniformInfo* info = nullptr;
    try {
        info = shader->getUniformInfo(uniform);
    } catch (std::exception &e) {
        return;
    }
    if (info == nullptr || info->baseType != gfx::Shader::UNIFORM_SAMPLER)
        return;
    shader->sendTextures(info, &tex, 1);
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

        size_t slot = cacheMatrix(data);
        const float* matrixData = getCachedMatrix(slot);

        if (uniform == "modelMatrix") {
            // Multi-slot path: same behaviour as send(vector<mat4>) for modelMatrix.
            // Each model occupies its own slot indexed by modelCount so the GPU
            // can distinguish per-object model matrices via jointInfo.z.
            std::memcpy(info->floats + modelCount * 16, matrixData, 16 * sizeof(float));
            shader->updateUniform(info, modelCount + 1);
            auto returnIdx = modelCount;
            modelCount++;
            return returnIdx;
        } else {
            // Single-slot path for global matrices (viewMatrix, projectionMatrix, etc.)
            std::memcpy(info->floats, matrixData, 16 * sizeof(float));
            shader->updateUniform(info, 1);
            return 0;
        }
    }
    return -1;
}

chai_shader *chai_shader::newShader() const {
    return new chai_shader();
}

void chai_shader::newFrame() {
    modelCount = 0;
    modelJointOffset = 0;
    m_nextMatrixSlot = 0;
}

void chai_shader::invalidateDescriptorSets() {
    if (shader != nullptr) {
        shader->invalidateDescriptorSets();
    }
}

// SendConstant queue implementation
void chai_shader::beginConstantBatch() {
    return;
    m_batchMode = true;
    m_constantQueue.clear();
}

void chai_shader::endConstantBatch() {
    return;
    flushConstantQueue();
    m_batchMode = false;
}

void chai_shader::flushConstantQueue() {
    if (m_constantQueue.empty()) {
        return;
    }
    
    if (!instance || !instance->isCreated()) {
        m_constantQueue.clear();
        return;
    }
    
    // Assign offsets assuming vec4 alignment (16 bytes per uniform)
    size_t currentOffset = 0;
    size_t maxSize = 0;
    
    for (auto& entry : m_constantQueue) {
        entry.offset = currentOffset;
        
        if (entry.isVec4) {
            entry.dataSize = entry.data.size() * 4 * sizeof(float);  // vec4 = 16 bytes
        } else {
            entry.dataSize = entry.boxedData.size() * sizeof(float);
        }
        
        currentOffset += entry.dataSize;
        if (entry.dataSize > maxSize) {
            maxSize = entry.dataSize;
        }
    }
    
    // Allocate merged buffer
    size_t totalSize = currentOffset;
    if (totalSize == 0) {
        m_constantQueue.clear();
        return;
    }
    
    m_pushConstantBuffer.resize(totalSize, 0);
    
    // Copy each constant to its correct offset in the merged buffer
    for (const auto& entry : m_constantQueue) {
        if (entry.dataSize == 0) continue;
        
        if (entry.isVec4) {
            // Convert vec4 data to floats
            std::vector<float> floats;
            floats.reserve(entry.data.size() * 4);
            for (const auto& v : entry.data) {
                floats.push_back(v.x);
                floats.push_back(v.y);
                floats.push_back(v.z);
                floats.push_back(v.w);
            }
            std::memcpy(m_pushConstantBuffer.data() + entry.offset, 
                       floats.data(), 
                       floats.size() * sizeof(float));
        } else {
            // Handle boxed values (convert to floats)
            std::vector<float> floats;
            for (const auto& bv : entry.boxedData) {
                try {
                    floats.push_back(chaiscript::boxed_cast<float>(bv));
                } catch (...) {}
            }
            if (!floats.empty()) {
                std::memcpy(m_pushConstantBuffer.data() + entry.offset,
                           floats.data(),
                           floats.size() * sizeof(float));
            }
        }
    }
    
    // Send the entire merged push constant block in ONE call
    if (!m_constantQueue.empty()) {
        const love::gfx::Shader::UniformInfo* firstInfo = nullptr;
        try {
            firstInfo = shader->getUniformInfo(m_constantQueue[0].uniformName);
        } catch (...) {}
        
        if (firstInfo) {
            shader->setPushConstant(firstInfo, m_pushConstantBuffer.data(), totalSize);
        }
    }
    
    m_constantQueue.clear();
}

void chai_shader::queueConstant(const std::string &uniform, const std::vector<glm::vec4> &data) {
    m_constantQueue.emplace_back(uniform, data);
}

void chai_shader::queueConstant(const std::string &uniform, const std::vector<chaiscript::Boxed_Value> &data) {
    m_constantQueue.emplace_back(uniform, data);
}

}
