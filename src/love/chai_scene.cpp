#include "chai_scene.h"
#include "../ChaiLove.h"

namespace love {
chai_scene::chai_scene() {
    
}
chai_scene::~chai_scene() {
    
}

bool chai_scene::destroy() {
    // sceneShader->shader->~Shader();
    return true;
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
    if (sceneShader != nullptr) {
        sceneShader->shader->~Shader();
    }
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

void chai_scene::drawMeshes(bool shadows) {
    int i = 0;
    gfx::OptionalColorD clearcolor;
    OptionalInt clearstencil(0);
    OptionalDouble cleardepth(1.0);
    auto cg = ChaiLove::getInstance()->chai_gfx;
    cg.instance->clear(clearcolor, clearstencil, cleardepth);
    for (auto mesh : meshes) {
        
        if (i == 0) { 
            auto cameraParams = mesh->cameraParams;

            float fov = cameraParams.at("fov")[0];
            float aspectRatio = cameraParams.at("aspectRatio")[0];
            float nearClip = cameraParams.at("near")[0];
            float farClip = cameraParams.at("far")[0];
            auto t2 = glm::perspective(fov, aspectRatio, nearClip, farClip);
            auto pm = glm::value_ptr(t2);

            auto lightParams = mesh->lightParams;

            auto direction = std::vector<chaiscript::Boxed_Value>();
            for (auto axis : lightParams["direction"]) {
                direction.push_back(chaiscript::Boxed_Value(axis));
            }
            sceneShader->send("lightDirection", direction);

            auto color = std::vector<chaiscript::Boxed_Value>();
            for (auto c : lightParams["color"]) {
                color.push_back(chaiscript::Boxed_Value(c));
            }
            sceneShader->send("lightColor", color);

            auto ambientColor = std::vector<chaiscript::Boxed_Value>();

            ambientColor.push_back(chaiscript::Boxed_Value(1.0f));
            ambientColor.push_back(chaiscript::Boxed_Value(1.0f));
            ambientColor.push_back(chaiscript::Boxed_Value(1.0f));

            sceneShader->send("ambientColor", ambientColor);

            auto intensity = std::vector<chaiscript::Boxed_Value>();
            intensity.push_back(chaiscript::Boxed_Value(lightParams["intensity"][0]));

            sceneShader->send("lightIntensity", intensity);

            // Calculate lightSpaceMatrix
            glm::vec3 lightPos = glm::vec3(lightParams["position"][0], lightParams["position"][1], lightParams["position"][2]);
            // glm::vec3 lightDir = glm::vec3(lightParams["direction"][0], lightParams["direction"][1], lightParams["direction"][2]);
            glm::vec3 lightDir = glm::vec3(0.0f, -1.0f, 0.0f);
            glm::mat4 lightView = glm::lookAt(lightPos, lightPos + lightDir, glm::vec3(0.0f, 0.0f, -1.0f));
            glm::mat4 lightProjection = glm::ortho(-7.5f, 7.5f, -7.5f, 7.5f, 1.0f, 1000.0f);
            glm::mat4 lightSpaceMatrix = lightProjection * lightView;

            auto lightSpaceMatrixBoxed = std::vector<chaiscript::Boxed_Value>();
            for (int i = 0; i < 16; ++i) {
                lightSpaceMatrixBoxed.push_back(chaiscript::Boxed_Value(glm::value_ptr(lightSpaceMatrix)[i]));
            }
            sceneShader->send("lightSpaceMatrix", lightSpaceMatrixBoxed);

            std::vector<chaiscript::Boxed_Value> projectionMatrix;
            if (shadows == true) {            
                for (int i = 0; i < 16; ++i) {
                    projectionMatrix.push_back(chaiscript::Boxed_Value(glm::value_ptr(lightProjection)[i]));
                }
            } else {
                for (int c = 0; c < 16; ++c) {
                    projectionMatrix.push_back(chaiscript::Boxed_Value(pm[c]));
                }
            }
            sceneShader->send("projectionMatrix", projectionMatrix);

            if (shadows == true) {
                sceneShader->send("shadow", std::vector<chaiscript::Boxed_Value>({ chaiscript::Boxed_Value(1) }));
                auto mat = sceneShader->shader->getUniformInfo("viewMatrix");
                auto data = mat->floats;
                for (int i = 0; i < 16; ++i) {
                    viewMatrix.push_back(chaiscript::Boxed_Value(data[i]));
                }
                std::vector<chaiscript::Boxed_Value> vm;
                for (int i = 0; i < 16; ++i) {
                    vm.push_back(chaiscript::Boxed_Value(glm::value_ptr(lightView)[i]));
                }
                sceneShader->send("viewMatrix", vm);
            } else {
                sceneShader->send("viewMatrix", viewMatrix);
                viewMatrix.clear();
            }            
        }
                
        auto matrix = matrices[i];
        mesh->draw(cg.instance, matrix, sceneShader, currentTime);
        i++;
    }
}

void chai_scene::draw() {
    auto cg = ChaiLove::getInstance()->chai_gfx;

    if (false && cg.reinit) {
        cg.hasReinit();
        printf("Reinit\n");
    } else {
        // cg.instance->setActive(true);
        // cg.instance->setShader();
        cg.instance->setShader(sceneShader->shader);
        cg.instance->setDepthMode(gfx::CompareMode::COMPARE_LEQUAL, true);

        if (shadowMapFBO != 0) {
            glDeleteFramebuffers(1, &shadowMapFBO);
        }

        // Create depth texture
        glGenFramebuffers(1, &shadowMapFBO);
        if (shadowMap == 0) {
            glGenTextures(1, &shadowMap); 
        }    
        
        glBindTexture(GL_TEXTURE_2D, shadowMap);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, cg.width-5, cg.height-5, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        GLfloat borderColor[] = { 1.0, 1.0, 1.0, 1.0 };
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

        // Attach depth texture as FBO's depth buffer
        glBindFramebuffer(cg.instance->FRAMEBUFFER, shadowMapFBO);
        glFramebufferTexture2D(cg.instance->FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowMap, 0);
        glClear(GL_DEPTH_BUFFER_BIT);

        GLuint err = glGetError();
        if (err != GL_NO_ERROR) {
            printf("ERROR: %d\n", err);
            printf("shadowMapFBO: %d\n", shadowMapFBO);
            printf("shadowMap: %d\n", shadowMap);
            printf("cg.width: %d\n", cg.width);
            printf("cg.height: %d\n", cg.height);
            printf("sceneShader: %d\n", sceneShader->shader);
            printf("cg.instance->FRAMEBUFFER: %d\n", cg.instance->FRAMEBUFFER);
        } else {
            
            currentTime += 0.01f;

            drawMeshes(true);

            glBindFramebuffer(cg.instance->FRAMEBUFFER, cg.instance->hw_render.get_current_framebuffer());
            gfx::OptionalColorD clearcolor;
            clearcolor = ColorD(0.0, 0.0, 0.0, 1.0); // Set the clear color to black with full opacity
            OptionalInt clearstencil(0);
            OptionalDouble cleardepth(1.0);
            cg.instance->clear(clearcolor, clearstencil, cleardepth);
            
            drawMeshes(false);
            
            cg.instance->setShader();
        }
    }
}

chai_scene *chai_scene::clone() const
{
	return new chai_scene(*this);
}

chai_scene *chai_scene::newScene() const
{
	return new chai_scene();
}
} // namespace love