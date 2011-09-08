// A startup script that hooks to scene added & scene cleared signals, and creates a local freelook camera upon either signal.
// Also adds Locate functionality to the SceneStructureWindow.

framework.Scene().SceneAdded.connect(OnSceneAdded);

if (!framework.IsHeadless())
{
    engine.ImportExtension("qt.core");
    engine.ImportExtension("qt.gui");
    ui.ContextMenuAboutToOpen.connect(OnContextMenu);
}

var scene = null;
var cameraEntityId = 0;
var entityLocatePosition = new float3(0, 0, 0);
var entityLocateDistance = 0;
var entityLocateMinDistance = 10.0; // Be at least this distance away from the object's bounding box when locating it

function OnSceneAdded(scenename)
{
    // Get pointer to scene through framework
    scene = framework.Scene().GetScene(scenename);
    scene.SceneCleared.connect(OnSceneCleared);
    scene.EntityCreated.connect(OnEntityCreated)
    CreateCamera(scene);
}

function OnSceneCleared(scene)
{
    CreateCamera(scene);
}

function OnEntityCreated(entity, change)
{
    if (entity.id == cameraEntityId)
        return; // This was the signal for our camera, ignore
    
    var oldCamera = scene.GetEntity(cameraEntityId);

    // If a freelookcamera entity is loaded from the scene, use it instead; delete the one we created
    if (entity.name == "FreeLookCamera")
    {
        if (entity.camera != null)
        {
            entity.camera.SetActive();
            cameraEntityId = entity.id;
        }
        scene.RemoveEntity(cameraEntityId);
    }
    // If a camera spawnpos entity is loaded, copy the transform
    if (entity.name == "FreeLookCameraSpawnPos")
    {
        if (oldCamera)
            oldCamera.placeable.transform = entity.placeable.transform;
    }
}

function CreateCamera(scene)
{
    if (scene.GetEntityByName("FreeLookCamera") != null)
        return;

    var entity = scene.CreateLocalEntity(["EC_Script", "EC_Camera", "EC_Placeable"]);
    entity.SetName("FreeLookCamera");
    entity.SetTemporary(true);

    var script = entity.GetComponent("EC_Script");
    script.type = "js";
    script.runOnLoad = true;
    var r = script.scriptRef;
    r.ref = "local://freelookcamera.js";
    script.scriptRef = r;

    cameraEntityId = entity.id;
}

function OnContextMenu(menu, targets)
{
    // Implement the "Locate" functionality
    var numPlaceableEntities = 0;
    var averagePosition = new float3(0, 0, 0);
    var maxDistanceAway = entityLocateMinDistance;
    for (var i = 0; i < targets.length; ++i)
    {
        var entity = null;
        // Recognize entity by properties
        if ("name" in targets[i] && "description" in targets[i])
            entity = targets[i];
        // If not an entity, it might be a component
        else if ("name" in targets[i] && "typeName" in targets[i])
            entity = targets[i].ParentEntity();

        if (entity && entity.placeable)
        {
            numPlaceableEntities++;
            averagePosition = averagePosition.Add(entity.placeable.WorldPosition());

            if (entity.mesh)
            {
                var distanceAway = entityLocateMinDistance + entity.mesh.WorldOBB().Diagonal().Length();
                if (distanceAway > maxDistanceAway)
                    maxDistanceAway = distanceAway;
            }
        }
    }

    if (numPlaceableEntities > 0)
    {
        entityLocatePosition = averagePosition.Div(numPlaceableEntities);
        entityLocateDistance = maxDistanceAway;

        menu.addSeparator();
        menu.addAction("Locate").triggered.connect(OnLocateTriggered);
    }
}

function OnLocateTriggered()
{
    var cameraEntity = scene.GetEntity(cameraEntityId);
    if (cameraEntity)
    {
        var transform = cameraEntity.placeable.transform;
        var finalPosition = entityLocatePosition.Add(cameraEntity.placeable.WorldOrientation().Mul(new float3(0,0,entityLocateDistance)));
        transform.pos = finalPosition;
        cameraEntity.placeable.transform = transform;
    }
}
