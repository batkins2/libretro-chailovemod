#include "chai_scene.h"
#include "../ChaiLove.h"

#include <memplumber.h>

namespace love {
chai_scene::chai_scene() {
    
}
chai_scene::~chai_scene() {
    // Explicitly clear all cache vectors to ensure proper cleanup
    viewMatrix.clear();  // Member variable, not just cache
    m_lightDirectionCache.clear();
    m_lightColorCache.clear();
    m_ambientColorCache.clear();
    m_intensityCache.clear();
    m_projectionMatrixCache.clear();
    m_lightSpaceMatrixCache.clear();
    m_vmCache.clear();
    m_modelMatrixCache.clear();
    m_lightIntensityCache.clear();
    m_ambientColorCache2.clear();
    m_projectionMatrixCache2.clear();
    m_viewMatrixCache.clear();
    m_shadowCache.clear();
    m_viewMatrixCache2.clear();
    m_jointInfoCache.clear();
    m_projectionMatrixBoxedCache.clear();
    m_viewMatrixBoxedCache.clear();
}

bool chai_scene::destroy() {
    sceneShader->shader->~Shader();
    sceneShader = nullptr;
    printf("Destroying scene\n");
    meshes = std::vector<chai_mesh *>();
    // if (shadowMapFBO != 0) {
    //     glDeleteFramebuffers(1, &shadowMapFBO);
    // }
    // if (shadowMap != 0) {
    //     glDeleteTextures(1, &shadowMap);
    // }
    // // Clean up scene framebuffer
    // if (sceneFramebuffer != 0) {
    //     glDeleteFramebuffers(1, &sceneFramebuffer);
    // }
    // if (sceneColorTexture != 0) {
    //     glDeleteTextures(1, &sceneColorTexture);
    // }
    // if (sceneDepthTexture != 0) {
    //     glDeleteTextures(1, &sceneDepthTexture);
    // }
    return true;
}

void chai_scene::addMesh(chai_mesh *mesh) {
    // mesh->getBoundingBox(glm::mat4(1.0f));
    printf("Adding mesh to scene %p\n", mesh);
    meshes.push_back(mesh);
    float arr[16] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f};
    matrices.push_back(Matrix4(arr));
    meshGroups[mesh->getId()] += 1;
}

void chai_scene::addChildMesh(chai_mesh *pmesh, chai_mesh *cmesh) {
    printf("Adding child mesh %p to parent mesh %p\n", cmesh, pmesh);
    auto it = std::find(meshes.begin(), meshes.end(), pmesh);
    if (it != meshes.end()) {
        int index = it - meshes.begin();
        meshChildren[index].emplace_back(cmesh);
        meshGroups[cmesh->getId()] += 1;
    } else {
        printf("Parent mesh not found in scene.\n");
    }
}

void chai_scene::removeChildMesh(chai_mesh *pmesh, chai_mesh *cmesh) {
    auto it = std::find(meshes.begin(), meshes.end(), pmesh);
    if (it != meshes.end()) {
        int index = it - meshes.begin();
        auto& children = meshChildren[index];
        children.erase(std::remove(children.begin(), children.end(), cmesh), children.end());
    } else {
        printf("Parent mesh not found in scene.\n");
    }
}

void chai_scene::addParticleSystem(chai_particles *ps) {
    particleSystems.push_back(ps);
}

void chai_scene::hideMesh(chai_mesh *mesh) {
    auto it = std::find(meshes.begin(), meshes.end(), mesh);
    if (it != meshes.end()) {
        meshes[it - meshes.begin()]->setVisible(false);
    }
}

void chai_scene::showMesh(chai_mesh *mesh) {
    auto it = std::find(meshes.begin(), meshes.end(), mesh);
    if (it != meshes.end()) {
        meshes[it - meshes.begin()]->setVisible(true);
    }
}

void chai_scene::setShader(chai_shader *shader, chai_shader *compute) {
    computeShader = compute;
    sceneShader = shader;   
}

void chai_scene::setMatrix(const glm::mat4 &matrix, int index) {
    // Convert glm::mat4 to Matrix4
    float mat[16];
    const float *ptr = glm::value_ptr(matrix);
    for (int i = 0; i < 16; i++) {
        mat[i] = ptr[i];
    }
    matrices[index] = Matrix4(mat);
}

void chai_scene::finalize() {
    // Perform any finalization steps if needed
}

bool isMeshInFrustum(chai_mesh *mesh, const glm::mat4 &viewProjectionMatrix) {
    // Get the mesh's bounding box
    auto bounds = mesh->getBoundingBox(viewProjectionMatrix);
    auto minBounds = bounds.first;
    auto maxBounds = bounds.second;

    // Extract frustum planes from the view-projection matrix
    glm::vec4 planes[6];
    // Extract frustum planes using the rows of the matrix (glm is column-major)
    glm::mat4 m = viewProjectionMatrix;
    planes[0] = glm::vec4(m[0][3] + m[0][0], m[1][3] + m[1][0], m[2][3] + m[2][0], m[3][3] + m[3][0]); // Left
    // printf("Frustum Plane 0: %f, %f, %f, %f\n", planes[0].x, planes[0].y, planes[0].z, planes[0].w);
    planes[1] = glm::vec4(m[0][3] - m[0][0], m[1][3] - m[1][0], m[2][3] - m[2][0], m[3][3] - m[3][0]); // Right
    // printf("Frustum Plane 1: %f, %f, %f, %f\n", planes[1].x, planes[1].y, planes[1].z, planes[1].w);
    planes[2] = glm::vec4(m[0][3] + m[0][1], m[1][3] + m[1][1], m[2][3] + m[2][1], m[3][3] + m[3][1]); // Bottom
    // printf("Frustum Plane 2: %f, %f, %f, %f\n", planes[2].x, planes[2].y, planes[2].z, planes[2].w);
    planes[3] = glm::vec4(m[0][3] - m[0][1], m[1][3] - m[1][1], m[2][3] - m[2][1], m[3][3] - m[3][1]); // Top
    // printf("Frustum Plane 3: %f, %f, %f, %f\n", planes[3].x, planes[3].y, planes[3].z, planes[3].w);
    planes[4] = glm::vec4(m[0][3] + m[0][2], m[1][3] + m[1][2], m[2][3] + m[2][2], m[3][3] + m[3][2]); // Near
    // printf("Frustum Plane 4: %f, %f, %f, %f\n", planes[4].x, planes[4].y, planes[4].z, planes[4].w);
    planes[5] = glm::vec4(m[0][3] - m[0][2], m[1][3] - m[1][2], m[2][3] - m[2][2], m[3][3] - m[3][2]); // Far
    // printf("Frustum Plane 5: %f, %f, %f, %f\n", planes[5].x, planes[5].y, planes[5].z, planes[5].w);

    // Normalize the planes
    for (int i = 0; i < 6; i++) {
        float length = glm::length(glm::vec3(planes[i]));
        planes[i] /= length;
    }

    // Check if the bounding box is outside any plane
    for (int i = 0; i < 6; i++) {
        glm::vec3 normal = glm::vec3(planes[i]);
        float distance = planes[i].w;
        // printf("Normalized Frustum Plane %d: %f, %f, %f, %f\n", i, normal.x, normal.y, normal.z, distance);
        // printf("Mesh Bounds: Min(%f, %f, %f), Max(%f, %f, %f)\n", minBounds.x, minBounds.y, minBounds.z, maxBounds.x, maxBounds.y, maxBounds.z);
        glm::vec3 farPoint = glm::vec3(
            (normal.x > 0) ? maxBounds.x : minBounds.x,
            (normal.y > 0) ? maxBounds.y : minBounds.y,
            (normal.z > 0) ? maxBounds.z : minBounds.z);
        // printf("Far Point for Plane %d: %f, %f, %f\n", i, farPoint.x, farPoint.y, farPoint.z);
        float farthestPointDistance = glm::dot(normal, farPoint) + distance;
        // printf("Farthest Point Distance to Plane %d: %f\n", i, farthestPointDistance);
        // If the farthest point is outside the plane, the box is outside the frustum
        if (farthestPointDistance < 0) {
            return false; // Completely outside
        }
    }

    return true; // Inside or intersecting
}

void chai_scene::drawMeshes(bool shadows, int view) {
    // if (shadows) {
    //     printf("Drawing shadows\n");
    // } else {
    //     printf("Drawing scene\n");
    // }
    int i = 0;
    // gfx::OptionalColorD clearcolor;
    // clearcolor = ColorD(0.0, 0.0, 0.0, 1.0); // Set the clear color to black with full opacity
    // OptionalInt clearstencil(0);
    // OptionalDouble cleardepth(1.0);
    auto& cg = ChaiLove::getInstance()->chai_gfx;
    // Get the command buffer and pipeline layout properly
    // auto commandBuffer = cg.instance->getCommandBufferForDataTransfer();
    // auto vulkanShader = static_cast<gfx::vulkan::Shader*>(sceneShader->shader);
    // auto pipelineLayout = vulkanShader->getGraphicsPipelineLayout();
    // cg.instance->clear(clearcolor, clearstencil, cleardepth);
    glm::mat4 vMatrix = glm::mat4(1.0f);
    glm::mat4 t2 = glm::mat4(1.0f);

    // glEnable(GL_BLEND);
    // glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    if (!shadows) {
        auto mesh = meshes[0];
        if (mesh->specData == nullptr) {
            mesh->loadSpecular("");
        }

        // tex = cg.instance->newTexture(settings, &slices);
        // tex->replacePixels(copyOfPixelData, dataSize*4, 0, 0, rect, false);
        
        // if (mesh->specularMap == 0) {
        
        //     // Create sampler2d specularMap
        //     glGenTextures(1, &mesh->specularMap);
        // }
        
        // Bind the specular map texture
        // glActiveTexture(GL_TEXTURE2); // Use texture unit 2 for specular map                        
        // glBindTexture(GL_TEXTURE_2D, mesh->specularMap);

        // Set the sampler uniform in your shader to use texture unit 0
        // GLint specularMapLoc = glGetUniformLocation(sceneShader->shader->getHandle(), "specularMap");
        // if (specularMapLoc >= 0) {
        //     glUniform1i(specularMapLoc, 2);
        // }
        // Set texture parameters (adjust as needed)
        // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // Load your specular map image data here (replace with your actual loading code)
        
        if (mesh->specData) {
            // glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, mesh->specularW, mesh->specularH, 0, GL_RGBA, GL_UNSIGNED_BYTE, mesh->specData);
            // glGenerateMipmap(GL_TEXTURE_2D);
            // Free specData if needed
        }

        // Set the sampler uniform in your shader to use texture unit 2
        // GLint specularMapLoc = glGetUniformLocation(sceneShader->shader->getHandle(), "specularMap");
        // if (specularMapLoc >= 0) {
        //     glUniform1i(specularMapLoc, 2); // 2 = GL_TEXTURE2
        // }

        // sceneShader->send("isSpecular", std::vector<chaiscript::Boxed_Value>({ chaiscript::Boxed_Value(1) }));

        
        // Unbind the texture
        // glBindTexture(GL_TEXTURE_2D, 0);
        // glActiveTexture(GL_TEXTURE0); // Switch back to texture unit 0 for the main texture
        // glDisable(GL_DEPTH_TEST);
        // auto mat = Matrix4(new float[16] {
        //     1.0f, 0.0f, 0.0f, 0.0f,
        //     0.0f, 1.0f, 0.0f, 0.0f,
        //     0.0f, 0.0f, 1.0f, 0.0f,
        //     0.0f, 0.0f, -1.0f, 1.0f
        // });
        // auto v = std::vector<chaiscript::Boxed_Value>();
        // for (int c = 0; c < 4; ++c) {
        //     v.push_back(chaiscript::Boxed_Value(mat.getColumn(c).x));
        //     v.push_back(chaiscript::Boxed_Value(mat.getColumn(c).y));
        //     v.push_back(chaiscript::Boxed_Value(mat.getColumn(c).z));
        //     v.push_back(chaiscript::Boxed_Value(mat.getColumn(c).w));
        // }
        // auto modelIdx = sceneShader->send("modelMatrix", v);

        printf("Testing for memory leaks around shader uniform updates\n");
        size_t leakCountBefore, leakCountAfter;
        uint64_t leakSizeBefore, leakSizeAfter;
        
        __mem_leak_check(leakCountBefore, leakSizeBefore, false, "", false);
        
        m_lightIntensityCache.clear();
        m_lightIntensityCache.push_back(glm::vec3(1.2f));
        sceneShader->send("lightIntensity", 1.2f);

        __mem_leak_check(leakCountAfter, leakSizeAfter, false, "", false);
    
        printf("Leak delta: %zu objects, %llu bytes\n", 
            leakCountAfter - leakCountBefore,
            leakSizeAfter - leakSizeBefore);

        m_ambientColorCache2.clear();
        m_ambientColorCache2.push_back(glm::vec3(0.9f, 0.9f, 0.9f));
        sceneShader->send("ambientColor", glm::vec3(0.9f, 0.9f, 0.9f));

        // Ortho projection matrix for fullscreen quad
        // This assumes the quad covers the entire screen, adjust as needed
        auto t2 = glm::ortho(0.0f, static_cast<float>(mesh->specularW),
            static_cast<float>(mesh->specularH), 0.0f, -1.0f, 1.0f);
        auto pm = glm::value_ptr(t2);
        printf("Testing for memory leaks around shader uniform updates\n");
        m_projectionMatrixCache2.clear();
        __mem_leak_check(leakCountBefore, leakSizeBefore, false, "", false);
        m_projectionMatrixCache2.push_back(t2);
        sceneShader->send("projectionMatrix", m_projectionMatrixCache2);
        __mem_leak_check(leakCountAfter, leakSizeAfter, false, "", false);
    
        printf("Leak delta: %zu objects, %llu bytes\n", 
            leakCountAfter - leakCountBefore,
            leakSizeAfter - leakSizeBefore);

        // Set the view matrix to identity for fullscreen quad
        auto vMatrix = glm::mat4(1.0f);
        m_viewMatrixCache.clear();
        // m_viewMatrixCache.push_back(vMatrix);
        // sceneShader->send("viewMatrix", m_viewMatrixCache[0]);

        // Draw a fullscreen quad (replace with your engine's quad draw if needed)
        // gfx::Texture::Settings settings;
        // settings.width = mesh->specularW;
        // settings.height = mesh->specularH;
        // settings.format = PIXELFORMAT_RGBA8_UNORM;
        // auto slices = gfx::Texture::Slices(gfx::TextureType::TEXTURE_2D);
        // auto gfx = Module::getInstance<gfx::Graphics>(Module::M_GRAPHICS);
        // if (background_tex == nullptr) {
        //     background_tex = gfx->newTexture(settings, &slices);
        
        
        //     Rect rect = Rect();
        //     rect.w = mesh->specularW;
        //     rect.h = mesh->specularH;
        //     background_tex->replacePixels(mesh->specData, mesh->specularW*mesh->specularH*4, 0, 0, rect, false);
        // }
        // // sceneShader->send("jointCount", std::vector<chaiscript::Boxed_Value>({ chaiscript::Boxed_Value(0) }));

        // sceneShader->sendConstant("jointInfo", std::vector<chaiscript::Boxed_Value>({ chaiscript::Boxed_Value(0.0f), chaiscript::Boxed_Value(0.0f), chaiscript::Boxed_Value((float)modelIdx), chaiscript::Boxed_Value(0.0f) }));
        // // MVP matrix
        // Matrix4 mvpMat = Matrix4(
        //     new float[16] {
        //         t2[0][0], t2[0][1], t2[0][2], t2[0][3],
        //         t2[1][0], t2[1][1], t2[1][2], t2[1][3],
        //         t2[2][0], t2[2][1], t2[2][2], t2[2][3],
        //         t2[3][0], t2[3][1], t2[3][2], t2[3][3]
        //     }
        // );
        // background_tex->draw3D(gfx, mvpMat, Colorf(1.0f, 1.0f, 1.0f, 1.0f));
    
        // glEnable(GL_DEPTH_TEST);
    }

    glm::mat4 viewProjectionMatrix = t2 * vMatrix;

    std::vector<int> deferredChildIndices;
    std::vector<int> deferredParentIndices;
    std::vector<int> deferredMeshIndices;

    for (auto mesh : meshes) {
        
        if (i == 0) {
            auto cameraParams = mesh->cameraParams[view];

            float fov = cameraParams.at("fov")[0];
            // printf("FOV: %f\n", fov);
            float aspectRatio = cameraParams.at("aspectRatio")[0];
            // printf("Aspect Ratio: %f\n", aspectRatio);
            float nearClip = cameraParams.at("near")[0];
            float farClip = cameraParams.at("far")[0];
            t2 = glm::perspective(fov, aspectRatio, nearClip, farClip);
            auto pm = glm::value_ptr(t2);

            auto lightParams = mesh->lightParams[view];

            m_lightDirectionCache.clear();
            // TODO: lightParams returns what type? Store as vec3
            for (auto axis : lightParams["direction"]) {
                m_lightDirectionCache.push_back(glm::vec3(axis));
            }
            sceneShader->send("lightDirection", m_lightDirectionCache[0]);

            m_lightColorCache.clear();
            // TODO: lightParams returns what type? Store as vec3
            for (auto c : lightParams["color"]) {
                m_lightColorCache.push_back(glm::vec3(c));
            }
            sceneShader->send("lightColor", m_lightColorCache[0]);

            m_ambientColorCache.clear();
            m_ambientColorCache.push_back(glm::vec3(1.0f, 1.0f, 1.0f));
            // TODO: Need send overload for vec3
            sceneShader->send("ambientColor", m_ambientColorCache[0]);

            m_intensityCache.clear();
            // TODO: lightParams["intensity"][0] returns what type?
            // m_intensityCache.push_back(glm::vec3());
            sceneShader->send("lightIntensity", lightParams["intensity"][0]);

            // Calculate lightSpaceMatrix
            glm::vec3 lightPos = glm::vec3(lightParams["position"][0], lightParams["position"][1], lightParams["position"][2]);
            // glm::vec3 lightDir = glm::vec3(lightParams["direction"][0], lightParams["direction"][1], lightParams["direction"][2]);
            glm::vec3 lightDir = glm::vec3(0.0f, -1.0f, 0.0f);
            glm::mat4 lightView = glm::lookAt(lightPos, lightPos + lightDir, glm::vec3(0.0f, 0.0f, -1.0f));
            glm::mat4 lightProjection = glm::ortho(-7.5f, 7.5f, -7.5f, 7.5f, 1.0f, 1000.0f);
            glm::mat4 lightSpaceMatrix = lightProjection * lightView;

            m_lightSpaceMatrixCache.clear();
            m_lightSpaceMatrixCache.push_back(lightSpaceMatrix);
            sceneShader->send("lightSpaceMatrix", m_lightSpaceMatrixCache[0]);

            m_projectionMatrixCache.clear();
            if (shadows == true) {
                m_projectionMatrixCache.push_back(lightProjection);
            } else {
                m_projectionMatrixCache.push_back(t2);
            }
            sceneShader->send("projectionMatrix", m_projectionMatrixCache);

            if (shadows == true) {
                sceneShader->sendInt("shadow", 1);
                auto mat = sceneShader->shader->getUniformInfo("viewMatrix");
                auto data = mat->floats;
                vMatrix = glm::mat4(
                    data[0], data[1], data[2], data[3],
                    data[4], data[5], data[6], data[7],
                    data[8], data[9], data[10], data[11],
                    data[12], data[13], data[14], data[15]
                );
                glm::mat4 viewMat = glm::mat4(
                    data[0], data[1], data[2], data[3],
                    data[4], data[5], data[6], data[7],
                    data[8], data[9], data[10], data[11],
                    data[12], data[13], data[14], data[15]
                );
                m_viewMatrixCache2.clear();
                m_viewMatrixCache2.push_back(viewMat);
                m_vmCache.clear();
                m_vmCache.push_back(lightView);
                sceneShader->send("viewMatrix", m_vmCache[0]);
                viewProjectionMatrix = lightSpaceMatrix;
            } else {
                sceneShader->send("viewMatrix", viewMatrix);
                viewMatrix.clear();
                auto mat = sceneShader->shader->getUniformInfo("viewMatrix");
                auto data = mat->floats;
                if (data == nullptr) {
                    vMatrix = glm::mat4(1.0f);
                } else {
                    vMatrix = glm::mat4(
                        data[0], data[1], data[2], data[3],
                        data[4], data[5], data[6], data[7],
                        data[8], data[9], data[10], data[11],
                        data[12], data[13], data[14], data[15]
                    );
                }
                viewProjectionMatrix = t2 * vMatrix;

                gfx::Texture::Settings settings;
                settings.width = mesh->specularW;
                settings.height = mesh->specularH;
                settings.format = PIXELFORMAT_RGBA8_UNORM;
                auto slices = gfx::Texture::Slices(gfx::TextureType::TEXTURE_2D);
                auto gfx = Module::getInstance<gfx::Graphics>(Module::M_GRAPHICS);
                if (background_tex == nullptr) {
                    background_tex = gfx->newTexture(settings, &slices);
                
                
                    Rect rect = Rect();
                    rect.w = mesh->specularW;
                    rect.h = mesh->specularH;
                    background_tex->replacePixels(mesh->specData, mesh->specularW*mesh->specularH*4, 0, 0, rect, false);
                }
                // sceneShader->send("jointCount", std::vector<chaiscript::Boxed_Value>({ chaiscript::Boxed_Value(0) }));

                auto xpos = cameraParams.at("position")[0];
                auto ypos = cameraParams.at("position")[1];
                auto zpos = cameraParams.at("position")[2];

                auto height = 2 * zpos * tan(fov / 2.0f);
                auto width = aspectRatio * height;

                float bmatData[16] = {
                    width, 0.0f, 0.0f, 0.0,
                    0.0f, height, 0.0f, 0.0f,
                    0.0f, 0.0f, 1.0f, 0.0f,
                    -data[12], ypos*(1.0f/3.0f), -zpos*0.9f, 1.0f
                };
                auto bmat = Matrix4(bmatData);
                glm::mat4 modelMat = glm::mat4(
                    bmat.getColumn(0).x, bmat.getColumn(0).y, bmat.getColumn(0).z, bmat.getColumn(0).w,
                    bmat.getColumn(1).x, bmat.getColumn(1).y, bmat.getColumn(1).z, bmat.getColumn(1).w,
                    bmat.getColumn(2).x, bmat.getColumn(2).y, bmat.getColumn(2).z, bmat.getColumn(2).w,
                    bmat.getColumn(3).x, bmat.getColumn(3).y, bmat.getColumn(3).z, bmat.getColumn(3).w
                );
                m_modelMatrixCache.clear();
                m_modelMatrixCache.push_back(modelMat);
                

                auto modelIdx = sceneShader->send("modelMatrix", m_modelMatrixCache[0]);
                // size_t leakCountBefore, leakCountAfter;
                // uint64_t leakSizeBefore, leakSizeAfter;
                
                // __mem_leak_check(leakCountBefore, leakSizeBefore, false, "", false);
                std::vector<glm::vec4> jointInfoVec = {
                    glm::vec4(0.0f, 0.0f, (float)modelIdx, 0.0f)
                };
                sceneShader->sendConstant("jointInfo", jointInfoVec);
                // __mem_leak_check(leakCountAfter, leakSizeAfter, false, "", false);
    
                // printf("Leak delta: %zu objects, %llu bytes\n", 
                //     leakCountAfter - leakCountBefore,
                    // leakSizeAfter - leakSizeBefore);
                // float verticalfov = fov;
                // float aspect = aspectRatio;
                // float n = nearClip;
                // float f = farClip;
                
                // Matrix4 mvpMat = mvpMat.perspective(verticalfov, aspect, n, f);
                // mvpMat = Matrix4(
                //     new float[16] {
                //         data[0], data[1], data[2], data[3],
                //         data[4], data[5], data[6], data[7],
                //         data[8], data[9], data[10], data[11],
                //         data[12], data[13], data[14], data[15]
                //     }
                // ) * mvpMat;


                // mvpMat = mvpMat * Matrix4(new float[16] {
                //     t2[0][0], t2[0][1], t2[0][2], t2[0][3],
                //     t2[1][0], t2[1][1], t2[1][2], t2[1][3],
                //     t2[2][0], t2[2][1], t2[2][2], t2[2][3],
                //     t2[3][0], t2[3][1], t2[3][2], t2[3][3]
                // });
                
                float iMatArr[16] = {
                    1.0f, 0.0f, 0.0f, 0.0f,
                    0.0f, 1.0f, 0.0f, 0.0f,
                    0.0f, 0.0f, 1.0f, 0.0f,
                    0.0f, 0.0f, 0.0f, 1.0f
                };
                auto iMat = Matrix4(iMatArr);

                background_tex->draw3D(gfx, iMat, Colorf(1.0f, 1.0f, 1.0f, 1.0f));
            }
            // printf("viewMatrix: \n");
            // for (int r = 0; r < 4; ++r) {
            //     printf("%f, %f, %f, %f\n", vMatrix[r][0], vMatrix[r][1], vMatrix[r][2], vMatrix[r][3]);
            // }
        }

        if (mesh->visible == false) {
            // printf("Mesh %d is not visible\n", i);
            i++;
            continue;
        }

        // auto mat = sceneShader->shader->getUniformInfo("viewMatrix");
        // auto data = mat->floats;
        // vMatrix = glm::mat4(
        //     data[0], data[1], data[2], data[3],
        //     data[4], data[5], data[6], data[7],
        //     data[8], data[9], data[10], data[11],
        //     data[12], data[13], data[14], data[15]
        // );
        // Perform frustum culling
        if (!isMeshInFrustum(mesh, viewProjectionMatrix)) {
            // printf("Mesh %d is outside the frustum\n", i);
            i++;            
            continue; // Skip meshes outside the frustum
        }

        bool defer = false;

        if (meshChildren.find(i) != meshChildren.end()) {
            int j = 0;
            for (auto child : meshChildren[i]) {
                if (child->visible == false) {
                    j++;
                    continue;
                }
                if (meshGroups[child->getId()] > 1) {
                    deferredChildIndices.push_back(j);
                    deferredParentIndices.push_back(i);
                    defer = true;
                }
                j++;
            }
            if (defer) {
                i++;
                continue;
            }
        }

        if (meshGroups[mesh->getId()] > 1) {
            deferredMeshIndices.push_back(i);
            i++;
            continue;
        }
        // sceneShader->send("jointCount", std::vector<chaiscript::Boxed_Value>({ chaiscript::Boxed_Value(0) }));

        // gfx::Graphics::flushBatchedDrawsGlobal();
        auto matrix = matrices[i];
        mesh->draw(cg.instance, matrix, sceneShader, deltaTime, computeShader);
        if (meshChildren.find(i) != meshChildren.end() && computeShader == nullptr) {
            // if (i == 4) {
            //     printf("Mesh %p breakpoint\n", mesh);
            //     isMeshInFrustum(mesh, viewProjectionMatrix);
            // }
            for (auto child : meshChildren[i]) {
                // printf("Drawing child mesh %p of parent mesh %p\n", child, mesh);
                if (child->visible == false) {
                    continue;
                }
                for (int j = 0; j < child->meshes.size(); ++j) {
                    float mArr[16] = {
                        1.0f, 0.0f, 0.0f, 0.0f,
                        0.0f, 1.0f, 0.0f, 0.0f,
                        0.0f, 0.0f, 1.0f, 0.0f,
                        0.0f, 0.0f, 0.0f, 1.0f
                    };
                    Matrix4 m = Matrix4(mArr);
                    // sceneShader->send("jointCount", std::vector<chaiscript::Boxed_Value>({ chaiscript::Boxed_Value(0) }));

                    child->meshes[j]->draw(cg.instance, m);
                }
            }
        }
        // cg.instance->present(nullptr);
        i++;
    }
    // Draw deferred child meshes in order of same index
    i = 0;
    std::vector<std::pair<int, int>> drawnPairs;
    for (auto childIndex : deferredChildIndices) {
        // printf("Drawing deferred child mesh index %d of parent mesh index %d\n", childIndex, deferredParentIndices[i]);
        // auto vbo = meshChildren[deferredParentIndices[i]][childIndex]->cachedVBOs.begin()->second;
        // printf("Using VBO %d\n", vbo);
        // glBindBuffer(GL_ARRAY_BUFFER, vbo);
        // printf("Buffer bound\n");
        for (auto parentId : deferredParentIndices) {
            int j = 0;
            for (auto child : meshChildren[parentId]) {
                if (child->getId() == meshChildren[deferredParentIndices[i]][childIndex]->getId()) {
                    if (child->visible == false || drawnPairs.end() != std::find(drawnPairs.begin(), drawnPairs.end(), std::make_pair(parentId, j))) {
                        j++;
                        continue;
                    }
                    auto drawnPair = std::make_pair(parentId, j);
                    // printf("Drawing child mesh %d of parent mesh %d\n", j, parentId);
                    drawnPairs.push_back(drawnPair);
                    // sceneShader->send("jointCount", std::vector<chaiscript::Boxed_Value>({ chaiscript::Boxed_Value(0) }));
                    // gfx::Graphics::flushBatchedDrawsGlobal();
                   
                    // Draw the parent mesh first
                    meshes[parentId]->draw(cg.instance, matrices[parentId], sceneShader, deltaTime, computeShader);
                    float identityData[16] = {
                        1.0f, 0.0f, 0.0f, 0.0f,
                        0.0f, 1.0f, 0.0f, 0.0f,
                        0.0f, 0.0f, 1.0f, 0.0f,
                        0.0f, 0.0f, 0.0f, 1.0f
                    };
                    Matrix4 m = Matrix4(identityData);
                    // sceneShader->send("jointCount", std::vector<chaiscript::Boxed_Value>({ chaiscript::Boxed_Value(0) }));

                    if (computeShader == nullptr) {
                        child->meshes[j]->draw(cg.instance, m);
                    }
                    // cg.instance->present(nullptr);
                }
                j++;
            }
        }
        i++;
    }

    // printf("Drawing deferred parent meshes\n");

    for (auto meshIndex : deferredMeshIndices) {
        auto mesh = meshes[meshIndex];
        auto matrix = matrices[meshIndex];
        // sceneShader->send("jointCount", std::vector<chaiscript::Boxed_Value>({ chaiscript::Boxed_Value(0) }));
        // gfx::Graphics::flushBatchedDrawsGlobal();

        mesh->draw(cg.instance, matrix, sceneShader, deltaTime, computeShader);
        if (meshChildren.find(meshIndex) != meshChildren.end()) {
            for (auto child : meshChildren[meshIndex]) {
                if (child->visible == false) {
                    continue;
                }
                for (int j = 0; j < child->meshes.size(); ++j) {
                    float identityData[16] = {
                        1.0f, 0.0f, 0.0f, 0.0f,
                        0.0f, 1.0f, 0.0f, 0.0f,
                        0.0f, 0.0f, 1.0f, 0.0f,
                        0.0f, 0.0f, 0.0f, 1.0f
                    };
                    Matrix4 m = Matrix4(identityData);
                    // sceneShader->send("jointCount", std::vector<chaiscript::Boxed_Value>({ chaiscript::Boxed_Value(0) }));
                    if (computeShader == nullptr) {
                        child->meshes[j]->draw(cg.instance, m);
                    }
                }
            }
        }
        // cg.instance->present(nullptr);
    }
    if (shadows == false && computeShader == nullptr) {
        for (auto ps : particleSystems) {
            // if (ps->visible == false) {
            //     continue;
            // }
            // gfx::Graphics::flushBatchedDrawsGlobal();
            ps->draw();
            // cg.instance->present(nullptr);
        }
    }
}

void chai_scene::draw(const glm::mat4 &viewMatrix1, const glm::mat4 &viewMatrix2, const glm::mat4 &viewMatrix3, const glm::mat4 &viewMatrix4, int viewCount) {
    auto& cg = ChaiLove::getInstance()->chai_gfx;
    // ChaiLove::getInstance()->chai_collisions.processDebug(1.0f / 60.0f, viewMatrix1);
    
    // Frame skipping logic
    if (false && skip > 0) {
        if (skip > 1) skip = -1;
        // auto fb = cg.instance->hw_render.get_current_framebuffer();
        // glBindFramebuffer(GL_DRAW_FRAMEBUFFER, sceneFramebuffer);
        // glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fb);
        // glBlitFramebuffer(0, 0, cg.width, cg.height, 
        //                  0, 0, cg.width, cg.height, 
        //                  GL_COLOR_BUFFER_BIT, GL_LINEAR);
        return;
    }

    // Initialize framebuffer if needed
    initFramebuffer();
    
    // Bind to our framebuffer for rendering
    // glBindFramebuffer(GL_FRAMEBUFFER, sceneFramebuffer);
    // glViewport(0, 0, cg.width / 4, cg.height / 4);

    // Clear the framebuffer
    // glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    if (false && cg.reinit) {
        cg.hasReinit();
        printf("Reinit\n");
    } else {  
        
        
        
        
        // sceneShader->shader->setBufferOffset("JointMatrixBlock", 0);
        // drawMeshes(false, 0);
        // cg.instance->dispatchThreadgroups(computeShader->shader, 16, 16, 16);
        // auto cs = computeShader;
        computeShader = nullptr;
        if (viewCount == 1) {
            size_t leakCountBefore, leakCountAfter;
            uint64_t leakSizeBefore, leakSizeAfter;
            
            __mem_leak_check(leakCountBefore, leakSizeBefore, false, "", false);
            cg.instance->setShader(sceneShader->shader);
            cg.instance->setDepthMode(gfx::CompareMode::COMPARE_LEQUAL, true);
            sceneShader->newFrame();
            sceneShader->send("viewMatrix", viewMatrix1);
            drawMeshes(false, 0);
            __mem_leak_check(leakCountAfter, leakSizeAfter, false, "", false);
    
            printf("Leak delta: %zu objects, %llu bytes\n", 
                leakCountAfter - leakCountBefore,
                leakSizeAfter - leakSizeBefore);
        }
        if (viewCount == 2) {
            
            cg.instance->setShader(sceneShader->shader);
            cg.instance->setDepthMode(gfx::CompareMode::COMPARE_LEQUAL, true);
            sceneShader->newFrame();
            sceneShader->send("viewMatrix", viewMatrix1);
            cg.instance->setSplitScreenViewport(0, 2);
            drawMeshes(false, 0);
            sceneShader->send("viewMatrix", viewMatrix2);            
            cg.instance->setSplitScreenViewport(1, 2);
            drawMeshes(false, 1);
        }
        if (viewCount == 3) {
            sceneShader->send("viewMatrix", viewMatrix1);
            drawMeshes(false, 0);
            sceneShader->send("viewMatrix", viewMatrix2);
            drawMeshes(false, 1);
            sceneShader->send("viewMatrix", viewMatrix3);
            drawMeshes(false, 2);
        }
        if (viewCount == 4) {
            sceneShader->send("viewMatrix", viewMatrix1);
            drawMeshes(false, 0);
            sceneShader->send("viewMatrix", viewMatrix2);
            drawMeshes(false, 1);
            sceneShader->send("viewMatrix", viewMatrix3);
            drawMeshes(false, 2);
            sceneShader->send("viewMatrix", viewMatrix4);
            drawMeshes(false, 3);
        }        
        // computeShader = cs;

        // cg.instance->setShader();

        // cg.instance->setShader(sceneShader->shader);
        // sceneShader->newFrame();
        
        // cg.instance->setDepthMode(gfx::CompareMode::COMPARE_LEQUAL, true);
        
        // sceneShader->send("viewMatrix", viewMatrix1);

        // drawMeshes(false, 0);
        // Add this new function after the existing print function:

//         if (!cg.instance || !cg.instance->isCreated()) {
//             std::printf("[CHAILOVE DEBUG] Cannot draw triangle - graphics not created\n");
//             return;
//         }
        
//         std::printf("[CHAILOVE DEBUG] Drawing test triangle...\n");

// // Use the engine's default shader by calling setShader() with no arguments
//         cg.instance->setShader();
//         std::printf("[CHAILOVE DEBUG] Using Love2D default shader (setShader())\n");

// // ...existing code...

// // Replace the triangle test section with this corrected version:

// // Replace the entire triangle test section with this:

// struct TextureVertex {
//     float x, y, z, w;           // position (4 floats = 16 bytes) - shader expects vec4
//     float u, v, s, t;           // texture coords (4 floats = 16 bytes) - shader expects vec4  
//     float r, g, b, a;           // color as floats (4 floats = 16 bytes) - shader expects vec4
//     // Total: 48 bytes per vertex
// };

// // Create a fullscreen triangle that should definitely be visible
// // Using coordinates that work with the projection matrix we saw in uniforms
// TextureVertex vertices[3] = {
//     // Fullscreen triangle covering entire screen
//     // The projection matrix expects large coordinates based on screen size (1440x1080)
//     { 0.0f, -540.0f, 0.0f, 1.0f,   0.5f, 0.0f, 0.0f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f },    // Top - RED
//     {-720.0f, 540.0f, 0.0f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f },    // Left - GREEN  
//     { 720.0f, 540.0f, 0.0f, 1.0f,   1.0f, 1.0f, 0.0f, 1.0f,   0.0f, 0.0f, 1.0f, 1.0f }     // Right - BLUE
// };

// // Create format with proper names matching the engine's expectations
// std::vector<love::gfx::Buffer::DataDeclaration> format;
// format.push_back(love::gfx::Buffer::DataDeclaration("VertexPosition", love::gfx::DATAFORMAT_FLOAT_VEC4));
// format.push_back(love::gfx::Buffer::DataDeclaration("VertexTexCoord", love::gfx::DATAFORMAT_FLOAT_VEC4));  
// format.push_back(love::gfx::Buffer::DataDeclaration("VertexColor", love::gfx::DATAFORMAT_FLOAT_VEC4));

// std::printf("[TRIANGLE TEST] Using fullscreen triangle with screen-space coordinates\n");
// std::printf("[TRIANGLE TEST] Vertex 0: pos(%.1f, %.1f, %.1f, %.1f) color(%.1f, %.1f, %.1f, %.1f)\n", 
//             vertices[0].x, vertices[0].y, vertices[0].z, vertices[0].w,
//             vertices[0].r, vertices[0].g, vertices[0].b, vertices[0].a);

// // Create the buffer with vertex data
// love::gfx::Buffer::Settings bufferSettings(love::gfx::BUFFERUSAGEFLAG_VERTEX, love::gfx::BUFFERDATAUSAGE_STATIC);
// auto buffer = cg.instance->newBuffer(bufferSettings, format, vertices, sizeof(vertices), 0);

// love::gfx::BufferBindings buffers;
// buffers.set(2, buffer, 0);

// // Set vertex attributes - try using the CommonFormat approach instead of manual setup
// love::gfx::VertexAttributes attributes;
// // Clear and reset to ensure clean state
// // attributes.clear();

// // Use the engine's standard attribute locations
// attributes.set(love::gfx::ATTRIB_POS, love::gfx::DATAFORMAT_FLOAT_VEC4, 0, 1);                                    
// attributes.set(love::gfx::ATTRIB_TEXCOORD, love::gfx::DATAFORMAT_FLOAT_VEC4, sizeof(float) * 4, 1);                   
// attributes.set(love::gfx::ATTRIB_COLOR, love::gfx::DATAFORMAT_FLOAT_VEC4, sizeof(float) * 8, 1);                   
// attributes.setBufferLayout(1, sizeof(TextureVertex), love::gfx::STEP_PER_VERTEX);               

// std::printf("[TRIANGLE DEBUG] enableBits after setup: 0x%X\n", attributes.enableBits);
// std::printf("[TRIANGLE DEBUG] Expected enableBits: 0x%X\n", (1u << love::gfx::ATTRIB_POS) | (1u << love::gfx::ATTRIB_TEXCOORD) | (1u << love::gfx::ATTRIB_COLOR));

// // Check each attribute individually:
// std::printf("[TRIANGLE DEBUG] ATTRIB_POS bit (1<<%d): 0x%X - %s\n", 
//             love::gfx::ATTRIB_POS, (1u << love::gfx::ATTRIB_POS),
//             (attributes.enableBits & (1u << love::gfx::ATTRIB_POS)) ? "ENABLED" : "DISABLED");
            
// std::printf("[TRIANGLE DEBUG] ATTRIB_TEXCOORD bit (1<<%d): 0x%X - %s\n", 
//             love::gfx::ATTRIB_TEXCOORD, (1u << love::gfx::ATTRIB_TEXCOORD),
//             (attributes.enableBits & (1u << love::gfx::ATTRIB_TEXCOORD)) ? "ENABLED" : "DISABLED");
            
// std::printf("[TRIANGLE DEBUG] ATTRIB_COLOR bit (1<<%d): 0x%X - %s\n", 
//             love::gfx::ATTRIB_COLOR, (1u << love::gfx::ATTRIB_COLOR),
//             (attributes.enableBits & (1u << love::gfx::ATTRIB_COLOR)) ? "ENABLED" : "DISABLED");

// std::printf("[TRIANGLE TEST] Vertex attributes set - enableBits: 0x%X\n", attributes.enableBits);
// std::printf("[TRIANGLE TEST] Buffer layout - stride: %zu bytes\n", sizeof(TextureVertex));

// // Debug the buffer binding calculation
// std::printf("[TRIANGLE DEBUG] Buffer binding calculation:\n");
// std::printf("[TRIANGLE DEBUG] VERTEX_BUFFER_BINDING_START = %d\n", 1); // We know this is 1
// std::printf("[TRIANGLE DEBUG] Buffer index for ATTRIB_POS: %d\n", 1);
// std::printf("[TRIANGLE DEBUG] Calculated binding for ATTRIB_POS: %d\n", 1 + 1);

// // Debug the buffer layout
// std::printf("[TRIANGLE DEBUG] Buffer layout for index 1:\n");
// std::printf("[TRIANGLE DEBUG] Stride: %d bytes\n", attributes.bufferLayouts[1].stride);

// // Debug what's actually in the BufferBindings
// std::printf("[TRIANGLE DEBUG] BufferBindings debug:\n");
// std::printf("[TRIANGLE DEBUG] useBits: 0x%X\n", buffers.useBits);
// for (int i = 0; i < 8; i++) {
//     if (buffers.useBits & (1u << i)) {
//         std::printf("[TRIANGLE DEBUG] Buffer %d: resource=%p, offset=%zu\n", 
//                     i, buffers.info[i].buffer, buffers.info[i].offset);
//     }
// }

// // Use identity transform for simple 2D rendering
// love::Matrix4 transform;  // Identity matrix

// // Temporarily disable depth testing
// cg.instance->setDepthMode(gfx::CompareMode::COMPARE_ALWAYS, false);
// std::printf("[CHAILOVE DEBUG] Disabled depth testing for triangle test\n");

// love::gfx::Graphics::TempTransform tempTransform(cg.instance, transform);

// // Get or create a default white texture for MainTex
// love::gfx::Texture* whiteTexture = cg.instance->getTextureOrDefaultForActiveShader(nullptr);

// love::gfx::Graphics::DrawCommand cmd(&attributes, &buffers);
// cmd.primitiveType = love::gfx::PRIMITIVE_TRIANGLES;
// cmd.vertexCount = 3;
// cmd.instanceCount = 1;
// cmd.texture = whiteTexture;  // This provides the MainTex uniform

// std::printf("[TRIANGLE TEST] Draw command setup complete - issuing draw...\n");

// cg.instance->draw(cmd);

// // Re-enable depth testing
// cg.instance->setDepthMode(gfx::CompareMode::COMPARE_LEQUAL, true);

// std::printf("[CHAILOVE DEBUG] Fullscreen triangle test completed\n");
// std::printf("[CHAILOVE DEBUG] Simple white triangle draw command issued with default shader\n");
        
    
    
        cg.instance->setShader();       
        
        // cg.instance->setActive(true);
        // cg.instance->setShader();
        // cg.instance->setShader(sceneShader->shader);
        // GLuint err = glGetError();
        // if (err != GL_NO_ERROR) {
        //     GLint maxLength = 0;
        //     glGetShaderiv(sceneShader->shader->getHandle(), GL_INFO_LOG_LENGTH, &maxLength);
        //     std::vector<GLchar> infoLog(maxLength);
        //     glGetShaderInfoLog(sceneShader->shader->getHandle(), maxLength, &maxLength, &infoLog[0]);
        //     printf("ERROR::SHADER::COMPILATION_FAILED\n%s\n", infoLog);
        //     printf("ERROR: %d\n", err);            
        //     printf("sceneShader: %d\n", sceneShader->shader);
        // } else {            
        //     // printf("sceneShader: %d\n", sceneShader->shader);
        // }
        // // glBindFramebuffer(cg.instance->FRAMEBUFFER, cg.instance->hw_render.get_current_framebuffer());
        // cg.instance->setDepthMode(gfx::CompareMode::COMPARE_LEQUAL, true);  

        // glEnable(GL_DEPTH_TEST);

        // err = glGetError();
        // if (err != GL_NO_ERROR) {
        //     printf("ERROR: 1\n");
        // }
        
        // GLfloat borderColor[] = { 1.0, 1.0, 1.0, 1.0 };

        // if (true) {
        //     if (shadowMapFBO != 0) {
        //         glDeleteFramebuffers(1, &shadowMapFBO);

        //         err = glGetError();
        //         if (err != GL_NO_ERROR) {
        //             printf("ERROR: 2\n");
        //         }
        //     }

        //     // Create depth texture
        //     glGenFramebuffers(1, &shadowMapFBO);
        //     err = glGetError();
        //     if (err != GL_NO_ERROR) {
        //         printf("ERROR: 3\n");
        //     }
        //     if (shadowMap == 0) {
        //         glGenTextures(1, &shadowMap); 

        //         err = glGetError();
        //         if (err != GL_NO_ERROR) {
        //             printf("ERROR: 4\n");
        //         }
        //     }    
            
        //     glActiveTexture(GL_TEXTURE1);
        //     glBindTexture(GL_TEXTURE_2D, shadowMap);
        //     GLint shadowMapLoc = glGetUniformLocation(sceneShader->shader->getHandle(), "shadowMap");
        //     if (shadowMapLoc >= 0) {
        //         glUniform1i(shadowMapLoc, 1); // 1 = GL_TEXTURE1
        //     }
        //     err = glGetError();
        //     if (err != GL_NO_ERROR) {
        //         printf("ERROR: 5\n");
        //     }
        
        //     glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, cg.width-5, cg.height-5, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
            
        //     err = glGetError();
        //     if (err != GL_NO_ERROR) {
        //         printf("ERROR: 6\n");
        //     }
        //     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        //     err = glGetError();
        //     if (err != GL_NO_ERROR) {
        //         printf("ERROR: 7\n");
        //     }
        //     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        //     err = glGetError();
        //     if (err != GL_NO_ERROR) {
        //         printf("ERROR: 8\n");
        //     }
        //     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        //     err = glGetError();
        //     if (err != GL_NO_ERROR) {
        //         printf("ERROR: 9\n");
        //     }
        //     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        //     err = glGetError();
        //     if (err != GL_NO_ERROR) {
        //         printf("ERROR: 10\n");
        //     }
            
        //     glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

        //     err = glGetError();
        //     if (err != GL_NO_ERROR) {
        //         printf("ERROR: 11\n");
        //     }
            
        //     // Attach depth texture as FBO's depth buffer
        //     glBindFramebuffer(cg.instance->FRAMEBUFFER, shadowMapFBO);
        //     err = glGetError();
        //     if (err != GL_NO_ERROR) {
        //         printf("ERROR: 12\n");
        //     }
        //     glFramebufferTexture2D(cg.instance->FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowMap, 0);
        //     err = glGetError();
        //     if (err != GL_NO_ERROR) {
        //         printf("ERROR: 13\n");
        //     }
        //     glClear(GL_DEPTH_BUFFER_BIT);
        // }

        // err = glGetError();
        // if (err != GL_NO_ERROR) {
        //     printf("ERROR: %d\n", err);
        //     printf("shadowMapFBO: %d\n", shadowMapFBO);
        //     printf("shadowMap: %d\n", shadowMap);
        //     printf("cg.width: %d\n", cg.width);
        //     printf("cg.height: %d\n", cg.height);
        //     printf("sceneShader: %d\n", sceneShader->shader);
        //     printf("cg.instance->FRAMEBUFFER: %d\n", cg.instance->FRAMEBUFFER);
        // } else {
            
        //     // currentTime += 500.0f;
            
        //     if (viewCount == 2) {
        //         glViewport(0, 0, cg.width, cg.height);
        //         sceneShader->send("viewMatrix", viewMatrix1); 
        //         glActiveTexture(GL_TEXTURE0);               
        //         drawMeshes(true, 0);
        //         glViewport(0, cg.height*0.5, cg.width, cg.height*0.5);
        //         auto fb = cg.instance->hw_render.get_current_framebuffer();
        //         glBindFramebuffer(cg.instance->FRAMEBUFFER, fb);
        //         glActiveTexture(GL_TEXTURE0);
        //         drawMeshes(false, 0);
        //         cg.instance->setDepthMode(gfx::CompareMode::COMPARE_LEQUAL, true);


        //         if (shadowMapFBO != 0) {
        //             glDeleteFramebuffers(1, &shadowMapFBO);
        //         }

        //         // Create depth texture
        //         glGenFramebuffers(1, &shadowMapFBO);
                
        //         if (shadowMap == 0) {
        //             glGenTextures(1, &shadowMap); 

        //         }    
                
        //         glBindTexture(GL_TEXTURE_2D, shadowMap);
                
            
        //         glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, cg.width-5, cg.height-5, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
                
        //         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                
        //         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                
        //         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
                
        //         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
            
        //         // GLfloat borderColor[] = { 1.0, 1.0, 1.0, 1.0 };
        //         glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

        //         // Attach depth texture as FBO's depth buffer
        //         glBindFramebuffer(cg.instance->FRAMEBUFFER, shadowMapFBO);
                
        //         glFramebufferTexture2D(cg.instance->FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowMap, 0);
        //         glClear(GL_DEPTH_BUFFER_BIT);
        //         glViewport(0, 0, cg.width, cg.height);
        //         sceneShader->send("viewMatrix", viewMatrix2);
        //         glActiveTexture(GL_TEXTURE0);
        //         drawMeshes(true, 1);
        //         glViewport(0, 0, cg.width, cg.height*0.5);
        //         fb = cg.instance->hw_render.get_current_framebuffer();
        //         glBindFramebuffer(cg.instance->FRAMEBUFFER, fb);
        //         glActiveTexture(GL_TEXTURE0);
        //         drawMeshes(false, 1);
        //         cg.instance->setShader();
        //         // auto fb = cg.instance->hw_render.get_current_framebuffer();
        //         // printf("fb: %d %d\n", fb, cg.instance->FRAMEBUFFER);
                
        //         // glBindFramebuffer(cg.instance->FRAMEBUFFER, fb);
        //         // gfx::OptionalColorD clearcolor;
        //         // clearcolor = ColorD(0.0, 0.0, 0.0, 1.0); // Set the clear color to black with full opacity
        //         // OptionalInt clearstencil(0);
        //         // OptionalDouble cleardepth(1.0);
        //         // cg.instance->clear(clearcolor, clearstencil, cleardepth);
        //         // sceneShader->send("viewMatrix", viewMatrix1);  
               
                
        //         // fb = cg.instance->hw_render.get_current_framebuffer();
                
        //         // printf("fb: %d %d\n", fb, cg.instance->FRAMEBUFFER);
        //         // glBindFramebuffer(cg.instance->FRAMEBUFFER, fb);
        //         // gfx::OptionalColorD clearcolor;
        //         // clearcolor = ColorD(0.0, 0.0, 0.0, 0.0); // Set the clear color to black with full opacity
        //         // OptionalInt clearstencil(0);
        //         // OptionalDouble cleardepth(1.0);
        //         // cg.instance->clear(clearcolor, clearstencil, cleardepth);
        //         // sceneShader->send("viewMatrix", viewMatrix2);  
               
        //     } else if (viewCount > 2) {
        //         glViewport(0, 0, cg.width, cg.height);
                
        //         sceneShader->send("viewMatrix", viewCount > 3 ? viewMatrix3 : viewMatrix2);  
        //         glActiveTexture(GL_TEXTURE0);
        //         drawMeshes(true, viewCount > 3 ? 2 : 1);
        //         glViewport(0, 0, cg.width*0.5, cg.height*0.5); 
        //         auto fb = cg.instance->hw_render.get_current_framebuffer();
        //         glBindFramebuffer(cg.instance->FRAMEBUFFER, fb);
        //         glActiveTexture(GL_TEXTURE0);
        //         drawMeshes(false, viewCount > 3 ? 2 : 1);
                
        
        //         cg.instance->setDepthMode(gfx::CompareMode::COMPARE_LEQUAL, true);


        //         if (shadowMapFBO != 0) {
        //             glDeleteFramebuffers(1, &shadowMapFBO);
        //         }

        //         // Create depth texture
        //         glGenFramebuffers(1, &shadowMapFBO);
                
        //         if (shadowMap == 0) {
        //             glGenTextures(1, &shadowMap); 

        //         }    
                
        //         glBindTexture(GL_TEXTURE_2D, shadowMap);
                
            
        //         glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, cg.width-5, cg.height-5, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
                
        //         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                
        //         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                
        //         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
                
        //         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
            
        //         // GLfloat borderColor[] = { 1.0, 1.0, 1.0, 1.0 };
        //         glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

        //         // Attach depth texture as FBO's depth buffer
        //         glBindFramebuffer(cg.instance->FRAMEBUFFER, shadowMapFBO);
                
        //         glFramebufferTexture2D(cg.instance->FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowMap, 0);
        //         glClear(GL_DEPTH_BUFFER_BIT);                
        //         glViewport(0, 0, cg.width, cg.height);
        //         sceneShader->send("viewMatrix", viewCount > 3 ? viewMatrix4 : viewMatrix3);
        //         glActiveTexture(GL_TEXTURE0);
        //         drawMeshes(true, viewCount > 3 ? 3 : 2);
        //         glViewport(cg.width*0.5, 0, cg.width*0.5, cg.height*0.5);
        //         fb = cg.instance->hw_render.get_current_framebuffer();
        //         glBindFramebuffer(cg.instance->FRAMEBUFFER, fb);
        //         glActiveTexture(GL_TEXTURE0);
        //         drawMeshes(false, viewCount > 3 ? 3 : 2);
        //         cg.instance->setDepthMode(gfx::CompareMode::COMPARE_LEQUAL, true);


        //         if (shadowMapFBO != 0) {
        //             glDeleteFramebuffers(1, &shadowMapFBO);
        //         }

        //         // Create depth texture
        //         glGenFramebuffers(1, &shadowMapFBO);
                
        //         if (shadowMap == 0) {
        //             glGenTextures(1, &shadowMap); 

        //         }    
                
        //         glBindTexture(GL_TEXTURE_2D, shadowMap);
                
            
        //         glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, cg.width-5, cg.height-5, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
                
        //         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                
        //         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                
        //         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
                
        //         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
            
        //         // GLfloat borderColor[] = { 1.0, 1.0, 1.0, 1.0 };
        //         glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

        //         // Attach depth texture as FBO's depth buffer
        //         glBindFramebuffer(cg.instance->FRAMEBUFFER, shadowMapFBO);
                
        //         glFramebufferTexture2D(cg.instance->FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowMap, 0);
        //         glClear(GL_DEPTH_BUFFER_BIT);
        //         glViewport(0, 0, cg.width, cg.height);
        //         sceneShader->send("viewMatrix", viewMatrix1);
        //         glActiveTexture(GL_TEXTURE0);
        //         drawMeshes(true, 0);
        //         if (viewCount > 3) {                    
        //             glViewport(0, cg.height*0.5, cg.width*0.5, cg.height*0.5);
        //         } else {
        //             glViewport(0, cg.height*0.5, cg.width, cg.height*0.5);
        //         }
        //         fb = cg.instance->hw_render.get_current_framebuffer();
        //         glBindFramebuffer(cg.instance->FRAMEBUFFER, fb);
        //         glActiveTexture(GL_TEXTURE0);
        //         drawMeshes(false, 0);
        //         cg.instance->setDepthMode(gfx::CompareMode::COMPARE_LEQUAL, true);


        //         if (shadowMapFBO != 0) {
        //             glDeleteFramebuffers(1, &shadowMapFBO);
        //         }

        //         // Create depth texture
        //         glGenFramebuffers(1, &shadowMapFBO);
                
        //         if (shadowMap == 0) {
        //             glGenTextures(1, &shadowMap); 

        //         }    
                
        //         glBindTexture(GL_TEXTURE_2D, shadowMap);
                
            
        //         glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, cg.width-5, cg.height-5, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
                
        //         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                
        //         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                
        //         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
                
        //         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
            
        //         // GLfloat borderColor[] = { 1.0, 1.0, 1.0, 1.0 };
        //         glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

        //         // Attach depth texture as FBO's depth buffer
        //         glBindFramebuffer(cg.instance->FRAMEBUFFER, shadowMapFBO);
                
        //         glFramebufferTexture2D(cg.instance->FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowMap, 0);
        //         glClear(GL_DEPTH_BUFFER_BIT);
        //         if (viewCount > 3) {                        
        //             glViewport(0, 0, cg.width, cg.height);
        //             sceneShader->send("viewMatrix", viewMatrix2);
        //             glActiveTexture(GL_TEXTURE0);
        //             drawMeshes(true, 1);
        //             glViewport(cg.width*0.5, cg.height*0.5, cg.width*0.5, cg.height*0.5);
        //             fb = cg.instance->hw_render.get_current_framebuffer();
        //             glBindFramebuffer(cg.instance->FRAMEBUFFER, fb);
        //             glActiveTexture(GL_TEXTURE0);
        //             drawMeshes(false, 1);
        //         }
        //         cg.instance->setShader();

        //         // auto fb = cg.instance->hw_render.get_current_framebuffer();
        //         // glBindFramebuffer(cg.instance->FRAMEBUFFER, fb);
        //         // gfx::OptionalColorD clearcolor;
        //         // clearcolor = ColorD(0.0, 0.0, 0.0, 1.0); // Set the clear color to black with full opacity
        //         // OptionalInt clearstencil(0);
        //         // OptionalDouble cleardepth(1.0);
        //         // cg.instance->clear(clearcolor, clearstencil, cleardepth);
                
        //         // glViewport(0, 0, cg.width*0.5, cg.height*0.5);                
        //         // sceneShader->send("viewMatrix", viewMatrix1);  
        //         // drawMeshes(false);
                
        //         // glViewport(cg.width*0.5, 0, cg.width*0.5, cg.height*0.5);
        //         // sceneShader->send("viewMatrix", viewMatrix2);  
        //         // drawMeshes(false);

        //         // glViewport(0, cg.height*0.5, cg.width*0.5, cg.height*0.5);
        //         // sceneShader->send("viewMatrix", viewMatrix3);
        //         // drawMeshes(false);

        //         // glViewport(cg.width*0.5, cg.height*0.5, cg.width*0.5, cg.height*0.5);
        //         // sceneShader->send("viewMatrix", viewMatrix4);
        //         // drawMeshes(false);
        //     } else if (viewCount == 1) {
        //         glViewport(0, 0, cg.width, cg.height);
        //         sceneShader->send("viewMatrix", viewMatrix1);
        //         if (true) {
        //             glActiveTexture(GL_TEXTURE0);
        //             drawMeshes(true, 0);
        //             auto fb = cg.instance->hw_render.get_current_framebuffer();
        //             // printf("fb: %d %d\n", fb, cg.instance->FRAMEBUFFER);
        //             glBindFramebuffer(cg.instance->FRAMEBUFFER, fb);
        //             // gfx::OptionalColorD clearcolor;
        //             // clearcolor = ColorD(0.0, 0.0, 0.0, 1.0); // Set the clear color to black with full opacity
        //             // OptionalInt clearstencil(0);
        //             // OptionalDouble cleardepth(1.0);
        //             // cg.instance->clear(clearcolor, clearstencil, cleardepth);

        //             glActiveTexture(GL_TEXTURE0);             
        //             drawMeshes(false, 0);
        //         } else {
        //             auto fb = cg.instance->hw_render.get_current_framebuffer();
        //             // printf("fb: %d %d\n", fb, cg.instance->FRAMEBUFFER);
        //             glBindFramebuffer(cg.instance->FRAMEBUFFER, fb);
        //             gfx::OptionalColorD clearcolor;
        //             clearcolor = ColorD(0.0, 0.0, 0.0, 1.0); // Set the clear color to black with full opacity
        //             OptionalInt clearstencil(0);
        //             OptionalDouble cleardepth(1.0);
        //             cg.instance->clear(clearcolor, clearstencil, cleardepth);
        //             glActiveTexture(GL_TEXTURE0);
                    
        //             drawMeshes(false, 0);
        //         }
        //         cg.instance->setShader();
        //     } else {
        //         glActiveTexture(GL_TEXTURE0);
        //         cg.instance->setShader();
        //     }
        //     // cg.instance->setShader();
        // }
    }
}

void chai_scene::prepareScreen() {
    auto& cg = ChaiLove::getInstance()->chai_gfx;
    cg.instance->setActive(true);
    cg.instance->setShader(sceneShader->shader);
    cg.instance->setDepthMode(gfx::CompareMode::COMPARE_LEQUAL, true);

    // Create a framebuffer for the loading screen
    auto fb = cg.instance->hw_render.get_current_framebuffer();
    // glBindFramebuffer(cg.instance->FRAMEBUFFER, fb);
    
    // glViewport(0, 0, cg.width, cg.height);
    // glDisable(GL_DEPTH_TEST);
    // glEnable(GL_BLEND);
    // glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    

    // Set the projection matrix for the loading screen to an orthographic projection
    glm::mat4 projectionMatrix = glm::ortho(0.0f, static_cast<float>(cg.width), static_cast<float>(cg.height), 0.0f, -1.0f, 1.0f);
    
    m_projectionMatrixBoxedCache.clear();
    m_projectionMatrixBoxedCache.push_back(projectionMatrix);
    sceneShader->send("projectionMatrix", m_projectionMatrixBoxedCache);
    // Set the view matrix for the loading screen
    glm::mat4 viewMatrix = glm::mat4(1.0f); // Identity matrix for the loading screen
    
    m_viewMatrixBoxedCache.clear();
    m_viewMatrixBoxedCache.push_back(viewMatrix);
    sceneShader->send("viewMatrix", m_viewMatrixBoxedCache);
    
}

void chai_scene::initFramebuffer() {
    if (framebufferInitialized) return;
    
    auto& cg = ChaiLove::getInstance()->chai_gfx;
    
    // Generate framebuffer
    // glGenFramebuffers(1, &sceneFramebuffer);
    // glBindFramebuffer(GL_FRAMEBUFFER, sceneFramebuffer);
    
    // // Generate color texture
    // glGenTextures(1, &sceneColorTexture);
    // glBindTexture(GL_TEXTURE_2D, sceneColorTexture);
    // glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, cg.width, cg.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sceneColorTexture, 0);
    
    // // Generate depth texture
    // glGenTextures(1, &sceneDepthTexture);
    // glBindTexture(GL_TEXTURE_2D, sceneDepthTexture);
    // glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, cg.width, cg.height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, sceneDepthTexture, 0);
    
    // // Check framebuffer completeness
    // if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    //     printf("Framebuffer not complete!\n");
    // }
    
    // glBindFramebuffer(GL_FRAMEBUFFER, 0);
    framebufferInitialized = true;
}

void chai_scene::update(float dt) {
    currentTime += dt;
    deltaTime = dt;
    frameOddEven = !frameOddEven;
    skip += 1;

    // Update framerate calculations
    m_frameTime = dt;
    m_frameCount++;
    m_fpsUpdateTime += dt;
    
    // Add current frame time to history
    m_frameTimes.push_back(dt);
    if (m_frameTimes.size() > m_maxFrameHistory) {
        m_frameTimes.erase(m_frameTimes.begin());
    }
    
    // Update FPS every interval (default 1 second)
    if (m_fpsUpdateTime >= m_fpsUpdateInterval) {
        m_fps = m_frameCount / m_fpsUpdateTime;
        
        // Reset counters
        m_frameCount = 0;
        m_fpsUpdateTime = 0.0f;
        
        // Optional: Print FPS for debugging
        // printf("FPS: %.2f (Avg: %.2f, Min: %.2f, Max: %.2f)\n", 
        //        m_fps, getAverageFramerate(), getMinFramerate(), getMaxFramerate());
    }
}

float chai_scene::getFramerate() {
    return m_fps;
}

float chai_scene::getAverageFramerate() {
    if (m_frameTimes.empty()) {
        return 0.0f;
    }
    
    float sum = 0.0f;
    for (float frameTime : m_frameTimes) {
        sum += frameTime;
    }
    
    float avgFrameTime = sum / m_frameTimes.size();
    return (avgFrameTime > 0.0f) ? (1.0f / avgFrameTime) : 0.0f;
}

float chai_scene::getMinFramerate() {
    if (m_frameTimes.empty()) {
        return 0.0f;
    }
    
    float maxFrameTime = *std::max_element(m_frameTimes.begin(), m_frameTimes.end());
    return (maxFrameTime > 0.0f) ? (1.0f / maxFrameTime) : 0.0f;
}

float chai_scene::getMaxFramerate() {
    if (m_frameTimes.empty()) {
        return 0.0f;
    }
    
    float minFrameTime = *std::min_element(m_frameTimes.begin(), m_frameTimes.end());
    return (minFrameTime > 0.0f) ? (1.0f / minFrameTime) : 0.0f;
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