
#include "scene.h"

#include "defs.h"
#include "graphics/graphics.h"
#include "models/models.h"
#include "materials/shadow_caster.h"
#include "materials/subject.h"
#include "materials/light.h"
#include "materials/point_light_rendered.h"
#include "util/time.h"
#include "sk64/skelatool_defs.h"
#include "controls/controller.h"
#include "shadow_map.h"
#include "../physics/point_constraint.h"
#include "../physics/debug_renderer.h"
#include "../controls/controller.h"
#include "../physics/collision_scene.h"
#include "../levels/static_render.h"
#include "../levels/levels.h"
#include "../scene/portal_surface.h"
#include "../math/mathf.h"
#include "./hud.h"
#include "dynamic_scene.h"
#include "../audio/soundplayer.h"
#include "../audio/clips.h"
#include "../levels/cutscene_runner.h"
#include "../util/memory.h"
#include "../decor/decor_object_list.h"
#include "signals.h"

struct Vector3 gPortalGunOffset = {0.100957, -0.113587, -0.28916};
struct Vector3 gPortalGunForward = {0.1f, -0.1f, 1.0f};
struct Vector3 gPortalGunUp = {0.0f, 1.0f, 0.0f};

Lights1 gSceneLights = gdSPDefLights1(128, 128, 128, 128, 128, 128, 0, 127, 0);

void sceneInit(struct Scene* scene) {
    signalsInit(1);

    cameraInit(&scene->camera, 45.0f, 0.125f * SCENE_SCALE, 80.0f * SCENE_SCALE);
    playerInit(&scene->player, levelGetLocation(gCurrentLevel->startLocation));

    portalInit(&scene->portals[0], 0);
    portalInit(&scene->portals[1], PortalFlagsOddParity);

    for (int i = 0; i < MAX_CUBES; ++i) {
        cubeInit(&scene->cubes[i]);

        scene->cubes[i].rigidBody.transform.position.x = 1.0f;
        scene->cubes[i].rigidBody.transform.position.y = 0.5f;
        scene->cubes[i].rigidBody.transform.position.z = 6.0f + i;
        scene->cubes[i].rigidBody.currentRoom = 1;

        quatAxisAngle(&gRight, M_PI * 0.125f, &scene->cubes[i].rigidBody.transform.rotation);
        scene->cubes[i].rigidBody.angularVelocity = gOneVec;
    }

    scene->buttonCount = 1;
    scene->buttons = malloc(sizeof(struct Button) * scene->buttonCount);
    struct Vector3 buttonPos;
    buttonPos.x = 5.0f;
    buttonPos.y = 0.0f;
    buttonPos.z = 3.0f;
    buttonInit(&scene->buttons[0], &buttonPos, 1, 0);

    scene->decorCount = 0;
    scene->decor = malloc(sizeof(struct DecorObject*) * scene->decorCount);
    struct Transform decorTransform;
    transformInitIdentity(&decorTransform);
    decorTransform.position.x = 3.0f;
    decorTransform.position.y = 1.5f;
    decorTransform.position.z = 3.2f;
    // quatAxisAngle(&gRight, 1.0f, &decorTransform.rotation);
    scene->decor[0] = decorObjectNew(decorObjectDefinitionForId(DECOR_TYPE_CYLINDER), &decorTransform, 1);
    // vector3Scale(&gForward, &scene->decor[0]->rigidBody.angularVelocity, 10.0f);
    // scene->decor[0]->rigidBody.velocity.y = 1.0f;
    // scene->decor[0]->rigidBody.velocity.z = 1.0f;
    // scene->decor[0]->rigidBody.velocity.x = 1.0f;

    scene->doorCount = gCurrentLevel->doorCount;
    scene->doors = malloc(sizeof(struct Door) * scene->doorCount);
    for (int i = 0; i < scene->doorCount; ++i) {
        struct Transform doorTransform;
        doorTransform.position = gCurrentLevel->doors[i].location;
        doorTransform.rotation = gCurrentLevel->doors[i].rotation;
        doorTransform.scale = gOneVec;
        doorInit(&scene->doors[i], &doorTransform, 0, 0, gCurrentLevel->doors[i].doorwayIndex, 0);
    }

    // scene->player.grabbing = &scene->cubes[0].rigidBody;
}

void sceneRenderWithProperties(void* data, struct RenderProps* properties, struct RenderState* renderState) {
    struct Scene* scene = (struct Scene*)data;

    u64 visibleRooms = 0;
    staticRenderDetermineVisibleRooms(&properties->cullingInfo, properties->fromRoom, &visibleRooms);

    int closerPortal = vector3DistSqrd(&properties->camera.transform.position, &scene->portals[0].transform.position) > vector3DistSqrd(&properties->camera.transform.position, &scene->portals[1].transform.position) ? 0 : 1;
    int otherPortal = 1 - closerPortal;

    for (int i = 0; i < 2; ++i) {
        if (gCollisionScene.portalTransforms[closerPortal] && 
            properties->fromPortalIndex != closerPortal && 
            staticRenderIsRoomVisible(visibleRooms, gCollisionScene.portalRooms[closerPortal])) {
            portalRender(
                &scene->portals[closerPortal], 
                gCollisionScene.portalTransforms[otherPortal] ? &scene->portals[otherPortal] : NULL, 
                properties, 
                sceneRenderWithProperties, 
                data, 
                renderState
            );
        }

        closerPortal = 1 - closerPortal;
        otherPortal = 1 - otherPortal;
    }

    staticRender(&properties->camera.transform, &properties->cullingInfo, visibleRooms, renderState);
}

#define SOLID_COLOR        0, 0, 0, ENVIRONMENT, 0, 0, 0, ENVIRONMENT

void sceneRenderPerformanceMetrics(struct Scene* scene, struct RenderState* renderState, struct GraphicsTask* task) {
    if (!scene->lastFrameTime) {
        return;
    }

    gDPSetCycleType(renderState->dl++, G_CYC_1CYCLE);
    gDPSetFillColor(renderState->dl++, (GPACK_RGBA5551(0, 0, 0, 1) << 16 | GPACK_RGBA5551(0, 0, 0, 1)));
    gDPSetCombineMode(renderState->dl++, SOLID_COLOR, SOLID_COLOR);
    gDPSetEnvColor(renderState->dl++, 32, 32, 32, 255);
    gSPTextureRectangle(renderState->dl++, 32 << 2, 32 << 2, (32 + 256) << 2, (32 + 16) << 2, 0, 0, 0, 1, 1);
    gDPPipeSync(renderState->dl++);
    gDPSetEnvColor(renderState->dl++, 32, 255, 32, 255);
    gSPTextureRectangle(renderState->dl++, 33 << 2, 33 << 2, (32 + 254 * scene->cpuTime / scene->lastFrameTime) << 2, (32 + 14) << 2, 0, 0, 0, 1, 1);
    gDPPipeSync(renderState->dl++);
}

void sceneRenderPortalGun(struct Scene* scene, struct RenderState* renderState) {
    struct Transform gunTransform;
    transformPoint(&scene->player.body.transform, &gPortalGunOffset, &gunTransform.position);
    struct Quaternion relativeRotation;
    quatLook(&gPortalGunForward, &gPortalGunUp, &relativeRotation);
    quatMultiply(&scene->player.body.transform.rotation, &relativeRotation, &gunTransform.rotation);
    gunTransform.scale = gOneVec;
    Mtx* matrix = renderStateRequestMatrices(renderState, 1);
    transformToMatrixL(&gunTransform, matrix, SCENE_SCALE);

    gSPMatrix(renderState->dl++, matrix, G_MTX_MODELVIEW | G_MTX_PUSH | G_MTX_MUL);
    gSPDisplayList(renderState->dl++, v_portal_gun_gfx);
    gSPPopMatrix(renderState->dl++, G_MTX_MODELVIEW);
}

void sceneRender(struct Scene* scene, struct RenderState* renderState, struct GraphicsTask* task) {
    gSPSetLights1(renderState->dl++, gSceneLights);
    
    struct RenderProps renderProperties;

    renderPropsInit(&renderProperties, &scene->camera, (float)SCREEN_WD / (float)SCREEN_HT, renderState, scene->player.body.currentRoom);

    renderProperties.camera = scene->camera;

    gDPSetRenderMode(renderState->dl++, G_RM_ZB_OPA_SURF, G_RM_ZB_OPA_SURF2);

    dynamicSceneRenderTouchingPortal(&scene->camera.transform, &renderProperties.cullingInfo, renderState);

    sceneRenderWithProperties(scene, &renderProperties, renderState);

    sceneRenderPortalGun(scene, renderState);

    gDPPipeSync(renderState->dl++);
    gDPSetRenderMode(renderState->dl++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
    gSPGeometryMode(renderState->dl++, G_ZBUFFER | G_LIGHTING | G_CULL_BOTH, G_SHADE);

    hudRender(renderState);

    sceneRenderPerformanceMetrics(scene, renderState, task);

    contactSolverDebugDraw(&gContactSolver, renderState);
}

void sceneCheckPortals(struct Scene* scene) {
    struct Ray raycastRay;
    struct Vector3 playerUp;
    raycastRay.origin = scene->player.body.transform.position;
    vector3Negate(&gForward, &raycastRay.dir);
    quatMultVector(&scene->player.body.transform.rotation, &raycastRay.dir, &raycastRay.dir);
    quatMultVector(&scene->player.body.transform.rotation, &gUp, &playerUp);

    if (controllerGetButtonDown(0, Z_TRIG)) {
        sceneFirePortal(scene, &raycastRay, &playerUp, 0, scene->player.body.currentRoom);
    }

    if (controllerGetButtonDown(0, R_TRIG | L_TRIG)) {
        sceneFirePortal(scene, &raycastRay, &playerUp, 1, scene->player.body.currentRoom);
    }
}

void sceneUpdate(struct Scene* scene) {
    OSTime frameStart = osGetTime();
    scene->lastFrameTime = frameStart - scene->lastFrameStart;

    signalsReset();

    playerUpdate(&scene->player, &scene->camera.transform);
    sceneCheckPortals(scene);

    for (int i = 0; i < MAX_CUBES; ++i) {
        cubeUpdate(&scene->cubes[i]);
    }

    for (int i = 0; i < scene->buttonCount; ++i) {
        buttonUpdate(&scene->buttons[i]);
    }

    for (int i = 0; i < scene->doorCount; ++i) {
        doorUpdate(&scene->doors[i]);
    }
    
    collisionSceneUpdateDynamics();

    levelCheckTriggers(&scene->player.body.transform.position);
    cutsceneRunnerUpdate(&gCutsceneRunner);

    scene->cpuTime = osGetTime() - frameStart;
    scene->lastFrameStart = frameStart;
}

int sceneFirePortal(struct Scene* scene, struct Ray* ray, struct Vector3* playerUp, int portalIndex, int roomIndex) {
    struct RaycastHit hit;

    if (!collisionSceneRaycast(&gCollisionScene, roomIndex, ray, 1000000.0f, 0, &hit)) {
        return 0;
    }

    int quadIndex = levelQuadIndex(hit.object);

    if (quadIndex == -1) {
        return 0;
    }

    struct Transform portalLocation;

    struct Vector3 hitDirection = hit.normal;

    if (portalIndex == 1) {
        vector3Negate(&hitDirection, &hitDirection);
    }

    portalLocation.position = hit.at;
    portalLocation.scale = gOneVec;
    if (fabsf(hit.normal.y) < 0.8) {
        quatLook(&hitDirection, &gUp, &portalLocation.rotation);
    } else {
        quatLook(&hitDirection, playerUp, &portalLocation.rotation);
    }

    return sceneOpenPortal(scene, &portalLocation, portalIndex, quadIndex, hit.roomIndex);
}

int sceneOpenPortal(struct Scene* scene, struct Transform* at, int portalIndex, int quadIndex, int roomIndex) {
    struct PortalSurfaceMapping surfaceMapping = gCurrentLevel->portalSurfaceMapping[quadIndex];

    for (int i = surfaceMapping.minPortalIndex; i < surfaceMapping.maxPortalIndex; ++i) {
        if (portalSurfaceGenerate(&gCurrentLevel->portalSurfaces[i], at, NULL, NULL)) {
            soundPlayerPlay(soundsPortalOpen2, 1.0f, 1.0f);
            
            scene->portals[portalIndex].transform = *at;
            gCollisionScene.portalTransforms[portalIndex] = &scene->portals[portalIndex].transform;
            gCollisionScene.portalRooms[portalIndex] = roomIndex;
            return 1;
        }
    }

    return 0;
}