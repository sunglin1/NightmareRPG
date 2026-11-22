# Read-only: dumps Scene_1_09 (and its subsequences) + level actors to Saved/seq_dump.json
# Run in Unreal Output Log (Cmd -> Python):
#   py "C:/Users/th304/Documents/GitHub/NightmareRPG/Scripts/dump_scene_1_09.py"
import json
import os
import unreal

SEQ_PATH = "/Game/Cinematics/Takes/Takes_20261110/Scene_1_09.Scene_1_09"
OUT = os.path.join(unreal.Paths.project_saved_dir(), "seq_dump.json")


def safe(fn, default=None):
    try:
        return fn()
    except Exception as e:  # noqa
        return default if default is not None else "ERR: %s" % e


def frame_val(f):
    return safe(lambda: f.frame_number.value if hasattr(f, "frame_number") else f.value)


def dump_section(sec):
    d = {
        "class": sec.get_class().get_name(),
        "start": safe(lambda: sec.get_start_frame()),
        "end": safe(lambda: sec.get_end_frame()),
        "row": safe(lambda: sec.get_row_index()),
        "active": safe(lambda: sec.is_active()),
    }
    if isinstance(sec, unreal.MovieSceneSkeletalAnimationSection):
        d["anim"] = safe(lambda: sec.params.animation.get_path_name())
    if isinstance(sec, unreal.MovieSceneSubSection):
        sub = safe(lambda: sec.get_sequence(), None)
        d["sub_sequence"] = sub.get_path_name() if sub else None
    if isinstance(sec, unreal.MovieSceneCameraCutSection):
        d["camera_binding_guid"] = safe(lambda: str(sec.get_camera_binding_id().get_editor_proxy_value().guid if hasattr(sec.get_camera_binding_id(), "get_editor_proxy_value") else sec.get_camera_binding_id().guid))
    chans = []
    for ch in safe(lambda: sec.get_all_channels(), []) or []:
        keys = safe(lambda: ch.get_keys(), []) or []
        kd = []
        for k in keys[:40]:
            kd.append([safe(lambda: k.get_time().frame_number.value), safe(lambda: str(k.get_value()))])
        chans.append({"name": safe(lambda: str(ch.get_name())), "n": len(keys), "keys": kd})
    d["channels"] = chans
    return d


def dump_track(tr):
    return {
        "class": tr.get_class().get_name(),
        "name": safe(lambda: str(tr.get_display_name())),
        "sections": [dump_section(s) for s in safe(lambda: tr.get_sections(), []) or []],
    }


def dump_seq(seq, world, seen):
    path = seq.get_path_name()
    if path in seen:
        return {"path": path, "dup": True}
    seen.add(path)
    d = {
        "path": path,
        "display_rate": safe(lambda: str(seq.get_display_rate())),
        "tick_rate": safe(lambda: str(seq.get_tick_resolution())),
        "start": safe(lambda: seq.get_playback_start()),
        "end": safe(lambda: seq.get_playback_end()),
        "master_tracks": [dump_track(t) for t in safe(lambda: seq.get_tracks(), []) or []],
        "bindings": [],
        "subs": [],
    }
    for b in safe(lambda: seq.get_bindings(), []) or []:
        bd = {
            "name": safe(lambda: str(b.get_display_name())),
            "guid": safe(lambda: str(b.get_id())),
            "parent": safe(lambda: str(b.get_parent().get_display_name())),
            "possessed_class": safe(lambda: b.get_possessed_object_class().get_name()),
            "bound": safe(lambda: [o.get_path_name() for o in seq.locate_bound_objects(b, world)]),
            "tracks": [dump_track(t) for t in safe(lambda: b.get_tracks(), []) or []],
        }
        d["bindings"].append(bd)
    for t in safe(lambda: seq.get_tracks(), []) or []:
        for s in safe(lambda: t.get_sections(), []) or []:
            if isinstance(s, unreal.MovieSceneSubSection):
                sub = safe(lambda: s.get_sequence(), None)
                if sub:
                    d["subs"].append(dump_seq(sub, world, seen))
    return d


def main():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    seq = unreal.load_asset(SEQ_PATH)
    out = {"world": world.get_path_name(), "sequence": dump_seq(seq, world, set()), "actors": []}
    for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
        loc = a.get_actor_location()
        rot = a.get_actor_rotation()
        cls = a.get_class().get_name()
        if cls in ("StaticMeshActor",):
            continue  # skip set dressing to keep file small
        out["actors"].append({
            "name": a.get_name(), "label": a.get_actor_label(), "class": cls,
            "loc": [round(loc.x), round(loc.y), round(loc.z)],
            "rot": [round(rot.pitch), round(rot.yaw), round(rot.roll)],
        })
    with open(OUT, "w", encoding="utf-8") as f:
        json.dump(out, f, ensure_ascii=False, indent=1, default=str)
    unreal.log("seq dump written: " + OUT)


main()
