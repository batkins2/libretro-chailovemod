#include "../ChaiLove.h"
#ifndef TINY_GLTF_H_
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
}
chai_collisions::~chai_collisions()
{
}
std::vector<int> chai_collisions::addRigidMesh(std::string meshPath) 
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
    for (size_t i = 0; i < model.meshes.size(); i++) {
        btTriangleMesh *mesh = new btTriangleMesh();
    
        auto primitive = model.meshes[i].primitives[0];
        auto positionAccessor = model.accessors[primitive.attributes["POSITION"]];
        auto positionBufferView = model.bufferViews[positionAccessor.bufferView];
        auto positionBuffer = model.buffers[positionBufferView.buffer];
        const float* positions = reinterpret_cast<const float*>(&positionBuffer.data[positionAccessor.byteOffset + positionBufferView.byteOffset]);

        auto indiceAccessor = model.accessors[primitive.indices];
        auto indiceBufferView = model.bufferViews[indiceAccessor.bufferView];
        auto indiceBuffer = model.buffers[indiceBufferView.buffer];

        if (indiceAccessor.componentType != TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
            const unsigned int* indices = reinterpret_cast<const unsigned int*>(&indiceBuffer.data[indiceAccessor.byteOffset + indiceBufferView.byteOffset]);
            for (size_t i = 0; i < indiceAccessor.count; ++i) {
                std::vector<float> vertices;
                // Extract and push positions
                for (size_t j = 0; j < 3; ++j) {
                    vertices.push_back(positions[indices[i] * 3 + j]);
                }
                mesh->addTriangle(btVector3(vertices[0], 0, 0), btVector3(0, vertices[1], 0), btVector3(0, 0, vertices[2]));
            }
        } else {
            const unsigned short* indices = reinterpret_cast<const unsigned short*>(&indiceBuffer.data[indiceAccessor.byteOffset + indiceBufferView.byteOffset]);
            for (size_t i = 0; i < indiceAccessor.count; ++i) {
                std::vector<float> vertices;
                // Extract and push positions
                for (size_t j = 0; j < 3; ++j) {
                    vertices.push_back(positions[indices[i] * 3 + j]);
                }
                mesh->addTriangle(btVector3(vertices[0], 0, 0), btVector3(0, vertices[1], 0), btVector3(0, 0, vertices[2]));
            }
        }

        btBvhTriangleMeshShape *shape = new btBvhTriangleMeshShape(mesh, true);
        btDefaultMotionState *motionState = new btDefaultMotionState(btTransform(btQuaternion(0, 0, 0, 1), btVector3(0, 0, 0)));
        btRigidBody::btRigidBodyConstructionInfo rigidBodyCI(0, motionState, shape, btVector3(0, 0, 0));
        btRigidBody *rigidBody = new btRigidBody(rigidBodyCI);
        
        rigidMeshes.emplace_back(new RigidMesh(mesh, rigidBody));     
        refs.push_back(count);
        count++;
    }
    return refs;
}
void chai_collisions::setRigidMeshPosition(std::vector<int> rigidMeshIndex, float x, float y, float z, std::vector<int> group = {0})
{
    for (auto i : rigidMeshIndex) {        
        auto r = rigidMeshes[i]->rigidBody;
        for (auto g : group) {
            if (worlds.find(g) == worlds.end()) {
                init(group[i]);
            }
            worlds[g]->dynamicsWorld->addRigidBody(r, btBroadphaseProxy::DefaultFilter, btBroadphaseProxy::StaticFilter | btBroadphaseProxy::DefaultFilter);
           
        }
        rigidMeshes[i]->rigidBody->setWorldTransform(btTransform(btQuaternion(0, 0, 0, 1), btVector3(x, y, z)));
    }              
}
int chai_collisions::addCharacterController()
{
    btPairCachingGhostObject *ghostObject = new btPairCachingGhostObject();
    btConvexShape *capsule = new btCapsuleShape(1, 1);
    ghostObject->setCollisionShape(capsule);
    ghostObject->setCollisionFlags(btCollisionObject::CF_CHARACTER_OBJECT);
    btScalar stepHeight = btScalar(0.35);
    btKinematicCharacterController *character = new btKinematicCharacterController(ghostObject, capsule, stepHeight);

    btTransform startTransform;
    startTransform.setIdentity();
    startTransform.setOrigin(btVector3(0, 1, 0));

    ghostObject->setWorldTransform(startTransform);

    characterControllers.emplace_back(new CharacterController(ghostObject, character));
   
    return characterControllers.size() - 1;
}
void chai_collisions::setCharacterControllerPosition(int characterIndex, float x, float y, float z, std::vector<int> group = {0})
{
    btPairCachingGhostObject *ghostObject = characterControllers[characterIndex]->ghostObject;
    
    for (auto i : group) {
        if (worlds.find(i) == worlds.end()) {
            init(i);
        }
        // characterControllers[characterIndex].group.push_back(group[i]);
        worlds[i]->dynamicsWorld->addCollisionObject(characterControllers[characterIndex]->ghostObject, btBroadphaseProxy::CharacterFilter, btBroadphaseProxy::StaticFilter | btBroadphaseProxy::DefaultFilter);
        worlds[i]->dynamicsWorld->addAction(characterControllers[characterIndex]->character);
    }
    
    ghostObject->setCollisionFlags(btCollisionObject::CF_CHARACTER_OBJECT);
    ghostObject->setWorldTransform(btTransform(btQuaternion(0, 0, 0, 1), btVector3(x, y, z)));
}
void chai_collisions::applyForceToCharacter(int characterIndex, float x, float y, float z)
{
    characterControllers[characterIndex]->character->setWalkDirection(btVector3(x, y, z));
}
void chai_collisions::applyForceToRigidMesh(int rigidMeshIndex, float x, float y, float z)
{
    rigidMeshes[rigidMeshIndex]->rigidBody->applyCentralForce(btVector3(x, y, z));
}
std::vector<float> chai_collisions::getCharacterController(int ref)
{
    return std::vector<float>{
        characterControllers[ref]->ghostObject->getWorldTransform().getOrigin().getX(), 
        characterControllers[ref]->ghostObject->getWorldTransform().getOrigin().getY(), 
        characterControllers[ref]->ghostObject->getWorldTransform().getOrigin().getZ()
    };
}
std::vector<float> chai_collisions::getRigidMesh(int ref)
{
    return std::vector<float>{
        rigidMeshes[ref]->rigidBody->getWorldTransform().getOrigin().getX(), 
        rigidMeshes[ref]->rigidBody->getWorldTransform().getOrigin().getY(), 
        rigidMeshes[ref]->rigidBody->getWorldTransform().getOrigin().getZ()
    };
}
void chai_collisions::init(int group = 0)
{    
    btBroadphaseInterface *broadphase = new btDbvtBroadphase();
    btDefaultCollisionConfiguration *collisionConfiguration = new btDefaultCollisionConfiguration();
    btCollisionDispatcher *dispatcher = new btCollisionDispatcher(collisionConfiguration);
    btSequentialImpulseConstraintSolver *solver = new btSequentialImpulseConstraintSolver();
    btDiscreteDynamicsWorld *dw = new btDiscreteDynamicsWorld(dispatcher, broadphase, solver, collisionConfiguration);
    auto w = new World(broadphase, collisionConfiguration, dispatcher, solver, dw);
    if (!group) {
        worlds = std::map<int, World*>();
    }
    worlds[group] = w;
    
    worlds[group]->dynamicsWorld->setGravity(btVector3(0, -10, 0));
}
void chai_collisions::destroy()
{
    for (auto &rm : rigidMeshes) {
        // for (int i = 0; i < rm.group.size(); i++) {
        //     worlds[rm.group[i]].dynamicsWorld->removeRigidBody(rm.rigidBody);
        // }
        worlds[0]->dynamicsWorld->removeRigidBody(rm->rigidBody);
        delete rm->rigidBody;
        delete rm->mesh;
    }
    rigidMeshes.clear();
    // for (auto &cc : characterControllers) {
    //     // for (int i = 0; i < cc.group.size(); i++) {
    //     //     worlds[cc.group[i]].dynamicsWorld->removeAction(cc.character);
    //     //     worlds[cc.group[i]].dynamicsWorld->removeCollisionObject(cc.ghostObject);
    //     // }
    //     // if (cc.character) {           
    //     //     delete cc.character;
    //     // }
    //     // if (cc.ghostObject) {            
    //     //     delete cc.ghostObject;
    //     // }
    // }
    characterControllers.clear();
    for (auto &dw : worlds) {
        delete dw.second;
    }
    worlds = std::map<int, World*>();
}
void chai_collisions::process()
{    
    for (auto &dw : worlds) {
        for (auto &cc : characterControllers) {
            cc->character->preStep(dw.second->dynamicsWorld);
            cc->character->playerStep(dw.second->dynamicsWorld, 1);
        }
        dw.second->dynamicsWorld->stepSimulation(1 / 60.f, 10);
    }
}
void chai_collisions::test() {
    btBroadphaseInterface *broadphase = new btDbvtBroadphase();
    btDefaultCollisionConfiguration *collisionConfiguration = new btDefaultCollisionConfiguration();
    btCollisionDispatcher *dispatcher = new btCollisionDispatcher(collisionConfiguration);
    btSequentialImpulseConstraintSolver *solver = new btSequentialImpulseConstraintSolver();
    btDiscreteDynamicsWorld *dynamicsWorld = new btDiscreteDynamicsWorld(dispatcher, broadphase, solver, collisionConfiguration);
    dynamicsWorld->setGravity(btVector3(0, -10, 0));
    btCollisionShape *groundShape = new btStaticPlaneShape(btVector3(0, 1, 0), 1);
    btCollisionShape *fallShape = new btSphereShape(1);
    btDefaultMotionState *groundMotionState = new btDefaultMotionState(btTransform(btQuaternion(0, 0, 0, 1), btVector3(0, -1, 0)));
    btRigidBody::btRigidBodyConstructionInfo groundRigidBodyCI(0, groundMotionState, groundShape, btVector3(0, 0, 0));
    btRigidBody *groundRigidBody = new btRigidBody(groundRigidBodyCI);
    dynamicsWorld->addRigidBody(groundRigidBody);
    btDefaultMotionState *fallMotionState = new btDefaultMotionState(btTransform(btQuaternion(0, 0, 0, 1), btVector3(0, 50, 0)));
    btScalar mass = 1;
    btVector3 fallInertia(0, 0, 0);
    fallShape->calculateLocalInertia(mass, fallInertia);
    btRigidBody::btRigidBodyConstructionInfo fallRigidBodyCI(mass, fallMotionState, fallShape, fallInertia);
    btRigidBody *fallRigidBody = new btRigidBody(fallRigidBodyCI);
    dynamicsWorld->addRigidBody(fallRigidBody);
    for (int i = 0; i < 300; i++) {
        dynamicsWorld->stepSimulation(1 / 60.f, 10);

        // print positions of all objects
        for (int j = dynamicsWorld->getNumCollisionObjects() - 1; j >= 0; j--) {
            btCollisionObject *obj = dynamicsWorld->getCollisionObjectArray()[j];
            btRigidBody *body = btRigidBody::upcast(obj);
            btTransform trans;
            if (body && body->getMotionState()) {
                body->getMotionState()->getWorldTransform(trans);
            } else {
                trans = obj->getWorldTransform();
            }
            printf("world pos object %d = %f,%f,%f\n", j, float(trans.getOrigin().getX()), float(trans.getOrigin().getY()), float(trans.getOrigin().getZ()));
        }
    }
    dynamicsWorld->removeRigidBody(fallRigidBody);
    delete fallRigidBody;
    dynamicsWorld->removeRigidBody(groundRigidBody);
    delete groundRigidBody;
    delete fallShape;
    delete groundShape;
    delete dynamicsWorld;
    delete solver;
    delete collisionConfiguration;
    delete dispatcher;
    delete broadphase;
}
} // namespace love