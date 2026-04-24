#include "chai_scene.h"
#include "../ChaiLove.h"
// #include "../../vendor/MemPlumber/memplumber.h"
// #include "../../vendor/MemPlumber/memplumber-internals.h"

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
static SIZE_T _ramSnap() {
    PROCESS_MEMORY_COUNTERS_EX pmc = {};
    pmc.cb = sizeof(pmc);
    GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc));
    return pmc.PrivateUsage;
}
#define RAM_SNAP(var) SIZE_T var = _ramSnap()
#define RAM_DELTA(label, before, after) do { \
    long long d = (long long)((after) - (before)); \
    if (d > 4096) std::printf("[DRAW LEAK] %s: +%lld bytes\n", label, d); \
} while(0)
#else
#define RAM_SNAP(var) (void)0
#define RAM_DELTA(label, before, after) (void)0
#endif

namespace love {
chai_scene::chai_scene() {
    
}
chai_scene::~chai_scene() {
    // Clean up shadow map
    if (shadowMapTexture != nullptr) {
        shadowMapTexture->release();
        shadowMapTexture = nullptr;
    }
    shadowMapInitialized = false;
    
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
    // Clean up shadow map
    if (shadowMapTexture != nullptr) {
        shadowMapTexture->release();
        shadowMapTexture = nullptr;
    }
    shadowMapInitialized = false;
    
    sceneShader->shader->~Shader();
    sceneShader = nullptr;
    printf("Destroying scene\n");
    meshes = std::vector<chai_mesh *>();
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
    // printf("Adding mesh to scene %p\n", mesh);
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
    // printf("Adding child mesh %p to parent mesh %p\n", cmesh, pmesh);
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

void chai_scene::setMatrix(const std::vector<float> &matrix, int index) {
    // Convert std::vector<float> to Matrix4
    float mat[16];
    for (int i = 0; i < 16; i++) {
        mat[i] = matrix[i];
    }
    matrices[index] = Matrix4(mat);
}

void chai_scene::finalize() {
    // Perform any finalization steps if needed
}

void chai_scene::initShadowMap() {
    if (shadowMapInitialized) return;
    
    auto gfx = Module::getInstance<gfx::Graphics>(Module::M_GRAPHICS);
    if (!gfx) {
        printf("[SHADOW] Error: Could not get graphics module\n");
        return;
    }
    
    // Create shadow map texture as a depth texture
    // Using DEPTH32_FLOAT for maximum compatibility as a readable render target
    gfx::Texture::Settings shadowSettings;
    shadowSettings.width = 1440;
    shadowSettings.height = 1080;
    shadowSettings.format = PIXELFORMAT_DEPTH32_FLOAT;
    shadowSettings.renderTarget = true;
    shadowSettings.readable = true;
    
    auto shadowSlices = gfx::Texture::Slices(gfx::TextureType::TEXTURE_2D);
    shadowMapTexture = gfx->newTexture(shadowSettings, &shadowSlices);
    
    // Do NOT set depthSampleMode: the shader samples shadowMap as a plain sampler2D
    // (texture2D(shadowMap, uv).r) and does the depth comparison manually.
    // depthSampleMode would require declaring the uniform as DepthImage (sampler2DShadow)
    // and using a vec3 coordinate, which this shader doesn't do.

    if (shadowMapTexture) {
        shadowMapInitialized = true;
        printf("[SHADOW] Shadow map initialized: %dx%d (DEPTH32_FLOAT format)\n", SHADOW_MAP_SIZE, SHADOW_MAP_SIZE);
    } else {
        printf("[SHADOW] Error: Failed to create shadow map texture\n");
    }
}

glm::mat4 chai_scene::calculateLightProjection(const glm::mat4 &cameraView, const glm::mat4 &cameraProjection, const glm::mat4 &lightView) {
    // printf("[LIGHT PROJECTION] Starting calculation\n");
    // fflush(stdout);

    // Derive camera lookat point in world space:
    // Camera position = inverse(cameraView)[3]
    // Camera forward  = -third column of cameraView (view matrix row extraction)
    glm::mat4 invCameraView = glm::inverse(cameraView);
    glm::vec3 camPos = glm::vec3(cameraView[3]);
    glm::vec3 camForward = -glm::vec3(cameraView[2][0], cameraView[2][1], cameraView[2][2]);

    // Project the lookat point a fixed distance ahead of the camera
    float lookAtDist = 0.0f;
    glm::vec3 lookAtWorld = camPos + camForward * lookAtDist;

    // printf("[LIGHT PROJECTION] camPos=(%.2f, %.2f, %.2f) camForward=(%.2f, %.2f, %.2f)\n",
    //        camPos.x, camPos.y, camPos.z, camForward.x, camForward.y, camForward.z);
    // printf("[LIGHT PROJECTION] lookAtWorld=(%.2f, %.2f, %.2f)\n",
    //        lookAtWorld.x, lookAtWorld.y, lookAtWorld.z);

    // Transform lookat point into light space to get the center (x, z) of the ortho box
    glm::vec4 lookAtLight = lightView * glm::vec4(lookAtWorld, 1.0f);
    float cx = lookAtLight.x;
    float cz = lookAtLight.z;

    // printf("[LIGHT PROJECTION] lookAtLight=(%.2f, %.2f, %.2f)\n",
    //        lookAtLight.x, lookAtLight.y, lookAtLight.z);

    // Shadow map is 1440x1080, aspect ratio 1.33:1
    float shadowWorldSize = 65.0f;
    float aspect = 1440.0f / 1080.0f;

    float orthoWidth  = shadowWorldSize;
    float orthoHeight = shadowWorldSize / aspect;

    // Center the ortho box on the camera lookat point (x, z in light space)
    float left   = cx - orthoWidth  / 2.0f;
    float right  = cx + orthoWidth  / 2.0f;
    float bottom = cz - orthoHeight / 2.0f;
    float top    = cz + orthoHeight / 2.0f;

    // Depth range centered on the lookat z in light space
    float nearPlane = cz - 100.0f;
    float farPlane  = cz + 100.0f;

    // printf("[LIGHT PROJECTION] Fixed ortho: [%.2f, %.2f] x [%.2f, %.2f], depth [%.2f, %.2f]\n",
    //        left, right, bottom, top, nearPlane, farPlane);
    // fflush(stdout);

    return glm::ortho(left, right, bottom, top, nearPlane, farPlane);
}

void chai_scene::renderShadowPass(const glm::mat4 &lightView, const glm::mat4 &lightProjection, int view) {
   
    auto& cg = ChaiLove::getInstance()->chai_gfx;
    auto vulkanGfx = dynamic_cast<gfx::vulkan::Graphics*>(cg.instance);
    if (!vulkanGfx) {
        printf("[SHADOW] ABORTING: vulkanGfx cast failed\n");
        return;
    }
    // if (vulkanGfx->getCurrentPipeline() == VK_NULL_HANDLE) {
    //     printf("[SHADOW] ABORTING: Pipeline not initialized\n");
    //     return;
    // }
    // printf("[SHADOW] === SHADOW PASS START ===\n");
    
    if (!shadowMapInitialized) {
        // printf("[SHADOW] shadowMapInitialized=false, calling initShadowMap()\n");
        initShadowMap();
    }
    
    if (!shadowMapTexture || !sceneShader) {
        printf("[SHADOW] ABORTING: shadowMapTexture=%p, sceneShader=%p\n", shadowMapTexture, sceneShader);
        return;
    }
    
    // printf("[SHADOW] shadowMapTexture OK, sceneShader OK\n");
    

    vulkanGfx->beginShadowRenderPass(sceneShader->shader, shadowMapTexture);
    
    // printf("[SHADOW] Calling submitGpuCommands(SUBMIT_RESTART) to end previous command buffer\n");
    // CRITICAL: End current command buffer and start a fresh one
    // This ensures the shadow pass is in a completely separate command buffer
    // vulkanGfx->submitGpuCommands(love::gfx::vulkan::SUBMIT_RESTART);
    // printf("[SHADOW] Returned from first submitGpuCommands\n");
    
    // For shadow map rendering, we MUST use a temporary color attachment because
    // Vulkan requires at least one color attachment. However, we set it to depth-only mode.
    // gfx::Graphics::RenderTargets shadowRenderTargets;
    
    // Create temporary color texture (needed for render pass, but won't be written to)
    // auto tempColorTex = cg.instance->getTemporaryTexture(PIXELFORMAT_RGBA8_UNORM, 
    //                                                      shadowMapTexture->getPixelWidth(0), 
    //                                                      shadowMapTexture->getPixelHeight(0), 
    //                                                      1);
    // shadowRenderTargets.colors.push_back(gfx::Graphics::RenderTarget(tempColorTex));
    // shadowRenderTargets.colors.push_back(gfx::Graphics::RenderTarget(shadowMapTexture));
    
    // printf("[SHADOW] Setting shadow render targets: colors=%zu, depth=%p, tempColor=%p\n", 
    //        shadowRenderTargets.colors.size(), shadowMapTexture, tempColorTex);
    
    // Switch to shadow map render target - this will start a NEW render pass
    // cg.instance->setRenderTargets(shadowRenderTargets);
    // printf("[SHADOW] setRenderTargets completed\n");
    
    // Disable ALL color writes immediately - we ONLY write depth
    // cg.instance->setColorMask({false, false, false, false});
    // printf("[SHADOW] Color mask disabled\n");
    
    // Enable depth testing and depth writes for shadow pass
    // cg.instance->setDepthMode(gfx::CompareMode::COMPARE_LESS, true);
    // printf("[SHADOW] Depth mode set\n");
    
    // Clear the depth buffer to far depth (1.0)
    // gfx::OptionalColorD clearColor;  // No color clear needed
    // love::OptionalInt clearStencil;  // No stencil in DEPTH32_FLOAT
    // love::OptionalDouble clearDepth(1.0);
    // cg.instance->clear(clearColor, clearStencil, clearDepth);
    // printf("[SHADOW] Clear completed\n");
    
    // Set shader uniforms for shadow pass
    // sceneShader->sendInt("shadow", 0);  // Not sampling shadows during shadow pass
    
    // // Set light space matrices for shadow rendering
    // glm::mat4 lightSpaceMatrix = lightProjection * lightView;
    // m_lightSpaceMatrixCache.clear();
    // m_lightSpaceMatrixCache.push_back(lightSpaceMatrix);
    // sceneShader->send("lightSpaceMatrix", m_lightSpaceMatrixCache[0]);
    // printf("[SHADOW] Light space matrix sent\n");
    
    // m_projectionMatrixCache.clear();
    // m_projectionMatrixCache.push_back(lightProjection);
    // sceneShader->send("projectionMatrix", m_projectionMatrixCache);
    // sceneShader->send("projectionMatrix2", m_projectionMatrixCache);
    // printf("[SHADOW] Projection matrix sent\n");
    
    // m_vmCache.clear();
    // m_vmCache.push_back(lightView);
    // sceneShader->send("viewMatrix", m_vmCache[0]);
    // printf("[SHADOW] View matrix sent\n");
    
    // Send the light-space transform so the vertex shader places geometry correctly in
    // the shadow map's depth range.  Without these, the shadow pass inherits the previous
    // 2D orthographic projection from the normal rendering pass, which maps 3D geometry to
    // clip-space z ≈ 1.0 (the far plane) for all vertices — COMPARE_LESS(1.0, 1.0) = false,
    // so every fragment fails the depth test and nothing is written.
    // glm::mat4 lightSpaceMatrix = lightProjection * lightView;
    // sceneShader->send("lightSpaceMatrix", lightSpaceMatrix);
    // std::vector<glm::mat4> projVec = { lightProjection };
    // sceneShader->send("projectionMatrix",  projVec);
    // sceneShader->send("projectionMatrix2", projVec);
    // std::vector<glm::mat4> viewVec = { lightView };
    // sceneShader->send("viewMatrix", viewVec[0]);

    // Signal the fragment shader that this is the shadow pass (miscInfo.x = 1.0).
    // The effect() function returns vec4(0.0) immediately when miscInfo.x > 0.5,
    // skipping all lighting/texturing and allowing only the depth write to complete.
    // sceneShader->sendConstant("miscInfo", {glm::vec4(1.0f, 0.0f, 0.0f, 0.0f)});
    // printf("[SHADOW] About to call drawMeshes(true, %d)\n", view);
    drawMeshes(true, view);
    // Reset miscInfo.x so normal-pass draws don't discard their fragments.
    // sceneShader->sendConstant("miscInfo", {glm::vec4(0.0f, 0.0f, 0.0f, 0.0f)});
    // printf("[SHADOW] Returned from drawMeshes\n");
    
    // CRITICAL: Flush batched draw state IMMEDIATELY
    // This ensures all shadow geometry is actually rendered to the shadow map
    // before we switch render targets. Without this, shadow draws get batched
    // with main scene renders, causing them to be in the same render pass.
    // printf("[SHADOW] Flushing batched draws to ensure shadow render pass completes\n");
    // gfx::Graphics::flushBatchedDrawsGlobal();
    // printf("[SHADOW] Batched draws flushed\n");
    
    // CRITICAL: End shadow command buffer and start fresh for main rendering
    // This ensures shadow map is completely finished before we switch back to screen
    // printf("[SHADOW] Calling submitGpuCommands(SUBMIT_RESTART) for shadow completion\n");
    // vulkanGfx->submitGpuCommands(love::gfx::vulkan::SUBMIT_RESTART);
    // printf("[SHADOW] Returned from second submitGpuCommands\n");
    
    // Restore default render target (back to screen) - this will start ANOTHER NEW render pass
    // printf("[SHADOW] Restoring screen render target\n");
    // cg.instance->setRenderTarget();
    // printf("[SHADOW] setRenderTarget() completed\n");
    
    // Re-enable color writes for normal rendering
    
    // printf("[SHADOW] Color mask re-enabled\n");
    
    // Reset renderingShadowMap flag via miscInfo constant (miscInfo.x = 0.0)
    // std::vector<glm::vec4> miscDataReset;
    // miscDataReset.push_back(glm::vec4(0.0f, 0.0f, 0.0f, 0.0f));
    // miscDataReset.push_back(glm::vec4(0.0f, 0.0f, 0.0f, 0.0f));
    // sceneShader->sendConstant("miscInfo", miscDataReset);
    
    vulkanGfx->endShadowRenderPass(sceneShader, shadowMapTexture);
    // cg.instance->setColorMask({true, true, true, true});
    // printf("[SHADOW] === SHADOW PASS COMPLETE ===\n");
    
    // vulkanGfx->submitGpuCommands(love::gfx::vulkan::SUBMIT_NOPRESENT, nullptr);
	// vulkanGfx->advanceFrame();
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
    static int sceneFrameCount = 0;
    static size_t lastSceneLeakCount = 0;
    static uint64_t lastSceneLeakSize = 0;
    size_t leakCountBefore, leakCountAfter;
    uint64_t leakSizeBefore, leakSizeAfter;
    
    // printf("[DRAWMESHES] Called with shadows=%s, view=%d, meshes.size()=%zu\n", 
    //        shadows ? "TRUE" : "FALSE", view, meshes.size());
    
    sceneFrameCount++;
    
    // Track which meshes have been drawn to prevent duplicates
    std::set<int> drawnMeshIndices;
    
    // if (sceneFrameCount % 60 == 1) {
    //     printf("[DRAW MESHES] Frame %d | shadows=%s | view=%d\n", 
    //            sceneFrameCount, shadows ? "TRUE" : "FALSE", view);
    // }
    
    // if (sceneFrameCount % 300 == 0) {
    //     __mem_leak_check(leakCountBefore, leakSizeBefore, true, "", false);
    // }
    
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

        // Upload specular env map to GPU on first frame, then bind every frame.
        if (background_tex == nullptr && mesh->specData != nullptr) {
            auto gfx = Module::getInstance<gfx::Graphics>(Module::M_GRAPHICS);
            gfx::Texture::Settings specSettings;
            specSettings.width  = mesh->specularW;
            specSettings.height = mesh->specularH;
            specSettings.format = PIXELFORMAT_RGBA8_UNORM;
            auto specSlices = gfx::Texture::Slices(gfx::TextureType::TEXTURE_2D);
            // background_tex = gfx->newTexture(specSettings, &specSlices);
            // Rect specRect = {};
            // specRect.w = mesh->specularW;
            // specRect.h = mesh->specularH;
            // background_tex->replacePixels(mesh->specData,
            //     mesh->specularW * mesh->specularH * 4, 0, 0, specRect, false);
        }
        if (background_tex != nullptr) {
            sceneShader->sendTexture("specularMap", background_tex);
        }

        
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

        // printf("Testing for memory leaks around shader uniform updates\n");
        // size_t leakCountBefore, leakCountAfter;
        // uint64_t leakSizeBefore, leakSizeAfter;
        
        // __mem_leak_check(leakCountBefore, leakSizeBefore, false, "", false);
        
        m_lightIntensityCache.clear();
        m_lightIntensityCache.push_back(glm::vec3(0.7f));
        sceneShader->send("lightIntensity", 0.7f);

        // __mem_leak_check(leakCountAfter, leakSizeAfter, false, "", false);
    
        // printf("Leak delta: %zu objects, %llu bytes\n", 
        //     leakCountAfter - leakCountBefore,
        //     leakSizeAfter - leakSizeBefore);

        m_ambientColorCache2.clear();
        m_ambientColorCache2.push_back(glm::vec3(0.9f, 0.9f, 0.9f));
        sceneShader->send("ambientColor", glm::vec3(0.9f, 0.9f, 0.9f));

        // Ortho projection matrix for fullscreen quad
        // This assumes the quad covers the entire screen, adjust as needed
        auto t2 = glm::ortho(0.0f, static_cast<float>(mesh->specularW),
            static_cast<float>(mesh->specularH), 0.0f, -1.0f, 1.0f);
        auto pm = glm::value_ptr(t2);
        // printf("Testing for memory leaks around shader uniform updates\n");
        m_projectionMatrixCache2.clear();
        // __mem_leak_check(leakCountBefore, leakSizeBefore, false, "", false);
        m_projectionMatrixCache2.push_back(t2);
        sceneShader->send("projectionMatrix", m_projectionMatrixCache2);
        sceneShader->send("projectionMatrix2", m_projectionMatrixCache2);
        // __mem_leak_check(leakCountAfter, leakSizeAfter, false, "", false);
    
        // printf("Leak delta: %zu objects, %llu bytes\n", 
            // leakCountAfter - leakCountBefore,
            // leakSizeAfter - leakSizeBefore);

        // Set the view matrix to identity for fullscreen quad
        auto vMatrix = glm::mat4(1.0f);
        // m_viewMatrixCache.clear();
        // m_viewMatrixCache.push_back(vMatrix);
        // sceneShader->send("viewMatrix", vMatrix);

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

    m_deferredChildIndices.clear();
    m_deferredParentIndices.clear();
    m_deferredMeshIndices.clear();

    if (sceneFrameCount % 60 == 1) {
        int multiGroupCount = 0;
        int multiGroupMeshId = -1;
        for (auto& pair : meshGroups) {
            if (pair.second > 1) {
                multiGroupCount++;
                multiGroupMeshId = pair.first;
                // printf("[MESH GROUPS DEBUG] Mesh ID %d has %d groups\n", pair.first, pair.second);
            }
        }
        // printf("[MESH GROUPS] Total meshes: %zu, Meshes with >1 group: %d (ID: %d)\n", 
        //        meshes.size(), multiGroupCount, multiGroupMeshId);
    }

    for (auto mesh : meshes) {
        // Check for null mesh first
        if (!mesh) {
            i++;
            continue;
        }
        
        if (i == 0) {
            // Bounds check to prevent crash
            if (view >= mesh->cameraParams.size() || view >= mesh->lightParams.size()) {
                i++;
                continue;
            }
            
            auto cameraParams = mesh->cameraParams[view];

            float fov = cameraParams.at("fov")[0];
            // printf("FOV: %f\n", fov);
            float aspectRatio = cameraParams.at("aspectRatio")[0];
            // printf("Aspect Ratio: %f\n", aspectRatio);
            float nearClip = cameraParams.at("near")[0];
            float farClip = cameraParams.at("far")[0];
            t2 = glm::perspective(fov, aspectRatio, nearClip, farClip);
            auto pm = glm::value_ptr(t2);

            auto lightParams = mesh->lightParams[0];

            m_lightDirectionCache.clear();
            // TODO: lightParams returns what type? Store as vec3
            int dirCount = 0;
            glm::vec3 lastDir(0.0f, -1.0f, 0.0f);
            for (auto axis : lightParams["direction"]) {
                lastDir[dirCount] = axis;
                dirCount++;
            }
            m_lightDirectionCache.push_back(lastDir); // Fallback if direction is not provided
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

            // Use cached light matrices calculated in draw() to ensure consistency
            // between shadow pass and main rendering pass
            glm::vec3 lightPos = glm::vec3(lightParams["position"][0], lightParams["position"][1], lightParams["position"][2]);
            
            // std::printf("Light Position: (%f, %f, %f)\n", lightPos.x, lightPos.y, lightPos.z);
            // std::printf("Cached Light View Matrix:\n%f %f %f %f\n%f %f %f %f\n%f %f %f %f\n%f %f %f %f\n",
            //     m_cachedLightView[0][0], m_cachedLightView[0][1], m_cachedLightView[0][2], m_cachedLightView[0][3],
            //     m_cachedLightView[1][0], m_cachedLightView[1][1], m_cachedLightView[1][2], m_cachedLightView[1][3],
            //     m_cachedLightView[2][0], m_cachedLightView[2][1], m_cachedLightView[2][2], m_cachedLightView[2][3],
            //     m_cachedLightView[3][0], m_cachedLightView[3][1], m_cachedLightView[3][2], m_cachedLightView[3][3]);
            // std::printf("Cached Light Space Matrix:\n%f %f %f %f\n%f %f %f %f\n%f %f %f %f\n%f %f %f %f\n",
            //     m_cachedLightSpaceMatrix[0][0], m_cachedLightSpaceMatrix[0][1], m_cachedLightSpaceMatrix[0][2], m_cachedLightSpaceMatrix[0][3],
            //     m_cachedLightSpaceMatrix[1][0], m_cachedLightSpaceMatrix[1][1], m_cachedLightSpaceMatrix[1][2], m_cachedLightSpaceMatrix[1][3],
            //     m_cachedLightSpaceMatrix[2][0], m_cachedLightSpaceMatrix[2][1], m_cachedLightSpaceMatrix[2][2], m_cachedLightSpaceMatrix[2][3],
            //     m_cachedLightSpaceMatrix[3][0], m_cachedLightSpaceMatrix[3][1], m_cachedLightSpaceMatrix[3][2], m_cachedLightSpaceMatrix[3][3]);

            m_lightSpaceMatrixCache.clear();
            m_lightSpaceMatrixCache.push_back(m_cachedLightSpaceMatrix);
            sceneShader->send("lightSpaceMatrix", m_lightSpaceMatrixCache[0]);

            m_projectionMatrixCache.clear();
            m_projectionMatrixCache.push_back(t2);  // Camera projection for normal pass
            sceneShader->send("projectionMatrix", m_projectionMatrixCache);
            sceneShader->send("projectionMatrix2", m_projectionMatrixCache);

            if (shadows == true) {
                // Shadow pass: vertex shader must transform vertices into light clip-space.
                // Use cached matrices for perfect consistency
                sceneShader->send("lightViewMatrix", m_cachedLightView);
                viewProjectionMatrix = m_cachedLightSpaceMatrix; // Already lightProjection * lightView
            } else {
                // Normal rendering pass: sample FROM shadow map
                sceneShader->sendInt("shadow", 1);
                // Set miscInfo.x = 0.0 to disable depth-only discard
                // std::vector<glm::vec4> miscData;
                // miscData.push_back(glm::vec4(0.0f, 0.0f, 0.0f, 0.0f));
                // miscData.push_back(glm::vec4(0.0f, 0.0f, 0.0f, 0.0f));
                // sceneShader->sendConstant("miscInfo", miscData);
                
                sceneShader->send("viewMatrix"+(view > 0 ? std::to_string(view+1) : ""), viewMatrix);
                viewMatrix.clear();
                auto mat = sceneShader->shader->getUniformInfo("viewMatrix"+(view > 0 ? std::to_string(view+1) : ""));
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
                

                // auto modelIdx = sceneShader->send("modelMatrix", std::vector<glm::mat4>({ modelMat }));
                // size_t leakCountBefore, leakCountAfter;
                // uint64_t leakSizeBefore, leakSizeAfter;
                
                // __mem_leak_check(leakCountBefore, leakSizeBefore, false, "", false);
                // m_jointInfoVec4Cache.resize(1);
                // m_jointInfoVec4Cache[0] = glm::vec4(0.0f, 0.0f, (float)modelIdx, 0.0f);
                // sceneShader->sendConstant("jointInfo", m_jointInfoVec4Cache);
                // sceneShader->send("modelMatrix", modelMat);
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
                
                // Use the model matrix computed above for the background
                // float bgMatArr[16] = {
                //     modelMat[0][0], modelMat[0][1], modelMat[0][2], modelMat[0][3],
                //     modelMat[1][0], modelMat[1][1], modelMat[1][2], modelMat[1][3],
                //     modelMat[2][0], modelMat[2][1], modelMat[2][2], modelMat[2][3],
                //     modelMat[3][0], modelMat[3][1], modelMat[3][2], modelMat[3][3]
                // };
                // auto bgMat = Matrix4(bgMatArr);

            }
            // printf("viewMatrix: \n");
            // for (int r = 0; r < 4; ++r) {
            //     printf("%f, %f, %f, %f\n", vMatrix[r][0], vMatrix[r][1], vMatrix[r][2], vMatrix[r][3]);
            // }
        }

        // Track multi-group meshes through all checks
        bool isMultiGroup = meshGroups[mesh->getId()] > 1;
        
        if (sceneFrameCount % 60 == 1 && isMultiGroup) {
            // printf("[MULTI-GROUP] Loop index i=%d | Mesh ID=%d | Groups=%d | hasChildren=%s\n", 
            //        i, mesh->getId(), meshGroups[mesh->getId()],
            //        (meshChildren.find(i) != meshChildren.end()) ? "YES" : "NO");
        }
        
        if (mesh->visible == false) {
            if (sceneFrameCount % 60 == 0 && isMultiGroup) {
                // printf("[SKIP INVISIBLE] Mesh %d (multi-group) is invisible\n", i);
            }
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
        // Perform frustum culling (but skip culling during shadow pass to capture all shadow casters)
        if (!shadows && !isMeshInFrustum(mesh, viewProjectionMatrix)) {
            if (sceneFrameCount % 60 == 0 && isMultiGroup) {
                // printf("[SKIP CULLED] Mesh %d (multi-group) outside frustum\n", i);
            }
            i++;            
            continue; // Skip meshes outside the frustum
        }

        // bool defer = false;

        // if (meshChildren.find(i) != meshChildren.end()) {
        //     if (sceneFrameCount % 60 == 0 && isMultiGroup) {
        //         printf("[CHECK CHILDREN] Mesh %d (multi-group) has %zu children\n", 
        //                i, meshChildren[i].size());
        //     }
        //     int j = 0;
        //     for (auto child : meshChildren[i]) {
        //         if (child->visible == false) {
        //             j++;
        //             continue;
        //         }
        //         if (meshGroups[child->getId()] > 1) {
        //             // m_deferredChildIndices.push_back(j);
        //             // m_deferredParentIndices.push_back(i);
        //             // defer = true;
        //         }
        //         j++;
        //     }
        //     if (defer) {
        //         if (sceneFrameCount % 60 == 0 && isMultiGroup) {
        //             printf("[SKIP CHILD DEFER] Mesh %d (multi-group) deferred due to child\n", 
        //                    i);
        //         }
        //         i++;
        //         continue;
        //     }
        // }

        // if (meshGroups[mesh->getId()] > 1) {
        //     if (sceneFrameCount % 60 == 0) {
        //         printf("[DEFER ADD] Mesh %d has %d groups, adding to deferred list\n", 
        //                i, meshGroups[mesh->getId()]);
        //     }
        //     m_deferredMeshIndices.push_back(i);
        //     i++;
        //     continue;
        // }
        
        // Bounds check for matrices array
        // if (i >= matrices.size()) {
        //     if (sceneFrameCount % 60 == 0 && isMultiGroup) {
        //         printf("[SKIP BOUNDS] Mesh %d (multi-group) exceeds matrices size\n", 
        //                i);
        //     }
        //     i++;
        //     continue;
        // }
        
        // sceneShader->send("jointCount", std::vector<chaiscript::Boxed_Value>({ chaiscript::Boxed_Value(0) }));

        // gfx::Graphics::flushBatchedDrawsGlobal();
        // Force update to populate offsetMatrices with physics data
        // mesh->update(std::vector<float>{0, 0, 0}, std::vector<float>{0, 0, 0}, std::vector<float>{1, 1, 1}, nullptr);
        
        // sceneShader->sendConstant("miscInfo", {glm::vec4(0.0f, 1.0f, mesh->specData != nullptr ? 1.0f : 0.0f, 0.0f)});

        // Pass static matrix as scene transform, physics data is applied inside draw()
        auto matrix = matrices[i];
        mesh->draw(cg.instance, matrix, sceneShader, view == 0 ? deltaTime : 0, computeShader, shadows);
        drawnMeshIndices.insert(i);  // Mark as drawn
        if (meshChildren.find(i) != meshChildren.end() && computeShader == nullptr) {
            // Begin batching constants for all children
            sceneShader->beginConstantBatch();
            
            auto c = 0;
            auto zOffset = 0.0001f;
            for (auto child : meshChildren[i]) {
                if (child->visible == false) { c++; continue; }
                for (int j = 0; j < child->meshes.size(); ++j) {
                    float mArr[16] = {
                        1.0f, 0.0f, 0.0f, 0.0f,
                        0.0f, 1.0f, 0.0f, 0.0f,
                        0.0f, 0.0f, 1.0f, 0.0f,
                        0.0f, 0.0f, 0.0f, 1.0f
                    };
                    Matrix4 m = Matrix4(mArr);
                    sceneShader->sendConstant("miscInfo", {glm::vec4(0.0f, 1.0f, child->specData != nullptr ? 1.0f : 0.0f, shadows ? 1.0f : 0.0f)});

                    if (true || c > 0) {
                        auto mm = mesh->m_matrixCache[0];
                        mm[3][2] += zOffset;
                        zOffset += 0.0001f;
                        auto mc = sceneShader->send("modelMatrix", std::vector<glm::mat4>({mm}));
                        auto jic = mesh->m_jointInfoCache[0];
                        jic.z = (float)mc;
                        sceneShader->sendConstant("jointInfo", std::vector<glm::vec4>({ jic }));
                    }
                    child->meshes[j]->draw(cg.instance, m);
                }
                c++;
            }
            
            // Flush all queued constants at once
            sceneShader->endConstantBatch();
        }
        i++;
    }
    // Draw deferred child meshes in order of same index
    // i = 0;
    // m_drawnPairs.clear();
    // for (auto childIndex : m_deferredChildIndices) {
    //     // Bounds check for deferred parent indices
    //     // if (i >= m_deferredParentIndices.size()) {
    //     //     i++;
    //     //     continue;
    //     // }
        
    //     // printf("Drawing deferred child mesh index %d of parent mesh index %d\n", childIndex, m_deferredParentIndices[i]);
    //     // auto vbo = meshChildren[m_deferredParentIndices[i]][childIndex]->cachedVBOs.begin()->second;
    //     // printf("Using VBO %d\n", vbo);
    //     // glBindBuffer(GL_ARRAY_BUFFER, vbo);
    //     // printf("Buffer bound\n");
    //     for (auto parentId : m_deferredParentIndices) {
    //         // Bounds check for meshChildren and arrays
    //         // if (meshChildren.find(parentId) == meshChildren.end() || 
    //         //     meshChildren.find(m_deferredParentIndices[i]) == meshChildren.end() ||
    //         //     childIndex >= meshChildren[m_deferredParentIndices[i]].size()) {
    //         //     continue;
    //         // }
            
    //         int j = 0;
    //         for (auto child : meshChildren[parentId]) {
    //             if (!child || child->getId() == meshChildren[m_deferredParentIndices[i]][childIndex]->getId()) {
    //                 if (child->visible == false || m_drawnPairs.end() != std::find(m_drawnPairs.begin(), m_drawnPairs.end(), std::make_pair(parentId, j))) {
    //                     j++;
    //                     continue;
    //                 }
    //                 auto drawnPair = std::make_pair(parentId, j);
    //                 // printf("Drawing child mesh %d of parent mesh %d\n", j, parentId);
    //                 m_drawnPairs.push_back(drawnPair);
    //                 // sceneShader->send("jointCount", std::vector<chaiscript::Boxed_Value>({ chaiscript::Boxed_Value(0) }));
    //                 // gfx::Graphics::flushBatchedDrawsGlobal();
                   
    //                 // Draw the parent mesh first
    //                 // Bounds check for meshes and matrices arrays
    //                 if (parentId >= meshes.size() || parentId >= matrices.size() || !meshes[parentId]) {
    //                     continue;
    //                 }
                    
    //                 // Skip if already drawn in main loop
    //                 if (drawnMeshIndices.find(parentId) == drawnMeshIndices.end()) {
    //                     meshes[parentId]->draw(cg.instance, matrices[parentId], sceneShader, deltaTime, computeShader);
    //                     drawnMeshIndices.insert(parentId);
    //                 }
                    
    //                 float identityData[16] = {
    //                     1.0f, 0.0f, 0.0f, 0.0f,
    //                     0.0f, 1.0f, 0.0f, 0.0f,
    //                     0.0f, 0.0f, 1.0f, 0.0f,
    //                     0.0f, 0.0f, 0.0f, 1.0f
    //                 };
    //                 Matrix4 m = Matrix4(identityData);
    //                 // sceneShader->send("jointCount", std::vector<chaiscript::Boxed_Value>({ chaiscript::Boxed_Value(0) }));

    //                 if (computeShader == nullptr && j < child->meshes.size() && child->meshes[j]) {
    //                     child->meshes[j]->draw(cg.instance, m);
    //                 }
    //                 // cg.instance->present(nullptr);
    //             }
    //             j++;
    //         }
    //     }
    //     i++;
    // }

    // // printf("Drawing deferred parent meshes\n");

    // // if (sceneFrameCount % 60 == 0) {
    // //     printf("[DEFERRED CHECK] m_deferredMeshIndices has %zu meshes\n", m_deferredMeshIndices.size());
    // //     if (m_deferredMeshIndices.size() > 0) {
    // //         printf("[DEFERRED LOOP] Processing %zu deferred meshes\n", m_deferredMeshIndices.size());
    // //     }
    // // }

    // for (auto meshIndex : m_deferredMeshIndices) {
    //     // Bounds check for deferred mesh indices
    //     if (meshIndex >= meshes.size() || meshIndex >= matrices.size() || !meshes[meshIndex]) {
    //         if (sceneFrameCount % 60 == 0) {
    //             printf("[DEFERRED SKIP] Mesh %d failed bounds check\n", meshIndex);
    //         }
    //         continue;
    //     }
        
    //     // Skip if already drawn
    //     if (drawnMeshIndices.find(meshIndex) != drawnMeshIndices.end()) {
    //         if (sceneFrameCount % 60 == 0) {
    //             printf("[DEFERRED SKIP] Mesh %d already drawn\n", meshIndex);
    //         }
    //         continue;
    //     }
        
    //     auto mesh = meshes[meshIndex];
    //     auto matrix = matrices[meshIndex];
        
    //     if (sceneFrameCount % 60 == 0) {
    //         printf("[DEFERRED DRAW] Drawing mesh %d\n", meshIndex);
    //     }
        
    //     // Draw with same method as main loop - mesh->draw handles shader setup
    //     mesh->draw(cg.instance, matrix, sceneShader, deltaTime, computeShader);
    //     drawnMeshIndices.insert(meshIndex);  // Mark as drawn
    //     if (meshChildren.find(meshIndex) != meshChildren.end()) {
    //         for (auto child : meshChildren[meshIndex]) {
    //             if (!child || child->visible == false) {
    //                 continue;
    //             }
    //             for (int j = 0; j < child->meshes.size(); ++j) {
    //                 if (!child->meshes[j]) {
    //                     continue;
    //                 }
    //                 float identityData[16] = {
    //                     1.0f, 0.0f, 0.0f, 0.0f,
    //                     0.0f, 1.0f, 0.0f, 0.0f,
    //                     0.0f, 0.0f, 1.0f, 0.0f,
    //                     0.0f, 0.0f, 0.0f, 1.0f
    //                 };
    //                 Matrix4 m = Matrix4(identityData);
    //                 // sceneShader->send("jointCount", std::vector<chaiscript::Boxed_Value>({ chaiscript::Boxed_Value(0) }));
    //                 if (computeShader == nullptr) {
    //                     child->meshes[j]->draw(cg.instance, m);
    //                 }
    //             }
    //         }
    //     }
    //     // cg.instance->present(nullptr);
    // }
    
    // if (sceneFrameCount % 300 == 0) {
    //     __mem_leak_check(leakCountAfter, leakSizeAfter, true, "", false);
        
    //     size_t deltaCount = leakCountAfter - leakCountBefore;
    //     uint64_t deltaSize = leakSizeAfter - leakSizeBefore;
        
    //     printf("[SCENE DRAW] Frame %d | Delta: %zu objects, %llu bytes | Total Delta since last: %zu objects, %llu bytes\n",
    //            sceneFrameCount, deltaCount, deltaSize,
    //            leakCountAfter - lastSceneLeakCount, leakSizeAfter - lastSceneLeakSize);
        
    //     printf("  m_lightDirectionCache size: %zu\n", m_lightDirectionCache.size());
    //     printf("  m_lightColorCache size: %zu\n", m_lightColorCache.size());
    //     printf("  m_deferredChildIndices size: %zu\n", m_deferredChildIndices.size());
    //     printf("  m_deferredParentIndices size: %zu\n", m_deferredParentIndices.size());
    //     printf("  m_deferredMeshIndices size: %zu\n", m_deferredMeshIndices.size());
        
    //     lastSceneLeakCount = leakCountAfter;
    //     lastSceneLeakSize = leakSizeAfter;
    // }
    
    if (false && shadows == false && computeShader == nullptr) {
        for (auto ps : particleSystems) {
            // if (ps->visible == false) {
            //     continue;
            // }
            // gfx::Graphics::flushBatchedDrawsGlobal();
            ps->draw();
            // cg.instance->present(nullptr);
        }

        gfx::Texture::Settings settings;
        settings.width = meshes[0]->specularW;
        settings.height = meshes[0]->specularH;
        settings.format = PIXELFORMAT_RGBA8_UNORM;
        auto slices = gfx::Texture::Slices(gfx::TextureType::TEXTURE_2D);
        auto gfx = Module::getInstance<gfx::Graphics>(Module::M_GRAPHICS);
        // background_tex is created in the early !shadows block above.
        // Only build the background quad mesh here if it doesn't exist yet.
        if (background_mesh == nullptr && background_tex != nullptr) {
            // Create a quad mesh for the background texture
            std::vector<gfx::Buffer::DataDeclaration> bgVertexFormat;
            bgVertexFormat.push_back(gfx::Buffer::DataDeclaration("VertexPosition", gfx::DATAFORMAT_FLOAT_VEC3, 0));
            bgVertexFormat.push_back(gfx::Buffer::DataDeclaration("VertexTexCoord", gfx::DATAFORMAT_FLOAT_VEC2, sizeof(float) * 3));
            bgVertexFormat.push_back(gfx::Buffer::DataDeclaration("VertexColor", gfx::DATAFORMAT_FLOAT_VEC4, sizeof(float) * 5));
            bgVertexFormat.push_back(gfx::Buffer::DataDeclaration("VertexWeight", gfx::DATAFORMAT_FLOAT_VEC4, sizeof(float) * 9));
            bgVertexFormat.push_back(gfx::Buffer::DataDeclaration("VertexNormal", gfx::DATAFORMAT_FLOAT_VEC3, sizeof(float) * 13));
            bgVertexFormat.push_back(gfx::Buffer::DataDeclaration("VertexJoint", gfx::DATAFORMAT_FLOAT_VEC4, sizeof(float) * 16));
            
            // Create quad vertices for NON-INDEXED drawing (6 vertices for 2 triangles)
            // Draw at Z=0 (camera position) to ensure it's visible
            // Triangle 1: (0,2,1), Triangle 2: (0,3,2) in clockwise winding
            // Normals point back towards camera (0,0,1)
            float farZ = -20.0f;  // At camera position - will be pushed back by depth clear
            float bgQuadData[] = {
                // Triangle 1
                // x,      y,      z,     u,   v,    r,  g,  b,  a,   w1, w2, w3, w4,   nx, ny, nz,   j1, j2, j3, j4
                -20.0f, -20.0f, farZ, 0.0f, 1.0f, 1.0f,0.0f,1.0f,1.0f, 0.0f,0.0f,0.0f,0.0f, 0.0f,0.0f,1.0f, 0.0f,0.0f,0.0f,0.0f, // v0
                 20.0f, +20.0f, farZ, 1.0f, 0.0f, 1.0f,0.0f,1.0f,1.0f, 0.0f,0.0f,0.0f,0.0f, 0.0f,0.0f,1.0f, 0.0f,0.0f,0.0f,0.0f, // v2
                 20.0f, -20.0f, farZ, 1.0f, 1.0f, 1.0f,0.0f,1.0f,1.0f, 0.0f,0.0f,0.0f,0.0f, 0.0f,0.0f,1.0f, 0.0f,0.0f,0.0f,0.0f, // v1
                // Triangle 2
                -20.0f, -20.0f, farZ, 0.0f, 1.0f, 1.0f,0.0f,1.0f,1.0f, 0.0f,0.0f,0.0f,0.0f, 0.0f,0.0f,1.0f, 0.0f,0.0f,0.0f,0.0f, // v0
                -20.0f, +20.0f, farZ, 0.0f, 0.0f, 1.0f,0.0f,1.0f,1.0f, 0.0f,0.0f,0.0f,0.0f, 0.0f,0.0f,1.0f, 0.0f,0.0f,0.0f,0.0f, // v3
                 20.0f, +20.0f, farZ, 1.0f, 0.0f, 1.0f,0.0f,1.0f,1.0f, 0.0f,0.0f,0.0f,0.0f, 0.0f,0.0f,1.0f, 0.0f,0.0f,0.0f,0.0f  // v2
            };
            
            background_mesh = gfx->newMesh(bgVertexFormat, bgQuadData, sizeof(bgQuadData), 
                                            gfx::PRIMITIVE_TRIANGLES, gfx::BUFFERDATAUSAGE_STATIC);
            background_mesh->setTexture(background_tex);
            
            // NO index buffer - use non-indexed drawing like scene meshes
        }
        // sceneShader->send("jointCount", std::vector<chaiscript::Boxed_Value>({ chaiscript::Boxed_Value(0) }));
        auto cameraParams = meshes[0]->cameraParams[view];
        auto xpos = cameraParams.at("position")[0];
        auto ypos = cameraParams.at("position")[1];
        auto zpos = cameraParams.at("position")[2];
        float fov = cameraParams.at("fov")[0];
        float aspectRatio = cameraParams.at("aspectRatio")[0];
        auto height = 2 * zpos * tan(fov / 2.0f);
        auto width = aspectRatio * height;

        float bmatData[16] = {
            width, 0.0f, 0.0f, 0.0,
            0.0f, height, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            -vMatrix[3][0], ypos*(1.0f/3.0f), -zpos*2.0f, 1.0f  // Push background much further back
        };
        auto bmat = Matrix4(bmatData);
        glm::mat4 modelMat = glm::mat4(
            bmat.getColumn(0).x, bmat.getColumn(0).y, bmat.getColumn(0).z, bmat.getColumn(0).w,
            bmat.getColumn(1).x, bmat.getColumn(1).y, bmat.getColumn(1).z, bmat.getColumn(1).w,
            bmat.getColumn(2).x, bmat.getColumn(2).y, bmat.getColumn(2).z, bmat.getColumn(2).w,
            bmat.getColumn(3).x, bmat.getColumn(3).y, bmat.getColumn(3).z, bmat.getColumn(3).w
        );
        // m_modelMatrixCache.clear();
        // m_modelMatrixCache.push_back(modelMat);
        
        // Draw background FIRST so it gets modelCount = 0
        // Use COMPARE_ALWAYS with depth write disabled so it always renders behind everything
        if (false && background_mesh != nullptr) {
            static bool debugOnce = false;
            if (!debugOnce) {
                std::printf("[BACKGROUND] Drawing background mesh: width=%.2f height=%.2f zpos=%.2f\n", 
                    width, height, zpos);
                std::printf("[BACKGROUND] Transform: [%.2f %.2f %.2f %.2f]\n", 
                    bmat.getColumn(3).x, bmat.getColumn(3).y, bmat.getColumn(3).z, bmat.getColumn(3).w);
                debugOnce = true;
            }
            
            // Disable culling and depth writes for background
            cg.instance->setMeshCullMode(gfx::CULL_NONE);
            cg.instance->setDepthMode(gfx::CompareMode::COMPARE_ALWAYS, false);
            
            // Try a simple full-screen quad transform (scale by view dimensions)
            float simpleTransform[16] = {
                50.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 50.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f,
                0.0f, 0.0f, -50.0f, 1.0f  // Put at Z=-50 (far behind)
            };
            Matrix4 simpleMatrix(simpleTransform);
            
            // Send modelMatrix through shader wrapper to maintain proper modelCount
            glm::mat4 simpleMat = glm::mat4(
                simpleMatrix.getColumn(0).x, simpleMatrix.getColumn(0).y, simpleMatrix.getColumn(0).z, simpleMatrix.getColumn(0).w,
                simpleMatrix.getColumn(1).x, simpleMatrix.getColumn(1).y, simpleMatrix.getColumn(1).z, simpleMatrix.getColumn(1).w,
                simpleMatrix.getColumn(2).x, simpleMatrix.getColumn(2).y, simpleMatrix.getColumn(2).z, simpleMatrix.getColumn(2).w,
                simpleMatrix.getColumn(3).x, simpleMatrix.getColumn(3).y, simpleMatrix.getColumn(3).z, simpleMatrix.getColumn(3).w
            );
            std::vector<glm::mat4> bgMatVec;
            bgMatVec.push_back(simpleMat);
            int modelIdx = sceneShader->send("modelMatrix", bgMatVec);
            
            // Batch background mesh constants
            sceneShader->beginConstantBatch();
            std::vector<glm::vec4> jointInfo = { glm::vec4(0.0f, 0.0f, (float)modelIdx, 0.0f) };
            sceneShader->sendConstant("jointInfo", jointInfo);
            sceneShader->endConstantBatch();
            
            background_mesh->draw(gfx, simpleMatrix);
            
            // Re-enable backface culling and normal depth testing for foreground objects
            cg.instance->setMeshCullMode(gfx::CULL_BACK);
            cg.instance->setDepthMode(gfx::CompareMode::COMPARE_LEQUAL, true);
        }
    }
}

void chai_scene::draw(const glm::mat4 &viewMatrix1, const glm::mat4 &viewMatrix2, const glm::mat4 &viewMatrix3, const glm::mat4 &viewMatrix4, int viewCount) {
    // MemPlumber::start();
    // static int drawCallCount = 0;
    // static int lastFrameDrawCalls = 0;
    // if (++drawCallCount % 60 == 1) {
    //     printf("[SCENE DRAW] Frame draw call #%d (last 60 frames had %d draw() calls)\n", drawCallCount, drawCallCount - lastFrameDrawCalls);
    //     lastFrameDrawCalls = drawCallCount;
    // }
    
    auto& cg = ChaiLove::getInstance()->chai_gfx;
    
    // Render collision debug visualization with camera
    // Create a simple perspective projection matrix for debug rendering
    float aspectRatio = (float)cg.width / (float)cg.height;
    glm::mat4 projectionMatrix = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 100.0f);
    // ChaiLove::getInstance()->chai_collisions.renderDebugWithCamera(viewMatrix1, projectionMatrix);
    
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
    
    // Initialize shadow map if not already done
    if (!shadowMapInitialized) {
        initShadowMap();
    }
    
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
            // size_t leakCountBefore, leakCountAfter;
            // uint64_t leakSizeBefore, leakSizeAfter;
            
            // __mem_leak_check(leakCountBefore, leakSizeBefore, false, "", false);
            
            if (sceneShader && sceneShader->shader) {
                cg.instance->setShader(sceneShader->shader);
            } else {
                printf("[SCENE] Warning: sceneShader=%p, shader=%p - using default shader\n", 
                    sceneShader, sceneShader ? sceneShader->shader : nullptr);
                fflush(stdout);
                cg.instance->setShader();
            }
            cg.instance->setDepthMode(gfx::CompareMode::COMPARE_LEQUAL, true);
            if (sceneShader) {
                RAM_SNAP(r0);
                // Reset per-frame model counters (modelCount, modelJointOffset, m_nextMatrixSlot).
                // This is chai_shader::newFrame(), NOT Shader::newFrame() — it does NOT
                // touch descriptor pools or ring buffers. Without this, modelCount grows
                // unbounded → out-of-bounds writes to info->floats → SIGSEGV.
                sceneShader->newFrame();
                
                // Calculate light space matrices for shadow rendering
                if (meshes.size() > 0 && shadowMapInitialized) {
                    auto mesh = meshes[0];
                    if (!mesh->lightParams.empty() && mesh->lightParams.size() > 0 && mesh->cameraParams.size() > 0) {
                        auto lightParams = mesh->lightParams[0];
                        auto cameraParams = mesh->cameraParams[0];
                        
                        // Get camera parameters
                        float fov = cameraParams.at("fov")[0];
                        float aspectRatio = cameraParams.at("aspectRatio")[0];
                        float nearClip = cameraParams.at("near")[0];
                        float farClip = cameraParams.at("far")[0];
                        glm::mat4 cameraProjection = glm::perspective(fov, aspectRatio, nearClip, farClip);
                        
                        glm::vec3 lightPos = glm::vec3(lightParams["position"][0], lightParams["position"][2], lightParams["position"][1]);
                        
                        // Extract actual light direction from lightParams
                        glm::vec3 lightDir = glm::vec3(0.0f, -1.0f, 0.0f);  // Default fallback
                        int dirIndex = 0;
                        for (auto axis : lightParams["direction"]) {
                            if (dirIndex < 3) {
                                lightDir[dirIndex] = axis;
                                dirIndex++;
                            }
                        }
                        
                        // Create light view matrix and rotate -90 degrees on Y axis
                        glm::mat4 baseLightView = glm::lookAt(lightPos, lightPos + lightDir, glm::vec3(1.0f, 0.0f, 0.0f));
                        // glm::mat4 xRotation = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 1.0f, 0.0f));
                        m_cachedLightView = baseLightView; // Move light view forward to encompass scene better

                        
                        // Calculate camera-aligned light projection
                        m_cachedLightProjection = calculateLightProjection(viewMatrix1, cameraProjection, m_cachedLightView);
                        m_cachedLightSpaceMatrix = m_cachedLightProjection * m_cachedLightView;
                        
                        // Render shadow pass to shadow map texture
                        RAM_SNAP(r1);
                        renderShadowPass(m_cachedLightView, m_cachedLightProjection, 0);
                        RAM_SNAP(r2);
                        RAM_DELTA("renderShadowPass", r1, r2);
                        // auto vgfx = dynamic_cast<gfx::vulkan::Graphics*>(cg.instance);
                        // vgfx->beginFrame();
                        // Bind shadow map for main rendering
                        if (shadowMapTexture) {
                            sceneShader->sendTexture("shadowMap", shadowMapTexture);
                        }

                        // Restore default render target (back to screen)
                        // cg.instance->setRenderTarget();
                        
                        // Re-enable color writes for normal rendering
                        cg.instance->setColorMask({true, true, true, true});
                    }
                }
                // Reset modelCount after shadow pass so main pass reuses model slots 0..N-1.
                // Shadow pass draws are already recorded, so its slots can be overwritten.
                sceneShader->newFrame();
                RAM_SNAP(r3);
                sceneShader->send("viewMatrix", viewMatrix1);
                RAM_SNAP(r4);
                RAM_DELTA("newFrame+sendView", r0, r4);
                RAM_DELTA("sendViewMatrix", r3, r4);
            }
            
            // Draw background BEFORE scene meshes so scene draws on top
            if (false && background_mesh != nullptr && meshes.size() > 0) {
                // CRITICAL FIX: Background must render behind everything
                // Use COMPARE_ALWAYS with DEPTH WRITE DISABLED so:
                // 1. Background always renders (COMPARE_ALWAYS)
                // 2. But doesn't block foreground objects (write=false keeps depth at far)
                // This allows foreground meshes to render on top with normal depth testing
                cg.instance->setMeshCullMode(gfx::CULL_NONE);
                cg.instance->setDepthMode(gfx::CompareMode::COMPARE_ALWAYS, false);  // CRITICAL: write=false
                
                // Use identity matrix since vertices are already positioned/scaled in model space
                float modelMatData[16] = {
                    1.0f, 0.0f, 0.0f, 0.0f,
                    0.0f, 1.0f, 0.0f, 0.0f,
                    0.0f, 0.0f, 1.0f, 0.0f,
                    0.0f, 0.0f, 0.0f, 1.0f
                };
                
                Matrix4 bgMatrix(modelMatData);

                // Send background model matrix to shader
                std::vector<glm::mat4> bgMatVec;
                bgMatVec.push_back(glm::make_mat4(modelMatData));
                int bgModelIdx = sceneShader->send("modelMatrix", bgMatVec);
                
                // Batch background constants
                sceneShader->beginConstantBatch();
                std::vector<glm::vec4> jointInfo = { glm::vec4(0.0f, 0.0f, (float)bgModelIdx, 0.0f) };
                sceneShader->sendConstant("jointInfo", jointInfo);
                sceneShader->endConstantBatch();
                
                background_mesh->draw(cg.instance, bgMatrix);
                
                // Restore normal state for scene meshes
                cg.instance->setMeshCullMode(gfx::CULL_BACK);
                cg.instance->setDepthMode(gfx::CompareMode::COMPARE_LEQUAL, true);
            }
            // sceneShader->sendConstant("miscInfo", {glm::vec4(0.0f, 1.0f, 1.0f, 0.0f)});  // Disable multiview, use standard rendering
            
            RAM_SNAP(r5);
            drawMeshes(false, 0);
            RAM_SNAP(r6);
            RAM_DELTA("drawMeshes", r5, r6);
            cg.instance->setShader(); 

            // drawMeshes(false, 0);
            // cg.instance->setShader();  
            // __mem_leak_check(leakCountAfter, leakSizeAfter, false, "", false);
    
            // printf("Leak delta: %zu objects, %llu bytes\n", 
            //     leakCountAfter - leakCountBefore,
            //     leakSizeAfter - leakSizeBefore);
        }
        if (viewCount == 2) {
            // Track render calls per frame to detect duplicate rendering
            // static size_t lastRenderFrame = SIZE_MAX;
            // static int renderCallsThisFrame = 0;
            // auto* vulkanGraphics = static_cast<love::gfx::vulkan::Graphics*>(cg.instance);
            // size_t currentVulkanFrame = vulkanGraphics->getCurrentFrame();
            // 
            // if (currentVulkanFrame != lastRenderFrame) {
            //     if (renderCallsThisFrame > 1) {
            //         printf("[SPLIT SCREEN WARNING] Frame %zu had %d render calls (should be 1)\n", 
            //             lastRenderFrame, renderCallsThisFrame);
            //     }
            //     renderCallsThisFrame = 0;
            //     lastRenderFrame = currentVulkanFrame;
            // }
            // renderCallsThisFrame++;
            // 
            // printf("[SPLIT SCREEN] Frame %zu, Render call #%d: Starting 2-view split-screen\n",
            //     currentVulkanFrame, renderCallsThisFrame);
            // 
            // // Log stack trace to identify where duplicate calls originate from
            // if (renderCallsThisFrame > 1) {
            //     printf("[SPLIT SCREEN CALL STACK] Duplicate call detected - checking if from script re-invocation\n");
            //     // The fact we're here means chai_scene::draw() was called again in the same frame
            //     // This must be from the ChaiScript draw() callback being invoked twice
            // }
            
            cg.instance->setShader(sceneShader->shader);
            cg.instance->setDepthMode(gfx::CompareMode::COMPARE_LEQUAL, true);
            // Reset per-frame model counters — see viewCount==1 path for details.
            sceneShader->newFrame();
            
            // Render shadow maps for both views if shadow mapping is enabled
            if (meshes.size() > 0 && shadowMapInitialized) {
                auto mesh = meshes[0];
                if (!mesh->lightParams.empty() && mesh->lightParams.size() > 0 && mesh->cameraParams.size() > 0) {
                    auto lightParams = mesh->lightParams[0];
                    auto cameraParams = mesh->cameraParams[0];
                    
                    // Get camera parameters
                    float fov = cameraParams.at("fov")[0];
                    float aspectRatio = cameraParams.at("aspectRatio")[0];
                    float nearClip = cameraParams.at("near")[0];
                    float farClip = cameraParams.at("far")[0];
                    glm::mat4 cameraProjection = glm::perspective(fov, aspectRatio, nearClip, farClip);
                    
                    glm::vec3 lightPos = glm::vec3(
                        lightParams["position"][0], 
                        lightParams["position"][1], 
                        lightParams["position"][2]
                    );
                    glm::vec3 lightDir = glm::vec3(0.0f, -1.0f, 0.0f);
                    m_cachedLightView = glm::lookAt(lightPos, lightPos + lightDir, glm::vec3(0.0f, 0.0f, -1.0f));
                    
                    // Calculate camera-aligned light projection
                    m_cachedLightProjection = calculateLightProjection(viewMatrix1, cameraProjection, m_cachedLightView);
                    m_cachedLightSpaceMatrix = m_cachedLightProjection * m_cachedLightView;
                    
                // printf("[SCENE RENDER] About to call renderShadowPass\n");
                    // Render shadow pass (can be shared between views if light is the same)
                    renderShadowPass(m_cachedLightView, m_cachedLightProjection, 0);
                // printf("[SCENE RENDER] Returned from renderShadowPass\n");
                    
                    // Bind shadow map for main rendering
                    if (shadowMapTexture) {
                        // printf("[SCENE RENDER] Binding shadow map texture\n");
                        sceneShader->sendTexture("shadowMap", shadowMapTexture);
                    }
                    // NOTE: renderShadowPass already restored render target and color mask
                }
            }
            
            // Since multiview is not supported on this hardware,
            // render both views as separate draws with different view matrices
            
            // Render view 1 (left side)
            // printf("[SPLIT SCREEN] Rendering view 1 (left)\n");
            sceneShader->send("viewMatrix", viewMatrix1);
            sceneShader->send("splitScreenMode", 3.0f);  // Disable multiview, use standard rendering
            cg.instance->setSplitScreenViewport(0, 2);  // Left half
            drawMeshes(false, 0);
            
            // Render view 2 (right side) - accumulate on same target
            // printf("[SPLIT SCREEN] Rendering view 2 (right)\n");
            sceneShader->send("viewMatrix2", viewMatrix2);
            
            
            cg.instance->setSplitScreenViewport(1, 2);  // Right half
            // sceneShader->beginConstantBatch();
            // sceneShader->sendConstant("miscInfo", {glm::vec4(1.0f, 0.0f, 0.0f, 0.0f)});  // Disable multiview, use standard rendering
            // sceneShader->endConstantBatch();
            drawMeshes(false, 1);
            
            // Reset viewport to fullscreen for next frame
            cg.instance->setSplitScreenViewport(0, 1);  // Reset to single viewport (fullscreen)
            
            cg.instance->setShader();  
        }
        if (viewCount == 3) {
            sceneShader->send("viewMatrix", viewMatrix1);
            drawMeshes(false, 0);
            sceneShader->send("viewMatrix", viewMatrix2);
            drawMeshes(false, 1);
            sceneShader->send("viewMatrix", viewMatrix3);
            drawMeshes(false, 2);
            cg.instance->setShader();  
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
            cg.instance->setShader();
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
    
    if (sceneShader && sceneShader->shader) {
        cg.instance->setShader(sceneShader->shader);
    } else {
        printf("[SCENE] Warning: prepareScreen - sceneShader=%p, shader=%p - using default shader\n", 
            sceneShader, sceneShader ? sceneShader->shader : nullptr);
        fflush(stdout);
        cg.instance->setShader();
    }
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
    sceneShader->send("projectionMatrix2", m_projectionMatrixBoxedCache);
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