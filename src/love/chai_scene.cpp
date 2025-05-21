#include "chai_scene.h"
#include "../ChaiLove.h"

namespace love {
chai_scene::chai_scene() {
    
}
chai_scene::~chai_scene() {
    
}

bool chai_scene::destroy() {
    sceneShader->shader->~Shader();
    sceneShader = nullptr;
    printf("Destroying scene\n");
    meshes = std::vector<chai_mesh *>();
    if (shadowMapFBO != 0) {
        glDeleteFramebuffers(1, &shadowMapFBO);
    }
    if (shadowMap != 0) {
        glDeleteTextures(1, &shadowMap);
    }
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

void chai_scene::setShader(chai_shader *shader) {
    if (sceneShader != nullptr) {
        // glDeleteShader(sceneShader->shader->getHandle());
        // sceneShader->shader->~Shader();
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

bool isMeshInFrustum(chai_mesh *mesh, const glm::mat4 &viewProjectionMatrix) {
    // Get the mesh's bounding box
    auto bounds = mesh->getBoundingBox(viewProjectionMatrix);
    auto minBounds = bounds.first;
    auto maxBounds = bounds.second;

    // Extract frustum planes from the view-projection matrix
    glm::vec4 planes[6];
    planes[0] = glm::vec4(viewProjectionMatrix[0][3] + viewProjectionMatrix[0][0], // Left
                          viewProjectionMatrix[1][3] + viewProjectionMatrix[1][0],
                          viewProjectionMatrix[2][3] + viewProjectionMatrix[2][0],
                          viewProjectionMatrix[3][3] + viewProjectionMatrix[3][0]);
    planes[1] = glm::vec4(viewProjectionMatrix[0][3] - viewProjectionMatrix[0][0], // Right
                          viewProjectionMatrix[1][3] - viewProjectionMatrix[1][0],
                          viewProjectionMatrix[2][3] - viewProjectionMatrix[2][0],
                          viewProjectionMatrix[3][3] - viewProjectionMatrix[3][0]);
    planes[2] = glm::vec4(viewProjectionMatrix[0][3] - viewProjectionMatrix[0][1], // Top
                          viewProjectionMatrix[1][3] - viewProjectionMatrix[1][1],
                          viewProjectionMatrix[2][3] - viewProjectionMatrix[2][1],
                          viewProjectionMatrix[3][3] - viewProjectionMatrix[3][1]);
    planes[3] = glm::vec4(viewProjectionMatrix[0][3] + viewProjectionMatrix[0][1], // Bottom
                          viewProjectionMatrix[1][3] + viewProjectionMatrix[1][1],
                          viewProjectionMatrix[2][3] + viewProjectionMatrix[2][1],
                          viewProjectionMatrix[3][3] + viewProjectionMatrix[3][1]);
    planes[4] = glm::vec4(viewProjectionMatrix[0][3] + viewProjectionMatrix[0][2], // Near
                          viewProjectionMatrix[1][3] + viewProjectionMatrix[1][2],
                          viewProjectionMatrix[2][3] + viewProjectionMatrix[2][2],
                          viewProjectionMatrix[3][3] + viewProjectionMatrix[3][2]);
    planes[5] = glm::vec4(viewProjectionMatrix[0][3] - viewProjectionMatrix[0][2], // Far
                          viewProjectionMatrix[1][3] - viewProjectionMatrix[1][2],
                          viewProjectionMatrix[2][3] - viewProjectionMatrix[2][2],
                          viewProjectionMatrix[3][3] - viewProjectionMatrix[3][2]);

    // Normalize the planes
    for (int i = 0; i < 6; i++) {
        float length = glm::length(glm::vec3(planes[i]));
        planes[i] /= length;
    }

    // Check if the bounding box is outside any plane
    for (int i = 0; i < 6; i++) {
        glm::vec3 normal = glm::vec3(planes[i]);
        float distance = planes[i].w;

        // Find the farthest point in the direction of the plane normal
        glm::vec3 farPoint = glm::vec3(
            (normal.x > 0) ? maxBounds.x : minBounds.x,
            (normal.y > 0) ? maxBounds.y : minBounds.y,
            (normal.z > 0) ? maxBounds.z : minBounds.z);

        // If the farthest point is outside the plane, the box is outside the frustum
        if (glm::dot(normal, farPoint) + distance < 0) {
            return false; // Completely outside
        }
    }

    return true; // Inside or intersecting
}

void chai_scene::drawMeshes(bool shadows, int view) {
    int i = 0;
    gfx::OptionalColorD clearcolor;
    OptionalInt clearstencil(0);
    OptionalDouble cleardepth(1.0);
    auto cg = ChaiLove::getInstance()->chai_gfx;
    cg.instance->clear(clearcolor, clearstencil, cleardepth);
    glm::mat4 vMatrix = glm::mat4(1.0f);
    glm::mat4 t2 = glm::mat4(1.0f);
    for (auto mesh : meshes) {
        if (mesh->visible == false) {
            i++;
            continue;
        }

        if (i == 0) { 
            auto cameraParams = mesh->cameraParams[view];

            float fov = cameraParams.at("fov")[0];
            float aspectRatio = cameraParams.at("aspectRatio")[0];
            float nearClip = cameraParams.at("near")[0];
            float farClip = cameraParams.at("far")[0];
            t2 = glm::perspective(fov, aspectRatio, nearClip, farClip);
            auto pm = glm::value_ptr(t2);

            auto lightParams = mesh->lightParams[view];

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
                vMatrix = glm::mat4(
                    data[0], data[1], data[2], data[3],
                    data[4], data[5], data[6], data[7],
                    data[8], data[9], data[10], data[11],
                    data[12], data[13], data[14], data[15]
                );
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
                
        // Perform frustum culling
        glm::mat4 viewProjectionMatrix = t2;
        if (!isMeshInFrustum(mesh, viewProjectionMatrix)) {
            // printf("Mesh %d is outside the frustum\n", i);
            i++;
            continue; // Skip meshes outside the frustum
        }
        auto matrix = matrices[i];
        mesh->draw(cg.instance, matrix, sceneShader, currentTime);
        i++;
    }
}

void chai_scene::draw(std::vector<chaiscript::Boxed_Value> viewMatrix1, std::vector<chaiscript::Boxed_Value> viewMatrix2, std::vector<chaiscript::Boxed_Value> viewMatrix3, std::vector<chaiscript::Boxed_Value> viewMatrix4, int viewCount) {
    auto cg = ChaiLove::getInstance()->chai_gfx;

    if (false && cg.reinit) {
        cg.hasReinit();
        printf("Reinit\n");
    } else {
        // cg.instance->setActive(true);
        // cg.instance->setShader();
        cg.instance->setShader(sceneShader->shader);
        GLuint err = glGetError();
        if (err != GL_NO_ERROR) {
            GLint maxLength = 0;
            glGetShaderiv(sceneShader->shader->getHandle(), GL_INFO_LOG_LENGTH, &maxLength);
            std::vector<GLchar> infoLog(maxLength);
            glGetShaderInfoLog(sceneShader->shader->getHandle(), maxLength, &maxLength, &infoLog[0]);
            printf("ERROR::SHADER::COMPILATION_FAILED\n%s\n", infoLog);
            printf("ERROR: %d\n", err);            
            printf("sceneShader: %d\n", sceneShader->shader);
        } else {            
            // printf("sceneShader: %d\n", sceneShader->shader);
        }
        // glBindFramebuffer(cg.instance->FRAMEBUFFER, cg.instance->hw_render.get_current_framebuffer());
        cg.instance->setDepthMode(gfx::CompareMode::COMPARE_LEQUAL, true);

        err = glGetError();
        if (err != GL_NO_ERROR) {
            printf("ERROR: 1\n");
        }

        if (shadowMapFBO != 0) {
            glDeleteFramebuffers(1, &shadowMapFBO);

            err = glGetError();
            if (err != GL_NO_ERROR) {
                printf("ERROR: 2\n");
            }
        }

        // Create depth texture
        glGenFramebuffers(1, &shadowMapFBO);
        err = glGetError();
        if (err != GL_NO_ERROR) {
            printf("ERROR: 3\n");
        }
        if (shadowMap == 0) {
            glGenTextures(1, &shadowMap); 

            err = glGetError();
            if (err != GL_NO_ERROR) {
                printf("ERROR: 4\n");
            }
        }    
        
        glBindTexture(GL_TEXTURE_2D, shadowMap);
        err = glGetError();
        if (err != GL_NO_ERROR) {
            printf("ERROR: 5\n");
        }
       
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, cg.width-5, cg.height-5, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
        
        err = glGetError();
        if (err != GL_NO_ERROR) {
            printf("ERROR: 6\n");
        }
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        err = glGetError();
        if (err != GL_NO_ERROR) {
            printf("ERROR: 7\n");
        }
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        err = glGetError();
        if (err != GL_NO_ERROR) {
            printf("ERROR: 8\n");
        }
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        err = glGetError();
        if (err != GL_NO_ERROR) {
            printf("ERROR: 9\n");
        }
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        err = glGetError();
        if (err != GL_NO_ERROR) {
            printf("ERROR: 10\n");
        }
        GLfloat borderColor[] = { 1.0, 1.0, 1.0, 1.0 };
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

        err = glGetError();
        if (err != GL_NO_ERROR) {
            printf("ERROR: 11\n");
        }
        // Attach depth texture as FBO's depth buffer
        glBindFramebuffer(cg.instance->FRAMEBUFFER, shadowMapFBO);
        err = glGetError();
        if (err != GL_NO_ERROR) {
            printf("ERROR: 12\n");
        }
        glFramebufferTexture2D(cg.instance->FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowMap, 0);
        err = glGetError();
        if (err != GL_NO_ERROR) {
            printf("ERROR: 13\n");
        }
        glClear(GL_DEPTH_BUFFER_BIT);

        err = glGetError();
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
            
            if (viewCount == 2) {
                glViewport(0, 0, cg.width, cg.height);
                sceneShader->send("viewMatrix", viewMatrix1);                
                drawMeshes(true, 0);
                glViewport(0, cg.height*0.5, cg.width, cg.height*0.5);
                auto fb = cg.instance->hw_render.get_current_framebuffer();
                glBindFramebuffer(cg.instance->FRAMEBUFFER, fb);
                drawMeshes(false, 0);
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
            
                // GLfloat borderColor[] = { 1.0, 1.0, 1.0, 1.0 };
                glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

                // Attach depth texture as FBO's depth buffer
                glBindFramebuffer(cg.instance->FRAMEBUFFER, shadowMapFBO);
                
                glFramebufferTexture2D(cg.instance->FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowMap, 0);
                glClear(GL_DEPTH_BUFFER_BIT);
                glViewport(0, 0, cg.width, cg.height);
                sceneShader->send("viewMatrix", viewMatrix2);
                drawMeshes(true, 1);
                glViewport(0, 0, cg.width, cg.height*0.5);
                fb = cg.instance->hw_render.get_current_framebuffer();
                glBindFramebuffer(cg.instance->FRAMEBUFFER, fb);
                drawMeshes(false, 1);
                // auto fb = cg.instance->hw_render.get_current_framebuffer();
                // printf("fb: %d %d\n", fb, cg.instance->FRAMEBUFFER);
                
                // glBindFramebuffer(cg.instance->FRAMEBUFFER, fb);
                // gfx::OptionalColorD clearcolor;
                // clearcolor = ColorD(0.0, 0.0, 0.0, 1.0); // Set the clear color to black with full opacity
                // OptionalInt clearstencil(0);
                // OptionalDouble cleardepth(1.0);
                // cg.instance->clear(clearcolor, clearstencil, cleardepth);
                // sceneShader->send("viewMatrix", viewMatrix1);  
               
                
                // fb = cg.instance->hw_render.get_current_framebuffer();
                
                // printf("fb: %d %d\n", fb, cg.instance->FRAMEBUFFER);
                // glBindFramebuffer(cg.instance->FRAMEBUFFER, fb);
                // gfx::OptionalColorD clearcolor;
                // clearcolor = ColorD(0.0, 0.0, 0.0, 0.0); // Set the clear color to black with full opacity
                // OptionalInt clearstencil(0);
                // OptionalDouble cleardepth(1.0);
                // cg.instance->clear(clearcolor, clearstencil, cleardepth);
                // sceneShader->send("viewMatrix", viewMatrix2);  
               
            } else if (viewCount > 2) {
                glViewport(0, 0, cg.width, cg.height);
                
                sceneShader->send("viewMatrix", viewMatrix1);  
                drawMeshes(true, 0);
                glViewport(0, 0, cg.width*0.5, cg.height*0.5); 
                auto fb = cg.instance->hw_render.get_current_framebuffer();
                glBindFramebuffer(cg.instance->FRAMEBUFFER, fb);
                drawMeshes(false, 0);
                
        
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
            
                // GLfloat borderColor[] = { 1.0, 1.0, 1.0, 1.0 };
                glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

                // Attach depth texture as FBO's depth buffer
                glBindFramebuffer(cg.instance->FRAMEBUFFER, shadowMapFBO);
                
                glFramebufferTexture2D(cg.instance->FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowMap, 0);
                glClear(GL_DEPTH_BUFFER_BIT);                
                glViewport(0, 0, cg.width, cg.height);
                sceneShader->send("viewMatrix", viewMatrix2);
                drawMeshes(true, 1);
                glViewport(cg.width*0.5, 0, cg.width*0.5, cg.height*0.5);
                fb = cg.instance->hw_render.get_current_framebuffer();
                glBindFramebuffer(cg.instance->FRAMEBUFFER, fb);
                drawMeshes(false, 1);
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
            
                // GLfloat borderColor[] = { 1.0, 1.0, 1.0, 1.0 };
                glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

                // Attach depth texture as FBO's depth buffer
                glBindFramebuffer(cg.instance->FRAMEBUFFER, shadowMapFBO);
                
                glFramebufferTexture2D(cg.instance->FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowMap, 0);
                glClear(GL_DEPTH_BUFFER_BIT);
                glViewport(0, 0, cg.width, cg.height);
                sceneShader->send("viewMatrix", viewMatrix3);
                drawMeshes(true, 2);
                if (viewCount > 3) {                    
                    glViewport(0, cg.height*0.5, cg.width*0.5, cg.height*0.5);
                } else {
                    glViewport(0, cg.height*0.5, cg.width, cg.height*0.5);
                }
                fb = cg.instance->hw_render.get_current_framebuffer();
                glBindFramebuffer(cg.instance->FRAMEBUFFER, fb);
                drawMeshes(false, 2);
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
            
                // GLfloat borderColor[] = { 1.0, 1.0, 1.0, 1.0 };
                glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

                // Attach depth texture as FBO's depth buffer
                glBindFramebuffer(cg.instance->FRAMEBUFFER, shadowMapFBO);
                
                glFramebufferTexture2D(cg.instance->FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowMap, 0);
                glClear(GL_DEPTH_BUFFER_BIT);
                if (viewCount > 3) {                        
                    glViewport(0, 0, cg.width, cg.height);
                    sceneShader->send("viewMatrix", viewMatrix4);
                    drawMeshes(true, 3);
                    glViewport(cg.width*0.5, cg.height*0.5, cg.width*0.5, cg.height*0.5);
                    fb = cg.instance->hw_render.get_current_framebuffer();
                    glBindFramebuffer(cg.instance->FRAMEBUFFER, fb);
                    drawMeshes(false, 3);
                }

                // auto fb = cg.instance->hw_render.get_current_framebuffer();
                // glBindFramebuffer(cg.instance->FRAMEBUFFER, fb);
                // gfx::OptionalColorD clearcolor;
                // clearcolor = ColorD(0.0, 0.0, 0.0, 1.0); // Set the clear color to black with full opacity
                // OptionalInt clearstencil(0);
                // OptionalDouble cleardepth(1.0);
                // cg.instance->clear(clearcolor, clearstencil, cleardepth);
                
                // glViewport(0, 0, cg.width*0.5, cg.height*0.5);                
                // sceneShader->send("viewMatrix", viewMatrix1);  
                // drawMeshes(false);
                
                // glViewport(cg.width*0.5, 0, cg.width*0.5, cg.height*0.5);
                // sceneShader->send("viewMatrix", viewMatrix2);  
                // drawMeshes(false);

                // glViewport(0, cg.height*0.5, cg.width*0.5, cg.height*0.5);
                // sceneShader->send("viewMatrix", viewMatrix3);
                // drawMeshes(false);

                // glViewport(cg.width*0.5, cg.height*0.5, cg.width*0.5, cg.height*0.5);
                // sceneShader->send("viewMatrix", viewMatrix4);
                // drawMeshes(false);
            } else {
                glViewport(0, 0, cg.width, cg.height);
                sceneShader->send("viewMatrix", viewMatrix1);
                drawMeshes(true, 0);
                auto fb = cg.instance->hw_render.get_current_framebuffer();
                // printf("fb: %d %d\n", fb, cg.instance->FRAMEBUFFER);
                glBindFramebuffer(cg.instance->FRAMEBUFFER, fb);
                gfx::OptionalColorD clearcolor;
                clearcolor = ColorD(0.0, 0.0, 0.0, 1.0); // Set the clear color to black with full opacity
                OptionalInt clearstencil(0);
                OptionalDouble cleardepth(1.0);
                cg.instance->clear(clearcolor, clearstencil, cleardepth);
                
                drawMeshes(false, 0);
            }            
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