# Replace broken (unbound) Visibility tracks with "Actor Hidden In Game" (bHidden) bool tracks, inverting key values.
# Only touches Visibility tracks that have no property binding; valid ones (e.g. Belica's LaserDot) are left alone.
import unreal

TARGETS = ["/Game/Cinematics/Scene_1_09_Cinematic"]


def prop_name(t):
    try:
        return str(t.get_property_name() or "")
    except Exception:
        return ""


def fix_seq(seq):
    fixed = 0
    for b in seq.get_bindings():
        for t in list(b.get_tracks()):
            if not isinstance(t, unreal.MovieSceneVisibilityTrack) or prop_name(t) not in ("", "None"):
                continue
            secs = []
            for s in t.get_sections():
                ch = s.get_all_channels()[0]
                secs.append({
                    "start": s.get_start_frame() if s.has_start_frame() else None,
                    "end": s.get_end_frame() if s.has_end_frame() else None,
                    "keys": [(k.get_time().frame_number.value, bool(k.get_value())) for k in ch.get_keys()],
                    "default": ch.get_default() if ch.has_default() else None,
                })
            b.remove_track(t)
            nt = b.add_track(unreal.MovieSceneBoolTrack)
            nt.set_property_name_and_path("bHidden", "bHidden")
            nt.set_display_name(unreal.Text("Actor Hidden In Game"))
            for d in secs:
                ns = nt.add_section()
                if d["start"] is not None and d["end"] is not None:
                    ns.set_range(d["start"], d["end"])
                else:
                    ns.set_start_frame_bounded(False)
                    ns.set_end_frame_bounded(False)
                ch = ns.get_all_channels()[0]
                if d["default"] is not None:
                    ch.set_default(not d["default"])
                for f, v in d["keys"]:
                    ch.add_key(unreal.FrameNumber(f), not v)
            print("FIXED %-30s keys(vis)=%s -> keys(bHidden)=%s" % (
                b.get_display_name(), [(f, v) for f, v in secs[0]["keys"]][:6], [(f, not v) for f, v in secs[0]["keys"]][:6]))
            fixed += 1
    return fixed


LSE = unreal.LevelSequenceEditorBlueprintLibrary
for path in TARGETS:
    seq = unreal.load_asset(path)
    n = fix_seq(seq)
    if n:
        print("saved", path, unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False))
    print(path, "fixed", n)
try:
    LSE.refresh_current_level_sequence()
except Exception:
    pass
