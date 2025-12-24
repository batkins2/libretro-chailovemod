#define GLM_FORCE_RADIANS
#include "glm/glm.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <iostream>
#include <atomic>

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
    
    // Core GLM implementations (defined in .cpp)
    glm::mat4 setTransformationMatrixCore(const glm::vec3& translation, const glm::vec3& rotation, const glm::vec3& scale, const glm::mat4& inputMatrix) const;
    glm::mat4 setProjectionMatrixCore(float fov, float aspect, float near, float far) const;
    glm::mat4 setOrthographicMatrixCore(float left, float right, float bottom, float top, float near, float far) const;
    glm::mat4 setViewMatrixCore(const glm::vec3& eye, const glm::vec3& target, const glm::vec3& up) const;
       
    // Vector<float> overloads for backward compatibility with ChaiScript
    // OLD VERSIONS - Use simple allocation (ChaiScript GC handles cleanup)
    // Kept for compatibility - optimized versions use thread_local
    inline std::vector<float> setTransformationMatrix(const std::vector<float>& translation, const std::vector<float>& rotation, const std::vector<float>& scale, const std::vector<float>& inputMatrix) const {
        glm::vec3 t(0.0f), r(0.0f), s(1.0f);
        glm::mat4 mat(1.0f);
        if (translation.size() >= 3) t = glm::vec3(translation[0], translation[1], translation[2]);
        if (rotation.size() >= 3) r = glm::vec3(rotation[0], rotation[1], rotation[2]);
        if (scale.size() >= 3) s = glm::vec3(scale[0], scale[1], scale[2]);
        if (inputMatrix.size() >= 16) {
            for (int col = 0; col < 4; ++col)
                for (int row = 0; row < 4; ++row)
                    mat[col][row] = inputMatrix[col * 4 + row];
        }
        return mat4ToVector(setTransformationMatrixCore(t, r, s, mat));
    }
    
    inline std::vector<float> setProjectionMatrix(float fov, float aspect, float nearPlane, float farPlane) const {
        return mat4ToVector(setProjectionMatrixCore(fov, aspect, nearPlane, farPlane));
    }
    
    inline std::vector<float> setProjectionMatrix(const std::vector<float>& params) const {
        if (params.size() >= 4) {
            return mat4ToVector(setProjectionMatrixCore(params[0], params[1], params[2], params[3]));
        }
        return mat4ToVector(glm::mat4(1.0f));
    }
    
    inline std::vector<float> setOrthographicMatrix(float left, float right, float bottom, float top, float nearPlane, float farPlane) const {
        return mat4ToVector(setOrthographicMatrixCore(left, right, bottom, top, nearPlane, farPlane));
    }
    
    inline std::vector<float> setOrthographicMatrix(const std::vector<float>& params) const {
        if (params.size() >= 6) {
            return mat4ToVector(setOrthographicMatrixCore(params[0], params[1], params[2], params[3], params[4], params[5]));
        }
        return mat4ToVector(glm::mat4(1.0f));
    }
    
    inline std::vector<float> setViewMatrix(const std::vector<float>& eye, const std::vector<float>& target, const std::vector<float>& up) const {
        glm::vec3 eyeVec(0.0f);
        glm::vec3 targetVec(0.0f);
        glm::vec3 upVec(0.0f);
        if (eye.size() >= 3) {
            eyeVec = glm::vec3(eye[0], eye[1], eye[2]);
        }
        if (target.size() >= 3) {
            targetVec = glm::vec3(target[0], target[1], target[2]);
        }
        if (up.size() >= 3) {
            upVec = glm::vec3(up[0], up[1], up[2]);
        }
        return mat4ToVector(glm::lookAt(eyeVec, targetVec, upVec));
    }
    
    // Helper to convert mat4 to vector of floats (for ChaiScript compatibility)
    std::vector<float> mat4ToVector(const glm::mat4& mat) const;
    
    // Simpler approach - just call the regular versions
    // ChaiScript's GC will handle cleanup, no thread_local complications
    inline std::vector<float> setProjectionMatrixOptimized(float fov, float aspect, float nearPlane, float farPlane) const {
        return setProjectionMatrix(fov, aspect, nearPlane, farPlane);
    }
    
    inline std::vector<float> setViewMatrixOptimized(const std::vector<float>& eye, const std::vector<float>& target, const std::vector<float>& up) const {
        return setViewMatrix(eye, target, up);
    }
    
    inline std::vector<float> setOrthographicMatrixOptimized(float left, float right, float bottom, float top, float nearPlane, float farPlane) const {
        return setOrthographicMatrix(left, right, bottom, top, nearPlane, farPlane);
    }
    
    inline std::vector<float> setTransformationMatrixOptimized(const std::vector<float>& translation, const std::vector<float>& rotation, const std::vector<float>& scale, const std::vector<float>& inputMatrix) const {
        return setTransformationMatrix(translation, rotation, scale, inputMatrix);
    }
    
    // In-place versions that modify output parameter (NO allocations, NO returns)
    inline void mat4ToVectorInPlace(const glm::mat4& mat, std::vector<float>& out) const {
        // static std::atomic<int> callCount{0};
        // static std::atomic<size_t> maxCapacity{0};
        // 
        // if (++callCount % 100 == 0) {
        //     std::cout << "[TRACE] mat4ToVectorInPlace called " << callCount << " times (IN-PLACE)" << std::endl;
        //     
        //     if (out.capacity() > maxCapacity) {
        //         maxCapacity = out.capacity();
        //         std::cout << "[LEAK WARNING] Vector capacity grew to " << out.capacity() << std::endl;
        //     }
        // }
        
        // Don't shrink - ChaiScript may hold references that would become invalid
        out.resize(16);
        out[0] = mat[0][0]; out[1] = mat[0][1]; out[2] = mat[0][2]; out[3] = mat[0][3];
        out[4] = mat[1][0]; out[5] = mat[1][1]; out[6] = mat[1][2]; out[7] = mat[1][3];
        out[8] = mat[2][0]; out[9] = mat[2][1]; out[10] = mat[2][2]; out[11] = mat[2][3];
        out[12] = mat[3][0]; out[13] = mat[3][1]; out[14] = mat[3][2]; out[15] = mat[3][3];
    }
    
    inline void setProjectionMatrixInPlace(float fov, float aspect, float nearPlane, float farPlane, std::vector<float>& out) const {
        // static std::atomic<int> callCount{0};
        // if (++callCount % 100 == 0) {
        //     std::cout << "[TRACE] setProjectionMatrixInPlace called " << callCount << " times (IN-PLACE)" << std::endl;
        // }
        mat4ToVectorInPlace(setProjectionMatrixCore(fov, aspect, nearPlane, farPlane), out);
    }
    
    inline void setViewMatrixInPlace(const std::vector<float>& eye, const std::vector<float>& target, const std::vector<float>& up, std::vector<float>& out) const {
        // static std::atomic<int> callCount{0};
        // if (++callCount % 100 == 0) {
        //     std::cout << "[TRACE] setViewMatrixInPlace called " << callCount << " times (IN-PLACE)" << std::endl;
        // }
        glm::vec3 eyeVec(0.0f), targetVec(0.0f), upVec(0.0f);
        if (eye.size() >= 3) eyeVec = glm::vec3(eye[0], eye[1], eye[2]);
        if (target.size() >= 3) targetVec = glm::vec3(target[0], target[1], target[2]);
        if (up.size() >= 3) upVec = glm::vec3(up[0], up[1], up[2]);
        mat4ToVectorInPlace(glm::lookAt(eyeVec, targetVec, upVec), out);
    }
    
    inline void setOrthographicMatrixInPlace(float left, float right, float bottom, float top, float nearPlane, float farPlane, std::vector<float>& out) const {
        static std::atomic<int> callCount{0};
        if (++callCount % 100 == 0) {
            std::cout << "[TRACE] setOrthographicMatrixInPlace called " << callCount << " times (IN-PLACE)" << std::endl;
        }
        mat4ToVectorInPlace(setOrthographicMatrixCore(left, right, bottom, top, nearPlane, farPlane), out);
    }
    
    inline void setTransformationMatrixInPlace(const std::vector<float>& translation, const std::vector<float>& rotation, const std::vector<float>& scale, const std::vector<float>& inputMatrix, std::vector<float>& out) {
        // static std::atomic<int> callCount{0};
        // if (++callCount % 100 == 0) {
        //     std::cout << "[TRACE] setTransformationMatrixInPlace called " << callCount << " times (IN-PLACE)" << std::endl;
        // }
        glm::vec3 t(0.0f), r(0.0f), s(1.0f);
        glm::mat4 mat(1.0f);
        if (translation.size() >= 3) t = glm::vec3(translation[0], translation[1], translation[2]);
        if (rotation.size() >= 3) r = glm::vec3(rotation[0], rotation[1], rotation[2]);
        if (scale.size() >= 3) s = glm::vec3(scale[0], scale[1], scale[2]);
        if (inputMatrix.size() >= 16) {
            for (int col = 0; col < 4; ++col)
                for (int row = 0; row < 4; ++row)
                    mat[col][row] = inputMatrix[col * 4 + row];
        }
        mat4ToVectorInPlace(setTransformationMatrixCore(t, r, s, mat), out);
    }
    
    // Helper to convert vector of floats to mat4
    inline glm::mat4 vectorToMat4(const std::vector<float>& vec) {
        glm::mat4 mat(1.0f);
        if (vec.size() >= 16) {
            for (int col = 0; col < 4; ++col) {
                for (int row = 0; row < 4; ++row) {
                    mat[col][row] = vec[col * 4 + row];
                }
            }
        }
        return mat;
    }
    
    // LEAK FIX: Direct matrix-to-float-array conversion (NO std::vector, NO ChaiScript boxing)
    // Returns a raw pointer to internal storage - valid until next call
    inline const float* mat4ToFloatArray(const glm::mat4& mat) {
        // Use static in single-threaded build, thread_local in multi-threaded
#ifdef CHAISCRIPT_NO_THREADS
        static std::array<float, 16> storage;
#else
        static thread_local std::array<float, 16> storage;
#endif
        storage[0] = mat[0][0]; storage[1] = mat[0][1]; storage[2] = mat[0][2]; storage[3] = mat[0][3];
        storage[4] = mat[1][0]; storage[5] = mat[1][1]; storage[6] = mat[1][2]; storage[7] = mat[1][3];
        storage[8] = mat[2][0]; storage[9] = mat[2][1]; storage[10] = mat[2][2]; storage[11] = mat[2][3];
        storage[12] = mat[3][0]; storage[13] = mat[3][1]; storage[14] = mat[3][2]; storage[15] = mat[3][3];
        return storage.data();
    }
};
}