#include "chai_matrices.h"
#include <iostream>
#include <atomic>

namespace love
{
static std::atomic<int> g_matrixCallCount{0};
static std::atomic<int> g_vectorAllocCount{0};

chai_matrices::chai_matrices()
{
}
chai_matrices::~chai_matrices()
{
}

glm::mat4 chai_matrices::setTransformationMatrixCore(const glm::vec3& translation, const glm::vec3& rotation, const glm::vec3& scale, const glm::mat4& mat) const
{
    glm::quat q = glm::quat(glm::radians(rotation));
    glm::mat4 result = glm::translate(mat, translation);
    result *= glm::mat4_cast(q);
    result = glm::scale(result, scale);
    return result;
}

glm::mat4 chai_matrices::setProjectionMatrixCore(float fov, float aspect, float near, float far) const
{
    return glm::perspective(fov, aspect, near, far);
}

glm::mat4 chai_matrices::setOrthographicMatrixCore(float left, float right, float bottom, float top, float near, float far) const
{
    return glm::ortho(left, right, bottom, top, near, far);
}

glm::mat4 chai_matrices::setViewMatrixCore(const glm::vec3& eye, const glm::vec3& target, const glm::vec3& up) const
{
    return glm::lookAt(eye, target, up);
}

std::vector<float> chai_matrices::mat4ToVector(const glm::mat4& mat) const
{
    int count = ++g_vectorAllocCount;
    if (count % 100 == 0) {
        std::cout << "[LEAK TRACE] mat4ToVector called " << count << " times" << std::endl;
    }
    // Simple stack array copy - no thread_local, no cache, just pure allocation
    float data[16] = {
        mat[0][0], mat[0][1], mat[0][2], mat[0][3],
        mat[1][0], mat[1][1], mat[1][2], mat[1][3],
        mat[2][0], mat[2][1], mat[2][2], mat[2][3],
        mat[3][0], mat[3][1], mat[3][2], mat[3][3]
    };
    return std::vector<float>(data, data + 16);
}
} // namespace love