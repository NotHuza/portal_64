#include "trigger_listener.h"

#include "../physics/collision_scene.h"
#include "../decor/decor_object_list.h"
#include "../levels/cutscene_runner.h"

extern struct ColliderTypeData gPlayerColliderData;

enum ObjectTriggerType triggerDetermineType(struct CollisionObject* objectEnteringTrigger) {
    if (objectEnteringTrigger->collider == &gPlayerColliderData) {
        return ObjectTriggerTypePlayer;
    }

    int decorType = decorIdForObjectDefinition((struct DecorObjectDefinition*)objectEnteringTrigger->collider);

    if (decorType == DECOR_TYPE_CUBE || DECOR_TYPE_CUBE_UNIMPORTANT) {
        return ObjectTriggerTypeCube;
    }

    return ObjectTriggerTypeNone;
}

void triggerTrigger(void* data, struct CollisionObject* objectEnteringTrigger) {
    struct TriggerListener* listener = data;

    struct Trigger* trigger = listener->trigger;

    struct Vector3 offset;
    vector3Sub(
        &objectEnteringTrigger->body->transform.position, 
        &listener->body.transform.position, 
        &offset
    );

    if (fabsf(offset.x) > listener->collisionData.sideLength.x ||
        fabsf(offset.y) > listener->collisionData.sideLength.y ||
        fabsf(offset.z) > listener->collisionData.sideLength.z) {
        // only trigger when triggering object is contained
        return;
    }

    enum ObjectTriggerType triggerType = triggerDetermineType(objectEnteringTrigger);

    for (int i = 0; i < trigger->triggerCount; ++i) {
        struct ObjectTriggerInfo* triggerInfo = &trigger->triggers[i];

        if (triggerInfo->objectType == triggerType) {
            if (triggerInfo->signalIndex != -1) {
                signalsSend(triggerInfo->signalIndex);
            }

            cutsceneTrigger(triggerInfo->cutsceneIndex, listener->triggerIndex);
        }
    }
}

void triggerInit(struct TriggerListener* listener, struct Trigger* trigger, int triggerIndex) {
    vector3Sub(&trigger->box.max, &trigger->box.min, &listener->collisionData.sideLength);
    vector3Scale(&listener->collisionData.sideLength, &listener->collisionData.sideLength, 0.5f);

    listener->colliderType.type = CollisionShapeTypeBox;
    listener->colliderType.data = &listener->collisionData; 
    listener->colliderType.bounce = 0.0f;
    listener->colliderType.friction = 0.0f;
    listener->colliderType.callbacks = &gCollisionBoxCallbacks;

    collisionObjectInit(&listener->collisionObject, &listener->colliderType, &listener->body, 1.0f, COLLISION_LAYERS_TANGIBLE);
    rigidBodyMarkKinematic(&listener->body);

    vector3Add(&trigger->box.max, &trigger->box.min, &listener->body.transform.position);
    vector3Scale(&listener->body.transform.position, &listener->body.transform.position, 0.5f);
    collisionObjectUpdateBB(&listener->collisionObject);

    listener->collisionObject.trigger = triggerTrigger;
    listener->collisionObject.data = listener;
    listener->trigger = trigger;
    listener->triggerIndex = triggerIndex;
    
    collisionSceneAddDynamicObject(&listener->collisionObject);
}