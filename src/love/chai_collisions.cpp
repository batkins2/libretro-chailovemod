#include "../ChaiLove.h"
#include <cmath> // For M_PI
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
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    
    for (size_t i = 0; i < model.meshes.size(); i++) {
        // Retrieve the transformation matrix for the model
        glm::mat4 modelMatrix = glm::mat4(1.0f);
        for (auto node : model.nodes) {
            if (node.mesh == i) {
                if (!node.matrix.empty()) {
                    modelMatrix = glm::make_mat4(node.matrix.data());
                } else {
                    if (!node.translation.empty()) {
                        modelMatrix = glm::translate(modelMatrix, glm::vec3(node.translation[0], node.translation[1], node.translation[2]));
                    }
                    if (!node.rotation.empty()) {
                        glm::quat rotation = glm::quat(node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]);
                        modelMatrix *= glm::mat4_cast(rotation);
                    }
                    if (!node.scale.empty()) {
                        modelMatrix = glm::scale(modelMatrix, glm::vec3(node.scale[0], node.scale[1], node.scale[2]));
                    }
                }
                break;
            }
        }

        printf("Model Matrix: %f, %f, %f, %f\n", modelMatrix[0][0], modelMatrix[0][1], modelMatrix[0][2], modelMatrix[0][3]);
        printf("Model Matrix: %f, %f, %f, %f\n", modelMatrix[1][0], modelMatrix[1][1], modelMatrix[1][2], modelMatrix[1][3]);
        printf("Model Matrix: %f, %f, %f, %f\n", modelMatrix[2][0], modelMatrix[2][1], modelMatrix[2][2], modelMatrix[2][3]);
        printf("Model Matrix: %f, %f, %f, %f\n", modelMatrix[3][0], modelMatrix[3][1], modelMatrix[3][2], modelMatrix[3][3]);

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
            std::vector<float> vertices;
            for (size_t i = 0; i < indiceAccessor.count; ++i) {
                
                // Extract and push positions
                for (size_t j = 0; j < 3; ++j) {
                    vertices.push_back(positions[indices[i] * 3 + j]);
                }
                // if (isnan(vertices[0]) || isnan(vertices[1]) || isnan(vertices[2])) {
                //     printf("nan: %f, %f, %f\n", vertices[0], vertices[1], vertices[2]);
                //     printf("indices: %d\n", indices[i]);
                //     printf("indices: %d\n", i);
                //     continue;
                // }
                // if (isnan(vertices[3]) || isnan(vertices[4]) || isnan(vertices[5])) {
                //     printf("nan: %f, %f, %f\n", vertices[3], vertices[4], vertices[5]);
                //     printf("indices: %d\n", indices[i]);
                //     printf("indices: %d\n", i);
                //     continue;
                // }
                // if (isnan(vertices[6]) || isnan(vertices[7]) || isnan(vertices[8])) {
                //     printf("nan: %f, %f, %f\n", vertices[6], vertices[7], vertices[8]);
                //     printf("indices: %d\n", indices[i]);
                //     printf("indices: %d\n", i);
                //     continue;
                // }
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
                        // Define the vertices of the triangle
                        glBegin(GL_TRIANGLES);
                        glVertex3f(vertices[0], vertices[1], vertices[2]);
                        glVertex3f(vertices[3], vertices[4], vertices[5]);
                        glVertex3f(vertices[6], vertices[7], vertices[8]);
                        glEnd();    
                    }
                    // printf("vertices: %f, %f, %f\n", vertices[0], vertices[1], vertices[2]);
                    // printf("vertices: %f, %f, %f\n", vertices[3], vertices[4], vertices[5]);
                    // printf("vertices: %f, %f, %f\n", vertices[6], vertices[7], vertices[8]);
                    mesh->addTriangle(btVector3(vertices[0], vertices[1], vertices[2]), btVector3(vertices[3], vertices[4], vertices[5]), btVector3(vertices[6], vertices[7], vertices[8]));
                    vertices.clear();
                }
                // printf("vertices: %f, %f, %f\n", vertices[0], vertices[1], vertices[2]);
                // printf("vertices: %f, %f, %f\n", vertices[3], vertices[4], vertices[5]);
                // printf("vertices: %f, %f, %f\n", vertices[6], vertices[7], vertices[8]);
            }
        } else {
            const unsigned short* indices = reinterpret_cast<const unsigned short*>(&indiceBuffer.data[indiceAccessor.byteOffset + indiceBufferView.byteOffset]);
            std::vector<float> vertices;
            
            for (size_t i = 0; i < indiceAccessor.count; ++i) {
               
                // Extract and push positions
                for (size_t j = 0; j < 3; ++j) {
                    vertices.push_back(positions[indices[i] * 3 + j]);
                }
                // if (isnan(vertices[0]) || isnan(vertices[1]) || isnan(vertices[2])) {
                //     printf("nan: %f, %f, %f\n", vertices[0], vertices[1], vertices[2]);
                //     printf("indices: %d\n", indices[i]);
                //     printf("indices: %d\n", i);
                //     continue;
                // }
                // if (isnan(vertices[3]) || isnan(vertices[4]) || isnan(vertices[5])) {
                //     printf("nan: %f, %f, %f\n", vertices[3], vertices[4], vertices[5]);
                //     printf("indices: %d\n", indices[i]);
                //     printf("indices: %d\n", i);
                //     continue;
                // }
                // if (isnan(vertices[6]) || isnan(vertices[7]) || isnan(vertices[8])) {
                //     printf("nan: %f, %f, %f\n", vertices[6], vertices[7], vertices[8]);
                //     printf("indices: %d\n", indices[i]);
                //     printf("indices: %d\n", i);
                //     continue;
                // }
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
                        // Define the vertices of the triangle
                        glBegin(GL_TRIANGLES);
                        glVertex3f(vertices[0], vertices[1], vertices[2]);
                        glVertex3f(vertices[3], vertices[4], vertices[5]);
                        glVertex3f(vertices[6], vertices[7], vertices[8]);
                        glEnd();    
                    }                 
                    // printf("vertices: %f, %f, %f\n", vertices[0], vertices[1], vertices[2]);
                    // printf("vertices: %f, %f, %f\n", vertices[3], vertices[4], vertices[5]);
                    // printf("vertices: %f, %f, %f\n", vertices[6], vertices[7], vertices[8]);
                    mesh->addTriangle(btVector3(vertices[0], vertices[1], vertices[2]), btVector3(vertices[3], vertices[4], vertices[5]), btVector3(vertices[6], vertices[7], vertices[8]));
                    vertices.clear();
                }
                // printf("vertices: %f, %f, %f\n", vertices[0], vertices[1], vertices[2]);
                // printf("vertices: %f, %f, %f\n", vertices[3], vertices[4], vertices[5]);
                // printf("vertices: %f, %f, %f\n", vertices[6], vertices[7], vertices[8]);
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
    // Reset OpenGL to fill mode (optional)
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    return refs;
}
void chai_collisions::setRigidMeshPosition(std::vector<int> rigidMeshIndex, float x, float y, float z, std::vector<int> group = {0})
{
    for (auto i : rigidMeshIndex) {        
        auto r = rigidMeshes[i]->rigidBody;
        for (auto g : group) {
            if (worlds->worlds.find(g) == worlds->worlds.end()) {
                init(g);
            }
            rigidMeshes[i]->group.push_back(g);
            worlds->worlds[g]->dynamicsWorld->addRigidBody(r, btBroadphaseProxy::StaticFilter, btBroadphaseProxy::DefaultFilter | btBroadphaseProxy::CharacterFilter);
        }
        rigidMeshes[i]->rigidBody->setCollisionFlags(btCollisionObject::CF_STATIC_OBJECT);
        rigidMeshes[i]->rigidBody->setWorldTransform(btTransform(btQuaternion(0, 0, 0, 1), btVector3(x, y, z)));
    }              
}
int chai_collisions::addCharacterController(int index)
{
    btPairCachingGhostObject *ghostObject = new btPairCachingGhostObject();
    btConvexShape *capsule = new btCapsuleShape(1.2f, 2.0f);
    ghostObject->setCollisionShape(capsule);
    ghostObject->setCollisionFlags(btCollisionObject::CF_CHARACTER_OBJECT);
    btScalar stepHeight = btScalar(0.35);
    btKinematicCharacterController *character = new btKinematicCharacterController(ghostObject, capsule, stepHeight, btVector3(0, 1, 0));
    character->setGravity(btVector3(0, -0.01, 0));    
    
    btTransform startTransform;
    startTransform.setIdentity();
    startTransform.setOrigin(btVector3(0, 1, 0));

    ghostObject->setWorldTransform(startTransform);

    ghostObject->setUserPointer(character); // Set the user pointer to the character controller

    ghostObject->setUserIndex(index); // Set the user pointer to the character controller
    characterControllers.emplace_back(new CharacterController(ghostObject, character));
    return characterControllers.size() - 1;
}
void chai_collisions::setCharacterControllerPosition(int characterIndex, float x, float y, float z, std::vector<int> group = {0})
{
    btPairCachingGhostObject *ghostObject = characterControllers[characterIndex]->ghostObject;
    
    for (auto i : group) {
        if (worlds->worlds.find(i) == worlds->worlds.end()) {
            init(i);
        }
        characterControllers[characterIndex]->group.push_back(i);
        worlds->worlds[i]->dynamicsWorld->addCollisionObject(characterControllers[characterIndex]->ghostObject, btBroadphaseProxy::CharacterFilter, btBroadphaseProxy::StaticFilter | btBroadphaseProxy::DefaultFilter);
        worlds->worlds[i]->dynamicsWorld->addCharacter(characterControllers[characterIndex]->character);
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
        characterControllers[ref]->ghostObject->getWorldTransform().getOrigin().getY()-2.0f, 
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
int chai_collisions::addBox(float x, float y, float z, float width, float height, float depth, std::vector<int> group = {0}, int index = 0)
{
    btTriangleMesh *mesh = new btTriangleMesh();

    // Define the vertices of an inner and outer wall box
    btVector3 vertices[8] = {
        btVector3(-width / 2, -height / 2, -depth / 2),
        btVector3(width / 2, -height / 2, -depth / 2),
        btVector3(width / 2, height / 2, -depth / 2),
        btVector3(-width / 2, height / 2, -depth / 2),
        btVector3(-width / 2, -height / 2, depth / 2),
        btVector3(width / 2, -height / 2, depth / 2),
        btVector3(width / 2, height / 2, depth / 2),
        btVector3(-width / 2, height / 2, depth / 2)
    };

    // Define the triangles of the box (6 faces, 2 triangles per face)
    int indices[12][3] = {
        {0, 1, 2}, {0, 2, 3}, // Front face
        {4, 5, 6}, {4, 6, 7}, // Back face
        {0, 1, 5}, {0, 5, 4}, // Bottom face
        {2, 3, 7}, {2, 7, 6}, // Top face
        {0, 3, 7}, {0, 7, 4}, // Left face
        {1, 2, 6}, {1, 6, 5}  // Right face
    };

    // Add the triangles to the mesh
    for (int i = 0; i < 12; ++i) {
        mesh->addTriangle(vertices[indices[i][0]], vertices[indices[i][1]], vertices[indices[i][2]]);
    }

    // Create the convex triangle mesh shape
    btCompoundShape *shape = new btCompoundShape();
    shape->addChildShape(btTransform(btQuaternion(0, 0, 0, 1), btVector3(0, 0, 0)), new btBvhTriangleMeshShape(mesh, true));

    // Set a non-zero mass to make the box dynamic
    // btScalar mass = 1.0f; // Adjust mass as needed
    // btVector3 inertia(0, 0, 0);
    // shape->calculateLocalInertia(mass, inertia);
    btDefaultMotionState *motionState = new btDefaultMotionState(btTransform(btQuaternion(0, 0, 0, 1), btVector3(x, y, z)));
    btRigidBody::btRigidBodyConstructionInfo rigidBodyCI(1.0f, motionState, shape, btVector3(0, 0, 0));
    btRigidBody *rigidBody = new btRigidBody(rigidBodyCI);
    rigidBody->setGravity(btVector3(0, 0, 0));
    // rigidBody->setLinearFactor(btVector3(1.9f, 0, 0));
    // Set friction and damping properties suitable for 1:1 character control
    // rigidBody->setFriction(0.45f); // Set a small positive friction value
    // rigidBody->setDamping(0.45f, 0.01f); // Set small positive damping values for stability
    // rigidBody->setRestitution(1.0f); // Set a small positive restitution value

    rigidBody->setUserPointer(rigidBody); // Set the user pointer to the character controller  
    rigidBody->setUserIndex(index);
    rigidBody->setUserIndex2(1);
    // Disable deactivation to keep the object active
    rigidBody->setActivationState(DISABLE_DEACTIVATION);
    rigidBody->setCollisionFlags(btCollisionObject::CF_NO_CONTACT_RESPONSE);
    cameraBox.emplace_back(rigidBody);
    rigidMeshes.emplace_back(new RigidMesh(shape, rigidBody));   
    for (auto i : group) {
        if (worlds->worlds.find(i) == worlds->worlds.end()) {
            init(i);
        }
        rigidMeshes.back()->group.push_back(i);
        worlds->worlds[i]->dynamicsWorld->addRigidBody(rigidBody, btBroadphaseProxy::DefaultFilter, btBroadphaseProxy::DefaultFilter | btBroadphaseProxy::CharacterFilter);
    }
    return rigidMeshes.size() - 1;
}
void chai_collisions::init(int group = 0)
{    
    btBroadphaseInterface *broadphase = new btDbvtBroadphase();
    broadphase->getOverlappingPairCache()->setInternalGhostPairCallback(new btGhostPairCallback());
    btDefaultCollisionConfiguration *collisionConfiguration = new btDefaultCollisionConfiguration();
    btCollisionDispatcher *dispatcher = new btCollisionDispatcher(collisionConfiguration);
    btSequentialImpulseConstraintSolver *solver = new btSequentialImpulseConstraintSolver();
    btDiscreteDynamicsWorld *dw = new btDiscreteDynamicsWorld(dispatcher, broadphase, solver, collisionConfiguration);
    auto w = new World(broadphase, collisionConfiguration, dispatcher, solver, dw);
    if (worlds == nullptr) {
        worlds = new WorldMap();
    }
    worlds->worlds[group] = w;
    
    worlds->worlds[group]->dynamicsWorld->setGravity(btVector3(0, -0.01, 0)); 
    
    worlds->worlds[group]->dynamicsWorld->setInternalTickCallback([](btDynamicsWorld *world, btScalar timeStep) {
        chai_collisions *self = static_cast<chai_collisions *>(world->getWorldUserInfo());
        btDispatcher *dispatcher = world->getDispatcher();
        const int numManifolds = dispatcher->getNumManifolds();
        btVector3 vel0 = btVector3(0, 0, 0);
        btVector3 vel1 = btVector3(0, 0, 0);
        std::map<int, bool> contact;
        std::map<int, btRigidBody*> body;
        for (int i = 0; i < self->cameraBox.size(); ++i) {
            contact[i] = false;
        }
        for (int m = 0; m < numManifolds; ++m) {
            auto *manifold = dispatcher->getManifoldByIndexInternal(m);
            const btRigidBody *body0 = static_cast<const btRigidBody *>(manifold->getBody0());
            const btRigidBody *body1 = static_cast<const btRigidBody *>(manifold->getBody1());
            
            auto numContacts = manifold->getNumContacts();
           
            if (body0->getUserIndex() == body1->getUserIndex() && numContacts > 0) {
                contact[body1->getUserIndex()] = true;
               
                btVector3 pos0 = body0->getWorldTransform().getOrigin();
                btVector3 pos1 = body1->getWorldTransform().getOrigin();

                // Calculate the collision direction
                btVector3 collisionDirection = pos1 - pos0;

                // Optional: Normalize the direction vector
                collisionDirection.normalize();

                auto b = self->cameraBox[body1->getUserIndex()];
                b->setLinearVelocity(btVector3(-collisionDirection.getX() * 6.0f, 0.0f, 0.0f));
            }
        }
        for (int i = 0; i < self->cameraBox.size(); ++i) {            
            if (!contact[i]) {
                auto b = self->cameraBox[i];                
                if (b != nullptr) {
                    auto v = b->getLinearVelocity();
                    b->setLinearVelocity(btVector3(v.getX()/1.5f, 0, 0));
                }
            }
        }
    }, this);

    // Initialize the debug drawer
    if (!debugDrawer) {
        debugDrawer = new OpenGLDebugDrawer();
        debugDrawer->setDebugMode(btIDebugDraw::DBG_DrawWireframe | btIDebugDraw::DBG_DrawAabb);
    }
    worlds->worlds[group]->dynamicsWorld->setDebugDrawer(debugDrawer);

}
void chai_collisions::destroy()
{
    for (auto &rm : rigidMeshes) {
        for (int i = 0; i < rm->group.size(); i++) {
            worlds->worlds[rm->group[i]]->dynamicsWorld->removeRigidBody(rm->rigidBody);
        }
        
        delete rm;
    }
    rigidMeshes.clear();
    for (auto &cc : characterControllers) {
        for (int i = 0; i < cc->group.size(); i++) {
            worlds->worlds[cc->group[i]]->dynamicsWorld->removeAction(cc->character);
            worlds->worlds[cc->group[i]]->dynamicsWorld->removeCollisionObject(cc->ghostObject);
        }
        delete cc;
    }
    characterControllers.clear();

    // delete debugDrawer;
    delete worlds;
}
void chai_collisions::process()
{    
    for (auto &dw : worlds->worlds) {
        for (auto &cc : characterControllers) {
            cc->character->preStep(dw.second->dynamicsWorld);
            cc->character->playerStep(dw.second->dynamicsWorld, 1);
            
            cc->character->setWalkDirection(btVector3(0.0f, 0.0f, 0.0f));

            btVector3 pos = cc->ghostObject->getWorldTransform().getOrigin();
            
            // printf("ghost pos: %f, %f, %f\n", pos.getX(), pos.getY(), pos.getZ());
        }
        dw.second->dynamicsWorld->stepSimulation(1 / 60.f, 10);  
        if (debugDrawer) {
            btVector3 gravity = dw.second->dynamicsWorld->getGravity();
            // printf("Gravity: %f, %f, %f\n", gravity.getX(), gravity.getY(), gravity.getZ());   

            btVector3 min, max;
            min.setValue(FLT_MAX, FLT_MAX, FLT_MAX);
            max.setValue(-FLT_MAX, -FLT_MAX, -FLT_MAX);

            for (int i = 0; i < dw.second->dynamicsWorld->getNumCollisionObjects(); i++) {
                btCollisionObject *obj = dw.second->dynamicsWorld->getCollisionObjectArray()[i];
                btVector3 aabbMin, aabbMax;
                obj->getCollisionShape()->getAabb(obj->getWorldTransform(), aabbMin, aabbMax);

                min.setMin(aabbMin);
                max.setMax(aabbMax);
            }

            // Calculate the center and size of the bounding box
            btVector3 center = (min + max) * 0.5;
            btVector3 size = max - min;

            // Set the camera target to the center of the geometry
            debugDrawer->cameraTarget = glm::vec3(center.getX(), center.getY(), center.getZ());

            // Position the camera far enough to fit the geometry
            float maxDimension = std::max(size.getX(), std::max(size.getY(), size.getZ()));
            float horizontalFOV = 2.0f * atan(tan(glm::radians(debugDrawer->fov) / 2.0f) * debugDrawer->aspectRatio);
            float distance = maxDimension / (2.0f * tan(horizontalFOV / 2.0f)) / 2.0f; // Adjust the divisor to control the distance
            debugDrawer->cameraPosition = glm::vec3(center.getX(), center.getY(), center.getZ() + distance);

            // Ensure the up vector is correct
            debugDrawer->upVector = glm::vec3(0.0f, 1.0f, 0.0f);

            // Adjust the near and far planes
            debugDrawer->nearPlane = 0.1f;
            debugDrawer->farPlane = distance + maxDimension * 2.0f;

            // Set the projection matrix
            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            glm::mat4 projection = glm::perspective(glm::radians(debugDrawer->fov), debugDrawer->aspectRatio, debugDrawer->nearPlane, debugDrawer->farPlane);
            glLoadMatrixf(glm::value_ptr(projection));

            // Set the view matrix
            glMatrixMode(GL_MODELVIEW);
            glLoadIdentity();
            glm::mat4 view = glm::lookAt(debugDrawer->cameraPosition, debugDrawer->cameraTarget, debugDrawer->upVector);
            glLoadMatrixf(glm::value_ptr(view));
            
            // printf("Camera Position: %f, %f, %f\n", debugDrawer->cameraPosition.x, debugDrawer->cameraPosition.y, debugDrawer->cameraPosition.z);
            // printf("Camera Target: %f, %f, %f\n", debugDrawer->cameraTarget.x, debugDrawer->cameraTarget.y, debugDrawer->cameraTarget.z);
            // printf("Up Vector: %f, %f, %f\n", debugDrawer->upVector.x, debugDrawer->upVector.y, debugDrawer->upVector.z);

            // printf("Bounding Box Min: %f, %f, %f\n", min.getX(), min.getY(), min.getZ());
            // printf("Bounding Box Max: %f, %f, %f\n", max.getX(), max.getY(), max.getZ());

            // Perform debug drawing
            dw.second->dynamicsWorld->debugDrawWorld();

            // test();
        }
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
    // Initialize the debug drawer
    // if (!debugDrawer) {
    //     debugDrawer = new OpenGLDebugDrawer();
    //     debugDrawer->setDebugMode(btIDebugDraw::DBG_DrawWireframe | btIDebugDraw::DBG_DrawAabb);
    // }
    dynamicsWorld->setDebugDrawer(debugDrawer);
    // Set the projection matrix
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glm::mat4 projection = glm::perspective(glm::radians(debugDrawer->fov), debugDrawer->aspectRatio, debugDrawer->nearPlane, debugDrawer->farPlane);
    glLoadMatrixf(glm::value_ptr(projection));

    // Set the view matrix
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glm::mat4 view = glm::lookAt(debugDrawer->cameraPosition, debugDrawer->cameraTarget, debugDrawer->upVector);
    glLoadMatrixf(glm::value_ptr(view));
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
            // Perform debug drawing
            dynamicsWorld->debugDrawWorld();
        }
        // break;
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
    // delete debugDrawer;
}
} // namespace love