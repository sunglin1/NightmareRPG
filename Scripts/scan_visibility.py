# Read-only: list every Visibility track (and bool tracks) in Scene_1_09 + subsequences, with keys and bound object.
import unreal

ROOTS = ["/Game/Cinematics/Takes/Takes_20261110/Scene_1_09.Scene_1_09",
         "/Game/Cinematics/Scene_1_09_Cinematic.Scene_1_09_Cinematic"]
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def walk(seq, depth, seen):
    if seq.get_path_name() in seen:
        return
    seen.add(seq.get_path_name())
    pad = "  " * depth
    print(pad + "SEQ", seq.get_path_name())
    for b in seq.get_bindings():
        kind = "spawnable" if b.get_object_template() else "possessable"
        try:
            cls = b.get_possessed_object_class().get_name() if kind == "possessable" else b.get_object_template().get_class().get_name()
        except Exception:
            cls = "?"
        parent = b.get_parent()
        pname = str(parent.get_display_name()) if parent and str(parent.get_display_name()) else ""
        for t in b.get_tracks():
            if isinstance(t, (unreal.MovieSceneVisibilityTrack, unreal.MovieSceneBoolTrack)):
                prop = ""
                try:
                    prop = "%s|%s" % (t.get_property_name(), t.get_property_path())
                except Exception:
                    pass
                for s in t.get_sections():
                    ch = s.get_all_channels()
                    keys = [(k.get_time().frame_number.value, k.get_value()) for k in ch[0].get_keys()] if ch else []
                    dflt = None
                    try:
                        dflt = ch[0].get_default() if ch[0].has_default() else None
                    except Exception:
                        pass
                    print(pad + "  BIND %-32s %-11s class=%-28s parent=%-24s TRACK %s prop=%s range=(%s,%s) default=%s keys=%s" % (
                        b.get_display_name(), kind, cls, pname, t.get_class().get_name(), prop,
                        s.get_start_frame() if s.has_start_frame() else "-inf", s.get_end_frame() if s.has_end_frame() else "+inf", dflt, keys))
    for t in seq.get_tracks():
        if isinstance(t, unreal.MovieSceneSubTrack):
            for s in t.get_sections():
                sub = s.get_sequence()
                if sub:
                    walk(sub, depth + 1, seen)


for r in ROOTS:
    walk(unreal.load_asset(r), 0, set())
