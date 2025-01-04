#include "glm/glm.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>


#ifdef __HAVE_CHAISCRIPT__
#include <chaiscript/chaiscript.hpp>
#endif
namespace love
{
class chai_matrices
{
    public:
    chai_matrices();
    ~chai_matrices();
    glm::mat4 setTransformationMatrixInternal(glm::vec3 translation, glm::vec3 rotation, glm::vec3 scale, glm::mat4 mat);
    std::vector<chaiscript::Boxed_Value> setTransformationMatrix(std::vector<chaiscript::Boxed_Value> translation, std::vector<chaiscript::Boxed_Value> rotation, std::vector<chaiscript::Boxed_Value> scale, std::vector<chaiscript::Boxed_Value> inputMatrix) {
        glm::vec3 t = glm::vec3(chaiscript::boxed_cast<float>(translation[0]), chaiscript::boxed_cast<float>(translation[1]), chaiscript::boxed_cast<float>(translation[2]));
        glm::vec3 r = glm::vec3(chaiscript::boxed_cast<float>(rotation[0]), chaiscript::boxed_cast<float>(rotation[1]), chaiscript::boxed_cast<float>(rotation[2]));
        glm::vec3 s = glm::vec3(chaiscript::boxed_cast<float>(scale[0]), chaiscript::boxed_cast<float>(scale[1]), chaiscript::boxed_cast<float>(scale[2]));
        glm::mat4 mat = glm::mat4(1.0f);
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                mat[i][j] = chaiscript::boxed_cast<float>(inputMatrix[i*4+j]);
            }
        }
        glm::mat4 matrix = setTransformationMatrixInternal(t, r, s, mat);
        std::vector<chaiscript::Boxed_Value> m;
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                m.push_back(chaiscript::Boxed_Value(matrix[i][j]));
            }
        }
        return m;
    };
    glm::mat4 setProjectionMatrixInternal(float fov, float aspect, float near, float far);
    std::vector<chaiscript::Boxed_Value> setProjectionMatrix(float f, float a, float n, float fa) {
        glm::mat4 matrix = setProjectionMatrixInternal(f, a, n, fa);
        std::vector<chaiscript::Boxed_Value> m;
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                m.push_back(chaiscript::Boxed_Value(matrix[i][j]));
            }
        }
        return m;
    };
    glm::mat4 setOrthographicMatrixInternal(float left, float right, float bottom, float top, float near, float far); 
    std::vector<chaiscript::Boxed_Value> setOrthographicMatrix(float l, float r, float b, float t, float n, float fa) {
        glm::mat4 matrix = setOrthographicMatrixInternal(l, r, b, t, n, fa);
        std::vector<chaiscript::Boxed_Value> m;
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                m.push_back(chaiscript::Boxed_Value(matrix[i][j]));
            }
        }
        return m;
    };
    glm::mat4 setViewMatrixInternal(glm::vec3 eye, glm::vec3 target, glm::vec3 up);
    std::vector<chaiscript::Boxed_Value> setViewMatrix(std::vector<chaiscript::Boxed_Value> eye, std::vector<chaiscript::Boxed_Value> target, std::vector<chaiscript::Boxed_Value> up) {
        glm::vec3 e = glm::vec3(chaiscript::boxed_cast<float>(eye[0]), chaiscript::boxed_cast<float>(eye[1]), chaiscript::boxed_cast<float>(eye[2]));
        glm::vec3 t = glm::vec3(chaiscript::boxed_cast<float>(target[0]), chaiscript::boxed_cast<float>(target[1]), chaiscript::boxed_cast<float>(target[2]));
        glm::vec3 u = glm::vec3(chaiscript::boxed_cast<float>(up[0]), chaiscript::boxed_cast<float>(up[1]), chaiscript::boxed_cast<float>(up[2]));
        glm::mat4 matrix = setViewMatrixInternal(e, t, u);
        std::vector<chaiscript::Boxed_Value> m;
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                m.push_back(chaiscript::Boxed_Value(matrix[i][j]));
            }
        }
        return m;
    };  
};
}