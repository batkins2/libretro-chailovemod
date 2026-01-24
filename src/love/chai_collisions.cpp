#include "../ChaiLove.h"
#ifdef _WIN32
#include <windows.h>
#endif
#include <cmath> // For M_PI
#include <GL/gl.h> // For OpenGL functions like glOrtho
#include <Jolt/Physics/Constraints/DistanceConstraint.h>
#include <Jolt/Physics/Constraints/FixedConstraint.h>
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
    setProcessFrequency(360); // Default to 60 FPS
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
    static constexpr JPH::ObjectLayer WHEEL = 4;  // Non-colliding wheel layer
	static constexpr JPH::ObjectLayer NUM_LAYERS = 5;
};


namespace Groups
{
    // Different collision groups for characters
    static constexpr uint32_t CHARACTER_0 = 1; // 0001
    static constexpr uint32_t CHARACTER_1 = 2; // 0010
    static constexpr uint32_t CHARACTER_2 = 4; // 0100
    static constexpr uint32_t CHARACTER_3 = 8; // 1000
    static constexpr uint32_t ALL_CHARACTERS = CHARACTER_0 | CHARACTER_1 | CHARACTER_2 | CHARACTER_3; // 1111
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
        JPH::BodyID body1ID = inBody1.GetID();
        JPH::BodyID body2ID = inBody2.GetID();
        // printf("Contact added between bodies %d and %d\n", body1ID, body2ID);
        // printf("Body 1 Layer: %d, Body 2 Layer: %d\n", inBody1.GetObjectLayer(), inBody2.GetObjectLayer());
        if (body1ID == body2ID)
            return;

        // Suppress collisions between a wheel and its own chassis (same vehicle tag)
        bool body1Wheel = inBody1.GetObjectLayer() == Layers::WHEEL;
        bool body2Wheel = inBody2.GetObjectLayer() == Layers::WHEEL;
        bool body1Chassis = inBody1.GetObjectLayer() == Layers::MOVING;
        bool body2Chassis = inBody2.GetObjectLayer() == Layers::MOVING;
        if ((body1Wheel && body2Chassis) || (body2Wheel && body1Chassis)) {
            uint64 tag1 = inBody1.GetUserData();
            uint64 tag2 = inBody2.GetUserData();
            if (tag1 != 0 && tag1 == tag2) {
                ioSettings.mIsSensor = true; // Disable collision response for wheel vs own chassis
                return;
            }
        }

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
                        // Get penetration depth
                        float penetrationDepth = inManifold.mPenetrationDepth;
                        // printf("Contact normal for character %s: (%.2f, %.2f, %.2f)\n", cc->charId.c_str(), inManifold.mWorldSpaceNormal.GetX(), inManifold.mWorldSpaceNormal.GetY(), inManifold.mWorldSpaceNormal.GetZ());
                        // auto vel = cc->character->GetLinearVelocity();
                        if (normalZ < 0) {
                            // printf("Front collision detected for character %s\n", cc->charId.c_str());
                            // cc->collidedZ = true;
                            cc->perimeterPenetration = penetrationDepth;
                        } else if (normalZ > 0) {
                            // printf("Back collision detected for character %s\n", cc->charId.c_str());
                            // cc->collidedX = true;
                            cc->perimeterPenetration = penetrationDepth;
                        } else {
                            cc->collidedZ = false;
                            // cc->perimeterPenetration = 0.0f;
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
                    // cc->collidedX = true;
                    // cc->collidedZ = true;
                    // printf("Character %s collided with character %d\n", cc->charId.c_str(), body1ID);
                    
                    // char2Index = i;
                }
                if (cc->bodyID == body1ID && cc->charId.find("player") == std::string::npos) {
                    // cc->collidedX = true;
                    // cc->collidedZ = true;
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
                        cc->collisionNormal = inManifold.mWorldSpaceNormal;
                        float penetrationDepth = inManifold.mPenetrationDepth;
                        // auto vel = cc->character->GetLinearVelocity();
                        if (normalZ < 0) {
                            // printf("Front collision detected for character %s\n", cc->charId.c_str());
                            // cc->collidedZ = true;
                            cc->perimeterPenetration = penetrationDepth;

                        } else if (normalZ > 0) {
                            // printf("Back collision detected for character %s\n", cc->charId.c_str());
                            // cc->collidedZ = true;
                            cc->perimeterPenetration = penetrationDepth;
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
            
            // if (event.normalZ != 0) {
            //     vel.SetZ(event.normalZ);
            //    // event.characterID->collidedZ = false; // Reset collision flag after processing
            //     bodyInterface.SetLinearVelocity(event.characterID->bodyID, vel);
            // }
            vel.SetZ(0);
            vel.SetY(0); // Zero out Y component to avoid affecting vertical movement
            
            if (event.normalX < 0 && vel.GetX() < 0) {
                // Do nothing, allow movement
                vel.SetX(vel.GetX() * 2.0f);
            } else if (event.normalX > 0 && vel.GetX() > 0) {
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
            // printf("Character collision: Setting velocities to zero\n");
            // bodyInterface.SetLinearVelocity(event.character1ID, vel1);
            // bodyInterface.SetLinearVelocity(event.character2ID, vel2);
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
        
        // printf("Character %s contacted layer %d\n", characterController->charId.c_str(), hitLayer);
        
        // Handle different collision types
        switch (hitLayer) {
            case Layers::AI:
                // Character hit AI - stop movement
                characterController->collidedX = true;
                characterController->collidedZ = true;
                ioSettings.mCanPushCharacter = false;      // AI can't push this character
                ioSettings.mCanReceiveImpulses = false;    // Character won't receive impulses from AI
                // printf("Character %s hit AI - blocking movement\n", characterController->charId.c_str());
                break;
                
            case Layers::MOVING:
                // Character hit moving object - allow some interaction
                ioSettings.mCanPushCharacter = true;       // Moving objects can push character
                ioSettings.mCanReceiveImpulses = true;     // Character can receive impulses
                // printf("Character %s hit moving object\n", characterController->charId.c_str());
                break;
                
            case Layers::NON_MOVING:
                // Character hit static geometry - full stop
                characterController->collidedX = true;
                characterController->collidedZ = true;
                ioSettings.mCanPushCharacter = false;      // Static objects can't be pushed
                ioSettings.mCanReceiveImpulses = false;    // No impulses from static objects
                // printf("Character %s hit static geometry - full stop\n", characterController->charId.c_str());
                break;
                
            case Layers::BOUNDARY:
                // Character hit boundary - hard stop
                characterController->collidedX = true;
                characterController->collidedZ = true;
                ioSettings.mCanPushCharacter = false;
                ioSettings.mCanReceiveImpulses = false;
                // printf("Character %s hit boundary - hard stop\n", characterController->charId.c_str());
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
                // printf("Character %s velocity stopped due to AI collision\n", characterController->charId.c_str());
                break;
                
            case Layers::MOVING:
                // Reduce velocity when hitting moving objects
                ioNewCharacterVelocity = ioNewCharacterVelocity * 0.5f;
                ioNewCharacterVelocity.SetY(inCharacterVelocity.GetY()); // Preserve Y velocity
                // printf("Character %s velocity reduced due to moving object collision\n", characterController->charId.c_str());
                break;
                
            case Layers::NON_MOVING:
            case Layers::BOUNDARY:
                // Project velocity along the surface for wall sliding
                JPH::Vec3 projectedVelocity = inCharacterVelocity - inContactNormal * inCharacterVelocity.Dot(inContactNormal);
                ioNewCharacterVelocity = projectedVelocity;
                // printf("Character %s sliding along surface\n", characterController->charId.c_str());
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
            // printf("Character %s X movement blocked\n", cc->charId.c_str());
        }
        if (normalZ > 0.7f) {
            cc->collidedZ = true;
            // printf("Character %s Z movement blocked\n", cc->charId.c_str());
        }
        
        // Handle ground contact
        if (normalY > 0.8f && contactNormal.GetY() > 0) {
            // Character is on ground - enable full friction
            ioSettings.mCanPushCharacter = false;
            // printf("Character %s on ground - full friction\n", cc->charId.c_str());
        } else if (normalY > 0.3f) {
            // Character on slope - reduced friction
            ioSettings.mCanPushCharacter = true;
            // printf("Character %s on slope - reduced friction\n", cc->charId.c_str());
        }
    }
};

std::vector<int> chai_collisions::addRigidMesh(std::string meshPath, int meshRef, bool makeConvex, bool makeShape) 
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
    
    // DEBUG: Print GLTF structure immediately after loading
    printf("\n========== GLTF FILE LOADED (meshRef %d) ==========\n", meshRef);
    printf("Path: %s\n", meshPath.c_str());
    printf("Nodes: %zu, Meshes: %zu\n", model.nodes.size(), model.meshes.size());
    for (size_t ni = 0; ni < model.nodes.size(); ni++) {
        auto& node = model.nodes[ni];
        printf("  Node[%zu]: name='%s', mesh=%d\n", ni, node.name.c_str(), node.mesh);
        if (!node.translation.empty()) {
            printf("    Translation: (%.3f, %.3f, %.3f)\n", 
                   node.translation[0], node.translation[1], node.translation[2]);
        } else {
            printf("    Translation: EMPTY (defaults to 0,0,0)\n");
        }
        if (!node.rotation.empty()) {
            printf("    Rotation: (%.3f, %.3f, %.3f, %.3f)\n", 
                   node.rotation[0], node.rotation[1], node.rotation[2], node.rotation[3]);
        } else {
            printf("    Rotation: EMPTY (defaults to identity)\n");
        }
    }
    printf("===================================================\n\n");
    
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
    // DEBUG: Print GLTF structure
    printf("\n[addRigidMesh] === GLTF STRUCTURE DEBUG ===\n");
    printf("Nodes: %zu, Meshes: %zu, Scenes: %zu\n", model.nodes.size(), model.meshes.size(), model.scenes.size());
    
    // Print all nodes with their properties
    for (size_t ni = 0; ni < model.nodes.size(); ni++) {
        auto& node = model.nodes[ni];
        printf("Node[%zu]: name='%s', mesh=%d, children=%zu\n", ni, node.name.c_str(), node.mesh, node.children.size());
        
        if (!node.matrix.empty()) {
            printf("  Has matrix (16 values)\n");
        }
        if (!node.translation.empty()) {
            printf("  Translation: (%.3f, %.3f, %.3f)\n", node.translation[0], node.translation[1], node.translation[2]);
        }
        if (!node.rotation.empty()) {
            printf("  Rotation: (%.3f, %.3f, %.3f, %.3f)\n", node.rotation[0], node.rotation[1], node.rotation[2], node.rotation[3]);
        }
        if (!node.scale.empty()) {
            printf("  Scale: (%.3f, %.3f, %.3f)\n", node.scale[0], node.scale[1], node.scale[2]);
        }
        if (!node.children.empty()) {
            printf("  Children nodes: ");
            for (auto childIdx : node.children) {
                printf("%d ", childIdx);
            }
            printf("\n");
        }
    }
    
    // Print scene hierarchy
    if (!model.scenes.empty()) {
        auto& scene = model.scenes[model.defaultScene >= 0 ? model.defaultScene : 0];
        printf("\nScene '%s' root nodes: ", scene.name.c_str());
        for (auto nodeIdx : scene.nodes) {
            printf("%d ", nodeIdx);
        }
        printf("\n");
    }
    printf("=================================\n\n");

    for (size_t i = 0; i < model.meshes.size(); i++) {
        vertices.clear();
        indices.clear();
        
        // Gather node transform so physics aligns with rendered mesh
        glm::vec3 nodeTranslation(0.0f);
        glm::quat nodeRotation(1.0f, 0.0f, 0.0f, 0.0f);
        glm::vec3 nodeScale(1.0f);
        glm::mat4 modelMatrix = glm::mat4(1.0f);
        bool applyTransformToVertices = false; // Only apply to vertices if we're using zero body position (old behavior)
        
        printf("[addRigidMesh] Looking for node with mesh index %zu\n", i);
        for (auto node : model.nodes) {
            if (node.mesh == (int)i) {
                printf("  FOUND matching node: mesh=%d, name='%s'\n", node.mesh, node.name.c_str());
                if (!node.matrix.empty()) {
                    glm::dmat4 nodeMatD = glm::make_mat4(node.matrix.data());
                    glm::mat4 nodeMat = glm::mat4(nodeMatD);
                    nodeTranslation = glm::vec3(nodeMat[3]);
                    nodeRotation = glm::quat_cast(nodeMat);
                    nodeScale = glm::vec3(
                        glm::length(glm::vec3(nodeMat[0])),
                        glm::length(glm::vec3(nodeMat[1])),
                        glm::length(glm::vec3(nodeMat[2]))
                    );
                } else {
                    if (!node.translation.empty()) {
                        nodeTranslation = glm::vec3(node.translation[0], node.translation[1], node.translation[2]);
                    }
                    if (!node.rotation.empty()) {
                        nodeRotation = glm::quat(node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]);
                    }
                    if (!node.scale.empty()) {
                        nodeScale = glm::vec3(node.scale[0], node.scale[1], node.scale[2]);
                    }
                }
                // Only apply scale to vertices (rotation and translation handled by body transform)
                modelMatrix = glm::scale(glm::mat4(1.0f), nodeScale);
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
                        // glBegin(GL_TRIANGLES);
                        // glVertex3f(vertices[0], vertices[1], vertices[2]);
                        // glVertex3f(vertices[3], vertices[4], vertices[5]);
                        // glVertex3f(vertices[6], vertices[7], vertices[8]);
                        // glEnd();    
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
                        // glBegin(GL_TRIANGLES);
                        // glVertex3f(vertices[0], vertices[1], vertices[2]);
                        // glVertex3f(vertices[3], vertices[4], vertices[5]);
                        // glVertex3f(vertices[6], vertices[7], vertices[8]);
                        // glEnd();    
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
        
        // Calculate mesh centroid for proper body positioning
        JPH::Vec3 meshMin(FLT_MAX, FLT_MAX, FLT_MAX);
        JPH::Vec3 meshMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);
        for (size_t vi = 0; vi + 2 < vertices.size(); vi += 3) {
            float vx = vertices[vi];
            float vy = vertices[vi + 1];
            float vz = vertices[vi + 2];
            
            // Validate vertex coordinates
            if (!isfinite(vx) || !isfinite(vy) || !isfinite(vz)) {
                printf("[addRigidMesh] WARNING: Non-finite vertex at index %zu: (%.2e, %.2e, %.2e), skipping bounds update\n", 
                       vi, vx, vy, vz);
                continue;
            }
            
            meshMin.SetX(std::min(meshMin.GetX(), vx));
            meshMax.SetX(std::max(meshMax.GetX(), vx));
            meshMin.SetY(std::min(meshMin.GetY(), vy));
            meshMax.SetY(std::max(meshMax.GetY(), vy));
            meshMin.SetZ(std::min(meshMin.GetZ(), vz));
            meshMax.SetZ(std::max(meshMax.GetZ(), vz));
        }
        
        // Validate bounds are finite before computing centroid
        if (!isfinite(meshMin.GetX()) || !isfinite(meshMin.GetY()) || !isfinite(meshMin.GetZ()) ||
            !isfinite(meshMax.GetX()) || !isfinite(meshMax.GetY()) || !isfinite(meshMax.GetZ())) {
            printf("[addRigidMesh] WARNING: Mesh %zu has invalid bounds, using zero position\n", i);
            meshMin = JPH::Vec3::sZero();
            meshMax = JPH::Vec3::sZero();
        }
        
        // Don't offset vertices - keep mesh at original position
        printf("[addRigidMesh] Mesh %zu bounds: (%.2f, %.2f, %.2f) to (%.2f, %.2f, %.2f)\n",
               i, meshMin.GetX(), meshMin.GetY(), meshMin.GetZ(),
               meshMax.GetX(), meshMax.GetY(), meshMax.GetZ());
        
        if (makeConvex) {
            // Create a convex hull shape that can be dynamic
            std::vector<JPH::Vec3> jphVertices;
            for (size_t vi = 0; vi + 2 < vertices.size(); vi += 3) {
                jphVertices.emplace_back(vertices[vi], vertices[vi + 1], vertices[vi + 2]);
            }
            
            if (!jphVertices.empty()) {
                JPH::ConvexHullShapeSettings convexSettings(jphVertices.data(), (int)jphVertices.size());
                convexSettings.mMaxConvexRadius = 0.0f;
                auto result = convexSettings.Create();
                if (result.HasError()) {
                    printf("ConvexHull creation failed: %s\n", result.GetError().c_str());
                    continue; // Skip this mesh
                }
                shape = result.Get();
            } else {
                continue; // No vertices, skip
            }
        } else {
            // Create a non-convex mesh shape (must be static)
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
            auto s = meshSettings.Create();
            if (s.HasError()) {
                printf("MeshShape creation failed: %s\n", s.GetError().c_str());
                continue;
            }
            shape = s.Get();
        }
        
        if (!shape) {
            continue; // Shape creation failed, skip
        }  

        // Set up body creation settings
        // If makeShape is true, create as kinematic for vehicle use
        JPH::EMotionType motionType;
        JPH::ObjectLayer objectLayer;
        
        if (makeShape) {
            // Kinematic bodies for vehicle parts (will be positioned manually)
            motionType = JPH::EMotionType::Kinematic;
            objectLayer = Layers::MOVING;
        } else if (makeConvex) {
            // Dynamic convex bodies
            motionType = JPH::EMotionType::Dynamic;
            objectLayer = Layers::MOVING;
        } else {
            // Static mesh floors
            motionType = JPH::EMotionType::Static;
            objectLayer = Layers::NON_MOVING;
        }
        
        // Use node translation directly for body position (no centroid offset)
        JPH::RVec3 bodyPosition(nodeTranslation.x, nodeTranslation.y, nodeTranslation.z);
        JPH::Quat bodyRotation(nodeRotation.x, nodeRotation.y, nodeRotation.z, nodeRotation.w);
        
        // DEBUG: Print what we're reading from GLTF
        printf("[addRigidMesh] Mesh %zu, meshRef %d: BodyPosition=(%.3f, %.3f, %.3f), Rotation=(%.3f, %.3f, %.3f, %.3f), Scale=(%.3f, %.3f, %.3f)\n",
               i, meshRef,
               bodyPosition.GetX(), bodyPosition.GetY(), bodyPosition.GetZ(),
               bodyRotation.GetX(), bodyRotation.GetY(), bodyRotation.GetZ(), bodyRotation.GetW(),
               nodeScale.x, nodeScale.y, nodeScale.z);
        
        JPH::BodyCreationSettings bodySettings(
            shape,
            bodyPosition, // Use actual GLTF node translation
            bodyRotation, // Use actual GLTF node rotation
            motionType,
            objectLayer
        );
        
        // Set mass properties only for dynamic bodies (convex and not makeShape)
        // Kinematic and static bodies don't need mass
        if (makeConvex && !makeShape) {
            bodySettings.mOverrideMassProperties = JPH::EOverrideMassProperties::MassAndInertiaProvided;
            bodySettings.mMassPropertiesOverride.SetMassAndInertiaOfSolidBox(JPH::Vec3(1.0f, 1.0f, 1.0f), 1.0f); // 1kg mass, not 100kg
            bodySettings.mGravityFactor = 1.0f; // Enable gravity for dynamic convex bodies
        }

        // Create the body
        auto ps = worlds->worlds[0]->physics_system;
        JPH::BodyInterface& bodyInterface = ps->GetBodyInterface();
        JPH::Body *body = bodyInterface.CreateBody(bodySettings);
        
        // Add and activate based on type
        if (makeShape || motionType == JPH::EMotionType::Static) {
            // Kinematic bodies and static meshes - don't activate
            bodyInterface.AddBody(body->GetID(), JPH::EActivation::DontActivate);
        } else {
            // Dynamic convex bodies - activate them so they fall
            bodyInterface.AddBody(body->GetID(), JPH::EActivation::Activate);
        }
        bodyInterface.SetFriction(body->GetID(), 1.0f);
        auto bodyID = body->GetID();

        // Store shape if makeShape is true (for vehicle creation)
        JPH::Ref<JPH::Shape> storedShape = makeShape ? shape : nullptr;
        // Store node transforms as-is for reference
        auto rm = new RigidMesh(bodyID, meshRef, storedShape, JPH::Vec3::sZero(), nodeTranslation, nodeRotation);
        rigidMeshes.push_back(rm);

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
        bodyInterface.SetPositionAndRotation(rigidMeshes[i]->bodyID, JPH::RVec3(x, y, z), JPH::Quat::sIdentity(), JPH::EActivation::Activate);

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
    for (auto i : rigidMeshIndex) {
        auto r = rigidMeshes[i]->bodyID;
        auto ps = worlds->worlds[0]->physics_system;
        JPH::BodyInterface& bodyInterface = ps->GetBodyInterface();
        
        // Don't toggle Kinematic bodies (sensors, boundaries, etc.)
        JPH::EMotionType currentMotionType = bodyInterface.GetMotionType(r);
        if (currentMotionType == JPH::EMotionType::Kinematic) {
            continue;
        }
        
        const JPH::Shape* shape = bodyInterface.GetShape(r);
        if (shape && shape->GetSubType() == JPH::EShapeSubType::Mesh) {
            // Mesh shapes should remain static; dynamic mesh shapes can produce NaN/Inf states
            bodyInterface.SetMotionType(r, JPH::EMotionType::Static, JPH::EActivation::DontActivate);
            bodyInterface.SetObjectLayer(r, Layers::NON_MOVING);
            bodyInterface.SetLinearVelocity(r, JPH::Vec3::sZero());
            bodyInterface.SetAngularVelocity(r, JPH::Vec3::sZero());
            continue;
        }
        auto comTransform = bodyInterface.GetCenterOfMassTransform(r);
        auto pos = comTransform.GetTranslation();
        bool posFinite = std::isfinite((float)pos.GetX()) && std::isfinite((float)pos.GetY()) && std::isfinite((float)pos.GetZ());

        if (!posFinite) {
            // Bail out if position is non-finite; keep static to avoid NaN propagation
            bodyInterface.SetMotionType(r, JPH::EMotionType::Static, JPH::EActivation::DontActivate);
            bodyInterface.SetObjectLayer(r, Layers::NON_MOVING);
            bodyInterface.SetLinearVelocity(r, JPH::Vec3::sZero());
            bodyInterface.SetAngularVelocity(r, JPH::Vec3::sZero());
            continue;
        }

        // Clear velocities before switching motion type to avoid NaN/Inf state
        bodyInterface.SetLinearVelocity(r, JPH::Vec3::sZero());
        bodyInterface.SetAngularVelocity(r, JPH::Vec3::sZero());

        if (enable) {
            // Enable physics - change to dynamic motion type so it can fall
            bodyInterface.SetMotionType(r, JPH::EMotionType::Dynamic, JPH::EActivation::Activate);
            bodyInterface.SetObjectLayer(r, Layers::MOVING);
        } else {
            // Disable physics - change back to static
            bodyInterface.SetMotionType(r, JPH::EMotionType::Static, JPH::EActivation::DontActivate);
            bodyInterface.SetObjectLayer(r, Layers::NON_MOVING);
        }
    }
    
}

void chai_collisions::createVehicle(int frontLeftWheelMeshRef, int frontRightWheelMeshRef, int rearLeftWheelMeshRef, int rearRightWheelMeshRef, int chassisMeshRef, float mass, float wheelRadius, float wheelWidth, float suspensionRestLength, float suspensionStiffness, float suspensionDamping, float suspensionCompression, float frictionSlip, float maxSuspensionTravelCm, float maxSuspensionForce)
{
    // Sanity check: ensure mass is reasonable (1kg to 10000kg)
    if (mass < 1.0f || mass > 10000.0f) {
        printf("[Vehicle] ERROR: Unrealistic mass %.2f (must be 1-10000 kg), using 1500 kg as default\n", mass);
        mass = 1500.0f;
    }
    
    // Clamp suspension parameters to reasonable ranges to prevent instability
    // Suspension frequency should be 1-3 Hz for most vehicles
    // For a vehicle with mass M, spring constant k gives frequency f = sqrt(k/M)/(2*pi)
    // So k = (2*pi*f)^2 * M. For f=2Hz and M=1000kg, k ≈ 79000 N/m
    suspensionStiffness = std::max(1000.0f, std::min(suspensionStiffness, 200000.0f)); // Clamp to 1000-200000
    suspensionDamping = std::max(100.0f, std::min(suspensionDamping, 50000.0f));     // Clamp to 100-50000
    suspensionCompression = std::max(0.1f, std::min(suspensionCompression, 1.0f));   // Clamp to 0.1-1.0
    // Clamp rest length to a sane range (meters)
    suspensionRestLength = std::max(0.1f, std::min(suspensionRestLength, 1.0f));     // Clamp to 0.1-1.0 m
    
    printf("[Vehicle] Initialization: mass=%.2f, suspensionStiffness=%.2f, suspensionDamping=%.2f, compression=%.2f\n",
           mass, suspensionStiffness, suspensionDamping, suspensionCompression);
    
    // Ensure world 0 is initialized before using it
    if (!worlds || worlds->worlds.find(0) == worlds->worlds.end()) {
        printf("[Vehicle] World 0 not initialized, calling init(0) before vehicle creation\n");
        init(0);
    }
    if (!worlds || worlds->worlds.find(0) == worlds->worlds.end() || !worlds->worlds[0]->physics_system) {
        printf("[Vehicle] ERROR: Physics world unavailable, aborting vehicle creation\n");
        return;
    }

    auto ps = worlds->worlds[0]->physics_system;
    JPH::BodyInterface& bodyInterface = ps->GetBodyInterface();
    
    // DEBUG: Print all rigid bodies and their meshRefs
    printf("\n=== ALL RIGID BODIES IN SCENE ===\n");
    for (auto* rm : rigidMeshes) {
        JPH::RVec3 pos = bodyInterface.GetPosition(rm->bodyID);
        printf("Body meshRef: %d, Position: (%.2f, %.2f, %.2f)\n", rm->meshRef, pos.GetX(), pos.GetY(), pos.GetZ());
    }
    printf("Chassis meshRef to find: %d\n", chassisMeshRef);
    printf("Wheel meshRefs to find: %d, %d, %d, %d\n", frontLeftWheelMeshRef, frontRightWheelMeshRef, rearLeftWheelMeshRef, rearRightWheelMeshRef);
    printf("================================\n\n");
    
    // Find existing chassis shape and position by meshRef
    RigidMesh* chassisRM = nullptr;
    JPH::Ref<JPH::Shape> chassisShape = nullptr;
    JPH::RVec3 chassisPos;
    JPH::Quat chassisRot;
    
    for (auto* rm : rigidMeshes) {
        if (rm->meshRef == chassisMeshRef) {
            chassisRM = rm;
            chassisShape = rm->shape;  // Retrieve stored shape
            
            // Get old body's transform before removing it
            chassisPos = bodyInterface.GetPosition(rm->bodyID);
            chassisRot = bodyInterface.GetRotation(rm->bodyID);
            
            if (!chassisShape) {
                printf("ERROR: Chassis meshRef %d has no stored shape (makeShape was false)\n", chassisMeshRef);
                return;
            }
            
            printf("Found chassis shape for meshRef %d at position (%.2f, %.2f, %.2f)\n", 
                   chassisMeshRef, chassisPos.GetX(), chassisPos.GetY(), chassisPos.GetZ());
            
            // Remove old body from physics system
            bodyInterface.RemoveBody(rm->bodyID);
            bodyInterface.DestroyBody(rm->bodyID);
            
            break;
        }
    }
    
    if (!chassisShape) {
        printf("ERROR: Chassis body with meshRef %d not found or has no shape\n", chassisMeshRef);
        return;
    }
    
    // Create new chassis body from shape with proper vehicle settings
    JPH::BodyCreationSettings chassisSettings(
        chassisShape,
        chassisPos,
        chassisRot,
        JPH::EMotionType::Dynamic,
        Layers::MOVING
    );
    
    // Set mass properties for the chassis
    chassisSettings.mOverrideMassProperties = JPH::EOverrideMassProperties::MassAndInertiaProvided;
    chassisSettings.mMassPropertiesOverride.mMass = mass;
    
    // Calculate inertia based on a box approximation (more stable than scaling)
    // For a rectangular box: I = (1/12) * m * (L^2 + W^2) for each axis
    // Use a conservative box estimate to avoid degenerate inertias
    float chassis_half_width = 1.0f;
    float chassis_half_depth = 1.5f;
    float chassis_half_height = 0.5f;
    float Ixx = (mass / 12.0f) * (4 * chassis_half_depth * chassis_half_depth + 4 * chassis_half_height * chassis_half_height);
    float Iyy = (mass / 12.0f) * (4 * chassis_half_width * chassis_half_width + 4 * chassis_half_height * chassis_half_height);
    float Izz = (mass / 12.0f) * (4 * chassis_half_width * chassis_half_width + 4 * chassis_half_depth * chassis_half_depth);
    chassisSettings.mMassPropertiesOverride.mInertia = JPH::Mat44::sScale(JPH::Vec3(Ixx, Iyy, Izz));
    
    chassisSettings.mGravityFactor = 1.0f;
    chassisSettings.mFriction = 0.5f;
    
    // Add damping to prevent tumbling during free fall
    // Angular damping is critical for stable falling
    chassisSettings.mAngularDamping = 0.9f;  // High angular damping to suppress rotation
    chassisSettings.mLinearDamping = 0.1f;   // Small linear damping to reduce air resistance
    
    printf("[Vehicle] Chassis mass: %.2f, Inertia: (%.2f, %.2f, %.2f)\n", mass, Ixx, Iyy, Izz);
    
    JPH::Body* chassisBody = bodyInterface.CreateBody(chassisSettings);
    // Prevent sleeping to ensure the chassis remains visible and updated
    chassisBody->SetAllowSleeping(false);
    bodyInterface.AddBody(chassisBody->GetID(), JPH::EActivation::Activate);
    JPH::BodyID chassisBodyID = chassisBody->GetID();
    
    // Ensure chassis starts with zero angular velocity
    bodyInterface.SetAngularVelocity(chassisBodyID, JPH::Vec3::sZero());

    // Tag bodies with vehicle ID for contact filtering (avoid chassis-wheel collisions per vehicle)
    uint64 vehicleTag = (uint64)(vehicles.size() + 1);
    bodyInterface.SetUserData(chassisBodyID, vehicleTag);
    
    // Place chassis on MOVING layer
    bodyInterface.SetObjectLayer(chassisBodyID, Layers::MOVING);
    
    // Update RigidMesh to point to new body
    chassisRM->bodyID = chassisBodyID;
    
    printf("Created new chassis body from shape, BodyID: %u\n", (uint32)chassisBodyID.GetIndex());
    JPH::Mat44 chassisTransform = JPH::Mat44::sRotationTranslation(chassisRot, chassisPos);
    
    // Extract wheel shapes and create new wheel bodies
    JPH::Vec3 wheelPositions[4];
    JPH::BodyID wheelBodyIDs[4];
    JPH::RVec3 wheelWorldPositions[4];
    RigidMesh* wheelRMs[4] = { nullptr, nullptr, nullptr, nullptr };
    JPH::Ref<JPH::Shape> wheelShapes[4];
    int wheelMeshRefs[4] = { frontLeftWheelMeshRef, frontRightWheelMeshRef, rearLeftWheelMeshRef, rearRightWheelMeshRef };
    
    printf("\n=== VEHICLE WHEEL SETUP ===\n");
    printf("Chassis shape available: %s\n", chassisShape ? "YES" : "NO");
    
    for (int i = 0; i < 4; i++) {
        bool found = false;
        for (auto* rm : rigidMeshes) {
            if (rm->meshRef == wheelMeshRefs[i]) {
                wheelRMs[i] = rm;
                wheelShapes[i] = rm->shape;  // Retrieve stored shape

                if (!wheelShapes[i]) {
                    printf("ERROR: Wheel %d (meshRef %d) has no stored shape (makeShape was false)\n", i, wheelMeshRefs[i]);
                    // Clean up already created bodies and abort
                    bodyInterface.RemoveBody(chassisBodyID);
                    bodyInterface.DestroyBody(chassisBodyID);
                    for (int j = 0; j < i; j++) {
                        bodyInterface.RemoveBody(wheelBodyIDs[j]);
                        bodyInterface.DestroyBody(wheelBodyIDs[j]);
                    }
                    return;
                }

                // Get wheel position from the old kinematic body - this was set correctly during addRigidMesh
                JPH::RVec3 wheelWorldPos = bodyInterface.GetPosition(rm->bodyID);
                JPH::Quat wheelRot = bodyInterface.GetRotation(rm->bodyID);
                
                printf("Wheel %d: old kinematic body position: (%.2f, %.2f, %.2f)\n",
                       i, wheelWorldPos.GetX(), wheelWorldPos.GetY(), wheelWorldPos.GetZ());
                
                wheelWorldPositions[i] = wheelWorldPos;
                
                printf("Wheel %d shape found, GLTF node position: (%.2f, %.2f, %.2f), world position: (%.2f, %.2f, %.2f)\n", 
                       i, rm->nodePosition.x, rm->nodePosition.y, rm->nodePosition.z,
                       wheelWorldPos.GetX(), wheelWorldPos.GetY(), wheelWorldPos.GetZ());
                
                // Remove old wheel body
                bodyInterface.RemoveBody(rm->bodyID);
                bodyInterface.DestroyBody(rm->bodyID);
                
                // Create new kinematic wheel body (VehicleConstraint handles physics, but wheels must collide with world)
                JPH::BodyCreationSettings wheelSettings(
                    wheelShapes[i],
                    wheelWorldPos,
                    wheelRot,
                    JPH::EMotionType::Dynamic,
                    Layers::WHEEL
                );
                
                JPH::Body* wheelBody = bodyInterface.CreateBody(wheelSettings);
                // Wheels are dynamic bodies attached to chassis via suspension constraint
                // Don't make them sensors; let collision layer filtering prevent chassis collisions
                wheelBody->SetIsSensor(false);
                
                // Prevent sleeping to ensure wheels remain visible and updated
                wheelBody->SetAllowSleeping(false);
                // Always keep wheels in the physics system; don't let them deactivate
                // This ensures they continue to be synced and rendered each frame
                bodyInterface.AddBody(wheelBody->GetID(), JPH::EActivation::Activate);
                wheelBodyIDs[i] = wheelBody->GetID();

                // Update the corresponding RigidMesh to reference the new wheel body
                if (wheelRMs[i]) {
                    wheelRMs[i]->bodyID = wheelBodyIDs[i];
                }

                // Tag wheel with same vehicle ID for contact filtering
                bodyInterface.SetUserData(wheelBody->GetID(), vehicleTag);
                
                // Place wheel on WHEEL layer (separate from MOVING so they don't collide)
                bodyInterface.SetObjectLayer(wheelBody->GetID(), Layers::WHEEL);
                
                // Ensure wheel stays active
                bodyInterface.ActivateBody(wheelBody->GetID());
                
                // Calculate relative position for vehicle
                JPH::Vec3 wheelRelative = JPH::Vec3(
                    (float)(wheelWorldPos.GetX() - chassisPos.GetX()),
                    (float)(wheelWorldPos.GetY() - chassisPos.GetY()),
                    (float)(wheelWorldPos.GetZ() - chassisPos.GetZ())
                );
                
                printf("Wheel %d: world=(%.2f, %.2f, %.2f), chassis=(%.2f, %.2f, %.2f), relative=(%.2f, %.2f, %.2f)\n",
                       i, wheelWorldPos.GetX(), wheelWorldPos.GetY(), wheelWorldPos.GetZ(),
                       chassisPos.GetX(), chassisPos.GetY(), chassisPos.GetZ(),
                       wheelRelative.GetX(), wheelRelative.GetY(), wheelRelative.GetZ());
                
                // Use relative world position directly 
                wheelPositions[i] = wheelRelative;
                
                printf("Wheel %d new BodyID: %u, local position: (%.2f, %.2f, %.2f)\n", 
                       i, (uint32)wheelBodyIDs[i].GetIndex(),
                       wheelPositions[i].GetX(), wheelPositions[i].GetY(), wheelPositions[i].GetZ());
                
                found = true;
                break;
            }
        }
        
        if (!found) {
            printf("ERROR: Wheel %d with meshRef %d not found in rigidMeshes\n", i, wheelMeshRefs[i]);
            // Clean up already created bodies
            bodyInterface.RemoveBody(chassisBodyID);
            bodyInterface.DestroyBody(chassisBodyID);
            for (int j = 0; j < i; j++) {
                bodyInterface.RemoveBody(wheelBodyIDs[j]);
                bodyInterface.DestroyBody(wheelBodyIDs[j]);
            }
            return;
        }
    }
    
    // Use only FixedConstraints to lock wheels to chassis
    // No VehicleConstraint - wheels collide with ground via layer filtering
    const JPH::BodyLockInterface& lockInterface = ps->GetBodyLockInterface();
    printf("[Vehicle] Setting up wheel constraints with anchor positions:\n");
    
    // Lock chassis to get its transform
    JPH::BodyLockWrite chassisLock(lockInterface, chassisBodyID);
    if (!chassisLock.Succeeded()) {
        printf("ERROR: Failed to lock chassis body\n");
        return;
    }
    JPH::Body& chassisBodyRef = chassisLock.GetBody();
    
    for (int i = 0; i < 4; i++) {
        // Validate wheel positions before constraining
        bool wheelPosValid = std::isfinite(wheelWorldPositions[i].GetX()) &&
                            std::isfinite(wheelWorldPositions[i].GetY()) &&
                            std::isfinite(wheelWorldPositions[i].GetZ());
        
        printf("  Wheel %d world position: (%.3f, %.3f, %.3f) valid=%d\n", i,
               wheelWorldPositions[i].GetX(), wheelWorldPositions[i].GetY(), wheelWorldPositions[i].GetZ(), wheelPosValid);
        
        if (!wheelPosValid) {
            printf("[Vehicle] WARNING: Wheel %d has invalid position, skipping constraint\n", i);
            continue;
        }
        
        // Use world positions directly for constraint points
        JPH::Vec3 constraintPoint(wheelWorldPositions[i].GetX(), wheelWorldPositions[i].GetY(), wheelWorldPositions[i].GetZ());
        
        JPH::FixedConstraintSettings fixedSettings;
        fixedSettings.mSpace = JPH::EConstraintSpace::WorldSpace;
        fixedSettings.mPoint1 = fixedSettings.mPoint2 = constraintPoint;
        fixedSettings.mAxisX1 = fixedSettings.mAxisX2 = JPH::Vec3::sAxisX();
        fixedSettings.mAxisY1 = fixedSettings.mAxisY2 = JPH::Vec3::sAxisY();
        
        // Lock wheel body to get reference
        JPH::BodyLockWrite wheelLock(lockInterface, wheelBodyIDs[i]);
        if (wheelLock.Succeeded()) {
            JPH::Body& wheelBody = wheelLock.GetBody();
            JPH::TwoBodyConstraint* fixedConstraint = fixedSettings.Create(chassisBodyRef, wheelBody);
            ps->AddConstraint(fixedConstraint);
        }
    }
    
    printf("=== VEHICLE SETUP COMPLETE (CONSTRAINT ACTIVE) ===\n");
    printf("Chassis BodyID: %u\n", (uint32)chassisBodyID.GetIndex());
    for (int i = 0; i < 4; i++) {
        printf("Wheel %d BodyID: %u, Position: (%.2f, %.2f, %.2f)\n", i, (uint32)wheelBodyIDs[i].GetIndex(), 
            wheelPositions[i].GetX(), wheelPositions[i].GetY(), wheelPositions[i].GetZ());
    }
    printf("=== VEHICLE SETUP COMPLETE (FIXED CONSTRAINTS ONLY) ===\n");
    printf("Chassis BodyID: %u\n", (uint32)chassisBodyID.GetIndex());
    for (int i = 0; i < 4; i++) {
        printf("Wheel %d BodyID: %u, Position: (%.2f, %.2f, %.2f)\n", i, (uint32)wheelBodyIDs[i].GetIndex(), 
            wheelPositions[i].GetX(), wheelPositions[i].GetY(), wheelPositions[i].GetZ());
    }
    printf("Wheels locked via FixedConstraints\n");
    printf("=========================================================\n\n");
    
    // Store vehicle data - note: no VehicleConstraint, using FixedConstraints only
    Vehicle* vehicle = new Vehicle(nullptr, chassisBodyID, wheelBodyIDs, wheelPositions,
                                   suspensionRestLength, suspensionStiffness, suspensionDamping, wheelRadius);
    vehicle->frameCounter = 0;
    vehicle->constraintActive = true;
    vehicles.push_back(vehicle);
    
        printf("[Vehicle] Created vehicle: chassis meshRef=%d, BodyID=%u, wheels=%u/%u/%u/%u\n",
            chassisMeshRef,
            (uint32)chassisBodyID.GetIndex(),
            (uint32)wheelBodyIDs[0].GetIndex(),
            (uint32)wheelBodyIDs[1].GetIndex(),
            (uint32)wheelBodyIDs[2].GetIndex(),
            (uint32)wheelBodyIDs[3].GetIndex());
        printf("[Vehicle] Suspension: Rest=%.2f, Stiffness=%.1f, Damping=%.1f\n", suspensionRestLength, suspensionStiffness, suspensionDamping);
}

int chai_collisions::addCharacterController(int index, int meshRef, std::string charId)
{
    if (characterControllers.size() > 0) {
        for (int i = 0; i < characterControllers.size(); i++) {
            if (characterControllers[i]->charId == charId) {
                characterControllers[i]->addMesh(meshRef);
                // printf("Character controller with charId %s already exists, added meshRef %d\n", charId.c_str(), meshRef);
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
        // JPH::CollisionGroup group;
        // if (charId == "player0") group.SetGroupID(Groups::CHARACTER_0);
        // if (charId == "player1") group.SetGroupID(Groups::CHARACTER_1);
        // if (charId == "player2") group.SetGroupID(Groups::CHARACTER_2);
        // if (charId == "player3") group.SetGroupID(Groups::CHARACTER_3);
        // bodyInterface.SetCollisionGroup(bodyID, group);
    }

    // Store character reference in your CharacterController
    characterControllers.emplace_back(new CharacterController(bodyID, meshRef, charId, character, index));
    
    // printf("Created character controller with charId %s at position (0, 5.0f, 0)\n", charId.c_str());
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

#ifdef JPH_DEBUG_RENDERER
// Libretro integration functions for debug renderer
std::vector<float> chai_collisions::getDebugRendererLineVertices(int worldGroup)
{
    std::vector<float> buffer;
    if (worlds && worlds->worlds.find(worldGroup) != worlds->worlds.end()) {
        auto world = worlds->worlds[worldGroup];
        if (world && world->debug_renderer) {
            auto debugRenderer = static_cast<DebugRendererImpl*>(world->debug_renderer);
            if (debugRenderer) {
                debugRenderer->GetLineVertexBuffer(buffer);
            }
        }
    }
    return buffer;
}

std::vector<float> chai_collisions::getDebugRendererTriangleVertices(int worldGroup)
{
    std::vector<float> buffer;
    if (worlds && worlds->worlds.find(worldGroup) != worlds->worlds.end()) {
        auto world = worlds->worlds[worldGroup];
        if (world && world->debug_renderer) {
            auto debugRenderer = static_cast<DebugRendererImpl*>(world->debug_renderer);
            if (debugRenderer) {
                debugRenderer->GetTriangleVertexBuffer(buffer);
            }
        }
    }
    return buffer;
}

size_t chai_collisions::getDebugRendererLineCount(int worldGroup)
{
    if (worlds && worlds->worlds.find(worldGroup) != worlds->worlds.end()) {
        auto world = worlds->worlds[worldGroup];
        if (world && world->debug_renderer) {
            auto debugRenderer = static_cast<DebugRendererImpl*>(world->debug_renderer);
            if (debugRenderer) {
                return debugRenderer->GetLines().size();
            }
        }
    }
    return 0;
}

size_t chai_collisions::getDebugRendererTriangleCount(int worldGroup)
{
    if (worlds && worlds->worlds.find(worldGroup) != worlds->worlds.end()) {
        auto world = worlds->worlds[worldGroup];
        if (world && world->debug_renderer) {
            auto debugRenderer = static_cast<DebugRendererImpl*>(world->debug_renderer);
            if (debugRenderer) {
                return debugRenderer->GetTriangles().size();
            }
        }
    }
    return 0;
}

void chai_collisions::clearDebugRendererGeometry(int worldGroup)
{
    if (worlds && worlds->worlds.find(worldGroup) != worlds->worlds.end()) {
        auto world = worlds->worlds[worldGroup];
        if (world && world->debug_renderer) {
            auto debugRenderer = static_cast<DebugRendererImpl*>(world->debug_renderer);
            if (debugRenderer) {
                debugRenderer->Clear();
            }
        }
    }
}
#endif // JPH_DEBUG_RENDERER

int chai_collisions::addBox(float x, float y, float z, float width, float height, float depth, std::vector<int> group = {0}, int index = 0)
{
    // Create a triangle mesh to represent the box in jolt physics
    JPH::TriangleList mesh;

    // Box vertices in local space
    JPH::Vec3 vertices[8] = {
        JPH::Vec3(-width/2, -height/2, -depth/2), // 0
        JPH::Vec3( width/2, -height/2, -depth/2), // 1
        JPH::Vec3( width/2, -height/2,  depth/2), // 2
        JPH::Vec3(-width/2, -height/2,  depth/2), // 3
        JPH::Vec3(-width/2,  height/2, -depth/2), // 4
        JPH::Vec3( width/2,  height/2, -depth/2), // 5
        JPH::Vec3( width/2,  height/2,  depth/2), // 6
        JPH::Vec3(-width/2,  height/2,  depth/2)  // 7
    };

    // Face definitions: each quad becomes 2 triangles
    struct Face {
        int indices[4]; // 4 vertices of the quad (counter-clockwise)
    };
    
    Face faces[6] = {
        {{0, 3, 2, 1}}, // Bottom: indices for quad vertices (CCW from bottom)
        {{4, 5, 6, 7}}, // Top: indices for quad vertices (CCW from top)
        {{0, 1, 5, 4}}, // Back: Z = -depth/2
        {{2, 3, 7, 6}}, // Front: Z = +depth/2
        {{0, 4, 7, 3}}, // Left: X = -width/2
        {{1, 2, 6, 5}}  // Right: X = +width/2
    };

    // Generate triangles for each face
    for (int f = 0; f < 6; f++) {
        Face& face = faces[f];
        
        // First triangle: v0, v1, v2
        mesh.push_back(JPH::Triangle(
            vertices[face.indices[0]], 
            vertices[face.indices[1]], 
            vertices[face.indices[2]]
        ));
        
        // Second triangle: v0, v2, v3
        mesh.push_back(JPH::Triangle(
            vertices[face.indices[0]], 
            vertices[face.indices[2]], 
            vertices[face.indices[3]]
        ));
    }

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
    
    // Kinematic bodies with MeshShape also need mass properties
    bodySettings.mOverrideMassProperties = JPH::EOverrideMassProperties::MassAndInertiaProvided;
    bodySettings.mMassPropertiesOverride.SetMassAndInertiaOfSolidBox(JPH::Vec3(width, height, depth), 1.0f);
    
    // JPH::CollisionGroup groupSettings;
    // if (index == 0) groupSettings.SetGroupID(Groups::CHARACTER_0);
    // if (index == 1) groupSettings.SetGroupID(Groups::CHARACTER_1);
    // if (index == 2) groupSettings.SetGroupID(Groups::CHARACTER_2);
    // if (index == 3) groupSettings.SetGroupID(Groups::CHARACTER_3);
    // bodySettings.mCollisionGroup = groupSettings;

    // bodySettings.mFriction = 1.0f;
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
    
    // Guard against null or invalid worlds
    if (!worlds || worlds->worlds.empty() || worlds->worlds.find(0) == worlds->worlds.end()) {
        return bb;
    }
    
    for (auto &m : rigidMeshes) {
        if (m->meshRef == mesh) {
            // Get world space bounds
            auto ps = worlds->worlds[0]->physics_system;
            if (!ps) continue;  // Skip if physics system is null
            JPH::BodyInterface& bodyInterface = ps->GetBodyInterface();
            
            // Guard against invalid body ID
            if (m->bodyID.IsInvalid()) continue;
            if (!bodyInterface.IsAdded(m->bodyID)) continue;
            
            auto bodyTransform = bodyInterface.GetCenterOfMassTransform(m->bodyID);
            JPH::Vec3 inScale(1.0f, 1.0f, 1.0f);
            
            // Guard against null shape
            auto shape = bodyInterface.GetShape(m->bodyID);
            if (!shape) continue;
            
            auto shapeBounds = shape->GetWorldSpaceBounds(bodyTransform, inScale);
            bb.push_back(std::make_pair(
                glm::vec3(shapeBounds.mMin.GetX(), shapeBounds.mMin.GetY(), shapeBounds.mMin.GetZ()),
                glm::vec3(shapeBounds.mMax.GetX(), shapeBounds.mMax.GetY(), shapeBounds.mMax.GetZ())
            ));

            // printf("RigidMesh meshRef %d bounding box min(%f, %f, %f) max(%f, %f, %f)\n", mesh,
            //     shapeBounds.mMin.GetX(), shapeBounds.mMin.GetY(), shapeBounds.mMin.GetZ(),
            //     shapeBounds.mMax.GetX(), shapeBounds.mMax.GetY(), shapeBounds.mMax.GetZ()
            // );

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
    // printf("[DEBUG getPhysicsObjects] Looking for mesh ID: %d, total rigidMeshes: %zu\n", mesh, rigidMeshes.size());
    for (auto &m : rigidMeshes) {
        // printf("[DEBUG getPhysicsObjects] Checking rigidMesh with meshRef: %d\n", m->meshRef);
        if (m->meshRef == mesh) {
            // printf("[DEBUG getPhysicsObjects] Found matching mesh! Fetching physics transform...\n");
            // Get world space bounds

                auto ps = worlds->worlds[0]->physics_system;
                JPH::BodyInterface& bodyInterface = ps->GetBodyInterface();

                // Use world-space position/rotation directly (avoid COM offsets)
                glm::vec3 translation(
                (float)bodyInterface.GetPosition(m->bodyID).GetX(),
                (float)bodyInterface.GetPosition(m->bodyID).GetY(),
                (float)bodyInterface.GetPosition(m->bodyID).GetZ());
                JPH::Quat bodyRot = bodyInterface.GetRotation(m->bodyID);
                glm::quat rotation(bodyRot.GetW(), bodyRot.GetX(), bodyRot.GetY(), bodyRot.GetZ());

                glm::mat4 rotationMatrix = glm::mat4_cast(rotation);
                glm::vec3 scale(1.0f, 1.0f, 1.0f); // scale vector
                glm::mat4 trs = glm::translate(glm::mat4(1.0f), translation)
                    * rotationMatrix
                    * glm::scale(glm::mat4(1.0f), scale);
                glm::mat4 modelMat = trs;
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
    // printf("[DEBUG getPhysicsObjects] Returning %zu physics objects for mesh %d\n", objects.size(), mesh);
    // fflush(stdout);
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
            return inObject2 == Layers::MOVING || inObject2 == Layers::AI || inObject2 == Layers::WHEEL;
        case Layers::MOVING:
            return inObject2 == Layers::NON_MOVING || inObject2 == Layers::AI; // MOVING doesn't collide with WHEEL
        case Layers::AI:
            return true;
        case Layers::BOUNDARY:
            return inObject2 == Layers::AI;
        case Layers::WHEEL:
            return inObject2 == Layers::NON_MOVING || inObject2 == Layers::AI; // WHEEL only collides with ground and AI, not MOVING
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
    static constexpr JPH::BroadPhaseLayer WHEEL(4);
	static constexpr JPH::uint NUM_LAYERS(5);
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
        mObjectToBroadPhase[Layers::WHEEL] = BroadPhaseLayers::WHEEL;
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
        case 4:	return "WHEEL";
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
			return true; // Allow wheels in broadphase
		 case Layers::AI:
            return true; // Allow wheels in broadphase
        case Layers::BOUNDARY:
            return inLayer2 == BroadPhaseLayers::AI; // Boundary collides with moving
        case Layers::WHEEL:
            return true; // Wheels participate in broadphase
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

    // One-time Jolt global initialization (allocator, factory, type registration)
    static bool sJoltInitialized = false;
    if (!sJoltInitialized) {
        using namespace JPH;
        RegisterDefaultAllocator();
        Trace = TraceImpl;
        JPH_IF_ENABLE_ASSERTS(AssertFailed = AssertFailedImpl;)
        if (Factory::sInstance == nullptr) {
            Factory::sInstance = new Factory();
        }
        RegisterTypes();
        sJoltInitialized = true;
    }

    // Create a new Jolt JPH::PhysicsSystem for this group if it doesn't exist
    if (worlds->worlds.find(group) == worlds->worlds.end()) {
        using namespace JPH;
        
        // We need a temp allocator for temporary allocations during the physics update. We're
        // pre-allocating 10 MB to avoid having to do allocations during the physics update.
        // B.t.w. 10 MB is way too much for this example but it is a typical value you can use.
        // If you don't want to pre-allocate you can also use TempAllocatorMalloc to fall back to
        // malloc / free.
        // Store in your world map (wrap in your World struct if needed)
        auto worldJolt = new WorldJolt();
        // Initialize job system AFTER allocator is registered

        const int hwThreads = static_cast<int>(JPH::thread::hardware_concurrency());
        int jobThreads = hwThreads > 1 ? hwThreads - 1 : 1;  // Keep at least one worker

        // Optional override via environment variable for diagnostics
        // CHAI_JOLT_THREADS: set number of worker threads (0 disables threads)
        // Example: CHAI_JOLT_THREADS=1 to minimize concurrency
    #ifdef _WIN32
        const char *envThreads = getenv("CHAI_JOLT_THREADS");
    #else
        const char *envThreads = std::getenv("CHAI_JOLT_THREADS");
    #endif
        if (envThreads && *envThreads)
        {
            int v = atoi(envThreads);
            if (v >= 0)
            {
            jobThreads = v;
            }
        }
        worldJolt->job_system = new JPH::JobSystemThreadPool(
            JPH::cMaxPhysicsJobs,
            JPH::cMaxPhysicsBarriers,
            jobThreads
        );

        // Allocate TempAllocatorImpl separately to avoid constructor heap issues
        worldJolt->temp_allocator = new JPH::TempAllocatorImpl(10 * 1024 * 1024);
        
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
        physics_system->SetGravity(JPH::Vec3(0.0f, -9.981f, 0.0f)); // Gravity pointing down the y-axis

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
        worldJolt->debug_renderer_frame_count = 0;
        worldJolt->debug_renderer_ready = false; // Will be set to true after frame delay

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
    
    if (!worlds) {
        return;
    }
    
     // Remove and delete all rigid meshes (Jolt bodies)
    for (auto &rm : rigidMeshes) {
        for (int i = 0; i < rm->group.size(); i++) {
            int group = rm->group[i];
            if (worlds->worlds.find(group) != worlds->worlds.end()) {

                auto ps = worlds->worlds[rm->group[i]]->physics_system;
                JPH::BodyInterface& bodyInterface = ps->GetBodyInterface();
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
        // Defensive: skip null or uninitialized worlds to avoid crashes
        if (!dw.second || !dw.second->physics_system) {
            printf("[Physics] Skipping null/invalid world entry in process()\n");
            continue;
        }
        
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

                    if (cc->perimeterPenetration > 0.01f || cc->perimeterPenetration < -0.01f)
                    {
                        // Push character out of penetration
                        JPH::Vec3 correctionVector = cc->collisionNormal * cc->perimeterPenetration * -1.1f;
                        JPH::Vec3 newPosition = cc->character->GetPosition() + correctionVector;

                        cc->character->SetPosition(newPosition, JPH::EActivation::Activate);
                        // printf("Character %s: Corrected penetration by %.3f units\n",
                            // cc->charId.c_str(), cc->perimeterPenetration);
                    }

                    if (abs(cc->collisionNormal.GetZ()) > 0.3f) {
                        // Z-axis collision (front/back walls)
                        if ((cc->collisionNormal.GetZ() < 0 && cc->velocityZ < 0) || (cc->collisionNormal.GetZ() > 0 && cc->velocityZ > 0)) {
                            cc->velocityZ = 0.0f;
                            cc->velocityY += 0.1f; // Small upward nudge to prevent sticking
                            // printf("Character %s: Z movement blocked (normal: %.2f, vel: %.2f)\n",
                                // cc->charId.c_str(), cc->collisionNormal.GetZ(), cc->velocityZ);
                        }
                    }

                    cc->perimeterPenetration = 0.0f;
                    cc->collisionNormal = JPH::Vec3::sZero();
                    // printf("Applying force to character %d: (%f, %f, %f)\n", cc->index, cc->velocityX, cc->velocityY, cc->velocityZ);
   
                    cc->character->SetLinearVelocity(JPH::Vec3(cc->velocityX, cc->velocityY + cc->character->GetLinearVelocity().GetY(), cc->velocityZ));
                    // cc->character->PostSimulation(cCollisionTolerance);
                    cc->collidedX = false;
                    cc->collidedZ = false;
                
            
                    // Handle character input/movement here if needed
                    // JPH::Vec3 currentVel = cc->character->GetLinearVelocity();
                    
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

        // Pre-step: Sync wheel transforms to avoid broadphase bounds mismatch
        // TEMPORARILY DISABLED: This was causing segfaults due to complex vehicle pointer access
        // Will re-enable after refactoring with proper synchronization
        /*
        {
            JPH::BodyInterface &preBodyInterface = dw.second->physics_system->GetBodyInterface();
            
            // Safety check: ensure vehicles vector isn't empty to avoid iterator issues
            if (!vehicles.empty()) {
                // Create a copy of the vehicle pointers to avoid iterator invalidation during modification
                std::vector<Vehicle*> vehicleCopy = vehicles;
                
                for (auto *vehicle : vehicleCopy) {
                    // Verify vehicle still exists in the original list
                    bool stillExists = false;
                    for (auto *v : vehicles) {
                        if (v == vehicle) {
                            stillExists = true;
                            break;
                        }
                    }
                    if (!stillExists) continue;
                    
                    if (!vehicle) {
                        printf("[Vehicle] WARNING: Null vehicle pointer in pre-step sync\n");
                        continue;
                    }
                    
                    if (vehicle->chassisBodyID.IsInvalid()) {
                        printf("[Vehicle] WARNING: Chassis body ID invalid in pre-step sync\n");
                        continue;
                    }

                    // Read chassis transform
                    JPH::RVec3 chassisPosPre = preBodyInterface.GetPosition(vehicle->chassisBodyID);
                    JPH::Quat chassisRotPre = preBodyInterface.GetRotation(vehicle->chassisBodyID);

                    if (!vehicle->constraintActive) {
                        // Free-fall phase: wheels follow chassis transform
                        for (int i = 0; i < 4; ++i) {
                            JPH::BodyID wid = vehicle->wheelBodyIDs[i];
                            if (wid.IsInvalid()) continue;
                            
                            // Check if body still exists in physics system
                            if (!preBodyInterface.IsAdded(wid)) continue;
                            
                            try {
                                JPH::Vec3 wheelRelative = chassisRotPre * vehicle->wheelLocalPos[i];
                                JPH::RVec3 wheelWorldPos = chassisPosPre + JPH::RVec3(wheelRelative.GetX(), wheelRelative.GetY(), wheelRelative.GetZ());
                                if (std::isfinite((float)wheelWorldPos.GetX()) &&
                                    std::isfinite((float)wheelWorldPos.GetY()) &&
                                    std::isfinite((float)wheelWorldPos.GetZ()) &&
                                    std::isfinite(chassisRotPre.GetX()) &&
                                    std::isfinite(chassisRotPre.GetY()) &&
                                    std::isfinite(chassisRotPre.GetZ()) &&
                                    std::isfinite(chassisRotPre.GetW())) {
                                    preBodyInterface.SetPositionAndRotation(wid, wheelWorldPos, chassisRotPre, JPH::EActivation::Activate);
                                    preBodyInterface.ActivateBody(wid);
                                }
                            } catch (...) {
                                printf("[Vehicle] WARNING: Exception in free-fall wheel sync for wheel %d\n", i);
                            }
                        }
                    } else {
                        // Active phase: use constraint-provided wheel transforms (last step state)
                        if (vehicle->vehicleConstraint == nullptr) {
                            printf("[Vehicle] WARNING: vehicleConstraint is null but constraintActive is true\n");
                            continue;
                        }
                        
                        const JPH::VehicleConstraint *constraint = vehicle->vehicleConstraint;
                        
                        for (int i = 0; i < 4; ++i) {
                            JPH::BodyID wid = vehicle->wheelBodyIDs[i];
                            if (wid.IsInvalid()) continue;
                            
                            // Check if body still exists in physics system
                            if (!preBodyInterface.IsAdded(wid)) continue;
                            
                            try {
                                JPH::RMat44 wt = constraint->GetWheelWorldTransform(i, JPH::Vec3::sAxisY(), JPH::Vec3::sAxisX());
                                JPH::RVec3 wheelPos = wt.GetTranslation();
                                JPH::Quat wheelRot = wt.GetQuaternion();
                                if (std::isfinite((float)wheelPos.GetX()) &&
                                    std::isfinite((float)wheelPos.GetY()) &&
                                    std::isfinite((float)wheelPos.GetZ()) &&
                                    std::isfinite(wheelRot.GetX()) &&
                                    std::isfinite(wheelRot.GetY()) &&
                                    std::isfinite(wheelRot.GetZ()) &&
                                    std::isfinite(wheelRot.GetW())) {
                                    preBodyInterface.SetPositionAndRotation(wid, wheelPos, wheelRot, JPH::EActivation::Activate);
                                    preBodyInterface.ActivateBody(wid);
                                }
                            } catch (...) {
                                printf("[Vehicle] WARNING: Exception updating wheel %d from constraint\n", i);
                            }
                        }
                    }
                }
            }
        }
        */

        // Step the Jolt simulation with fixed timestep
        const int maxSubSteps = 20;
        dw.second->physics_system->Update(actualDeltaTime, maxSubSteps, dw.second->temp_allocator, dw.second->job_system);

        // Update vehicle wheel transforms and suspension after physics step
        JPH::BodyInterface& bodyInterface = dw.second->physics_system->GetBodyInterface();
        std::vector<Vehicle*> brokenVehicles;  // Track vehicles to remove

        // Debug: report when no vehicles are present
        static int vehicleReportCounter = 0;
        if (vehicles.empty() && (vehicleReportCounter++ % 120 == 0)) {
            printf("[Vehicle Debug] No vehicles registered in update loop\n");
        }
        
        for (auto* vehicle : vehicles) {
            if (!vehicle) continue;  // Guard against null vehicle
            
            vehicle->frameCounter++;

            // Skip if chassis body is invalid
            if (vehicle->chassisBodyID.IsInvalid()) {
                brokenVehicles.push_back(vehicle);
                continue;
            }
            
            try {
                // Get chassis physics state
                auto isFiniteVec3 = [](const JPH::RVec3& v) {
                    return std::isfinite((float)v.GetX()) && std::isfinite((float)v.GetY()) && std::isfinite((float)v.GetZ());
                };
                auto isFiniteQuat = [](const JPH::Quat& q) {
                    return std::isfinite(q.GetX()) && std::isfinite(q.GetY()) && std::isfinite(q.GetZ()) && std::isfinite(q.GetW());
                };

                JPH::RVec3 chassisPos = bodyInterface.GetPosition(vehicle->chassisBodyID);
                JPH::Quat chassisRotChecked = bodyInterface.GetRotation(vehicle->chassisBodyID);
                JPH::Vec3 chassisVel = bodyInterface.GetLinearVelocity(vehicle->chassisBodyID);

                if (!isFiniteVec3(chassisPos) || !isFiniteQuat(chassisRotChecked)) {
                    printf("[Vehicle] ERROR: Chassis transform non-finite; removing vehicle. pos=(%f,%f,%f) rot=(%f,%f,%f,%f)\n",
                           (float)chassisPos.GetX(), (float)chassisPos.GetY(), (float)chassisPos.GetZ(),
                           chassisRotChecked.GetX(), chassisRotChecked.GetY(), chassisRotChecked.GetZ(), chassisRotChecked.GetW());
                    if (vehicle->vehicleConstraint && worlds->worlds.find(0) != worlds->worlds.end()) {
                        JPH::PhysicsSystem* physics_sys = worlds->worlds[0]->physics_system;
                        if (physics_sys) {
                            physics_sys->RemoveConstraint(vehicle->vehicleConstraint);
                            physics_sys->RemoveStepListener(vehicle->vehicleConstraint);
                        }
                    }
                    brokenVehicles.push_back(vehicle);
                    continue;
                }
                
                // Constraint is added immediately; ensure flag stays true
                vehicle->constraintActive = true;
                vehicle->activationWarmupFrames = std::max(0, vehicle->activationWarmupFrames);
                
                // === ACTIVE VEHICLE UPDATES ===
                // Constraint is active, update wheel positions and check for issues
                
                // Ensure chassis body is active (not sleeping)
                bodyInterface.ActivateBody(vehicle->chassisBodyID);
                
                // Debug output every 60 frames after activation
                if (vehicle->frameCounter % 60 == 0) {
                    JPH::Vec3 angVel = bodyInterface.GetAngularVelocity(vehicle->chassisBodyID);
                    float mass = bodyInterface.GetShape(vehicle->chassisBodyID)->GetMassProperties().mMass;
                    printf("[Vehicle] Frame %d: pos=(%.2f, %.2f, %.2f) vel=(%.2f, %.2f, %.2f) angVel=(%.2f, %.2f, %.2f) mass=%.2f\n",
                           vehicle->frameCounter, 
                           chassisPos.GetX(), chassisPos.GetY(), chassisPos.GetZ(),
                           chassisVel.GetX(), chassisVel.GetY(), chassisVel.GetZ(),
                           angVel.GetX(), angVel.GetY(), angVel.GetZ(), mass);
                }
                
                // SAFETY: If vehicle ascends too high, disable constraint
                if (chassisPos.GetY() > 50.0f) {
                    printf("[Vehicle] ERROR: Chassis at Y=%.2f (too high)! Disabling vehicle constraint.\n", chassisPos.GetY());
                    // Remove constraint to prevent further damage
                    if (vehicle->vehicleConstraint && worlds->worlds.find(0) != worlds->worlds.end()) {
                        JPH::PhysicsSystem* physics_sys = worlds->worlds[0]->physics_system;
                        if (physics_sys) {
                            physics_sys->RemoveConstraint(vehicle->vehicleConstraint);
                            physics_sys->RemoveStepListener(vehicle->vehicleConstraint);
                        }
                        vehicle->constraintActive = false;
                        vehicle->settledFrames = 0;
                    }
                    continue;
                }
                
                // Simple post-step wheel sync from constraint (safe, after physics step complete)
                // With fixed constraints attached, let Jolt manage wheel motion
                if (vehicle->vehicleConstraint != nullptr) {
                    try {
                        // The FixedConstraints and VehicleConstraint handle all wheel physics
                        // Just keep wheels active for rendering
                        for (int i = 0; i < 4; ++i) {
                            JPH::BodyID wid = vehicle->wheelBodyIDs[i];
                            if (wid.IsInvalid()) continue;
                            if (!bodyInterface.IsAdded(wid)) continue;
                            
                            // Keep wheels active
                            bodyInterface.ActivateBody(wid);
                        }
                        if (vehicle->activationWarmupFrames > 0) vehicle->activationWarmupFrames--;
                    } catch (...) {
                        // Silently ignore errors
                    }
                }
            } catch (...) {
                // Silently ignore vehicle update errors
            }
        }
        
        // Remove broken vehicles
        for (auto* broken : brokenVehicles) {
            auto it = std::find(vehicles.begin(), vehicles.end(), broken);
            if (it != vehicles.end()) {
                delete broken;
                vehicles.erase(it);
            }
        }

        // Process contact events less frequently
        static int contactCounter = 0;
        // if (contactCounter++ % 3 == 0) { // Every 3rd frame
            if (dw.second->contact_listener) {
                dw.second->contact_listener->ProcessPendingEvents(dw.second->physics_system);
            }
        // }

        // Update characters AFTER physics step (reduced frequency)
        // if (characterUpdateCounter % 2 == 0) {
            // for (auto &cc : characterControllers) {
            //     if (cc->character) {
            //         const float cCollisionTolerance = 0.05f; 
            //         cc->character->PostSimulation(cCollisionTolerance);
            //     }
            // }
        // }

        // Debug rendering at reduced frequency
        processDebugRendering(dw.second);
    }
}

void chai_collisions::processDebugRendering(WorldJolt* world)
{
    // If debug renderer is compiled out, do nothing
#ifndef JPH_DEBUG_RENDERER
    (void)world;
    return;
#else
    // Early exit if world is invalid
    if (!world) {
        return;
    }
    
    // Early exit if physics system is invalid
    if (!world->physics_system) {
        return;
    }
    
    // Early exit if debug renderer is not set
    if (!world->debug_renderer) {
        return;
    }
    
    // Increment frame counter for initialization tracking
    world->debug_renderer_frame_count++;
    
    // CRITICAL FIX: Wait 5 frames before first DrawBodies call to ensure proper initialization
    // This prevents crashes that occur when DrawBodies is called before the renderer is ready
    const uint32_t INITIALIZATION_FRAME_DELAY = 5;
    if (world->debug_renderer_frame_count < INITIALIZATION_FRAME_DELAY) {
        world->debug_renderer_ready = false;
        return;
    }
    
    // Mark debug renderer as ready after initialization frames
    if (world->debug_renderer_frame_count == INITIALIZATION_FRAME_DELAY) {
        world->debug_renderer_ready = true;
        printf("[COLLISION DEBUG] Debug renderer initialized and ready after %u frames\n", 
               INITIALIZATION_FRAME_DELAY);
        fflush(stdout);
    }
    
    // Only proceed if renderer is fully initialized
    if (!world->debug_renderer_ready) {
        return;
    }
#endif // JPH_DEBUG_RENDERER
    
    // Verify debug renderer cast and render (only when enabled)
#ifdef JPH_DEBUG_RENDERER
    auto debugRenderer = static_cast<DebugRendererImpl*>(world->debug_renderer);
    if (!debugRenderer) {
        printf("[COLLISION DEBUG] Failed to cast debug renderer to DebugRendererImpl\n");
        fflush(stdout);
        return;
    }
    
    try {
        // Clear previous frame's geometry
        debugRenderer->Clear();
        
        // Configure draw settings to reduce complexity
        JPH::BodyManager::DrawSettings settings;
        settings.mDrawGetSupportFunction = false;      // Disabled for performance
        settings.mDrawSupportDirection = false;        // Disabled for performance
        settings.mDrawGetSupportingFace = false;       // Disabled for performance
        settings.mDrawShape = true;                    // Draw collision shapes
        settings.mDrawShapeWireframe = true;           // Draw wireframe
        
        // Call DrawBodies to collect geometry into our renderer
        // This is the critical call that was crashing - it should be safe now with proper initialization
        world->physics_system->DrawBodies(settings, world->debug_renderer);
        
        // Log statistics occasionally for debugging
        static int debugPrintCounter = 0;
        if (++debugPrintCounter % 180 == 0) { // Every 3 seconds at 60fps
            size_t lineCount = debugRenderer->GetLines().size();
            size_t triCount = debugRenderer->GetTriangles().size();

            printf("[COLLISION DEBUG] Geometry: %zu lines, %zu triangles\n", lineCount, triCount);
            fflush(stdout);
        }
        
    } catch (const std::exception& ex) {
        printf("[COLLISION DEBUG] Exception in DrawBodies: %s\n", ex.what());
        fflush(stdout);
    } catch (...) {
        printf("[COLLISION DEBUG] Unknown exception in DrawBodies\n");
        fflush(stdout);
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
    // static glm::mat4 cachedProjection;
    // static glm::mat4 cachedView;
    // static int cameraUpdateCounter = 0;
    
    // // Only update camera matrices every 30 frames
    // if (cameraUpdateCounter++ % 30 == 0) {
    //     JPH::Vec3 boundsCenter = (worldBounds.mMin + worldBounds.mMax) * 0.5f;
    //     JPH::Vec3 boundsSize = worldBounds.mMax - worldBounds.mMin;
        
    //     float maxDimension = std::max(boundsSize.GetX(), std::max(boundsSize.GetY(), boundsSize.GetZ()));
    //     float fov = 45.0f;
    //     float aspectRatio = 16.0f / 9.0f;
    //     float distance = maxDimension / (2.0f * tan(glm::radians(fov) / 2.0f)) * 1.5f / 5.0f;

    //     glm::vec3 cameraPos = glm::vec3(
    //         boundsCenter.GetX() + distance * 0.7f,
    //         boundsCenter.GetY() + distance * 0.5f,
    //         boundsCenter.GetZ() + distance
    //     );
        
    //     glm::vec3 target = glm::vec3(boundsCenter.GetX(), boundsCenter.GetY(), boundsCenter.GetZ());
    //     glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
        
    //     cachedProjection = glm::perspective(glm::radians(fov), aspectRatio, 0.1f, distance * 3.0f);
    //     cachedView = glm::lookAt(cameraPos, target, up);
    // }
    
    // // Apply cached matrices
    // glMatrixMode(GL_PROJECTION);
    // glLoadMatrixf(glm::value_ptr(cachedProjection));
    
    // glMatrixMode(GL_MODELVIEW);
    // glLoadMatrixf(glm::value_ptr(cachedView));
}
} // namespace love