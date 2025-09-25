#define __HAVE_CHAI_COLLISIONS__
#include "../../vendor/jolt/Jolt/Jolt.h"
// Jolt includes
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyManager.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Character/Character.h>
#include <Jolt/Physics/Character/CharacterBase.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/HeightFieldShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/ScaledShape.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhase.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayerInterfaceMask.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayerInterfaceTable.h>
#ifdef JPH_DEBUG_RENDERER
#include <Jolt/Renderer/DebugRendererSimple.h>
#endif

#include <vector>
#include <future>
#ifndef __HAVE_CHAI_MESH__
#include "chai_mesh.h"
#endif

#include "glm/glm.hpp"


namespace love
{
class chai_collisions
{
    public:
    chai_collisions();
    ~chai_collisions();
    void enableDebugDrawer(); // Optional: Add a method to enable the debug drawer
    void test();
    void init(int group);
    void destroy();
    uint8_t* processDebug(float deltaTime, std::vector<chaiscript::Boxed_Value> viewMatrix);
    void process(float deltaTime);
    std::vector<int> addRigidMesh(std::string meshPath, int mesh, bool makeConvex, bool ragdoll);
    void setCharacterControllerPosition(int characterIndex, float x, float y, float z, std::vector<int> group);
    void setRigidMeshPosition(std::vector<int> rigidMeshIndex, float x, float y, float z, std::vector<int> group);
    void togglePhysics(std::vector<int> rigidMeshIndex, bool enable);
    int addCharacterController(int index, int meshRef, std::string charId);
    void applyForceToCharacter(int characterIndex, float x, float y, float z);
    void applyForceToRigidMesh(int rigidMeshIndex, float x, float y, float z);
    std::vector<float> getCharacterController(int ref);
    std::vector<float> getRigidMesh(int ref);
    int addBox(float x, float y, float z, float width, float height, float depth, std::vector<int> group, int index);
    std::vector<std::pair<glm::vec3, glm::vec3>> getBoundingBox(int mesh);
    std::vector<Matrix4> getPhysicsObjects(int mesh);
    int portalCollide(int index);
    void teleportCharacter(int characterIndex, float x, float y, float z);
    void teleportRigidMesh(std::vector<int> rigidMeshIndex, float x, float y, float z);
    void drawWireframeBox(const JPH::Vec3& min, const JPH::Vec3& max);
    void drawPhysicsDebug();

    void clearWorlds()
    {
        worlds = nullptr;
    }

    void debugDraw();
    uint8_t* buffer = nullptr;

    private:    
    class CharacterController
    {
        public:
        JPH::BodyID bodyID;
        JPH::Character* character; // Add this to store the Character
        std::vector<int> meshRef;
        std::string charId;
        std::vector<int> group;
        
        CharacterController(JPH::BodyID id, int mesh, std::string cId, JPH::Character* c) 
            : bodyID(id), meshRef({mesh}), charId(cId), character(c) {}
            
        ~CharacterController() {
            if (character) {
                character->RemoveFromPhysicsSystem();
                delete character;
            }
        }
            
        void addMesh(int mesh) {
            meshRef.push_back(mesh);
        }
    };
    class RigidMesh
    {
        public:
        RigidMesh(JPH::BodyID bodyID, int meshRef = 0)
        {
            this->bodyID = bodyID;
            this->group = {};
            this->meshRef = meshRef;
        }
        ~RigidMesh()
        {
            group.clear();
        }
        JPH::BodyID bodyID;
        std::vector<int> group;
        int meshRef;
    };
    class WorldJolt
    {
        public:
        WorldJolt()
        {
        }
        ~WorldJolt()
        {            
        }
        JPH::PhysicsSystem* physics_system = nullptr;
        JPH::BroadPhaseLayerInterface* broad_phase_layer_interface = nullptr;
        JPH::ObjectVsBroadPhaseLayerFilter* object_vs_broadphase_layer_filter = nullptr;
        JPH::ObjectLayerPairFilter* object_vs_object_layer_filter = nullptr;
        JPH::TempAllocatorImpl temp_allocator{ 10 * 1024 * 1024 };
        JPH::JobSystemThreadPool job_system{ JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, JPH::thread::hardware_concurrency() - 1 };
        #ifdef JPH_DEBUG_RENDERER
            JPH::DebugRendererSimple* debug_renderer = nullptr; // Reference to global instance, don't delete in destructor
        #endif
    };
    class WorldMap
    {
        public:
        WorldMap()
        {
            worlds = std::map<int, WorldJolt*>();
        }
        ~WorldMap()
        {
            for (auto& dw : worlds)
            {
                delete dw.second;
            }
            worlds.clear();
        }
        std::map<int, WorldJolt*> worlds;
    };
    std::vector<RigidMesh *> rigidMeshes;
    // std::vector<btRigidBody *> cameraBox;
    // std::vector<btRigidBody *> portalBox;
    std::vector<CharacterController *> characterControllers;    
    WorldMap *worlds = nullptr;    

    // class OpenGLDebugDrawer : public btIDebugDraw {
    //     private:
    //         int debugMode;

    //     public:
    //         OpenGLDebugDrawer() : debugMode(DBG_DrawWireframe) {}

    //         void drawLine(const btVector3 &from, const btVector3 &to, const btVector3 &color) override {
    //             // glColor3f(color.getX(), color.getY(), color.getZ());
    //             // glBegin(GL_LINES);
    //             // glVertex3f(from.getX(), from.getY(), from.getZ());
    //             // glVertex3f(to.getX(), to.getY(), to.getZ());
    //             // glEnd();
    //         }

    //         void drawContactPoint(const btVector3 &pointOnB, const btVector3 &normalOnB, btScalar distance, int lifeTime, const btVector3 &color) override {
    //             // glColor3f(color.getX(), color.getY(), color.getZ());
    //             // glBegin(GL_POINTS);
    //             // glVertex3f(pointOnB.getX(), pointOnB.getY(), pointOnB.getZ());
    //             // glEnd();

    //             btVector3 to = pointOnB + normalOnB * distance;
    //             drawLine(pointOnB, to, color);
    //         }

    //         void drawTriangle(const btVector3 &v0, const btVector3 &v1, const btVector3 &v2, const btVector3 &color, btScalar alpha) override {
    //             // glColor4f(color.getX(), color.getY(), color.getZ(), alpha);
    //             // Set triangle line thickness if needed
    //             glLineWidth(1.0f);
    //             // glBegin(GL_LINE_LOOP);
    //             // glVertex3f(v0.getX(), v0.getY(), v0.getZ());
    //             // glVertex3f(v1.getX(), v1.getY(), v1.getZ());
    //             // glVertex3f(v2.getX(), v2.getY(), v2.getZ());
    //             // glEnd();
    //         }

    //         void reportErrorWarning(const char *warningString) override {
    //             printf("Bullet Debug Warning: %s\n", warningString);
    //         }

    //         void draw3dText(const btVector3 &location, const char *textString) override {
    //             // Optional: Implement 3D text rendering if needed
    //         }

    //         void setDebugMode(int debugMode) override {
    //             this->debugMode = debugMode;
    //         }

    //         int getDebugMode() const override {
    //             return debugMode;
    //         }

    //         // Set up the camera
    //         glm::vec3 cameraPosition = glm::vec3(0.0f, 5.0f, 10.0f); // Camera position
    //         glm::vec3 cameraTarget = glm::vec3(0.0f, 0.0f, 0.0f);    // Look at the origin
    //         glm::vec3 upVector = glm::vec3(0.0f, 1.0f, 0.0f);        // Up vector
    //         float fov = 26.0f;                           // Field of view
    //         float aspectRatio = 16.0f / 9.0f;            // Aspect ratio
    //         float nearPlane = 0.1f;                      // Near clipping plane
    //         float farPlane = 100.0f;

    //         GLuint debugTexture = 0;
    //         GLuint debugFBO = 0;
    //     };
    //     OpenGLDebugDrawer *debugDrawer = nullptr;
    // };
};
} // namespace love