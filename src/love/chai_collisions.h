#define __HAVE_CHAI_COLLISIONS__
#include "../../vendor/bullet3/src/btBulletDynamicsCommon.h"
#include "../../vendor/bullet3/src/BulletCollision/CollisionDispatch/btGhostObject.h"
#include "../../vendor/bullet3/src/BulletDynamics/Character/btKinematicCharacterController.h"
#include <vector>

 #include <LinearMath/btIDebugDraw.h>

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
    void process();
    std::vector<int> addRigidMesh(std::string meshPath);
    void setCharacterControllerPosition(int characterIndex, float x, float y, float z, std::vector<int> group);
    void setRigidMeshPosition(std::vector<int> rigidMeshIndex, float x, float y, float z, std::vector<int> group);
    int addCharacterController();
    void applyForceToCharacter(int characterIndex, float x, float y, float z);
    void applyForceToRigidMesh(int rigidMeshIndex, float x, float y, float z);
    std::vector<float> getCharacterController(int ref);
    std::vector<float> getRigidMesh(int ref);
    int addBox(float x, float y, float z, float width, float height, float depth, std::vector<int> group);
    
    void clearWorlds()
    {
        worlds = nullptr;
    }

    private:
    class CharacterController
    {
        public:
        CharacterController(btPairCachingGhostObject *ghostObject = nullptr, btKinematicCharacterController *character = nullptr)
        {
            this->ghostObject = ghostObject;
            this->character = character;
            this->group = {};
        }
        ~CharacterController()
        {
            if (ghostObject != nullptr)
            {
                delete ghostObject;
            }
            if (character != nullptr)
            {
                delete character;
            }
            group.clear();
        }
        btPairCachingGhostObject *ghostObject;
        btKinematicCharacterController *character;
        std::vector<int> group;
    };
    class RigidMesh
    {
        public:
        RigidMesh(btTriangleMesh *mesh = nullptr, btRigidBody *rigidBody = nullptr)
        {
            this->mesh = mesh;
            this->rigidBody = rigidBody;
            this->group = {};
        }
        RigidMesh(btBoxShape *box = nullptr, btRigidBody *rigidBody = nullptr)
        {
            this->box = box;
            this->rigidBody = rigidBody;
            this->group = {};
        }
        ~RigidMesh()
        {
            if (mesh != nullptr)
            {
                delete mesh;
            }
            if (rigidBody != nullptr)
            {
                delete rigidBody;
            }
            group.clear();
        }
        btTriangleMesh *mesh;
        btBoxShape *box;
        btRigidBody *rigidBody;
        std::vector<int> group;
    };
    class World
    {
        public:
        World(btBroadphaseInterface *broadphase = nullptr, btDefaultCollisionConfiguration *collisionConfiguration = nullptr, btCollisionDispatcher *dispatcher = nullptr, btSequentialImpulseConstraintSolver *solver = nullptr, btDiscreteDynamicsWorld *dynamicsWorld = nullptr)
        {
            this->broadphase = broadphase;
            this->collisionConfiguration = collisionConfiguration;
            this->dispatcher = dispatcher;
            this->solver = solver;
            this->dynamicsWorld = dynamicsWorld;
        }
        ~World()
        {
            if (broadphase != nullptr)
            {
                delete broadphase;
            }
            if (collisionConfiguration != nullptr)
            {
                delete collisionConfiguration;
            }
            if (dispatcher != nullptr)
            {
                delete dispatcher;
            }
            if (solver != nullptr)
            {
                delete solver;
            }
            if (dynamicsWorld != nullptr)
            {
                delete dynamicsWorld;
            }
        }
        btBroadphaseInterface *broadphase;
        btDefaultCollisionConfiguration *collisionConfiguration;
        btCollisionDispatcher *dispatcher;
        btSequentialImpulseConstraintSolver *solver;
        btDiscreteDynamicsWorld *dynamicsWorld;
    };
    class WorldMap
    {
        public:
        WorldMap()
        {
            worlds = std::map<int, World *>();
        }
        ~WorldMap()
        {
            for (auto &dw : worlds)
            {
                delete dw.second;
            }
            worlds.clear();
        }
        std::map<int, World *> worlds;
    };
    std::vector<RigidMesh *> rigidMeshes;
    std::vector<CharacterController *> characterControllers;    
    WorldMap *worlds = nullptr;    

    class OpenGLDebugDrawer : public btIDebugDraw {
        private:
            int debugMode;

        public:
            OpenGLDebugDrawer() : debugMode(DBG_DrawWireframe) {}

            void drawLine(const btVector3 &from, const btVector3 &to, const btVector3 &color) override {
                glColor3f(color.getX(), color.getY(), color.getZ());
                glBegin(GL_LINES);
                glVertex3f(from.getX(), from.getY(), from.getZ());
                glVertex3f(to.getX(), to.getY(), to.getZ());
                glEnd();
            }

            void drawContactPoint(const btVector3 &pointOnB, const btVector3 &normalOnB, btScalar distance, int lifeTime, const btVector3 &color) override {
                glColor3f(color.getX(), color.getY(), color.getZ());
                glBegin(GL_POINTS);
                glVertex3f(pointOnB.getX(), pointOnB.getY(), pointOnB.getZ());
                glEnd();

                btVector3 to = pointOnB + normalOnB * distance;
                drawLine(pointOnB, to, color);
            }

            void drawTriangle(const btVector3 &v0, const btVector3 &v1, const btVector3 &v2, const btVector3 &color, btScalar alpha) override {
                glColor4f(color.getX(), color.getY(), color.getZ(), alpha);
                // Set triangle line thickness if needed
                glLineWidth(1.0f);
                glBegin(GL_LINE_LOOP);
                glVertex3f(v0.getX(), v0.getY(), v0.getZ());
                glVertex3f(v1.getX(), v1.getY(), v1.getZ());
                glVertex3f(v2.getX(), v2.getY(), v2.getZ());
                glEnd();
            }

            void reportErrorWarning(const char *warningString) override {
                printf("Bullet Debug Warning: %s\n", warningString);
            }

            void draw3dText(const btVector3 &location, const char *textString) override {
                // Optional: Implement 3D text rendering if needed
            }

            void setDebugMode(int debugMode) override {
                this->debugMode = debugMode;
            }

            int getDebugMode() const override {
                return debugMode;
            }

            // Set up the camera
            glm::vec3 cameraPosition = glm::vec3(0.0f, 5.0f, 10.0f); // Camera position
            glm::vec3 cameraTarget = glm::vec3(0.0f, 0.0f, 0.0f);    // Look at the origin
            glm::vec3 upVector = glm::vec3(0.0f, 1.0f, 0.0f);        // Up vector
            float fov = 45.0f;                           // Field of view
            float aspectRatio = 16.0f / 9.0f;            // Aspect ratio
            float nearPlane = 0.1f;                      // Near clipping plane
            float farPlane = 100.0f;
        };
        OpenGLDebugDrawer *debugDrawer = nullptr;
    };

} // namespace love