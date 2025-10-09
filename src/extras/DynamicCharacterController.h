#pragma once

#include <btBulletDynamicsCommon.h>
#include <btBulletCollisionCommon.h>

#include <BulletCollision/CollisionDispatch/btGhostObject.h>

class btKinematicCharacterController
{
private:
	// Physics
	btCollisionShape* m_pCollisionShape;
	btDefaultMotionState* m_pMotionState;
	btRigidBody* m_pRigidBody;
	btPairCachingGhostObject* m_pGhostObject;

	bool m_onGround;
	//bool m_onJumpableGround; // A bit lower contact than just onGround
	bool m_hittingWall;

	float m_bottomYOffset;
	float m_bottomRoundedRegionYOffset;

	float m_stepHeight;

	btTransform m_motionTransform;

	glm::vec3 m_manualVelocity;
	std::vector<glm::vec3> m_surfaceHitNormals;

	btVector3 m_previousPosition;

	float m_jumpRechargeTimer;

	void ParseGhostContacts();

	void UpdatePosition();
	void UpdateVelocity();

public:
	float m_deceleration;
	float m_maxSpeed;
	float m_jumpImpulse;

	float m_jumpRechargeTime;

	btKinematicCharacterController(const glm::vec3 spawnPos, float radius, float height, float mass, float stepHeight);
	~btKinematicCharacterController();

	// Acceleration vector in XZ plane
	void Walk(const glm::vec2 dir);

	// Ignores y
	void Walk(const glm::vec3 dir);

	void Update();

	void Jump();

	glm::vec3 GetPosition() const;
	glm::vec3 GetVelocity() const;

	bool IsOnGround() const;
};

