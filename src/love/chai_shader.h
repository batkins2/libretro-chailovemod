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
    size_t sendMap(const std::string &uniform, const std::map<int, glm::mat4> &data, const std::vector<int> &order);
    void sendConstant(const std::string &uniform, const std::vector<chaiscript::Boxed_Value> &data);
    void sendConstant(const std::string &uniform, const std::vector<glm::vec4> &data);
    int send(const std::string &uniform, const std::vector<chaiscript::Boxed_Value> &data);
    int send(const std::string &uniform, const std::vector<glm::mat4> &data);
    int send(const std::string &uniform, const glm::mat4 &data);
    int send(const std::string &uniform, const std::vector<int> &data);
    void send(const std::string &uniform, float data);
    void send(const std::string &uniform, const glm::vec3 &data);
    void send(const std::string &uniform, const std::vector<glm::vec3> &data);
    void newFrame();
    love::gfx::Graphics *instance;
    gfx::Shader *shader = nullptr;
    gfx::Shader *fragmentShader;
    int modelCount = 0;
    int modelJointOffset = 0;
    std::vector<float> m_floatCache;
    std::vector<int> m_intCache;
    std::vector<glm::mat4> m_mat4Cache;
    
    // LEAK FIX: Pre-allocated matrix storage to bypass ChaiScript Boxed_Value allocations
    // Instead of passing vectors through ChaiScript (which boxes them), store matrices here
    // and pass only the raw float pointer to shaders
    static constexpr size_t MAX_CACHED_MATRICES = 16;
    std::array<std::array<float, 16>, MAX_CACHED_MATRICES> m_matrixPool;
    size_t m_nextMatrixSlot = 0;
    
    // Store a matrix and return slot index (bypasses ChaiScript entirely)
    inline size_t cacheMatrix(const glm::mat4& mat) {
        size_t slot = m_nextMatrixSlot;
        m_nextMatrixSlot = (m_nextMatrixSlot + 1) % MAX_CACHED_MATRICES;
        
        auto& storage = m_matrixPool[slot];
        storage[0] = mat[0][0]; storage[1] = mat[0][1]; storage[2] = mat[0][2]; storage[3] = mat[0][3];
        storage[4] = mat[1][0]; storage[5] = mat[1][1]; storage[6] = mat[1][2]; storage[7] = mat[1][3];
        storage[8] = mat[2][0]; storage[9] = mat[2][1]; storage[10] = mat[2][2]; storage[11] = mat[2][3];
        storage[12] = mat[3][0]; storage[13] = mat[3][1]; storage[14] = mat[3][2]; storage[15] = mat[3][3];
        
        return slot;
    }
    
    // Get cached matrix as raw pointer (zero allocations, ChaiScript never sees it)
    inline const float* getCachedMatrix(size_t slot) const {
        return m_matrixPool[slot % MAX_CACHED_MATRICES].data();
    }
};
}