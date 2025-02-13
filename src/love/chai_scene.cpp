#include "chai_scene.h"
#include "../ChaiLove.h"

namespace love {
chai_scene::chai_scene() {
    
}
chai_scene::~chai_scene() {
    
}

void chai_scene::addMesh(chai_mesh *mesh) {
    meshes.push_back(mesh);
    matrices.push_back(Matrix4(new float[16] {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f}));   
}

void chai_scene::setShader(chai_shader *shader) {
    sceneShader = shader;   
}

void chai_scene::setMatrix(std::vector<chaiscript::Boxed_Value> matrix, int index) {
    auto mat = new float[16] {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f};
    for (int i = 0; i < 16; i++) {
        mat[i] = chaiscript::boxed_cast<float>(matrix[i]);
    }
    matrices[index] = Matrix4(mat);
}

void chai_scene::finalize() {
    // Perform any finalization steps if needed
}

void chai_scene::draw() {
    auto cg = ChaiLove::getInstance()->chai_gfx;
    cg.instance->setShader(sceneShader->shader);
    cg.instance->setDepthMode(gfx::CompareMode::COMPARE_LEQUAL, true);
    
    std::vector<chaiscript::Boxed_Value> projectionMatrix;
    
    int i = 0;
    for (auto mesh : meshes) {
        
        if (i == 0) { 
            auto cameraParams = mesh->cameraParams;

            float fov = cameraParams.at("fov")[0];
            float aspectRatio = cameraParams.at("aspectRatio")[0];
            float nearClip = cameraParams.at("near")[0];
            float farClip = cameraParams.at("far")[0];
            auto t2 = glm::perspective(fov, aspectRatio, nearClip, farClip);
            auto pm = glm::value_ptr(t2);

            for (int c = 0; c < 16; ++c) {
                projectionMatrix.push_back(chaiscript::Boxed_Value(pm[c]));
            }
            sceneShader->send("projectionMatrix", projectionMatrix);
        }
        gfx::OptionalColorD clearcolor;
        OptionalInt clearstencil(0);
        OptionalDouble cleardepth(1.0);
        cg.instance->clear(clearcolor, clearstencil, cleardepth);
        auto matrix = matrices[i];
        mesh->draw(cg.instance, matrix, sceneShader);
        i++;
    }
    cg.instance->setShader();
}
} // namespace love