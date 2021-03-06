// For conditions of distribution and use, see copyright notice in license.txt

#pragma once

#include "CoreTypes.h"
#include "SceneFwd.h"
#include "PhysicsModuleApi.h"
#include "Math/float3.h"
#include "Math/MathFwd.h"

#include <LinearMath/btIDebugDraw.h>

#include <set>
#include <QObject>
#include <QVector>

#include <boost/enable_shared_from_this.hpp>

class btCollisionConfiguration;
class btBroadphaseInterface;
class btConstraintSolver;
class btDiscreteDynamicsWorld;
class btDispatcher;
class btCollisionObject;
class EC_RigidBody;
class Transform;
class OgreWorld;

/// Result of a raycast to the physical representation of a scene.
/** @sa Physics::PhysicsWorld
    @todo Remove the QObject inheritance here, and expose as a struct to scripts. */
class PhysicsRaycastResult : public QObject
{
    Q_OBJECT
    Q_PROPERTY(Entity* entity READ getentity);
    Q_PROPERTY(float3 pos READ getpos);
    Q_PROPERTY(float3 normal READ getnormal);
    Q_PROPERTY(float distance READ getdistance);

public:
    Entity* getentity() const { return entity; }
    float3 getpos() const { return pos; }
    float3 getnormal() const { return normal; }
    float getdistance() const { return distance; }
    
    Entity* entity;
    float3 pos;
    float3 normal;
    float distance;
};

namespace Physics
{

class PhysicsModule;

/// A physics world that encapsulates a Bullet physics world
class PHYSICS_MODULE_API PhysicsWorld : public QObject, public btIDebugDraw, public boost::enable_shared_from_this<PhysicsWorld>
{
    Q_OBJECT
    
    friend class PhysicsModule;
    friend class ::EC_RigidBody;
    
public:
    PhysicsWorld(ScenePtr scene, bool isClient);
    virtual ~PhysicsWorld();
    
    /// Step the physics world. May trigger several internal simulation substeps, according to the deltatime given.
    void Simulate(f64 frametime);
    
    /// Process collision from an internal sub-step (Bullet post-tick callback)
    void ProcessPostTick(float substeptime);
    
    /// Dynamic scene property name
    static const char* PropertyName() { return "physics"; }
    
    /// IDebugDraw override
    virtual void drawLine(const btVector3& from, const btVector3& to, const btVector3& color);
    
    /// IDebugDraw override
    virtual void reportErrorWarning(const char* warningString);
    
    /// IDebugDraw override
    virtual void drawContactPoint(const btVector3& PointOnB, const btVector3& normalOnB, btScalar distance, int lifeTime, const btVector3& color) {}
    
    /// IDebugDraw override
    virtual void draw3dText(const btVector3& location,const char* textString) {}
    
    /// IDebugDraw override
    virtual void setDebugMode(int debugMode) { debugDrawMode_ = debugMode; }
    
    /// IDebugDraw override
    virtual int getDebugMode() const { return debugDrawMode_; }
    
    /// Returns the set of collisions that occurred during the previous frame.
    /// \important Use this function only for debugging, the availability of this set data structure is not guaranteed in the future.
    const std::set<std::pair<btCollisionObject*, btCollisionObject*> > &PreviousFrameCollisions() const { return previousCollisions_; }

public slots:
    /// Set physics update period (= length of each simulation step.) By default 1/60th of a second.
    /** @param updatePeriod Update period */
    void SetPhysicsUpdatePeriod(float updatePeriod);
    
    /// Set maximum physics substeps to perform on a single frame. Once this maximum is reached, time will appear to slow down if framerate is too low.
    /** @param steps Maximum physics substeps */
    void SetMaxSubSteps(int steps);
    
    /// Return internal physics timestep
    float GetPhysicsUpdatePeriod() const { return physicsUpdatePeriod_; }
    
    /// Return amount of maximum physics substeps on a single frame.
    int GetMaxSubSteps() const { return maxSubSteps_; }
    
    /// Set gravity that affects all moving objects of the physics world
    /** @param gravity Gravity vector */
    void SetGravity(const float3& gravity);
    
    /// Raycast to the world. Returns only a single (the closest) result.
    /** @param origin World origin position
        @param direction Direction to raycast to. Will be normalized automatically
        @param maxdistance Length of ray
        @param collisionlayer Collision layer. Default has all bits set.
        @param collisionmask Collision mask. Default has all bits set.
        @return result PhysicsRaycastResult structure */
    PhysicsRaycastResult* Raycast(const float3& origin, const float3& direction, float maxdistance, int collisiongroup = -1, int collisionmask = -1);
    
    /// Return gravity
    float3 GetGravity() const;
    
    /// Return the Bullet world object
    btDiscreteDynamicsWorld* GetWorld() const;
    
    /// Return whether the physics world is for a client scene. Client scenes only simulate local entities' motion on their own.
    bool IsClient() const { return isClient_; }
    
    /// Enable/disable debug geometry
    void SetDrawDebugGeometry(bool enable);
    
    /// Get debug geometry enabled status
    bool GetDrawDebugGeometry() const { return drawDebugGeometry_; }
    
    /// Enable/disable physics simulation
    void SetRunPhysics(bool enable) { runPhysics_ = enable; }
    
    /// Return whether simulation is on
    bool GetRunPhysics() const { return runPhysics_; }
    
signals:
    /// A physics collision has happened between two entities. 
    /** Note: both rigidbodies participating in the collision will also emit a signal separately. 
        Also, if there are several contact points, the signal will be sent multiple times for each contact.
        @param entityA The first entity
        @param entityB The second entity
        @param position World position of collision
        @param normal World normal of collision
        @param distance Contact distance
        @param impulse Impulse applied to the objects to separate them
        @param newCollision True if same collision did not happen on the previous frame.
                If collision has multiple contact points, newCollision can only be true for the first of them. */
    void PhysicsCollision(Entity* entityA, Entity* entityB, const float3& position, const float3& normal, float distance, float impulse, bool newCollision);
    
    /// Emitted before the simulation steps. Note: emitted only once per frame, not before each substep.
    /** @param frametime Length of simulation steps */
    void AboutToUpdate(float frametime);
    
    /// Emitted after each simulation step
    /** @param frametime Length of simulation step */
    void Updated(float frametime);
    
private:
    /// Bullet collision config
    btCollisionConfiguration* collisionConfiguration_;
    /// Bullet collision dispatcher
    btDispatcher* collisionDispatcher_;
    /// Bullet collision broadphase
    btBroadphaseInterface* broadphase_;
    /// Bullet constraint equation solver
    btConstraintSolver* solver_;
    /// Bullet physics world
    btDiscreteDynamicsWorld* world_;
    
    /// Length of one physics simulation step
    float physicsUpdatePeriod_;
    /// Maximum amount of physics simulation substeps to run on a frame
    int maxSubSteps_;
    
    /// Client scene flag
    bool isClient_;
    
    /// Parent scene
    SceneWeakPtr scene_;
    
    /// Previous frame's collisions. We store these to know whether the collision was new or "ongoing"
    std::set<std::pair<btCollisionObject*, btCollisionObject*> > previousCollisions_;
    
    /// Draw physics debug geometry, if debug drawing enabled
    void DrawDebugGeometry();
    
    /// Debug geometry enabled flag
    bool drawDebugGeometry_;
    
    /// Debug geometry manually enabled/disabled (with physicsdebug console command). If true, do not automatically enable/disable debug geometry anymore
    bool drawDebugManuallySet_;
    
    /// Whether should run physics. Default true
    bool runPhysics_;
    
    /// Bullet debug draw / debug behaviour flags
    int debugDrawMode_;
    
    /// Cached OgreWorld pointer for drawing debug geometry
    OgreWorld* cachedOgreWorld_;
    
    /// Debug draw-enabled rigidbodies. Note: these pointers are never dereferenced, it is just used for counting
    std::set<EC_RigidBody*> debugRigidBodies_;
};
}
