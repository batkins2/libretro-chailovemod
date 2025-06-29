#include "chai_matrices.h"

namespace love
{
chai_matrices::chai_matrices()
{
}
chai_matrices::~chai_matrices()
{
}
glm::mat4 chai_matrices::setTransformationMatrixInternal(glm::vec3 translation, glm::vec3 rotation, glm::vec3 scale, glm::mat4 mat)
{
    glm::quat q = glm::quat(glm::radians(rotation));
    mat = glm::translate(mat, translation);
    mat *= glm::mat4_cast(q);
    mat = glm::scale(mat, scale);
    return mat;
}
glm::mat4 chai_matrices::setProjectionMatrixInternal(float fov, float aspect, float near, float far)
{
    return glm::perspective(fov, aspect, near, far);
}
glm::mat4 chai_matrices::setOrthographicMatrixInternal(float left, float right, float bottom, float top, float near, float far)
{
    return glm::ortho(left, right, bottom, top, near, far);
}
glm::mat4 chai_matrices::setViewMatrixInternal(glm::vec3 eye, glm::vec3 target, glm::vec3 up)
{
    return glm::lookAt(eye, target, up);
}
} // namespace love