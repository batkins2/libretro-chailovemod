#include "../ChaiLove.h"
#include <cmath> // For M_PI
#include <GL/gl.h> // For OpenGL functions like glOrtho
#ifndef TINY_GLTF_H_
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define TINYGLTF_USE_RAPIDJSON
#define TINYGLTF_NO_FS
#define TINYGLTF_NO_STB_IMAGE
// #define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_EXTERNAL_IMAGE
#include "../../vendor/tinygltf/tiny_gltf.h"
#endif

namespace love
{
chai_collisions::chai_collisions()
{
    setProcessFrequency(30); // Default to 30 FPS
}
chai_collisions::~chai_collisions()
{
}

// Layer that objects can be in, determines which other objects it can collide with
// Typically you at least want to have 1 layer for moving bodies and 1 layer for static bodies, but you can have more
// layers if you want. E.g. you could have a layer for high detail collision (which is not used by the physics simulation
// but only if you do collision testing).
namespace Layers
{
	static constexpr JPH::ObjectLayer NON_MOVING = 0;
	static constexpr JPH::ObjectLayer MOVING = 1;
	static constexpr JPH::ObjectLayer AI = 2;
    static constexpr JPH::ObjectLayer BOUNDARY = 3;
	static constexpr JPH::ObjectLayer NUM_LAYERS = 4;
};

class chai_collisions::MyContactListener : public JPH::ContactListener
{
private:
    chai_collisions* mCollisionSystem;

    struct PendingEvent {
        chai_collisions::CharacterController* characterID;
        chai_collisions::RigidMesh* otherBodyID;
        float normalX; // Store the contact normal's X component
        float normalZ; // Add Z component for front/back collisions
    };
    std::vector<PendingEvent> mPendingEvents;
    struct PendingCharacterEvent {
        JPH::BodyID character1ID;
        JPH::BodyID character2ID;
    };
    std::vector<PendingCharacterEvent> mPendingCharacterEvents;
    
public:
    MyContactListener(chai_collisions* collisionSystem) : mCollisionSystem(collisionSystem) {}
    
    virtual void OnContactAdded(const JPH::Body &inBody1, const JPH::Body &inBody2, const JPH::ContactManifold &inManifold, JPH::ContactSettings &ioSettings) override
    {
        if (inBody1.GetObjectLayer() == Layers::NON_MOVING || inBody2.GetObjectLayer() == Layers::NON_MOVING) {
            // If one of the bodies is a non-moving object, ignore the contact
            return;
        }
        JPH::BodyID body1ID = inBody1.GetID();
        JPH::BodyID body2ID = inBody2.GetID();
        // printf("Contact added between bodies %d and %d\n", body1ID, body2ID);
        // printf("Body 1 Layer: %d, Body 2 Layer: %d\n", inBody1.GetObjectLayer(), inBody2.GetObjectLayer());
        if (body1ID == body2ID)
            return;

        if (inBody1.GetObjectLayer() == 2 && inBody2.GetObjectLayer() == 2 &&
            inBody1.GetUserData() == 1 && inBody2.GetUserData() == 1) {
            // Ignore collisions between AI characters
            ioSettings.mIsSensor = true; // Make the contact a sensor to avoid physical response
            return;
        }

        // Check if either body is a character controller
        for (auto &cc : mCollisionSystem->characterControllers) {
            if (cc->bodyID == body1ID || cc->bodyID == body2ID) {
                for (auto &r : mCollisionSystem->rigidMeshes) {
                    auto ref = ("player" + std::to_string(r->meshRef));
                    // printf("Comparing character %s with ref %s\n", cc->charId.c_str(), ref.c_str());
                    if (ref == cc->charId && (r->bodyID == body1ID || r->bodyID == body2ID)) {
                        // Get manifold side contact points (-x or +x)
                        float normalX = inManifold.mWorldSpaceNormal.GetX();
                        float normalZ = inManifold.mWorldSpaceNormal.GetZ();
                        // auto vel = cc->character->GetLinearVelocity();
                        if (normalZ < 0) {
                            // printf("Front collision detected for character %s\n", cc->charId.c_str());
                            // cc->collidedZ = true;
                        } else {
                            cc->collidedZ = false;
                        }
                        // Queue the event for processing after physics step
                        mPendingEvents.emplace_back(PendingEvent{cc, r, normalX, normalZ});
                        break;
                    }
                }
                break;
            }
        }
        
        // if (inBody1.GetObjectLayer() == 1 && inBody2.GetObjectLayer() == 2) {
        //     for (auto &cc : mCollisionSystem->characterControllers) {
        //         if (cc->bodyID == body2ID) {
        //             cc->collidedX = true;
        //             cc->collidedZ = true;
        //             printf("Character %s collided with moving object %d\n", cc->charId.c_str(), body1ID);
        //             mPendingCharacterEvents.emplace_back(PendingCharacterEvent{body1ID, body2ID});
        //         }
        //     }
        // } else if (inBody1.GetObjectLayer() == 2 && inBody2.GetObjectLayer() == 1) {
        //     for (auto &cc : mCollisionSystem->characterControllers) {
        //         if (cc->bodyID == body1ID) {
        //             cc->collidedX = true;
        //             cc->collidedZ = true;
        //             printf("Character %s collided with moving object %d\n", cc->charId.c_str(), body2ID);
        //             mPendingCharacterEvents.emplace_back(PendingCharacterEvent{body1ID, body2ID});
                    
        //         }
        //     }
        // if (inBody1.GetObjectLayer() == 2 && inBody2.GetObjectLayer() == 2) {
        //     printf("Character collision detected between bodies %d and %d\n", body1ID, body2ID);
        //     int i = 0;
        //     int char1Index = -1;
        //     int char2Index = -1;
        //     for (auto &cc : mCollisionSystem->characterControllers) {
        //         if (cc->bodyID == body2ID) {
        //             // cc->collidedX = true;
        //             // cc->collidedZ = true;
        //             printf("Character %s collided with character %d\n", cc->charId.c_str(), body1ID);
                    
        //             char2Index = i;
        //         }
        //         if (cc->bodyID == body1ID) {
        //             // cc->collidedX = true;
        //             // cc->collidedZ = true;
        //             printf("Character %s collided with character %d\n", cc->charId.c_str(), body2ID);
                    
        //             char1Index = i;
        //         }
        //         i++;
        //     }
        //     if (char1Index != -1 && char2Index != -1) {
        //         mCollisionSystem->characterControllers[char1Index]->collidedX = true;
        //         mCollisionSystem->characterControllers[char1Index]->collidedZ = true;
        //         mCollisionSystem->characterControllers[char2Index]->collidedX = true;
        //         mCollisionSystem->characterControllers[char2Index]->collidedZ = true;
        //         mPendingCharacterEvents.emplace_back(PendingCharacterEvent{body1ID, body2ID});
        //     }
        // }
        // bool isEdgeContact = (inManifold.mRelativeContactPointsOn1.size() <= 2 && 
        //                 inManifold.mPenetrationDepth < 0.05f);

        // if (isEdgeContact) {
        //     // Reduce friction for edge contacts to prevent sticking
        //     ioSettings.mCombinedFriction *= 0.3f;
        //     // printf("Edge contact detected\n");
        // }
        

        // Check if either body is a boundary
        // if (inBody1.GetObjectLayer() == 3 || inBody2.GetObjectLayer() == 3) {
        //     // If one of the bodies is a boundary, ignore the contact
        //     ioSettings.isSensor = true; // Make the contact a sensor to avoid physical response
        // }
        
        
    }

    virtual void OnContactPersisted(const JPH::Body &inBody1, const JPH::Body &inBody2, const JPH::ContactManifold &inManifold, JPH::ContactSettings &ioSettings) override
    {
        if (inBody1.GetObjectLayer() == Layers::AI && inBody2.GetObjectLayer() == Layers::AI) {
            JPH::BodyID body1ID = inBody1.GetID();
            JPH::BodyID body2ID = inBody2.GetID();
            for (auto &cc : mCollisionSystem->characterControllers) {
                if (cc->bodyID == body2ID && cc->charId.find("player") == std::string::npos) {
                    cc->collidedX = true;
                    cc->collidedZ = true;
                    // printf("Character %s collided with character %d\n", cc->charId.c_str(), body1ID);
                    
                    // char2Index = i;
                }
                if (cc->bodyID == body1ID && cc->charId.find("player") == std::string::npos) {
                    cc->collidedX = true;
                    cc->collidedZ = true;
                    // printf("Character %s collided with character %d\n", cc->charId.c_str(), body2ID);
                    
                }
            }
        }

        JPH::BodyID body1ID = inBody1.GetID();
        JPH::BodyID body2ID = inBody2.GetID();

        // Check if either body is a character controller
        for (auto &cc : mCollisionSystem->characterControllers) {
            if (cc->bodyID == body1ID || cc->bodyID == body2ID) {
                for (auto &r : mCollisionSystem->rigidMeshes) {
                    auto ref = ("player" + std::to_string(r->meshRef));
                    // printf("Comparing character %s with ref %s\n", cc->charId.c_str(), ref.c_str());
                    if (ref == cc->charId && (r->bodyID == body1ID || r->bodyID == body2ID)) {
                        // Get manifold side contact points (-x or +x)
                        float normalX = inManifold.mWorldSpaceNormal.GetX();
                        float normalZ = inManifold.mWorldSpaceNormal.GetZ();
                        // auto vel = cc->character->GetLinearVelocity();
                        if (normalZ < 0) {
                            printf("Front collision detected for character %s\n", cc->charId.c_str());
                            cc->collidedZ = true;
                        } else if (normalZ > 0) {
                            printf("Back collision detected for character %s\n", cc->charId.c_str());
                            cc->collidedZ = true;
                        } else {
                            cc->collidedZ = false;
                        }
                        // Queue the event for processing after physics step
                        mPendingEvents.emplace_back(PendingEvent{cc, r, normalX, normalZ});
                        break;
                    }
                }
                break;
            }
        }
    }

    virtual void OnContactRemoved(const JPH::SubShapeIDPair &inSubShapePair) override
    {
        // Handle contact removal if needed
    }

    void ProcessPendingEvents(JPH::PhysicsSystem* physicsSystem) {
        if (mPendingEvents.empty() && mPendingCharacterEvents.empty()) return;
        
        JPH::BodyInterface& bodyInterface = physicsSystem->GetBodyInterface();
        
        auto pendingEvents = std::vector<PendingEvent>();

        for (const auto& event : mPendingEvents) {
            // Now it's safe to modify physics bodies
            auto vel = bodyInterface.GetLinearVelocity(event.characterID->bodyID);
            if (event.normalZ < 0) {
                // vel.SetZ(200.0f);
                // event.characterID->collidedZ = false; // Reset collision flag after processing
                // bodyInterface.SetLinearVelocity(event.characterID->bodyID, vel);
            }
            vel.SetY(0); // Zero out Y component to avoid affecting vertical movement
            vel.SetZ(0);
            if (event.normalX < 0 && vel.GetX() < 0) {
                // Do nothing, allow movement
                vel.SetX(vel.GetX() * 2.0f);
            } else if (event.normalX < 0 && vel.GetX() > 0) {
                // Do nothing, allow movement
                vel.SetX(vel.GetX() * 2.0f);
            } else {
                vel.SetX(0);
            }
            bodyInterface.SetLinearVelocity(event.otherBodyID->bodyID, vel);
            // bodyInterface.SetAngularVelocity(event.otherBodyID, JPH::Vec3::sZero());
            bodyInterface.ActivateBody(event.otherBodyID->bodyID);
            pendingEvents.push_back(PendingEvent{event.characterID, event.otherBodyID, 0.0f, 0.0f}); // Clear normalX and normalZ after processing
        }
        
        for (const auto& event : mPendingCharacterEvents) {
            // Now it's safe to modify physics bodies
            auto vel1 = bodyInterface.GetLinearVelocity(event.character1ID);
            auto vel2 = bodyInterface.GetLinearVelocity(event.character2ID);
            vel1.SetX(0); // Zero out X component to avoid affecting horizontal movement
            vel2.SetX(0);
            vel1.SetZ(0);
            vel2.SetZ(0);
            printf("Character collision: Setting velocities to zero\n");
            bodyInterface.SetLinearVelocity(event.character1ID, vel1);
            bodyInterface.SetLinearVelocity(event.character2ID, vel2);
        }

        mPendingEvents.clear();
        mPendingEvents = pendingEvents; // Retain unprocessed events
        mPendingCharacterEvents.clear();
    }
};

// Add this class after MyContactListener in chai_collisions.h:
class CharacterContactListener : public JPH::CharacterContactListener
{
private:
    chai_collisions* mCollisionSystem;
    
public:
    CharacterContactListener(chai_collisions* collisionSystem) : mCollisionSystem(collisionSystem) {}
    
    // Called when character hits something
    virtual void OnContactAdded(const JPH::CharacterVirtual *inCharacter, const JPH::BodyID &inBodyID2, const JPH::SubShapeID &inSubShapeID2, JPH::RVec3Arg inContactPosition, JPH::Vec3Arg inContactNormal, JPH::CharacterContactSettings &ioSettings) override {
        // Find which character controller this belongs to by comparing character pointers
        chai_collisions::CharacterController* characterController = nullptr;
        for (auto& cc : mCollisionSystem->characterControllers) {
            // Compare the character pointers directly instead of using GetBodyID()
            if (cc->character && cc->characterVirtual == inCharacter) {
                characterController = cc;
                break;
            }
        }
        
        if (!characterController) {
            return;
        }
        
        // Get the physics system
        auto ps = mCollisionSystem->worlds->worlds[0]->physics_system;
        JPH::BodyInterface& bodyInterface = ps->GetBodyInterface();
        
        // Get what we hit
        JPH::ObjectLayer hitLayer = bodyInterface.GetObjectLayer(inBodyID2);
        
        printf("Character %s contacted layer %d\n", characterController->charId.c_str(), hitLayer);
        
        // Handle different collision types
        switch (hitLayer) {
            case Layers::AI:
                // Character hit AI - stop movement
                characterController->collidedX = true;
                characterController->collidedZ = true;
                ioSettings.mCanPushCharacter = false;      // AI can't push this character
                ioSettings.mCanReceiveImpulses = false;    // Character won't receive impulses from AI
                printf("Character %s hit AI - blocking movement\n", characterController->charId.c_str());
                break;
                
            case Layers::MOVING:
                // Character hit moving object - allow some interaction
                ioSettings.mCanPushCharacter = true;       // Moving objects can push character
                ioSettings.mCanReceiveImpulses = true;     // Character can receive impulses
                printf("Character %s hit moving object\n", characterController->charId.c_str());
                break;
                
            case Layers::NON_MOVING:
                // Character hit static geometry - full stop
                characterController->collidedX = true;
                characterController->collidedZ = true;
                ioSettings.mCanPushCharacter = false;      // Static objects can't be pushed
                ioSettings.mCanReceiveImpulses = false;    // No impulses from static objects
                printf("Character %s hit static geometry - full stop\n", characterController->charId.c_str());
                break;
                
            case Layers::BOUNDARY:
                // Character hit boundary - hard stop
                characterController->collidedX = true;
                characterController->collidedZ = true;
                ioSettings.mCanPushCharacter = false;
                ioSettings.mCanReceiveImpulses = false;
                printf("Character %s hit boundary - hard stop\n", characterController->charId.c_str());
                break;
        }
        
        // Analyze contact for better collision response
        analyzeCharacterContact(characterController, inContactNormal, ioSettings);
    }

    virtual void OnContactSolve(const JPH::CharacterVirtual *inCharacter, const JPH::BodyID &inBodyID2, const JPH::SubShapeID &inSubShapeID2, JPH::RVec3Arg inContactPosition, JPH::Vec3Arg inContactNormal, JPH::Vec3Arg inContactVelocity, const JPH::PhysicsMaterial *inContactMaterial, JPH::Vec3Arg inCharacterVelocity, JPH::Vec3 &ioNewCharacterVelocity) override
    {
        // Find the character controller by comparing character pointers
        chai_collisions::CharacterController* characterController = nullptr;
        for (auto& cc : mCollisionSystem->characterControllers) {
            if (cc->character && cc->characterVirtual == inCharacter) {
                characterController = cc;
                break;
            }
        }
        
        if (!characterController) {
            return;
        }
        
        // Get what we hit
        auto ps = mCollisionSystem->worlds->worlds[0]->physics_system;
        JPH::BodyInterface& bodyInterface = ps->GetBodyInterface();
        JPH::ObjectLayer hitLayer = bodyInterface.GetObjectLayer(inBodyID2);
        
        // Modify velocity based on what was hit
        switch (hitLayer) {
            case Layers::AI:
                // Stop horizontal movement when hitting AI
                ioNewCharacterVelocity.SetX(0.0f);
                ioNewCharacterVelocity.SetZ(0.0f);
                // Keep Y velocity for gravity/jumping
                printf("Character %s velocity stopped due to AI collision\n", characterController->charId.c_str());
                break;
                
            case Layers::MOVING:
                // Reduce velocity when hitting moving objects
                ioNewCharacterVelocity = ioNewCharacterVelocity * 0.5f;
                ioNewCharacterVelocity.SetY(inCharacterVelocity.GetY()); // Preserve Y velocity
                printf("Character %s velocity reduced due to moving object collision\n", characterController->charId.c_str());
                break;
                
            case Layers::NON_MOVING:
            case Layers::BOUNDARY:
                // Project velocity along the surface for wall sliding
                JPH::Vec3 projectedVelocity = inCharacterVelocity - inContactNormal * inCharacterVelocity.Dot(inContactNormal);
                ioNewCharacterVelocity = projectedVelocity;
                printf("Character %s sliding along surface\n", characterController->charId.c_str());
                break;
        }
    }
    
private:
    void analyzeCharacterContact(chai_collisions::CharacterController* cc, JPH::Vec3Arg contactNormal, JPH::CharacterContactSettings& ioSettings) {
        // Determine collision direction for precise movement blocking
        float normalX = abs(contactNormal.GetX());
        float normalZ = abs(contactNormal.GetZ());
        float normalY = abs(contactNormal.GetY());
        
        // Block movement in specific directions based on contact normal
        if (normalX > 0.7f) {
            cc->collidedX = true;
            printf("Character %s X movement blocked\n", cc->charId.c_str());
        }
        if (normalZ > 0.7f) {
            cc->collidedZ = true;
            printf("Character %s Z movement blocked\n", cc->charId.c_str());
        }
        
        // Handle ground contact
        if (normalY > 0.8f && contactNormal.GetY() > 0) {
            // Character is on ground - enable full friction
            ioSettings.mCanPushCharacter = false;
            printf("Character %s on ground - full friction\n", cc->charId.c_str());
        } else if (normalY > 0.3f) {
            // Character on slope - reduced friction
            ioSettings.mCanPushCharacter = true;
            printf("Character %s on slope - reduced friction\n", cc->charId.c_str());
        }
    }
};

std::vector<int> chai_collisions::addRigidMesh(std::string meshPath, int meshRef, bool makeConvex, bool ragdoll) 
{
    auto cl = ChaiLove::getInstance();
    auto f = cl->getFSModule();
    auto s = f.getSize(meshPath);
    auto file = f.readBuffer(meshPath, s);   

    // Allocate a character array and copy the file data into it
    unsigned char* data = new unsigned char[s];
    std::memcpy(data, file, s);

    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;
    loader.LoadBinaryFromMemory(&model, &err, &warn, data, s, "", false);
    std::vector<int> refs;
    auto count = rigidMeshes.size();  
    
    // if (model.cameras.size() > 0) {
    //     auto camera = model.cameras[0];

    //     if (camera.type == "perspective") {
    //         debugDrawer->fov = camera.perspective.yfov;
    //         debugDrawer->aspectRatio = camera.perspective.aspectRatio;
    //         debugDrawer->nearPlane = camera.perspective.znear;
    //         debugDrawer->farPlane = camera.perspective.zfar;
    //     }

    //     // Retrieve the camera position
    //     for (auto node : model.nodes) {
    //         if (node.camera == 0) {
    //             glm::vec3 cameraPosition(0.0f);
    //             glm::vec3 cameraUp(0.0f, 1.0f, 0.0f); // Default up vector
    //             glm::vec3 cameraDirection(0.0f, 0.0f, -1.0f); // Default target vector
    //             if (!node.translation.empty()) {
    //                 cameraPosition = glm::vec3(node.translation[0], node.translation[1], node.translation[2]);
    //             }
    //             if (!node.rotation.empty()) {
    //                 glm::quat rotation = glm::quat(node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]);
    //                 glm::mat4 rotationMatrix = glm::mat4_cast(rotation);
    //                 cameraUp = glm::vec3(rotationMatrix * glm::vec4(0.0f, 1.0f, 0.0f, 0.0f));
    //                 cameraDirection = glm::vec3(rotationMatrix * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f));
    //             }
    //             glm::vec3 cameraTarget = cameraPosition + cameraDirection;
    //             debugDrawer->cameraPosition = cameraPosition;
    //             debugDrawer->upVector = cameraUp;
    //             debugDrawer->cameraTarget = cameraTarget;
    //             break;
    //         }
    //     }
    // } else {
    //     debugDrawer->fov = 1920.0f / 1080.0f;
    //     debugDrawer->aspectRatio = 1.33f;
    //     debugDrawer->nearPlane = 0.01;
    //     debugDrawer->farPlane = 1000.0f;

    //     glm::vec3 cameraPosition(0.0f);
    //     glm::vec3 cameraUp(0.0f, 1.0f, 0.0f); // Default up vector
    //     glm::vec3 cameraDirection(0.0f, 0.0f, -1.0f); // Default target vector

    //     glm::vec3 cameraTarget = cameraPosition + cameraDirection;
    //     debugDrawer->cameraPosition = cameraPosition;
    //     debugDrawer->upVector = cameraUp;
    //     debugDrawer->cameraTarget = cameraTarget;
    // }

    // Set OpenGL to wireframe mode
    // glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    
    // Create a ragdoll mesh
    JPH::Ref<JPH::Shape> ragdollShape;
    std::vector<float> vertices;
    std::vector<uint32_t> indices;
    for (size_t i = 0; i < model.meshes.size(); i++) {
        
        // Retrieve the transformation matrix for the model
        glm::mat4 modelMatrix = glm::mat4(1.0f);
        for (auto node : model.nodes) {
            if (node.mesh == i) {
                if (!node.matrix.empty()) {
                    // modelMatrix = glm::make_mat4(node.matrix.data());
                } else {
                    // if (!node.translation.empty()) {
                    //     modelMatrix = glm::translate(modelMatrix, glm::vec3(node.translation[0], node.translation[1], node.translation[2]));
                    // }
                    // if (!node.rotation.empty()) {
                    //     glm::quat rotation = glm::quat(node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]);
                    //     modelMatrix *= glm::mat4_cast(rotation);
                    // }
                    if (!node.scale.empty()) {
                        modelMatrix = glm::scale(modelMatrix, glm::vec3(node.scale[0], node.scale[1], node.scale[2]));
                    }
                }
                break;
            }
        }

        // printf("Model Matrix: %f, %f, %f, %f\n", modelMatrix[0][0], modelMatrix[0][1], modelMatrix[0][2], modelMatrix[0][3]);
        // printf("Model Matrix: %f, %f, %f, %f\n", modelMatrix[1][0], modelMatrix[1][1], modelMatrix[1][2], modelMatrix[1][3]);
        // printf("Model Matrix: %f, %f, %f, %f\n", modelMatrix[2][0], modelMatrix[2][1], modelMatrix[2][2], modelMatrix[2][3]);
        // printf("Model Matrix: %f, %f, %f, %f\n", modelMatrix[3][0], modelMatrix[3][1], modelMatrix[3][2], modelMatrix[3][3]);

        // btTriangleMesh *mesh = new btTriangleMesh();
        // btConvexHullShape *convexShape = new btConvexHullShape();            
    
        auto primitive = model.meshes[i].primitives[0];
        auto positionAccessor = model.accessors[primitive.attributes["POSITION"]];
        auto positionBufferView = model.bufferViews[positionAccessor.bufferView];
        auto positionBuffer = model.buffers[positionBufferView.buffer];
        const float* positions = reinterpret_cast<const float*>(&positionBuffer.data[positionAccessor.byteOffset + positionBufferView.byteOffset]);

        auto indiceAccessor = model.accessors[primitive.indices];
        auto indiceBufferView = model.bufferViews[indiceAccessor.bufferView];
        auto indiceBuffer = model.buffers[indiceBufferView.buffer];
        if (indiceAccessor.componentType != TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
            const unsigned int* rawIndices = reinterpret_cast<const unsigned int*>(&indiceBuffer.data[indiceAccessor.byteOffset + indiceBufferView.byteOffset]);
            for (size_t i = 0; i < indiceAccessor.count; ++i) {
                indices.push_back(static_cast<uint32_t>(rawIndices[i]));
                // Extract and push positions
                for (size_t j = 0; j < 3; ++j) {
                    vertices.push_back(positions[rawIndices[i] * 3 + j]);
                }
                if (vertices.size() == 9) {
                    auto m = modelMatrix * glm::vec4(vertices[0], vertices[1], vertices[2], 1.0f);
                    vertices[0] = m.x;
                    vertices[1] = m.y;
                    vertices[2] = m.z;
                    m = modelMatrix * glm::vec4(vertices[3], vertices[4], vertices[5], 1.0f);
                    vertices[3] = m.x;
                    vertices[4] = m.y;
                    vertices[5] = m.z;
                    m = modelMatrix * glm::vec4(vertices[6], vertices[7], vertices[8], 1.0f);
                    vertices[6] = m.x;
                    vertices[7] = m.y;
                    vertices[8] = m.z;
                    if (false) {
                        glBegin(GL_TRIANGLES);
                        glVertex3f(vertices[0], vertices[1], vertices[2]);
                        glVertex3f(vertices[3], vertices[4], vertices[5]);
                        glVertex3f(vertices[6], vertices[7], vertices[8]);
                        glEnd();    
                    }
                    if (makeConvex) {
                        if (!isfinite(vertices[0]) || !isfinite(vertices[1]) || !isfinite(vertices[2]) ||
                            !isfinite(vertices[3]) || !isfinite(vertices[4]) || !isfinite(vertices[5]) ||
                            !isfinite(vertices[6]) || !isfinite(vertices[7]) || !isfinite(vertices[8])) {
                            printf("Skipping non-finite vertices: %f, %f, %f\n", vertices[0], vertices[1], vertices[2]);
                            printf("Skipping non-finite vertices: %f, %f, %f\n", vertices[3], vertices[4], vertices[5]);
                            printf("Skipping non-finite vertices: %f, %f, %f\n", vertices[6], vertices[7], vertices[8]);
                        } else {
                            // convexShape->addPoint(btVector3(vertices[0], vertices[1], vertices[2]));
                            // convexShape->addPoint(btVector3(vertices[3], vertices[4], vertices[5]));
                            // convexShape->addPoint(btVector3(vertices[6], vertices[7], vertices[8]));
                        }
                    } else {
                        // mesh->addTriangle(btVector3(vertices[0], vertices[1], vertices[2]), btVector3(vertices[3], vertices[4], vertices[5]), btVector3(vertices[6], vertices[7], vertices[8]));
                    }
                }
            }
        } else {
            const unsigned short* rawIndices = reinterpret_cast<const unsigned short*>(&indiceBuffer.data[indiceAccessor.byteOffset + indiceBufferView.byteOffset]);
            for (size_t i = 0; i < indiceAccessor.count; ++i) {
                indices.push_back(static_cast<uint32_t>(rawIndices[i]));
                // Extract and push positions
                for (size_t j = 0; j < 3; ++j) {
                    vertices.push_back(positions[rawIndices[i] * 3 + j]);
                }
                if (vertices.size() == 9) {
                    auto m = modelMatrix * glm::vec4(vertices[0], vertices[1], vertices[2], 1.0f);
                    vertices[0] = m.x;
                    vertices[1] = m.y;
                    vertices[2] = m.z;
                    m = modelMatrix * glm::vec4(vertices[3], vertices[4], vertices[5], 1.0f);
                    vertices[3] = m.x;
                    vertices[4] = m.y;
                    vertices[5] = m.z;
                    m = modelMatrix * glm::vec4(vertices[6], vertices[7], vertices[8], 1.0f);
                    vertices[6] = m.x;
                    vertices[7] = m.y;
                    vertices[8] = m.z;
                    if (false) {
                        glBegin(GL_TRIANGLES);
                        glVertex3f(vertices[0], vertices[1], vertices[2]);
                        glVertex3f(vertices[3], vertices[4], vertices[5]);
                        glVertex3f(vertices[6], vertices[7], vertices[8]);
                        glEnd();    
                    }                 
                    if (makeConvex) {
                        if (!isfinite(vertices[0]) || !isfinite(vertices[1]) || !isfinite(vertices[2]) ||
                            !isfinite(vertices[3]) || !isfinite(vertices[4]) || !isfinite(vertices[5]) ||
                            !isfinite(vertices[6]) || !isfinite(vertices[7]) || !isfinite(vertices[8])) {
                            printf("Skipping non-finite vertices: %f, %f, %f\n", vertices[0], vertices[1], vertices[2]);
                            printf("Skipping non-finite vertices: %f, %f, %f\n", vertices[3], vertices[4], vertices[5]);
                            printf("Skipping non-finite vertices: %f, %f, %f\n", vertices[6], vertices[7], vertices[8]);
                        } else {
                            // convexShape->addPoint(btVector3(vertices[0], vertices[1], vertices[2]));
                            // convexShape->addPoint(btVector3(vertices[3], vertices[4], vertices[5]));
                            // convexShape->addPoint(btVector3(vertices[6], vertices[7], vertices[8]));
                        }
                    } else {
                        // mesh->addTriangle(btVector3(vertices[0], vertices[1], vertices[2]), btVector3(vertices[3], vertices[4], vertices[5]), btVector3(vertices[6], vertices[7], vertices[8]));
                    }
                }
            }
        
        }

        // Create Jolt mesh shape
        JPH::Ref<JPH::Shape> shape;
        if (makeConvex) {
            if (ragdoll) {                
                // ragdollShape->addChildShape(btTransform::getIdentity(), convexShape);
                // refs = std::vector<int>(); // Clear refs since we're making a ragdoll
                // count = rigidMeshes.size();
            }
            if (!ragdoll) {
                // btDefaultMotionState *motionState = new btDefaultMotionState(btTransform(btQuaternion(0, 0, 0, 1), btVector3(0, 0, 0)));
                if (ragdoll) {
                    // btRigidBody::btRigidBodyConstructionInfo rigidBodyCI(0, motionState, ragdollShape, btVector3(0, 0, 0));
                    // btRigidBody *rigidBody = new btRigidBody(rigidBodyCI);
                    // rigidBody->setWorldTransform(btTransform(btQuaternion(modelMatrix[0][0], modelMatrix[1][1], modelMatrix[2][2], 1), btVector3(modelMatrix[3][0], modelMatrix[3][1], modelMatrix[3][2])));
                    // rigidBody->setUserIndex(-1);
                    // rigidMeshes.emplace_back(new RigidMesh(mesh, rigidBody, meshRef));  
                } else {
                    // Convert flat float vector to std::vector<JPH::Vec3>
                    // std::vector<JPH::Vec3> jphVertices;
                    // for (size_t vi = 0; vi + 2 < vertices.size(); vi += 3) {
                    //     jphVertices.emplace_back(vertices[vi], vertices[vi + 1], vertices[vi + 2]);
                    // }
                    // if (!jphVertices.empty()) {
                    //     auto inNumPoints = static_cast<int>(jphVertices.size());
                    //     JPH::ConvexShapeSettings convexSettings(jphVertices, inNumPoints);
                    //     shape = convexSettings.Create().Get();
                    // }
                    // btRigidBody::btRigidBodyConstructionInfo rigidBodyCI(0, motionState, convexShape, btVector3(0, 0, 0));
                    // btRigidBody *rigidBody = new btRigidBody(rigidBodyCI);
                    // rigidBody->setWorldTransform(btTransform(btQuaternion(modelMatrix[0][0], modelMatrix[1][1], modelMatrix[2][2], 1), btVector3(modelMatrix[3][0], modelMatrix[3][1], modelMatrix[3][2])));
                    // rigidBody->setUserIndex(-1);
                    // rigidMeshes.emplace_back(new RigidMesh(mesh, rigidBody, meshRef));  
                }
            }
        } else {
            std::vector<JPH::Vec3> jphVertices;
            for (size_t vi = 0; vi + 2 < vertices.size(); vi += 3) {
                jphVertices.emplace_back(vertices[vi], vertices[vi + 1], vertices[vi + 2]);
            }
            JPH::TriangleList triangleList;
            for (size_t vi = 0; vi + 2 < jphVertices.size(); vi += 3) {
                triangleList.push_back(JPH::Triangle(jphVertices[vi], jphVertices[vi + 1], jphVertices[vi + 2]));
            }
            JPH::PhysicsMaterialList materialList;
            materialList.push_back(JPH::PhysicsMaterial::sDefault);
            JPH::MeshShapeSettings meshSettings(triangleList, materialList);
            printf("Creating mesh with %zu triangles\n", triangleList.size());
            auto s = meshSettings.Create();
            printf("ERROR: MeshShape creation failed: %s\n", s.GetError().c_str());
            printf("Created mesh shape\n");  
            shape = s.Get();
            printf("Got mesh shape\n");  

            // Set up body creation settings
            JPH::BodyCreationSettings bodySettings(
                shape,
                JPH::RVec3(0, 0, 0), // Position (set as needed)
                JPH::Quat::sIdentity(), // Rotation (set as needed)
                JPH::EMotionType::Static, // Motion type
                Layers::NON_MOVING    
            );

            // Create the body
            auto ps = worlds->worlds[0]->physics_system;
            JPH::BodyInterface& bodyInterface = ps->GetBodyInterface();
            JPH::Body *body = bodyInterface.CreateBody(bodySettings);
            bodyInterface.SetFriction(body->GetID(), 1.0f);
            bodyInterface.AddBody(body->GetID(), JPH::EActivation::DontActivate);
            auto bodyID = body->GetID();


        auto rm = new RigidMesh(bodyID, meshRef);
        rigidMeshes.push_back(rm);
                    
            // btBvhTriangleMeshShape *shape = new btBvhTriangleMeshShape(mesh, true);
            // btDefaultMotionState *motionState = new btDefaultMotionState(btTransform(btQuaternion(0, 0, 0, 1), btVector3(0, 0, 0)));
            // btRigidBody::btRigidBodyConstructionInfo rigidBodyCI(0, motionState, shape, btVector3(0, 0, 0));    
            // btRigidBody *rigidBody = new btRigidBody(rigidBodyCI);
            // rigidBody->setWorldTransform(btTransform(btQuaternion(modelMatrix[0][0], modelMatrix[1][1], modelMatrix[2][2], 1), btVector3(modelMatrix[3][0], modelMatrix[3][1], modelMatrix[3][2])));
            // rigidBody->setUserIndex(-1);
            // rigidMeshes.emplace_back(new RigidMesh(mesh, rigidBody, meshRef));   
        }
        
        

        refs.push_back(count);
        count++;
    }
    // Reset OpenGL to fill mode (optional)
    // glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    return refs;
}
void chai_collisions::setRigidMeshPosition(std::vector<int> rigidMeshIndex, float x, float y, float z, std::vector<int> group)
{
    // int idx = 0;
    // btTransform transform;
    for (auto i : rigidMeshIndex) {
        // Determine the group for this rigid mesh (default to 0 if not set)
        int groupIndex = 0;
        if (!rigidMeshes[i]->group.empty()) {
            groupIndex = rigidMeshes[i]->group[0];
        }
        if (!group.empty()) {
            groupIndex = group[0];
        }

        auto ps = worlds->worlds[groupIndex]->physics_system;
        JPH::BodyInterface& bodyInterface = ps->GetBodyInterface();

        // Get the current rotation (optional, keep current orientation)
        JPH::Quat rotation = bodyInterface.GetRotation(rigidMeshes[i]->bodyID);

        // Set the new position (and keep current rotation)
        bodyInterface.SetPositionAndRotation(rigidMeshes[i]->bodyID, JPH::RVec3(x, y, z), JPH::Quat::sIdentity(), JPH::EActivation::DontActivate);

        // auto r = rigidMeshes[i]->rigidBody;
        // for (auto g : group) {
        //     if (worlds->worlds.find(g) == worlds->worlds.end()) {
        //         init(g);
        //     }
        //     rigidMeshes[i]->group.push_back(g);
        //     worlds->worlds[g]->dynamicsWorld->addRigidBody(r, btBroadphaseProxy::StaticFilter, btBroadphaseProxy::DefaultFilter | btBroadphaseProxy::CharacterFilter);
        // }
        // rigidMeshes[i]->rigidBody->setCollisionFlags(btCollisionObject::CF_STATIC_OBJECT);

        // if (true || idx == 0) {
        //     transform = rigidMeshes[i]->rigidBody->getWorldTransform();
        // }
        // rigidMeshes[i]->rigidBody->setWorldTransform(btTransform(btQuaternion(transform.getBasis()[0][0], transform.getBasis()[1][1], transform.getBasis()[2][2], 1), (btVector3(x, y, z) + btVector3(transform.getOrigin().getX(), transform.getOrigin().getY(), transform.getOrigin().getZ()))));
        // transform = rigidMeshes[i]->rigidBody->getWorldTransform();
        // printf("x,y,z: %f,%f,%f\n", transform.getOrigin().getX(), transform.getOrigin().getY(), transform.getOrigin().getZ());
        // idx++;
    }              
}

void chai_collisions::teleportRigidMesh(std::vector<int> rigidMeshIndex, float x, float y, float z)
{
    // for (auto i : rigidMeshIndex) {
    //     auto r = rigidMeshes[i]->rigidBody;
    //     r->setWorldTransform(btTransform(btQuaternion(0, 0, 0, 1), btVector3(x, y, z)));
    //     r->setLinearVelocity(btVector3(0, 0, 0));
    //     r->setAngularVelocity(btVector3(0, 0, 0));
    // }
}

void chai_collisions::togglePhysics(std::vector<int> rigidMeshIndex, bool enable)
{
    // for (auto i : rigidMeshIndex) {
    //     auto r = rigidMeshes[i]->rigidBody;
    //     if (enable) {
    //         worlds->worlds[0]->dynamicsWorld->removeRigidBody(r);
            
    //         r->setCollisionFlags(btCollisionObject::CF_DYNAMIC_OBJECT);
    //         r->setActivationState(DISABLE_DEACTIVATION); // Disable deactivation to keep the rigid body active
    //         btVector3 inertia;
    //         btScalar mass = 1000.0f; // Set mass to 1.0f for the rigid body
    //         r->getCollisionShape()->calculateLocalInertia(mass, inertia); 
    //         r->setMassProps(mass, inertia);
    //         // r->setCollisionFlags(r->getCollisionFlags() & ~btCollisionObject::CF_STATIC_OBJECT);
    //         // r->setLinearFactor(btVector3(0.1, 0.1, 0.1)); // Enable movement in all directions
    //         r->setGravity(btVector3(0, -9.81f, 0));
    //         r->setLinearVelocity(btVector3(0, 0, 0)); // Reset linear velocity
    //         r->setAngularVelocity(btVector3(0, 0, 0)); // Reset angular velocity
    //         r->setFriction(0.1f); // Set friction to a reasonable value
    //         r->setRestitution(0.0f); // Set restitution to a reasonable value
    //         r->setRollingFriction(0.1f); // Set rolling friction to a
    //         r->setSpinningFriction(0.1f); // Set spinning friction to a reasonable value
    //         r->setDamping(0.1f, 0.1f);       

    //         // applyForceToRigidMesh(i, 0, 5, 0); // Reset any previous forces applied to the rigid body
           
    //         worlds->worlds[0]->dynamicsWorld->addRigidBody(r); 
    //         // Set the collision flags to dynamic

    //     } else {
    //         // Turn off gravity and set the rigid body to static
    //         r->setCollisionFlags(r->getCollisionFlags() | btCollisionObject::CF_STATIC_OBJECT);
    //         r->setGravity(btVector3(0, 0, 0));
    //     }
    // }
}
int chai_collisions::addCharacterController(int index, int meshRef, std::string charId)
{
    if (characterControllers.size() > 0) {
        for (int i = 0; i < characterControllers.size(); i++) {
            if (characterControllers[i]->charId == charId) {
                characterControllers[i]->addMesh(meshRef);
                printf("Character controller with charId %s already exists, added meshRef %d\n", charId.c_str(), meshRef);
                return -1;
            }
        }
    }

    // Create character settings (like in CharacterTest.cpp)
    JPH::Ref<JPH::CharacterSettings> settings = new JPH::CharacterSettings();
    settings->mMaxSlopeAngle = JPH::DegreesToRadians(45.0f);
    settings->mLayer = Layers::AI; // Different layer for each character
    
    // Create capsule shape
    JPH::CapsuleShapeSettings capsuleSettings(0.5f, 0.6f);
    auto shapeResult = capsuleSettings.Create();
    if (shapeResult.HasError()) {
        printf("Failed to create capsule shape: %s\n", shapeResult.GetError().c_str());
        return -1;
    }
    settings->mShape = shapeResult.Get();
    settings->mFriction = 1.0f;

    settings->mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisY(), -0.5f); // Support plane
    
    // Create character (like in CharacterTest.cpp)
    JPH::Character* character = new JPH::Character(
        settings, 
        JPH::RVec3(0, 5.0f, 0), 
        JPH::Quat::sIdentity(), 
        0, 
        worlds->worlds[0]->physics_system);
    
    // Add to physics system (like in CharacterTest.cpp)
    character->AddToPhysicsSystem(JPH::EActivation::Activate);
    // Get body ID
    JPH::BodyID bodyID = character->GetBodyID();
    auto& bodyInterface = worlds->worlds[0]->physics_system->GetBodyInterface();
    bodyInterface.SetFriction(bodyID, 1.0f);

    if (charId.find("player") != std::string::npos) {
        bodyInterface.SetUserData(bodyID, 1);
    }

    // Store character reference in your CharacterController
    characterControllers.emplace_back(new CharacterController(bodyID, meshRef, charId, character, index));
    
    printf("Created character controller with charId %s at position (0, 5.0f, 0)\n", charId.c_str());
    return characterControllers.size() - 1;
}
void chai_collisions::setCharacterControllerPosition(int characterIndex, float x, float y, float z, std::vector<int> group = {0})
{
    // Default to group 0 if not provided
    int groupIndex = 0;
    if (!characterControllers[characterIndex]->group.empty()) {
        groupIndex = characterControllers[characterIndex]->group[0];
    }
    if (!group.empty()) {
        groupIndex = group[0];
    }

    // Get the Jolt PhysicsSystem for the group
    // auto ps = worlds->worlds[groupIndex]->physics_system;
    // JPH::BodyInterface& bodyInterface = ps->GetBodyInterface();

    // Set the new position (teleport character)
    characterControllers[characterIndex]->character->SetPosition(JPH::RVec3(x, y, z));

    // btPairCachingGhostObject *ghostObject = characterControllers[characterIndex]->ghostObject;
    
    // for (auto i : group) {
    //     if (worlds->worlds.find(i) == worlds->worlds.end()) {
    //         init(i);
    //     }
    //     characterControllers[characterIndex]->group.push_back(i);
    //     if (ghostObject->getUserIndex() < 0) {
    //         worlds->worlds[i]->dynamicsWorld->addCollisionObject(ghostObject, btBroadphaseProxy::CharacterFilter, btBroadphaseProxy::StaticFilter | btBroadphaseProxy::DefaultFilter);
    //     } else {
    //         worlds->worlds[i]->dynamicsWorld->addCollisionObject(ghostObject, btBroadphaseProxy::CharacterFilter, btBroadphaseProxy::StaticFilter | btBroadphaseProxy::DefaultFilter);
    //     }
    //     worlds->worlds[i]->dynamicsWorld->addCharacter(characterControllers[characterIndex]->character);
    // }
    
    // ghostObject->setCollisionFlags(btCollisionObject::CF_CHARACTER_OBJECT);
    // ghostObject->setWorldTransform(btTransform(btQuaternion(0, 0, 0, 1), btVector3(x, y, z)));
}

void chai_collisions::teleportCharacter(int characterIndex, float x, float y, float z)
{
    characterControllers[characterIndex]->character->SetPosition(JPH::RVec3(x, y, z));
    // btPairCachingGhostObject *ghostObject = characterControllers[characterIndex]->ghostObject;
    // ghostObject->setWorldTransform(btTransform(btQuaternion(0, 0, 0, 1), btVector3(x, y, z)));
}

void chai_collisions::applyForceToCharacter(int characterIndex, float x, float y, float z)
{
    // if (characterControllers[characterIndex]->collidedX && x != 0) {
    //     // characterControllers[characterIndex]->collidedX = false;
    //     x = 0;
    // }
    // if (characterControllers[characterIndex]->collidedZ && z != 0) {
    //     // characterControllers[characterIndex]->collidedZ = false;
    //     z = 0;
    // }
    // Apply a walk direction to the character controller
    characterControllers[characterIndex]->velocityX = x;
    characterControllers[characterIndex]->velocityY = y;
    characterControllers[characterIndex]->velocityZ = z;
    // characterControllers[characterIndex]->character->setWalkDirection(btVector3(x, y, z));
}
void chai_collisions::applyForceToRigidMesh(int rigidMeshIndex, float x, float y, float z)
{
    // rigidMeshes[rigidMeshIndex]->rigidBody->applyCentralForce(btVector3(x, y, z));
}
std::vector<float> chai_collisions::getCharacterController(int ref)
{
    // Get the Jolt PhysicsSystem for the group (default to group 0)
    int groupIndex = 0;
    if (!characterControllers[ref]->group.empty()) {
        groupIndex = characterControllers[ref]->group[0];
    }

    auto ps = worlds->worlds[groupIndex]->physics_system;
    JPH::BodyInterface& bodyInterface = ps->GetBodyInterface();

    // Get the world space position of the character controller's body
    JPH::RVec3 pos = bodyInterface.GetCenterOfMassTransform(characterControllers[ref]->bodyID).GetTranslation();

    return std::vector<float>{ (float)pos.GetX(), (float)pos.GetY(), (float)pos.GetZ() };
    // return std::vector<float>{
    //     characterControllers[ref]->ghostObject->getWorldTransform().getOrigin().getX(),
    //     characterControllers[ref]->ghostObject->getWorldTransform().getOrigin().getY(),
    //     characterControllers[ref]->ghostObject->getWorldTransform().getOrigin().getZ()
    // };
    // return std::vector<float>{0,0,0};
}
std::vector<float> chai_collisions::getRigidMesh(int ref)
{
    // Get the Jolt PhysicsSystem for the group (default to group 0)
    int groupIndex = 0;
    if (!rigidMeshes[ref]->group.empty()) {
        groupIndex = rigidMeshes[ref]->group[0];
    }

    auto ps = worlds->worlds[groupIndex]->physics_system;
    JPH::BodyInterface& bodyInterface = ps->GetBodyInterface();

    // Get the world space position of the rigid mesh's body
    JPH::RVec3 pos = bodyInterface.GetCenterOfMassTransform(rigidMeshes[ref]->bodyID).GetTranslation();

    return std::vector<float>{ (float)pos.GetX(), (float)pos.GetY(), (float)pos.GetZ() };
}

int chai_collisions::addBox(float x, float y, float z, float width, float height, float depth, std::vector<int> group = {0}, int index = 0)
{
    // Create a triangle mesh to represent the box in jolt physics
    JPH::TriangleList mesh;

    // Create a triangle mesh to represent the box
    mesh.push_back(JPH::Triangle(JPH::Vec3(-width / 2, -height / 2, -depth / 2), JPH::Vec3(width / 2, -height / 2, -depth / 2), JPH::Vec3(width / 2, height / 2, -depth / 2)));
    mesh.push_back(JPH::Triangle(JPH::Vec3(-width / 2, -height / 2, -depth / 2), JPH::Vec3(width / 2, height / 2, -depth / 2), JPH::Vec3(-width / 2, height / 2, -depth / 2)));
    mesh.push_back(JPH::Triangle(JPH::Vec3(-width / 2, -height / 2, depth / 2), JPH::Vec3(width / 2, -height / 2, depth / 2), JPH::Vec3(width / 2, height / 2, depth / 2)));
    mesh.push_back(JPH::Triangle(JPH::Vec3(-width / 2, -height / 2, depth / 2), JPH::Vec3(width / 2, height / 2, depth / 2), JPH::Vec3(-width / 2, height / 2, depth / 2)));
    mesh.push_back(JPH::Triangle(JPH::Vec3(-width / 2, -height / 2, -depth / 2), JPH::Vec3(width / 2, -height / 2, -depth / 2), JPH::Vec3(width / 2, -height / 2, depth / 2)));
    mesh.push_back(JPH::Triangle(JPH::Vec3(-width / 2, -height / 2, -depth / 2), JPH::Vec3(width / 2, -height / 2, depth / 2), JPH::Vec3(-width / 2, -height / 2, depth / 2)));
    mesh.push_back(JPH::Triangle(JPH::Vec3(-width / 2, height / 2, -depth / 2), JPH::Vec3(width / 2, height / 2, -depth / 2), JPH::Vec3(width / 2, height / 2, depth / 2)));
    mesh.push_back(JPH::Triangle(JPH::Vec3(-width / 2, height / 2, -depth / 2), JPH::Vec3(width / 2, height / 2, depth / 2), JPH::Vec3(-width / 2, height / 2, depth / 2)));
    mesh.push_back(JPH::Triangle(JPH::Vec3(-width / 2, -height / 2, -depth / 2), JPH::Vec3(-width / 2, height / 2, -depth / 2), JPH::Vec3(-width / 2, height / 2, depth / 2)));
    mesh.push_back(JPH::Triangle(JPH::Vec3(-width / 2, -height / 2, -depth / 2), JPH::Vec3(-width / 2, height / 2, depth / 2), JPH::Vec3(-width / 2, -height / 2, depth / 2)));
    mesh.push_back(JPH::Triangle(JPH::Vec3(width / 2, -height / 2, -depth / 2), JPH::Vec3(width / 2, height / 2, -depth / 2), JPH::Vec3(width / 2, height / 2, depth / 2)));
    mesh.push_back(JPH::Triangle(JPH::Vec3(width / 2, -height / 2, -depth / 2), JPH::Vec3(width / 2, height / 2, depth / 2), JPH::Vec3(width / 2, -height / 2, depth / 2)));

    // Create the mesh shape from settings
    JPH::MeshShapeSettings meshSettings(mesh, {JPH::PhysicsMaterial::sDefault});
    auto shapeResult = meshSettings.Create();
    if (!shapeResult.IsValid())
    {
        printf("ERROR: MeshShape creation failed: %s\n", shapeResult.GetError().c_str());
        return 0;
    }
    JPH::Ref<JPH::Shape> shape = shapeResult.Get();

    // Set up body creation settings
    JPH::BodyCreationSettings bodySettings(
        shape, // Shape
        JPH::RVec3(x, y, z), // Position
        JPH::Quat::sIdentity(), // Rotation
        JPH::EMotionType::Kinematic, // Motion type
        Layers::BOUNDARY // Collision layer
    );
    bodySettings.mIsSensor = true; // Make it a sensor
    // Set gravity to zero
    bodySettings.mGravityFactor = 0.0f;
    bodySettings.mFriction = 1.0f;
    // bodySettings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
    // bodySettings.mMassPropertiesOverride.mMass = 1.0f; // Set
    auto ps = worlds->worlds[0]->physics_system;
    JPH::BodyInterface& bodyInterface = ps->GetBodyInterface();
    // Create the body
    JPH::Body *body = bodyInterface.CreateBody(bodySettings);
    
    bodyInterface.AddBody(body->GetID(), JPH::EActivation::DontActivate);
    auto bodyID = body->GetID();
    auto rm = new RigidMesh(bodyID, index);
    rigidMeshes.push_back(rm);

    for (auto i : group) {
        if (worlds->worlds.find(i) == worlds->worlds.end()) {
            init(i);
        }
        rigidMeshes.back()->group.push_back(i);
        // worlds->worlds[i]->dynamicsWorld->addRigidBody(rigidBody, btBroadphaseProxy::DefaultFilter, btBroadphaseProxy::DefaultFilter | btBroadphaseProxy::CharacterFilter);
    }
    return rigidMeshes.size() - 1;

    // btTriangleMesh *mesh = new btTriangleMesh();

    // // Define the vertices of an inner and outer wall box
    // btVector3 vertices[8] = {
    //     btVector3(-width / 2, -height / 2, -depth / 2),
    //     btVector3(width / 2, -height / 2, -depth / 2),
    //     btVector3(width / 2, height / 2, -depth / 2),
    //     btVector3(-width / 2, height / 2, -depth / 2),
    //     btVector3(-width / 2, -height / 2, depth / 2),
    //     btVector3(width / 2, -height / 2, depth / 2),
    //     btVector3(width / 2, height / 2, depth / 2),
    //     btVector3(-width / 2, height / 2, depth / 2)
    // };

    // // Define the triangles of the box (6 faces, 2 triangles per face)
    // int indices[12][3] = {
    //     {0, 1, 2}, {0, 2, 3}, // Front face
    //     {4, 5, 6}, {4, 6, 7}, // Back face
    //     {0, 1, 5}, {0, 5, 4}, // Bottom face
    //     {2, 3, 7}, {2, 7, 6}, // Top face
    //     {0, 3, 7}, {0, 7, 4}, // Left face
    //     {1, 2, 6}, {1, 6, 5}  // Right face
    // };

    // // Add the triangles to the mesh
    // for (int i = 0; i < 12; ++i) {
    //     mesh->addTriangle(vertices[indices[i][0]], vertices[indices[i][1]], vertices[indices[i][2]]);
    // }

    // // Create the convex triangle mesh shape
    // btCompoundShape *shape = new btCompoundShape();
    // shape->addChildShape(btTransform(btQuaternion(0, 0, 0, 1), btVector3(0, 0, 0)), new btBvhTriangleMeshShape(mesh, true));

    // // Set a non-zero mass to make the box dynamic
    // // btScalar mass = 1.0f; // Adjust mass as needed
    // // btVector3 inertia(0, 0, 0);
    // // shape->calculateLocalInertia(mass, inertia);
    // btDefaultMotionState *motionState = new btDefaultMotionState(btTransform(btQuaternion(0, 0, 0, 1), btVector3(0, -1, 0)));
    // btRigidBody::btRigidBodyConstructionInfo rigidBodyCI(1.0f, motionState, shape, btVector3(0, 0, 0));
    // btRigidBody *rigidBody = new btRigidBody(rigidBodyCI);
    // rigidBody->setGravity(btVector3(0, 0, 0));
    // // rigidBody->setLinearVelocity(btVector3(0, 0, 0));
    // // rigidBody->setAngularVelocity(btVector3(0, 0, 0));
    // // rigidBody->setLinearFactor(btVector3(1.9f, 0, 0));
    // // Set friction and damping properties suitable for 1:1 character control
    // // rigidBody->setFriction(0.45f); // Set a small positive friction value
    // // rigidBody->setDamping(0.45f, 0.01f); // Set small positive damping values for stability
    // // rigidBody->setRestitution(1.0f); // Set a small positive restitution value

    // rigidBody->setUserPointer(rigidBody); // Set the user pointer to the character controller  
    // rigidBody->setUserIndex(index);
    // rigidBody->setUserIndex2(1);
    // // Disable deactivation to keep the object active
    // rigidBody->setActivationState(DISABLE_DEACTIVATION);
    // rigidBody->setCollisionFlags(btCollisionObject::CF_NO_CONTACT_RESPONSE);
    // if (index < 4) {
    //     cameraBox.emplace_back(rigidBody);
    // } else {
    //     portalBox.emplace_back(rigidBody);
    // }
    // rigidMeshes.emplace_back(new RigidMesh(shape, rigidBody));
    // for (auto i : group) {
    //     if (worlds->worlds.find(i) == worlds->worlds.end()) {
    //         init(i);
    //     }
    //     rigidMeshes.back()->group.push_back(i);
    //     worlds->worlds[i]->dynamicsWorld->addRigidBody(rigidBody, btBroadphaseProxy::DefaultFilter, btBroadphaseProxy::DefaultFilter | btBroadphaseProxy::CharacterFilter);
    // }
    // return rigidMeshes.size() - 1;
    // return 0;
}

std::vector<std::pair<glm::vec3, glm::vec3>> chai_collisions::getBoundingBox(int mesh)
{
    std::vector<std::pair<glm::vec3, glm::vec3>> bb;
    for (auto &m : rigidMeshes) {
        if (m->meshRef == mesh) {
            // Get world space bounds

            auto ps = worlds->worlds[0]->physics_system;
            JPH::BodyInterface& bodyInterface = ps->GetBodyInterface();
            auto bodyTransform = bodyInterface.GetCenterOfMassTransform(m->bodyID);
            JPH::Vec3 inScale(1.0f, 1.0f, 1.0f);
            auto shapeBounds = bodyInterface.GetShape(m->bodyID)->GetWorldSpaceBounds(bodyTransform, inScale);
            bb.push_back(std::make_pair(
                glm::vec3(shapeBounds.mMin.GetX(), shapeBounds.mMin.GetY(), shapeBounds.mMin.GetZ()),
                glm::vec3(shapeBounds.mMax.GetX(), shapeBounds.mMax.GetY(), shapeBounds.mMax.GetZ())
            ));

            // btCollisionShape* shape = m->rigidBody->getCollisionShape();
            // btTransform world = m->rigidBody->getWorldTransform();
            // btVector3 minV(FLT_MAX, FLT_MAX, FLT_MAX);
            // btVector3 maxV(-FLT_MAX, -FLT_MAX, -FLT_MAX);

            // if (shape->getShapeType() == COMPOUND_SHAPE_PROXYTYPE) {
            //     btCompoundShape* compound = static_cast<btCompoundShape*>(shape);
            //     for (int i = 0; i < compound->getNumChildShapes(); ++i) {
            //         const btTransform& childLocal = compound->getChildTransform(i);
            //         btCollisionShape* childShape = compound->getChildShape(i);
            //         btTransform childWorld = world * childLocal;
            //         btVector3 childMin, childMax;
            //         childShape->getAabb(childWorld, childMin, childMax);
            //         minV.setMin(childMin);
            //         maxV.setMax(childMax);
            //     }
            // } else {
            //     shape->getAabb(world, minV, maxV);
            // }

            // // printf("x,y,z: %f,%f,%f\n", world.getOrigin().getX(), world.getOrigin().getY(), world.getOrigin().getZ());
            // // printf("minV: %f,%f,%f\n", minV.getX(), minV.getY(), minV.getZ());
            // // printf("maxV: %f,%f,%f\n", maxV.getX(), maxV.getY(), maxV.getZ());
            // bb.push_back(std::make_pair(
            //     glm::vec3(minV.getX(), minV.getY(), minV.getZ()),
            //     glm::vec3(maxV.getX(), maxV.getY(), maxV.getZ())
            // ));
        }
    }
    if (bb.size() == 0) {
        for (auto &c : characterControllers) {
            for (auto &m : c->meshRef) {
                if (m == mesh) {
                    auto ps = worlds->worlds[0]->physics_system;
                    JPH::BodyInterface& bodyInterface = ps->GetBodyInterface();
                    auto bodyTransform = bodyInterface.GetCenterOfMassTransform(c->bodyID);
                    auto shapeBounds = bodyInterface.GetShape(c->bodyID)->GetWorldSpaceBounds(bodyTransform, JPH::Vec3(1.0f, 1.0f, 1.0f));
                    bb.push_back(std::make_pair(
                        glm::vec3(shapeBounds.mMin.GetX(), shapeBounds.mMin.GetY(), shapeBounds.mMin.GetZ()),
                        glm::vec3(shapeBounds.mMax.GetX(), shapeBounds.mMax.GetY(), shapeBounds.mMax.GetZ())
                    ));
                    break;
                }
            }
            if (bb.size() > 0) {
                break;
            }
        }
    }
    return bb;
}

std::vector<Matrix4> chai_collisions::getPhysicsObjects(int mesh)
{
    std::vector<Matrix4> objects;
    for (auto &m : rigidMeshes) {
        if (m->meshRef == mesh) {
            // Get world space bounds

            auto ps = worlds->worlds[0]->physics_system;
            JPH::BodyInterface& bodyInterface = ps->GetBodyInterface();
            auto bodyTransform = bodyInterface.GetCenterOfMassTransform(m->bodyID);
            glm::mat4 modelMat(1.0f);
            glm::vec3 translation((float)bodyTransform.GetTranslation().GetX(), (float)bodyTransform.GetTranslation().GetY(), (float)bodyTransform.GetTranslation().GetZ());
            JPH::Quat bodyRot = bodyInterface.GetRotation(m->bodyID);
            glm::quat rotation(bodyRot.GetW(), bodyRot.GetX(), bodyRot.GetY(), bodyRot.GetZ()); // glm::quat(w, x, y, z)
            // If JPH::Quat stores (x, y, z, w), reorder as follows:
            // glm::quat rotation(bodyRot.GetW(), bodyRot.GetX(), bodyRot.GetY(), bodyRot.GetZ());
            // But if you get a runtime error, try:
            // glm::quat rotation(bodyRot.GetW(), bodyRot.GetX(), bodyRot.GetY(), bodyRot.GetZ());
            glm::mat4 rotationMatrix = glm::mat4_cast(rotation);
            glm::vec3 scale(1.0f, 1.0f, 1.0f); // scale vector
            glm::mat4 trs = glm::translate(glm::mat4(1.0f), translation)
                    * rotationMatrix
                    * glm::scale(glm::mat4(1.0f), scale);
            modelMat = modelMat * trs;
            Matrix4 mat = Matrix4(new float[16] {
                modelMat[0][0], modelMat[0][1], modelMat[0][2], modelMat[0][3],
                modelMat[1][0], modelMat[1][1], modelMat[1][2], modelMat[1][3],
                modelMat[2][0], modelMat[2][1], modelMat[2][2], modelMat[2][3],
                modelMat[3][0], modelMat[3][1], modelMat[3][2], modelMat[3][3],
            });
            objects.push_back(mat);
    //         // Check if the rigid body is a compound shape (ragdoll)
    //         // If it is, we need to get the transform of each child shape
    //         btCollisionShape* shape = m->rigidBody->getCollisionShape();
    //         btCompoundShape* compoundShape = static_cast<btCompoundShape*>(shape);
    //         if (compoundShape->getNumChildShapes() > 0) {                
    //             for (int i = 0; i < compoundShape->getNumChildShapes(); i++) {
    //                 btTransform childTransform = compoundShape->getChildTransform(i);
    //                 glm::mat4 modelMat(1.0f);
    //                 btTransform transform = m->rigidBody->getWorldTransform() * childTransform;
    //                 glm::vec3 translation((float)transform.getOrigin().getX(), (float)transform.getOrigin().getY(), (float)transform.getOrigin().getZ());
    //                 btQuaternion btRot = transform.getRotation();
    //                 glm::quat rotation(btRot.getW(), btRot.getX(), btRot.getY(), btRot.getZ());
    //                 glm::mat4 rotationMatrix = glm::mat4_cast(rotation);
    //                 glm::vec3 scale(1.0f, 1.0f, 1.0f); // scale vector
    //                 glm::mat4 trs = glm::translate(glm::mat4(1.0f), translation)
    //                         * rotationMatrix
    //                         * glm::scale(glm::mat4(1.0f), scale);
    //                 modelMat = modelMat * trs;
    //                 Matrix4 mat = Matrix4(new float[16] {
    //                     modelMat[0][0], modelMat[0][1], modelMat[0][2], modelMat[0][3],
    //                     modelMat[1][0], modelMat[1][1], modelMat[1][2], modelMat[1][3],
    //                     modelMat[2][0], modelMat[2][1], modelMat[2][2], modelMat[2][3],
    //                     modelMat[3][0], modelMat[3][1], modelMat[3][2], modelMat[3][3],
    //                 });
    //                 objects.push_back(mat);
    //             }
    //         } else {
    //             glm::mat4 modelMat(1.0f);
    //             btTransform transform = m->rigidBody->getWorldTransform();
    //             glm::vec3 translation((float)transform.getOrigin().getX(), (float)transform.getOrigin().getY(), (float)transform.getOrigin().getZ());
    //             btQuaternion btRot = transform.getRotation();
    //             glm::quat rotation(btRot.getW(), btRot.getX(), btRot.getY(), btRot.getZ());
    //             glm::mat4 rotationMatrix = glm::mat4_cast(rotation);
    //             glm::vec3 scale(1.0f, 1.0f, 1.0f); // scale vector
    //             glm::mat4 trs = glm::translate(glm::mat4(1.0f), translation)
    //                     * rotationMatrix
    //                     * glm::scale(glm::mat4(1.0f), scale);

    //             modelMat = modelMat * trs;
                
    //             Matrix4 mat = Matrix4(new float[16] {
    //                 modelMat[0][0], modelMat[0][1], modelMat[0][2], modelMat[0][3],
    //                 modelMat[1][0], modelMat[1][1], modelMat[1][2], modelMat[1][3],
    //                 modelMat[2][0], modelMat[2][1], modelMat[2][2], modelMat[2][3],
    //                 modelMat[3][0], modelMat[3][1], modelMat[3][2], modelMat[3][3],
    //             });
    //             // printf("Origin: %f, %f, %f\n", 
    //                 // transform.getOrigin().getX(), 
    //                 // transform.getOrigin().getY(), 
    //                 // transform.getOrigin().getZ());
    //             objects.push_back(mat);
    //         }
        }
    }
    return objects;
    // return std::vector<Matrix4>{};
}

int chai_collisions::portalCollide(int index)
{
    // if (index < 0 || index >= characterControllers.size()) {
    //     return -1;
    // }
    // auto portalIndex = characterControllers[index]->ghostObject->getUserIndex3();
    // characterControllers[index]->ghostObject->setUserIndex3(-1);
    // return portalIndex;
    return -1;
}

// Callback for traces, connect this to your own trace function if you have one
static void TraceImpl(const char *inFMT, ...)
{
	// Format the message
	va_list list;
	va_start(list, inFMT);
	char buffer[1024];
	vsnprintf(buffer, sizeof(buffer), inFMT, list);
	va_end(list);

	// Print to the TTY
	std::cout << buffer << std::endl;
}

#ifdef JPH_DEBUG_RENDERER
class DebugRendererImpl : public JPH::DebugRendererSimple
{
public:
    DebugRendererImpl() {}
    ~DebugRendererImpl() {}
    
    virtual void DrawLine(JPH::RVec3Arg inFrom, JPH::RVec3Arg inTo, JPH::ColorArg inColor) override
    {
        glBegin(GL_LINES);
        glColor3f(inColor.r, inColor.g, inColor.b);
        glVertex3f((float)inFrom.GetX(), (float)inFrom.GetY(), (float)inFrom.GetZ());
        glVertex3f((float)inTo.GetX(), (float)inTo.GetY(), (float)inTo.GetZ());
        glEnd();
    }

    virtual void DrawTriangle(JPH::RVec3Arg inV1, JPH::RVec3Arg inV2, JPH::RVec3Arg inV3, JPH::ColorArg inColor, ECastShadow inCastShadow) override
    {
        glBegin(GL_TRIANGLES);
        glColor3f(inColor.r, inColor.g, inColor.b);
        glVertex3f((float)inV1.GetX(), (float)inV1.GetY(), (float)inV1.GetZ());
        glVertex3f((float)inV2.GetX(), (float)inV2.GetY(), (float)inV2.GetZ());
        glVertex3f((float)inV3.GetX(), (float)inV3.GetY(), (float)inV3.GetZ());
        glEnd();
    }

    // Add missing pure virtual functions
    virtual void DrawText3D(JPH::RVec3Arg inPosition, const std::string_view& inString, JPH::ColorArg inColor, float inHeight = 0.5f) override
    {
        // Simple implementation - draw a point instead of text
        glPointSize(5.0f);
        glBegin(GL_POINTS);
        glColor3f(inColor.r, inColor.g, inColor.b);
        glVertex3f((float)inPosition.GetX(), (float)inPosition.GetY(), (float)inPosition.GetZ());
        glEnd();
        glPointSize(1.0f);
    }

    // virtual JPH::DebugRenderer::Batch CreateTriangleBatch(const JPH::DebugRenderer::Triangle *inTriangles, int inTriangleCount) override
    // {
    //     // Simple implementation - return empty batch
    //     return JPH::DebugRenderer::Batch();
    // }

   

    // virtual JPH::DebugRenderer::Batch CreateTriangleBatch(const JPH::DebugRenderer::Vertex *inVertices, int inVertexCount, const JPH::uint32 *inIndices, int inIndexCount) override
    // {
    //     // Simple implementation - return empty batch
    //     return JPH::DebugRenderer::Batch();
    // }

    // virtual void DrawGeometry(JPH::RMat44Arg inModelMatrix, const JPH::AABox &inWorldSpaceBounds, float inLODScaleSq, JPH::ColorArg inModelColor, const JPH::DebugRenderer::GeometryRef &inGeometry) override
    // {
    //     // Simple implementation - draw the bounding box as wireframe
    //     JPH::Vec3 min = inWorldSpaceBounds.mMin;
    //     JPH::Vec3 max = inWorldSpaceBounds.mMax;
        
    //     glColor3f(inModelColor.r, inModelColor.g, inModelColor.b);
        
    //     // Draw wireframe box
    //     glBegin(GL_LINES);
    //     // Bottom face
    //     glVertex3f(min.GetX(), min.GetY(), min.GetZ());
    //     glVertex3f(max.GetX(), min.GetY(), min.GetZ());
    //     glVertex3f(max.GetX(), min.GetY(), min.GetZ());
    //     glVertex3f(max.GetX(), min.GetY(), max.GetZ());
    //     glVertex3f(max.GetX(), min.GetY(), max.GetZ());
    //     glVertex3f(min.GetX(), min.GetY(), max.GetZ());
    //     glVertex3f(min.GetX(), min.GetY(), max.GetZ());
    //     glVertex3f(min.GetX(), min.GetY(), min.GetZ());
        
    //     // Top face
    //     glVertex3f(min.GetX(), max.GetY(), min.GetZ());
    //     glVertex3f(max.GetX(), max.GetY(), min.GetZ());
    //     glVertex3f(max.GetX(), max.GetY(), min.GetZ());
    //     glVertex3f(max.GetX(), max.GetY(), max.GetZ());
    //     glVertex3f(max.GetX(), max.GetY(), max.GetZ());
    //     glVertex3f(min.GetX(), max.GetY(), max.GetZ());
    //     glVertex3f(min.GetX(), max.GetY(), max.GetZ());
    //     glVertex3f(min.GetX(), max.GetY(), min.GetZ());
        
    //     // Vertical edges
    //     glVertex3f(min.GetX(), min.GetY(), min.GetZ());
    //     glVertex3f(min.GetX(), max.GetY(), min.GetZ());
    //     glVertex3f(max.GetX(), min.GetY(), min.GetZ());
    //     glVertex3f(max.GetX(), max.GetY(), min.GetZ());
    //     glVertex3f(max.GetX(), min.GetY(), max.GetZ());
    //     glVertex3f(max.GetX(), max.GetY(), max.GetZ());
    //     glVertex3f(min.GetX(), min.GetY(), max.GetZ());
    //     glVertex3f(min.GetX(), max.GetY(), max.GetZ());
    //     glEnd();
    // }

    // Remove the old DrawGeometry function that takes a JPH::Shape pointer
    // This function signature doesn't exist in the current Jolt API
};
#endif // JPH_DEBUG_RENDERER

#ifdef JPH_ENABLE_ASSERTS

// Callback for asserts, connect this to your own assert handler if you have one
static bool AssertFailedImpl(const char *inExpression, const char *inMessage, const char *inFile, JPH::uint inLine)
{
	// Print to the TTY
	std::cout << inFile << ":" << inLine << ": (" << inExpression << ") " << (inMessage != nullptr? inMessage : "") << std::endl;

	// Breakpoint
	return false;
};

#endif // JPH_ENABLE_ASSERTS

/// Class that determines if two object layers can collide
class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter
{
public:
	virtual bool					ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override
	{
        switch (inObject1)
        {
        case Layers::NON_MOVING:
            return inObject2 == Layers::MOVING || inObject2 == Layers::AI; // Non moving only collides with moving
        case Layers::MOVING:
            return true; // Moving collides with everything
        case Layers::AI:
            return true; // AI only collides with other AI
        case Layers::BOUNDARY:
            return inObject2 == Layers::AI; // Boundary collides with AI
        default:
            JPH_ASSERT(false);
            return false;
        }
        
    }
};

// Each broadphase layer results in a separate bounding volume tree in the broad phase. You at least want to have
// a layer for non-moving and moving objects to avoid having to update a tree full of static objects every frame.
// You can have a 1-on-1 mapping between object layers and broadphase layers (like in this case) but if you have
// many object layers you'll be creating many broad phase trees, which is not efficient. If you want to fine tune
// your broadphase layers define JPH_TRACK_BROADPHASE_STATS and look at the stats reported on the TTY.
namespace BroadPhaseLayers
{
	static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
	static constexpr JPH::BroadPhaseLayer MOVING(1);
	static constexpr JPH::BroadPhaseLayer AI(2);
    static constexpr JPH::BroadPhaseLayer BOUNDARY(3);
	static constexpr JPH::uint NUM_LAYERS(4);
};

// BroadPhaseLayerInterface implementation
// This defines a mapping between object and broadphase layers.
class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface
{
public:
    BPLayerInterfaceImpl()
    {
        // Create a mapping table from object to broad phase layer
        mObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
        mObjectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
        mObjectToBroadPhase[Layers::AI] = BroadPhaseLayers::AI;
        mObjectToBroadPhase[Layers::BOUNDARY] = BroadPhaseLayers::BOUNDARY;
    }

    virtual JPH::uint GetNumBroadPhaseLayers() const override
    {
        return BroadPhaseLayers::NUM_LAYERS;
    }

    virtual JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override
    {
        JPH_ASSERT(inLayer < Layers::NUM_LAYERS);
        return mObjectToBroadPhase[inLayer];
    }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    virtual const char * GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override
    {
        switch (inLayer.GetValue())
        {
        case 0:	return "NON_MOVING";
        case 1:	return "MOVING";
        case 2:	return "AI";
        case 3:	return "BOUNDARY";
        default:	JPH_ASSERT(false); return "INVALID";
        }
    }
#endif // JPH_EXTERNAL_PROFILE || JPH_PROFILE_ENABLED

private:
    JPH::BroadPhaseLayer mObjectToBroadPhase[Layers::NUM_LAYERS];
};

/// Class that determines if an object layer can collide with a broadphase layer
class ObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter
{
public:
	virtual bool				ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override
	{
		switch (inLayer1)
		{
		case Layers::NON_MOVING:
			return inLayer2 == BroadPhaseLayers::MOVING || inLayer2 == BroadPhaseLayers::AI;
		case Layers::MOVING:
			return true;
		 case Layers::AI:
            return true; // AI only collides with non-moving and other AI
        case Layers::BOUNDARY:
            return inLayer2 == BroadPhaseLayers::AI; // Boundary collides with moving
		default:
			JPH_ASSERT(false);
			return false;
		}
	}
};

// An example activation listener
class MyBodyActivationListener : public JPH::BodyActivationListener
{
public:
	virtual void		OnBodyActivated(const JPH::BodyID &inBodyID, uint64 inBodyUserData) override
	{
		std::cout << "A body got activated" << std::endl;
	}

	virtual void		OnBodyDeactivated(const JPH::BodyID &inBodyID, uint64 inBodyUserData) override
	{
		std::cout << "A body went to sleep" << std::endl;
	}
};

void chai_collisions::init(int group = 0)
{    
    // Initialize Jolt Physics system for this group
    if (worlds == nullptr) {
        worlds = new WorldMap();
    }

    // Create a new Jolt JPH::PhysicsSystem for this group if it doesn't exist
    if (worlds->worlds.find(group) == worlds->worlds.end()) {
        using namespace JPH;
        // Register allocation hook. In this example we'll just let Jolt use malloc / free but you can override these if you want (see Memory.h).
        // This needs to be done before any other Jolt function is called.
        RegisterDefaultAllocator();

        // Install trace and assert callbacks
        Trace = TraceImpl;
        JPH_IF_ENABLE_ASSERTS(AssertFailed = AssertFailedImpl;)

        // Create a factory, this class is responsible for creating instances of classes based on their name or hash and is mainly used for deserialization of saved data.
        // It is not directly used in this example but still required.
        Factory::sInstance = new Factory();

        // Register all physics types with the factory and install their collision handlers with the CollisionDispatch class.
        // If you have your own custom shape types you probably need to register their handlers with the CollisionDispatch before calling this function.
        // If you implement your own default material (PhysicsMaterial::sDefault) make sure to initialize it before this function or else this function will create one for you.
        RegisterTypes();

        // We need a temp allocator for temporary allocations during the physics update. We're
        // pre-allocating 10 MB to avoid having to do allocations during the physics update.
        // B.t.w. 10 MB is way too much for this example but it is a typical value you can use.
        // If you don't want to pre-allocate you can also use TempAllocatorMalloc to fall back to
        // malloc / free.
        // Store in your world map (wrap in your World struct if needed)
        auto worldJolt = new WorldJolt();
        
        // This is the max amount of rigid bodies that you can add to the physics system. If you try to add more you'll get an error.
        // Note: This value is low because this is a simple test. For a real project use something in the order of 65536.
        const uint cMaxBodies = 1024;

        // This determines how many mutexes to allocate to protect rigid bodies from concurrent access. Set it to 0 for the default settings.
        const uint cNumBodyMutexes = 0;

        // This is the max amount of body pairs that can be queued at any time (the broad phase will detect overlapping
        // body pairs based on their bounding boxes and will insert them into a queue for the narrowphase). If you make this buffer
        // too small the queue will fill up and the broad phase jobs will start to do narrow phase work. This is slightly less efficient.
        // Note: This value is low because this is a simple test. For a real project use something in the order of 65536.
        const uint cMaxBodyPairs = 1024;

        // This is the maximum size of the contact constraint buffer. If more contacts (collisions between bodies) are detected than this
        // number then these contacts will be ignored and bodies will start interpenetrating / fall through the world.
        // Note: This value is low because this is a simple test. For a real project use something in the order of 10240.
        const uint cMaxContactConstraints = 1024;

        // Create mapping table from object layer to broadphase layer
        // Note: As this is an interface, PhysicsSystem will take a reference to this so this instance needs to stay alive!
        // Also have a look at BroadPhaseLayerInterfaceTable or BroadPhaseLayerInterfaceMask for a simpler interface.
        // Allocate interfaces on the heap so their lifetime matches the physics system
        auto broad_phase_layer_interface = new BPLayerInterfaceImpl();
        auto object_vs_broadphase_layer_filter = new ObjectVsBroadPhaseLayerFilterImpl();
        auto object_vs_object_layer_filter = new ObjectLayerPairFilterImpl();

        // Now we can create the actual physics system.
        PhysicsSystem* physics_system = new PhysicsSystem();
        physics_system->Init(cMaxBodies, cNumBodyMutexes, cMaxBodyPairs, cMaxContactConstraints, *broad_phase_layer_interface, *object_vs_broadphase_layer_filter, *object_vs_object_layer_filter);
        
        // Set gravity
        // physics_system->SetGravity(JPH::Vec3(0.0f, -9.81f, 0.0f)); // Gravity pointing down the y-axis

        auto contactListener = new MyContactListener(this);
        // auto characterContactListener = new CharacterContactListener(this);
        physics_system->SetContactListener(contactListener);

        worldJolt->physics_system = physics_system;
        worldJolt->broad_phase_layer_interface = broad_phase_layer_interface;
        worldJolt->object_vs_broadphase_layer_filter = object_vs_broadphase_layer_filter;
        worldJolt->object_vs_object_layer_filter = object_vs_object_layer_filter;
        worldJolt->contact_listener = contactListener;
        // worldJolt->character_contact_listener = characterContactListener;
        
        #ifdef JPH_DEBUG_RENDERER

        worldJolt->debug_renderer = new DebugRendererImpl();

        #endif // JPH_DEBUG_RENDERER

        worlds->worlds[group] = worldJolt;
    }
    // // Create task scheduler for multithreading
    // btITaskScheduler *taskScheduler = btCreateDefaultTaskScheduler();
    // btSetTaskScheduler(taskScheduler);
    // btBroadphaseInterface *broadphase = new btDbvtBroadphase();
    // broadphase->getOverlappingPairCache()->setInternalGhostPairCallback(new btGhostPairCallback());
    // btDefaultCollisionConfiguration *collisionConfiguration = new btDefaultCollisionConfiguration();
    // btCollisionDispatcherMt *dispatcher = new btCollisionDispatcherMt(collisionConfiguration);
    // btSequentialImpulseConstraintSolverMt *solver = new btSequentialImpulseConstraintSolverMt();
    // btConstraintSolverPoolMt *pool = new btConstraintSolverPoolMt(100);
    // btDiscreteDynamicsWorldMt *dw = new btDiscreteDynamicsWorldMt(dispatcher, broadphase, pool, solver, collisionConfiguration);
    // auto w = new World(broadphase, collisionConfiguration, dispatcher, solver, dw);
    // if (worlds == nullptr) {
    //     worlds = new WorldMap();
    // }
    // worlds->worlds[group] = w;
    
    // worlds->worlds[group]->dynamicsWorld->setGravity(btVector3(0, -9.81f, 0)); 
    
    // worlds->worlds[group]->dynamicsWorld->setInternalTickCallback([](btDynamicsWorld *world, btScalar timeStep) {
    //     chai_collisions *self = static_cast<chai_collisions *>(world->getWorldUserInfo());
    //     btDispatcher *dispatcher = world->getDispatcher();
    //     const int numManifolds = dispatcher->getNumManifolds();
    //     btVector3 vel0 = btVector3(0, 0, 0);
    //     btVector3 vel1 = btVector3(0,  0, 0);
    //     std::map<int, bool> contact;
    //     std::map<int, btRigidBody*> body;
    //     for (int i = 0; i < self->cameraBox.size(); ++i) {
    //         contact[i] = false;
    //     }
    //     for (int m = 0; m < numManifolds; ++m) {
    //         auto *manifold = dispatcher->getManifoldByIndexInternal(m);
    //         const btRigidBody *body0 = static_cast<const btRigidBody *>(manifold->getBody0());
    //         const btRigidBody *body1 = static_cast<const btRigidBody *>(manifold->getBody1());
            
    //         auto numContacts = manifold->getNumContacts();
           
    //         if (body0->getUserIndex() == body1->getUserIndex() && numContacts > 0) {
    //             contact[body1->getUserIndex()] = true;
               
    //             btVector3 pos0 = body0->getWorldTransform().getOrigin();
    //             btVector3 pos1 = body1->getWorldTransform().getOrigin();

    //             // Calculate the collision direction
    //             btVector3 collisionDirection = pos1 - pos0;

    //             // Optional: Normalize the direction vector
    //             collisionDirection.normalize();

    //             if (body1->getUserIndex() > -1 && body1->getUserIndex() < 4) {
    //                 auto b = self->cameraBox[body1->getUserIndex()];
    //                 b->setLinearVelocity(btVector3(-collisionDirection.getX() * 6.0f, 0.0f, 0.0f));
    //                 auto c = self->characterControllers[body1->getUserIndex()]->character;
    //                 // auto v = c->getLinearVelocity();
    //                 // c->getGhostObject()->setLinearVelocity(btVector3(v.getX(), v.getY(), 0.0f));
    //                 c->getGhostObject()->setUserIndex2(collisionDirection.getZ() > 0 ? 1 : collisionDirection.getZ() < 0 ? -1 : 0);
    //             }            
    //         } else if (body1->getUserIndex() >= 4 && body0->getUserIndex() >= 0 && body0->getUserIndex() < 4 && numContacts > 0) {
    //             contact[body1->getUserIndex()] = true;
    //             auto c = self->characterControllers[body0->getUserIndex()]->character;
    //             c->getGhostObject()->setUserIndex3(body1->getUserIndex());
    //         } else if (body0->getUserIndex() >= 4 && body1->getUserIndex() >= 0 && body1->getUserIndex() < 4 && numContacts > 0) {
    //             contact[body0->getUserIndex()] = true;
    //             auto c = self->characterControllers[body1->getUserIndex()]->character;
    //             c->getGhostObject()->setUserIndex3(body0->getUserIndex());
    //         }      
    //     }
    //     for (int i = 0; i < self->cameraBox.size(); ++i) { 
    //         self->cameraBox[i]->setGravity(btVector3(0, 0, 0));   
    //         if (i < 4) {        
    //             if (!contact[i]) {
    //                 auto b = self->cameraBox[i];                
    //                 if (b != nullptr) {
    //                     auto v = b->getLinearVelocity();
    //                     b->setLinearVelocity(btVector3(v.getX()/1.5f, 0, 0));
    //                     if (self->characterControllers.size() > i) {
    //                         auto c = self->characterControllers[i]->character->getGhostObject();
    //                         c->setUserIndex2(0);
    //                     }
    //                 }
    //             }
    //         }
    //     }
    //     for (int i = 0; i < self->portalBox.size(); ++i) { 
    //         self->portalBox[i]->setGravity(btVector3(0, 0, 0));
    //         self->portalBox[i]->setLinearVelocity(btVector3(0, 0, 0));
    //     }
    // }, this);

    // // Initialize the debug drawer
    // if (debugDrawer == nullptr) { 
    //     debugDrawer = new OpenGLDebugDrawer();
    //     debugDrawer->setDebugMode(btIDebugDraw::DBG_DrawWireframe | btIDebugDraw::DBG_DrawAabb);
    //     worlds->worlds[group]->dynamicsWorld->setDebugDrawer(debugDrawer);
    // }
    

}
void chai_collisions::destroy()
{
     // Remove and delete all rigid meshes (Jolt bodies)
    for (auto &rm : rigidMeshes) {
        for (int i = 0; i < rm->group.size(); i++) {
            int group = rm->group[i];
            if (worlds->worlds.find(group) != worlds->worlds.end()) {

                JPH::BodyInterface& bodyInterface = JPH::PhysicsSystem().GetBodyInterface();
                bodyInterface.RemoveBody(rm->bodyID);
                bodyInterface.DestroyBody(rm->bodyID);                
            }
        }
        delete rm;
    }
    rigidMeshes.clear();

    // Remove and delete all character controllers (if you have Jolt-based characters)
    for (auto &cc : characterControllers) {
        // Implement character cleanup if you use Jolt for characters
        delete cc;
    }
    characterControllers.clear();

    // Clear any other containers
    // cameraBox.clear();
    // portalBox.clear();

    // Delete debug drawer if you have a Jolt debug drawer
    // delete debugDrawer;
    // debugDrawer = nullptr;

    // Delete all Jolt physics systems and worlds
    if (worlds) {
        for (auto &w : worlds->worlds) {
            delete w.second; // Assumes WorldJolt* or similar
        }
        worlds->worlds.clear();
        delete worlds;
        worlds = nullptr;
    }

    // for (auto &rm : rigidMeshes) {
    //     for (int i = 0; i < rm->group.size(); i++) {
    //         worlds->worlds[rm->group[i]]->dynamicsWorld->removeRigidBody(rm->rigidBody);
    //     }
        
    //     delete rm;
    // }
    // rigidMeshes.clear();
    // rigidMeshes = std::vector<RigidMesh*>();
    // for (auto &cc : characterControllers) {
    //     for (int i = 0; i < cc->group.size(); i++) {
    //         worlds->worlds[cc->group[i]]->dynamicsWorld->removeAction(cc->character);
    //         worlds->worlds[cc->group[i]]->dynamicsWorld->removeCollisionObject(cc->ghostObject);
    //     }
    //     delete cc;
    // }
    // characterControllers.clear();
    // characterControllers = std::vector<CharacterController*>();

    // cameraBox.clear();
    // cameraBox = std::vector<btRigidBody*>();

    // delete debugDrawer;
    // debugDrawer = nullptr;
}
uint8_t* chai_collisions::processDebug(float deltaTime, std::vector<chaiscript::Boxed_Value> viewMatrix)
{
    // auto width = 1920; // Set your desired width
    // auto height = 1080; // Set your desired height
    // if (!debugDrawer->debugTexture) {
    //     glGenTextures(1, &debugDrawer->debugTexture);
    // }

    // glBindTexture(GL_TEXTURE_2D, debugDrawer->debugTexture);
    // glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // if (!debugDrawer->debugFBO) {
    //     glGenFramebuffers(1, &debugDrawer->debugFBO);
    // }

    // glBindFramebuffer(GL_FRAMEBUFFER, debugDrawer->debugFBO);
    // glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, debugDrawer->debugTexture, 0);

    // glViewport(0, 0, width, height);
    // glEnable(GL_BLEND);
    // glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    // glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    

    // for (auto &dw : worlds->worlds) {
    //     for (auto &cc : characterControllers) {
    //         auto stopZ = cc->ghostObject->getUserIndex2();
    //         auto v = cc->character->getLinearVelocity();
    //         if (stopZ > 0 && v.getZ() < 0 && cc->ghostObject->getUserIndex() >= 0) {                
    //             cc->character->setWalkDirection(btVector3(v.getX(), v.getY(), 0.0f));
    //         }
    //         if (stopZ < 0 && v.getZ() > 0 && cc->ghostObject->getUserIndex() >= 0) {
    //             cc->character->setWalkDirection(btVector3(v.getX(), v.getY(), 0.0f));
    //         }
    //         cc->character->preStep(dw.second->dynamicsWorld);
    //         cc->character->playerStep(dw.second->dynamicsWorld, 1);
            
    //         cc->character->setWalkDirection(btVector3(0.0f, 0.0f, 0.0f));

    //         btVector3 pos = cc->ghostObject->getWorldTransform().getOrigin();
            
    //         // printf("ghost pos: %f, %f, %f\n", pos.getX(), pos.getY(), pos.getZ());
    //     }
    //     float timeStep = deltaTime;
    //     float fixedTimeStep = 1.0f / 60.0f;
    //     dw.second->dynamicsWorld->stepSimulation(timeStep, 10, fixedTimeStep);   
    //     if (false && debugDrawer) {
    //         std::vector<float> prepD;
    //         for (auto d : viewMatrix) {
    //             auto v = chaiscript::boxed_cast<float>(d);
    //             prepD.push_back(v);
    //         }

    //         btVector3 gravity = dw.second->dynamicsWorld->getGravity();
    //         // printf("Gravity: %f, %f, %f\n", gravity.getX(), gravity.getY(), gravity.getZ());   

    //         btVector3 min, max;
    //         min.setValue(FLT_MAX, FLT_MAX, FLT_MAX);
    //         max.setValue(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    //         for (int i = 0; i < dw.second->dynamicsWorld->getNumCollisionObjects(); i++) {
    //             btCollisionObject *obj = dw.second->dynamicsWorld->getCollisionObjectArray()[i];
    //             btVector3 aabbMin, aabbMax;
    //             obj->getCollisionShape()->getAabb(obj->getWorldTransform(), aabbMin, aabbMax);

    //             min.setMin(aabbMin);
    //             max.setMax(aabbMax);
    //         }

    //         // Calculate the center and size of the bounding box
    //         btVector3 center = (min + max) * 0.5;
    //         btVector3 size = max - min;

    //         // Set the camera target to the center of the geometry
    //         debugDrawer->cameraTarget = glm::vec3(-prepD[12], prepD[13]*0.4f, prepD[14]*2.0f);

    //         // Position the camera far enough to fit the geometry
    //         float maxDimension = std::max(size.getX(), std::max(size.getY(), size.getZ()));
    //         float horizontalFOV = 2.0f * atan(tan(glm::radians(debugDrawer->fov) / 2.0f) * debugDrawer->aspectRatio);
    //         float distance = maxDimension / (2.0f * tan(horizontalFOV / 2.0f)) / 2.0f; // Adjust the divisor to control the distance
    //         debugDrawer->cameraPosition = glm::vec3(-prepD[12], -prepD[13]*2.4f, -prepD[14]/0.4f);
            
    //         debugDrawer->fov = 7.0f; // Set the field of view

    //         // Ensure the up vector is correct
    //         debugDrawer->upVector = glm::vec3(0.0f, 1.0f, 0.0f);

    //         // Adjust the near and far planes
    //         debugDrawer->nearPlane = 0.1f;
    //         // debugDrawer->farPlane = distance + maxDimension * 2.0f;
    //         debugDrawer->farPlane = 100.0f;

    //         // Set the projection matrix
    //         glMatrixMode(GL_PROJECTION);
    //         glLoadIdentity();
    //         glm::mat4 projection = glm::perspective(glm::radians(debugDrawer->fov), debugDrawer->aspectRatio, debugDrawer->nearPlane, debugDrawer->farPlane);
    //         glLoadMatrixf(glm::value_ptr(projection));

    //         // Set the view matrix
    //         glMatrixMode(GL_MODELVIEW);
    //         glLoadIdentity();
    //         glm::mat4 view = glm::lookAt(debugDrawer->cameraPosition, debugDrawer->cameraTarget, debugDrawer->upVector);
    //         glLoadMatrixf(glm::value_ptr(view));
            
    //         // printf("Camera Position: %f, %f, %f\n", debugDrawer->cameraPosition.x, debugDrawer->cameraPosition.y, debugDrawer->cameraPosition.z);
    //         // printf("Camera Target: %f, %f, %f\n", debugDrawer->cameraTarget.x, debugDrawer->cameraTarget.y, debugDrawer->cameraTarget.z);
    //         // printf("Up Vector: %f, %f, %f\n", debugDrawer->upVector.x, debugDrawer->upVector.y, debugDrawer->upVector.z);

    //         // printf("Bounding Box Min: %f, %f, %f\n", min.getX(), min.getY(), min.getZ());
    //         // printf("Bounding Box Max: %f, %f, %f\n", max.getX(), max.getY(), max.getZ());

    //         // Perform debug drawing
    //         dw.second->dynamicsWorld->debugDrawWorld();

    //         // test();
    //     }
    // }    

    // if (buffer == nullptr) {       
    //     buffer = new uint8_t[width * height * 4];
    // }
    // // Read the pixels from the framebuffer
    // // glBindFramebuffer(GL_FRAMEBUFFER, debugDrawer->debugFBO);
    // // glReadBuffer(ChaiLove::getInstance()->chai_gfx.COLORATTACH);
    
    // glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, buffer);

    // // Flip framebuffer vertically
    // for (int y = 0; y < height / 2; ++y) {
    //     int oppositeY = height - 1 - y;
    //     for (int x = 0; x < width * 4; ++x) {
    //         std::swap(buffer[y * width * 4 + x], buffer[oppositeY * width * 4 + x]);
    //     }
    // }
    // // glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // return buffer;
    return nullptr;
}

void chai_collisions::debugDraw()
{    
    // printf("Debug Draw Texture: %d\n", debugDrawer->debugTexture);
    // // Render texture
    // // auto cg = ChaiLove::getInstance()->chai_gfx;
    // // auto fb = cg.instance->hw_render.get_current_framebuffer();
    // // glBindFramebuffer(GL_FRAMEBUFFER, 0); // Switch to window framebuffer
    // glViewport(0, 0, 1920, 1080);
    // // glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // glEnable(GL_TEXTURE_2D);
    // glBindTexture(GL_TEXTURE_2D, debugDrawer->debugTexture);

    // glMatrixMode(GL_PROJECTION);
    // glLoadIdentity();
    // glOrtho(0, 1920, 0, 1080, -1, 1);

    // glMatrixMode(GL_MODELVIEW);
    // glLoadIdentity();

    // glBegin(GL_QUADS);
    // glTexCoord2f(0.0f, 0.0f); glVertex2f(0.0f, 0.0f);
    // glTexCoord2f(1.0f, 0.0f); glVertex2f(1920.0f, 0.0f);
    // glTexCoord2f(1.0f, 1.0f); glVertex2f(1920.0f, 1080.0f);
    // glTexCoord2f(0.0f, 1.0f); glVertex2f(0.0f, 1080.0f);
    // glEnd();   
    // glDisable(GL_TEXTURE_2D);
}

void chai_collisions::process(float deltaTime)
{  
    // Accumulate time
    m_lastProcessTime += deltaTime;
    
    // Only process if enough time has passed
    // if (m_lastProcessTime < m_processInterval && !m_needsUpdate) {
    //     return;
    // }
    
    float actualDeltaTime = m_lastProcessTime;
    m_lastProcessTime = 0.0f;
    m_needsUpdate = false;
    
    // Cap delta time to prevent large jumps
    actualDeltaTime = std::min(actualDeltaTime, 0.033f); // Max 33ms step
    
    // For each Jolt world/group
    for (auto &dw : worlds->worlds) {
        
        // Update characters BEFORE physics step (reduced frequency)
        static int characterUpdateCounter = 0;
        // if (characterUpdateCounter % 2 == 0) { // Update characters every 2nd physics step
            for (auto &cc : characterControllers) {
                if (cc->character) {
                    const float cCollisionTolerance = 0.05f; 
           
                
                    if (cc->collidedX)
                    {
                        cc->velocityX = 0;
                    }

                    if (cc->collidedZ)
                    {
                        cc->velocityZ *= -5.0f; // Bounce back with increased force
                    }
                    // printf("Applying force to character %d: (%f, %f, %f)\n", cc->index, cc->velocityX, cc->velocityY, cc->velocityZ);
   
                    cc->character->SetLinearVelocity(JPH::Vec3(cc->velocityX, cc->velocityY + cc->character->GetLinearVelocity().GetY(), cc->velocityZ));
                    // cc->character->PostSimulation(cCollisionTolerance);
                    cc->collidedX = false;
                    cc->collidedZ = false;
                
            
                    // Handle character input/movement here if needed
                    JPH::Vec3 currentVel = cc->character->GetLinearVelocity();
                    
                    // Only print debug info occasionally
                    // static int debugCounter = 0;
                    // if (debugCounter++ % 180 == 0) { // Every 3 seconds at 60fps
                    //     printf("Character Y pos: %.2f, Y vel: %.2f\n", 
                    //            cc->character->GetPosition().GetY(), currentVel.GetY());
                    // }
                }
            }
        // }
        characterUpdateCounter++;

        // Step the Jolt simulation with fixed timestep
        const int maxSubSteps = 10;
        dw.second->physics_system->Update(deltaTime, maxSubSteps, &dw.second->temp_allocator, &dw.second->job_system);

        // Process contact events less frequently
        static int contactCounter = 0;
        // if (contactCounter++ % 3 == 0) { // Every 3rd frame
            if (dw.second->contact_listener) {
                dw.second->contact_listener->ProcessPendingEvents(dw.second->physics_system);
            }
        // }

        // Update characters AFTER physics step (reduced frequency)
        // if (characterUpdateCounter % 2 == 0) {
            
        // }

        // Debug rendering at reduced frequency
        processDebugRendering(dw.second);
    }
}

void chai_collisions::processDebugRendering(WorldJolt* world)
{
    m_lastDebugTime += m_processInterval;
    
    // Only render debug info at reduced frequency
    if (m_lastDebugTime < m_debugInterval) {
        return;
    }
    
    m_lastDebugTime = 0.0f;
    
    #ifdef JPH_DEBUG_RENDERER
    if (world->debug_renderer) {
        // Clear the screen
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Set up OpenGL state for debug rendering
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // Calculate world bounds only when needed (cache this)
        static JPH::AABox cachedWorldBounds;
        static int boundsUpdateCounter = 0;
        static bool hasCachedBounds = false;
        
        // Update bounds every 60 frames (1 second at 60fps)
        if (boundsUpdateCounter++ % 60 == 0) {
            hasCachedBounds = calculateWorldBounds(cachedWorldBounds, world);
        }
        
        // Set up camera based on cached bounds
        if (hasCachedBounds) {
            setupDebugCamera(cachedWorldBounds);
        } else {
            setupFallbackCamera();
        }
        
        // Draw physics bodies at reduced detail
        JPH::BodyManager::DrawSettings settings;
        settings.mDrawGetSupportFunction = false;      // Disabled for performance
        settings.mDrawSupportDirection = false;        // Disabled for performance
        settings.mDrawGetSupportingFace = false;       // Disabled for performance
        settings.mDrawShape = true;
        settings.mDrawShapeWireframe = true;
    
        world->physics_system->DrawBodies(settings, world->debug_renderer);
    }
    #endif // JPH_DEBUG_RENDERER
}

void chai_collisions::test() {
    // btBroadphaseInterface *broadphase = new btDbvtBroadphase();
    // btDefaultCollisionConfiguration *collisionConfiguration = new btDefaultCollisionConfiguration();
    // btCollisionDispatcher *dispatcher = new btCollisionDispatcher(collisionConfiguration);
    // btSequentialImpulseConstraintSolver *solver = new btSequentialImpulseConstraintSolver();
    // btDiscreteDynamicsWorld *dynamicsWorld = new btDiscreteDynamicsWorld(dispatcher, broadphase, solver, collisionConfiguration);
    // dynamicsWorld->setGravity(btVector3(0, -10, 0));
    // btCollisionShape *groundShape = new btStaticPlaneShape(btVector3(0, 1, 0), 1);
    // btCollisionShape *fallShape = new btSphereShape(1);
    // btDefaultMotionState *groundMotionState = new btDefaultMotionState(btTransform(btQuaternion(0, 0, 0, 1), btVector3(0, -1, 0)));
    // btRigidBody::btRigidBodyConstructionInfo groundRigidBodyCI(0, groundMotionState, groundShape, btVector3(0, 0, 0));
    // btRigidBody *groundRigidBody = new btRigidBody(groundRigidBodyCI);
    // dynamicsWorld->addRigidBody(groundRigidBody);
    // btDefaultMotionState *fallMotionState = new btDefaultMotionState(btTransform(btQuaternion(0, 0, 0, 1), btVector3(0, 50, 0)));
    // btScalar mass = 1;
    // btVector3 fallInertia(0, 0, 0);
    // fallShape->calculateLocalInertia(mass, fallInertia);
    // btRigidBody::btRigidBodyConstructionInfo fallRigidBodyCI(mass, fallMotionState, fallShape, fallInertia);
    // btRigidBody *fallRigidBody = new btRigidBody(fallRigidBodyCI);
    // dynamicsWorld->addRigidBody(fallRigidBody);
    // // Initialize the debug drawer
    // // if (!debugDrawer) {
    // //     debugDrawer = new OpenGLDebugDrawer();
    // //     debugDrawer->setDebugMode(btIDebugDraw::DBG_DrawWireframe | btIDebugDraw::DBG_DrawAabb);
    // // }
    // dynamicsWorld->setDebugDrawer(debugDrawer);
    // // Set the projection matrix
    // glMatrixMode(GL_PROJECTION);
    // glLoadIdentity();
    // glm::mat4 projection = glm::perspective(glm::radians(debugDrawer->fov), debugDrawer->aspectRatio, debugDrawer->nearPlane, debugDrawer->farPlane);
    // glLoadMatrixf(glm::value_ptr(projection));

    // // Set the view matrix
    // glMatrixMode(GL_MODELVIEW);
    // glLoadIdentity();
    // glm::mat4 view = glm::lookAt(debugDrawer->cameraPosition, debugDrawer->cameraTarget, debugDrawer->upVector);
    // glLoadMatrixf(glm::value_ptr(view));
    // for (int i = 0; i < 300; i++) {
    //     dynamicsWorld->stepSimulation(1 / 60.f, 10);

    //     // print positions of all objects
    //     for (int j = dynamicsWorld->getNumCollisionObjects() - 1; j >= 0; j--) {
    //         btCollisionObject *obj = dynamicsWorld->getCollisionObjectArray()[j];
    //         btRigidBody *body = btRigidBody::upcast(obj);
    //         btTransform trans;
    //         if (body && body->getMotionState()) {
    //             body->getMotionState()->getWorldTransform(trans);
    //         } else {
    //             trans = obj->getWorldTransform();
    //         }
    //         printf("world pos object %d = %f,%f,%f\n", j, float(trans.getOrigin().getX()), float(trans.getOrigin().getY()), float(trans.getOrigin().getZ()));
    //         // Perform debug drawing
    //         dynamicsWorld->debugDrawWorld();
    //     }
    //     // break;
    // }
    // dynamicsWorld->removeRigidBody(fallRigidBody);
    // delete fallRigidBody;
    // dynamicsWorld->removeRigidBody(groundRigidBody);
    // delete groundRigidBody;
    // delete fallShape;
    // delete groundShape;
    // delete dynamicsWorld;
    // delete solver;
    // delete collisionConfiguration;
    // delete dispatcher;
    // delete broadphase;
    // delete debugDrawer;
}
bool chai_collisions::calculateWorldBounds(JPH::AABox& worldBounds, WorldJolt* world)
{
    bool hasBounds = false;
    JPH::BodyInterface& bodyInterface = world->physics_system->GetBodyInterface();
    
    // Only process active bodies for bounds calculation
    int processedBodies = 0;
    const int maxBodiesToProcess = 50; // Limit processing per frame
    
    // Process rigid meshes (with limit)
    for (int i = 0; i < rigidMeshes.size() && processedBodies < maxBodiesToProcess; i++) {
        auto& rm = rigidMeshes[i];
        if (bodyInterface.IsAdded(rm->bodyID) && bodyInterface.IsActive(rm->bodyID)) {
            auto bodyTransform = bodyInterface.GetCenterOfMassTransform(rm->bodyID);
            JPH::Vec3 inScale(1.0f, 1.0f, 1.0f);
            auto shapeBounds = bodyInterface.GetShape(rm->bodyID)->GetWorldSpaceBounds(bodyTransform, inScale);
            
            if (!hasBounds) {
                worldBounds = shapeBounds;
                hasBounds = true;
            } else {
                worldBounds.Encapsulate(shapeBounds);
            }
            processedBodies++;
        }
    }
    
    // Process character controllers (with limit)
    for (int i = 0; i < characterControllers.size() && processedBodies < maxBodiesToProcess; i++) {
        auto& cc = characterControllers[i];
        if (bodyInterface.IsAdded(cc->bodyID) && bodyInterface.IsActive(cc->bodyID)) {
            auto bodyTransform = bodyInterface.GetCenterOfMassTransform(cc->bodyID);
            JPH::Vec3 inScale(1.0f, 1.0f, 1.0f);
            auto shapeBounds = bodyInterface.GetShape(cc->bodyID)->GetWorldSpaceBounds(bodyTransform, inScale);
            
            if (!hasBounds) {
                worldBounds = shapeBounds;
                hasBounds = true;
            } else {
                worldBounds.Encapsulate(shapeBounds);
            }
            processedBodies++;
        }
    }
    
    return hasBounds;
}

void chai_collisions::setupDebugCamera(const JPH::AABox& worldBounds)
{
    static glm::mat4 cachedProjection;
    static glm::mat4 cachedView;
    static int cameraUpdateCounter = 0;
    
    // Only update camera matrices every 30 frames
    if (cameraUpdateCounter++ % 30 == 0) {
        JPH::Vec3 boundsCenter = (worldBounds.mMin + worldBounds.mMax) * 0.5f;
        JPH::Vec3 boundsSize = worldBounds.mMax - worldBounds.mMin;
        
        float maxDimension = std::max(boundsSize.GetX(), std::max(boundsSize.GetY(), boundsSize.GetZ()));
        float fov = 45.0f;
        float aspectRatio = 16.0f / 9.0f;
        float distance = maxDimension / (2.0f * tan(glm::radians(fov) / 2.0f)) * 1.5f / 5.0f;

        glm::vec3 cameraPos = glm::vec3(
            boundsCenter.GetX() + distance * 0.7f,
            boundsCenter.GetY() + distance * 0.5f,
            boundsCenter.GetZ() + distance
        );
        
        glm::vec3 target = glm::vec3(boundsCenter.GetX(), boundsCenter.GetY(), boundsCenter.GetZ());
        glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
        
        cachedProjection = glm::perspective(glm::radians(fov), aspectRatio, 0.1f, distance * 3.0f);
        cachedView = glm::lookAt(cameraPos, target, up);
    }
    
    // Apply cached matrices
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(glm::value_ptr(cachedProjection));
    
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(glm::value_ptr(cachedView));
}
} // namespace love