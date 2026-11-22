# Scrub the open sequence to the frame in Saved/goto.txt; on a second run (mode=info) print spawned camera transforms.
import os
import unreal

LSE = unreal.LevelSequenceEditorBlueprintLibrary
cfg = open(os.path.join(unreal.Paths.project_saved_dir(), "goto.txt")).read().split()
frame, mode = int(cfg[0]), cfg[1] if len(cfg) > 1 else "go"
if mode == "go":
    seq = unreal.load_asset("/Game/Cinematics/Scene_1_09_Cinematic")
    if LSE.get_current_level_sequence() != seq:
        LSE.open_level_sequence(seq)
    LSE.set_current_time(frame)
    LSE.refresh_current_level_sequence()
    print("AT", frame)
else:
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
        n = a.get_actor_label()
        if a.get_class().get_name() == "CineCameraActor":
            l, r = a.get_actor_location(), a.get_actor_rotation()
            fov = a.get_cine_camera_component().field_of_view
            print("CAM %s|%s %.1f,%.1f,%.1f %.2f,%.2f fov=%.1f" % (n, a.get_name(), l.x, l.y, l.z, r.pitch, r.yaw, fov))
        elif n.startswith("Cam0") or n in ("SK_Whisper", "sol_8_low_PhysicsAsset", "sol_8_low_PhysicsAsset2", "SK_Door") or a.get_class().get_name() == "LtBelicaPlayerCharacter_C":
            l, r = a.get_actor_location(), a.get_actor_rotation()
            print("%s|%s loc=%d,%d,%d rot=%d,%d hidden=%s" % (n, a.get_name(), l.x, l.y, l.z, r.pitch, r.yaw, a.is_hidden_ed() if hasattr(a, "is_hidden_ed") else "?"))
