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
#include <Jolt/Physics/Character/CharacterVirtual.h>
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
#include <Jolt/Physics/Vehicle/VehicleConstraint.h>
#include <Jolt/Physics/Vehicle/WheeledVehicleController.h>
#ifdef JPH_DEBUG_RENDERER
#include <Jolt/Renderer/DebugRendererSimple.h>

// Forward declaration of DebugRendererImpl
class DebugRendererImpl;
#endif

#include <vector>
#include <future>
#ifndef __HAVE_CHAI_MESH__
#include "chai_mesh.h"
#endif

#include "glm/glm.hpp"

namespace love
{
#ifdef JPH_DEBUG_RENDERER
// Vulkan-compatible debug renderer that collects geometry for rendering
class DebugRendererImpl : public JPH::DebugRendererSimple
{
public:
    struct Vertex {
        float x, y, z;      // Position
        float r, g, b, a;   // Color
    };
    
    struct LineSegment {
        Vertex from;
        Vertex to;
    };
    
    struct Triangle {
        Vertex v1;
        Vertex v2;
        Vertex v3;
    };
    
    std::vector<LineSegment> lines;
    std::vector<Triangle> triangles;
    
    DebugRendererImpl() : JPH::DebugRendererSimple(), lines(), triangles() {}
    virtual ~DebugRendererImpl() override
    {
        lines.clear();
        triangles.clear();
    }
    
    // Override all required virtual methods from JPH::DebugRendererSimple
    virtual void DrawLine(JPH::RVec3Arg inFrom, JPH::RVec3Arg inTo, JPH::ColorArg inColor) override
    {
        // Guard against nullptr or uninitialized state
        if (!this) return;
        
        try {
            Vertex from, to;
            from.x = (float)inFrom.GetX();
            from.y = (float)inFrom.GetY();
            from.z = (float)inFrom.GetZ();
            from.r = inColor.r;
            from.g = inColor.g;
            from.b = inColor.b;
            from.a = inColor.a;
            
            to.x = (float)inTo.GetX();
            to.y = (float)inTo.GetY();
            to.z = (float)inTo.GetZ();
            to.r = inColor.r;
            to.g = inColor.g;
            to.b = inColor.b;
            to.a = inColor.a;
            
            lines.push_back({from, to});
        } catch (...) {
            // Silently ignore exceptions in DrawLine
        }
    }

    virtual void DrawTriangle(JPH::RVec3Arg inV1, JPH::RVec3Arg inV2, JPH::RVec3Arg inV3, JPH::ColorArg inColor, ECastShadow inCastShadow) override
    {
        // Guard against nullptr or uninitialized state
        if (!this) return;
        
        try {
            Vertex v1, v2, v3;
            
            v1.x = (float)inV1.GetX();
            v1.y = (float)inV1.GetY();
            v1.z = (float)inV1.GetZ();
            v1.r = inColor.r;
            v1.g = inColor.g;
            v1.b = inColor.b;
            v1.a = inColor.a;
            
            v2.x = (float)inV2.GetX();
            v2.y = (float)inV2.GetY();
            v2.z = (float)inV2.GetZ();
            v2.r = inColor.r;
            v2.g = inColor.g;
            v2.b = inColor.b;
            v2.a = inColor.a;
            
            v3.x = (float)inV3.GetX();
            v3.y = (float)inV3.GetY();
            v3.z = (float)inV3.GetZ();
            v3.r = inColor.r;
            v3.g = inColor.g;
            v3.b = inColor.b;
            v3.a = inColor.a;
            
            triangles.push_back({v1, v2, v3});
        } catch (...) {
            // Silently ignore exceptions in DrawTriangle
        }
    }

    virtual void DrawText3D(JPH::RVec3Arg inPosition, const std::string_view& inString, JPH::ColorArg inColor, float inHeight = 0.5f) override
    {
        // Text rendering is skipped in Vulkan debug renderer
        // This is optional, so we provide a no-op implementation
    }

    const std::vector<LineSegment>& GetLines() const { return lines; }
    const std::vector<Triangle>& GetTriangles() const { return triangles; }
    void Clear() { lines.clear(); triangles.clear(); }
    size_t GetLineVertexCount() const { return lines.size() * 2; }
    size_t GetTriangleVertexCount() const { return triangles.size() * 3; }
    
    void GetLineVertexBuffer(std::vector<float>& outBuffer) const
    {
        outBuffer.clear();
        outBuffer.reserve(lines.size() * 2 * 7);
        for (const auto& line : lines) {
            outBuffer.push_back(line.from.x);
            outBuffer.push_back(line.from.y);
            outBuffer.push_back(line.from.z);
            outBuffer.push_back(line.from.r);
            outBuffer.push_back(line.from.g);
            outBuffer.push_back(line.from.b);
            outBuffer.push_back(line.from.a);
            outBuffer.push_back(line.to.x);
            outBuffer.push_back(line.to.y);
            outBuffer.push_back(line.to.z);
            outBuffer.push_back(line.to.r);
            outBuffer.push_back(line.to.g);
            outBuffer.push_back(line.to.b);
            outBuffer.push_back(line.to.a);
        }
    }
    
    void GetTriangleVertexBuffer(std::vector<float>& outBuffer) const
    {
        outBuffer.clear();
        outBuffer.reserve(triangles.size() * 3 * 7);
        for (const auto& tri : triangles) {
            outBuffer.push_back(tri.v1.x);
            outBuffer.push_back(tri.v1.y);
            outBuffer.push_back(tri.v1.z);
            outBuffer.push_back(tri.v1.r);
            outBuffer.push_back(tri.v1.g);
            outBuffer.push_back(tri.v1.b);
            outBuffer.push_back(tri.v1.a);
            outBuffer.push_back(tri.v2.x);
            outBuffer.push_back(tri.v2.y);
            outBuffer.push_back(tri.v2.z);
            outBuffer.push_back(tri.v2.r);
            outBuffer.push_back(tri.v2.g);
            outBuffer.push_back(tri.v2.b);
            outBuffer.push_back(tri.v2.a);
            outBuffer.push_back(tri.v3.x);
            outBuffer.push_back(tri.v3.y);
            outBuffer.push_back(tri.v3.z);
            outBuffer.push_back(tri.v3.r);
            outBuffer.push_back(tri.v3.g);
            outBuffer.push_back(tri.v3.b);
            outBuffer.push_back(tri.v3.a);
        }
    }
};
#endif

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
    std::vector<int> addRigidMesh(std::string meshPath, int mesh, bool makeConvex, bool makeShape);
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
    std::vector<Matrix4> getPhysicsObjects(int mesh, std::vector<float> scale);
    int portalCollide(int index);
    void teleportCharacter(int characterIndex, float x, float y, float z);
    void teleportRigidMesh(std::vector<int> rigidMeshIndex, float x, float y, float z);
    void drawWireframeBox(const JPH::Vec3& min, const JPH::Vec3& max);
    void drawPhysicsDebug();
    void createVehicle(int frontLeftWheelMeshRef, int frontRightWheelMeshRef, int rearLeftWheelMeshRef, int rearRightWheelMeshRef, int chassisMeshRef, float mass, float wheelRadius, float wheelWidth, float suspensionRestLength, float suspensionStiffness, float suspensionDamping, float suspensionCompression, float frictionSlip, float maxSuspensionTravelCm, float maxSuspensionForce, float scaleX, float scaleY, float scaleZ);

    void clearWorlds()
    {
        worlds = nullptr;
    }

    void debugDraw();
    std::vector<float> getVehicleBoneTransform(int vehicleIndex, const std::string& boneName);
    uint8_t* buffer = nullptr;
     
    class CharacterController
    {
        public:
        JPH::BodyID bodyID;
        JPH::Character* character; // Add this to store the Character
        std::vector<int> meshRef;
        std::string charId;
        std::vector<int> group;
        int index = 0; // Index to identify the character controller
        bool collidedX = false;
        bool collidedZ = false;
        float perimeterPenetration = 0.0f; // Amount of penetration into walls
        float velocityX = 0.0f; // Horizontal velocity
        float velocityZ = 0.0f; // Horizontal velocity
        float velocityY = 0.0f; // Vertical velocity for jumping and gravity
        JPH::CharacterVirtual* characterVirtual = nullptr; // Pointer to the CharacterVirtual instance
        JPH::Vec3 collisionNormal = JPH::Vec3::sZero(); // Store the collision normal

        CharacterController(JPH::BodyID id, int mesh, std::string cId, JPH::Character* c, int cIndex = 0) 
            : bodyID(id), meshRef({mesh}), charId(cId), character(c), index(cIndex) {}

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
        RigidMesh(JPH::BodyID bodyID, int meshRef = 0, JPH::Ref<JPH::Shape> shape = nullptr, JPH::Vec3 centroid = JPH::Vec3::sZero(),
                  glm::vec3 nodePos = glm::vec3(0), glm::quat nodeRot = glm::quat(1,0,0,0))
        {
            this->bodyID = bodyID;
            this->group = {};
            this->meshRef = meshRef;
            this->shape = shape;
            this->centroid = centroid;
            this->nodePosition = nodePos;
            this->nodeRotation = nodeRot;
        }
        ~RigidMesh()
        {
            group.clear();
        }
        JPH::BodyID bodyID;
        std::vector<int> group;
        int meshRef;
        JPH::Ref<JPH::Shape> shape;  // Store shape for vehicle creation
        JPH::Vec3 centroid;  // Store mesh centroid for position calculations
        glm::vec3 nodePosition;  // GLTF node translation
        glm::quat nodeRotation;  // GLTF node rotation
        glm::vec3 nodeScale;  // GLTF node scale
        JPH::Vec3 com;  // Store original center of mass before geometry recentering
    };

    class Vehicle
    {
        public:
        Vehicle(JPH::VehicleConstraint* constraint, JPH::BodyID chassisID, const JPH::BodyID wheelIDs[4], const JPH::Vec3 wheelLocalPositions[4],
                float suspRestLen = 0.5f, float suspStiffness = 100.0f, float suspDamping = 5.0f, float wheelRadius = 0.3f)
            : vehicleConstraint(constraint), chassisBodyID(chassisID), frameCounter(0),
              suspensionRestLength(suspRestLen), suspensionStiffness(suspStiffness), suspensionDamping(suspDamping), wheelRadius(wheelRadius),
              constraintActive(false), settledFrames(0), restVelocityThreshold(0.5f), framesUntilActivation(60), activationWarmupFrames(5), rigidSuspension(false),
              chassisMeshRef(-1)
        {
            for (int i = 0; i < 4; i++) {
                wheelBodyIDs[i] = wheelIDs[i];
                wheelLocalPos[i] = wheelLocalPositions[i];
                wheelVelocity[i] = JPH::Vec3::sZero();  // Track wheel velocity for damping
                wheelCOMOffsets[i] = JPH::Vec3::sZero();  // Initialize COM offsets
            }
            lastVelocity = JPH::Vec3::sZero();
        }
        ~Vehicle()
        {
            // Note: VehicleConstraint is owned by PhysicsSystem and cleaned up there
        }
        JPH::VehicleConstraint* vehicleConstraint;
        JPH::BodyID chassisBodyID;
        JPH::BodyID wheelBodyIDs[4];
        JPH::Vec3 wheelLocalPos[4];  // Local position of each wheel relative to chassis
        JPH::Vec3 wheelVelocity[4];  // Track wheel contact velocity for suspension damping
        JPH::Vec3 wheelCOMOffsets[4];  // Original COM offset for each wheel (for correcting visual orbiting)
        int chassisMeshRef;  // Reference to the chassis mesh for querying armature bones
        chai_mesh* chassisMeshPtr = nullptr;  // Direct pointer to chassis mesh for skeleton data
        int frameCounter;
        
        // Armature bone override transforms (for skeletal animation control from physics)
        struct BoneTransform {
            JPH::RVec3 position;
            JPH::Quat rotation;
            bool active;
            BoneTransform() : position(JPH::RVec3::sZero()), rotation(JPH::Quat::sIdentity()), active(false) {}
        };
        BoneTransform chassisBoneTransform;
        BoneTransform wheelBoneTransforms[4];
        
        // Suspension parameters
        float suspensionRestLength;
        float suspensionStiffness;
        float suspensionDamping;
        float wheelRadius;

        // Suspension mode
        bool rigidSuspension;             // When true, lock wheel to chassis offset
        
        // Constraint activation tracking
        bool constraintActive;              // Whether constraint has been added to physics system
        int settledFrames;                  // Number of consecutive frames chassis has been at rest
        float restVelocityThreshold;        // Velocity threshold to consider chassis "at rest" (m/s)
        int framesUntilActivation;          // Number of settled frames required before activation
        JPH::Vec3 lastVelocity;             // Last frame's velocity for rest detection
        int activationWarmupFrames;         // Frames to use chassis-based wheel sync after activation
    };

    class MyContactListener;
    class CharacterContactListener;

    class WorldJolt
    {
        public:
        WorldJolt()
            : job_system(nullptr), temp_allocator(nullptr)
        {
        }
        ~WorldJolt()
        {
            if (job_system != nullptr) {
                delete job_system;
                job_system = nullptr;
            }
            if (temp_allocator != nullptr) {
                delete temp_allocator;
                temp_allocator = nullptr;
            }
        }
        JPH::PhysicsSystem* physics_system = nullptr;
        JPH::BroadPhaseLayerInterface* broad_phase_layer_interface = nullptr;
        JPH::ObjectVsBroadPhaseLayerFilter* object_vs_broadphase_layer_filter = nullptr;
        JPH::ObjectLayerPairFilter* object_vs_object_layer_filter = nullptr;
        JPH::TempAllocatorImpl* temp_allocator = nullptr;  // Defer 10MB allocation
        JPH::JobSystemThreadPool* job_system = nullptr;  // Defer initialization until after allocator is set up
        MyContactListener* contact_listener = nullptr;
        CharacterContactListener* character_contact_listener = nullptr;
        #ifdef JPH_DEBUG_RENDERER
            JPH::DebugRendererSimple* debug_renderer = nullptr; // Reference to global instance, don't delete in destructor
            uint32_t debug_renderer_frame_count = 0; // Track frames since debug renderer creation
            bool debug_renderer_ready = false; // Flag to indicate debug renderer is fully initialized
        #endif
    };
    class WorldMap
    {
        public:
        WorldMap()
        {
            // Default constructor for std::map is sufficient
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
    // Map to store DEF-Wheel bone positions from chassis meshes: meshRef -> [4 wheel positions]
    std::map<int, std::vector<glm::vec3>> chassisWheelBones;
    // std::vector<btRigidBody *> cameraBox;
    // std::vector<btRigidBody *> portalBox;
    std::vector<CharacterController *> characterControllers;
    std::vector<Vehicle *> vehicles;
    WorldMap *worlds = nullptr;    

    void setProcessFrequency(float fps) { m_processInterval = 1.0f / fps; }
    void setDebugFrequency(float fps) { m_debugInterval = 1.0f / fps; }
    void processDebugRendering(WorldJolt* world);
    bool calculateWorldBounds(JPH::AABox& worldBounds, WorldJolt* world);    
    void setupDebugCamera(const JPH::AABox& worldBounds);

    #ifdef JPH_DEBUG_RENDERER
    // Libretro integration functions for debug renderer
    std::vector<float> getDebugRendererLineVertices(int worldGroup);
    std::vector<float> getDebugRendererTriangleVertices(int worldGroup);
    size_t getDebugRendererLineCount(int worldGroup);
    size_t getDebugRendererTriangleCount(int worldGroup);
    void clearDebugRendererGeometry(int worldGroup);
    #endif

    private:
    float m_lastProcessTime = 0.0f;
    float m_processInterval = 1.0f / 60.0f; // Process at 60 FPS max
    float m_lastDebugTime = 0.0f;
    float m_debugInterval = 1.0f / 30.0f;   // Debug at 30 FPS max
    bool m_needsUpdate = true;
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