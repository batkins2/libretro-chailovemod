#define __HAVE_CHAI_COLLISIONS__
#include "../../vendor/bullet3/src/btBulletDynamicsCommon.h"
#include "../../vendor/bullet3/src/BulletCollision/CollisionDispatch/btGhostObject.h"
#include "../../vendor/bullet3/src/BulletDynamics/Character/btKinematicCharacterController.h"
#include <vector>

namespace love
{
class chai_collisions
{
    public:
    class CharacterController
    {
        public:
        CharacterController(btPairCachingGhostObject *ghostObject = nullptr, btKinematicCharacterController *character = nullptr)
        {
            this->ghostObject = ghostObject;
            this->character = character;
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
        }
        btPairCachingGhostObject *ghostObject;
        btKinematicCharacterController *character;
        // std::vector<int> group;
    };
    class RigidMesh
    {
        public:
        RigidMesh(btTriangleMesh *mesh = nullptr, btRigidBody *rigidBody = nullptr)
        {
            this->mesh = mesh;
            this->rigidBody = rigidBody;
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
        }
        btTriangleMesh *mesh;
        btRigidBody *rigidBody;
        // std::vector<int> group;
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
    chai_collisions();
    ~chai_collisions();
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
    std::vector<RigidMesh *> rigidMeshes;
    std::vector<CharacterController *> characterControllers;
    std::map<int, World *> worlds;
};
} // namespace love