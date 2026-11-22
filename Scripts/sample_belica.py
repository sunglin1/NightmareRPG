# Read-only: sample Belica's recorded root transform (key times are in display frames)
import bisect
import unreal

SUB = "/Game/Cinematics/Takes/Takes_20261110/Scene_1_09_Subscenes/LtBelicaPlayerCharacter_Scene_1_09.LtBelicaPlayerCharacter_Scene_1_09"
sub = unreal.load_asset(SUB)


def chan_fn(ch):
    keys = ch.get_keys()
    ts = [k.get_time().frame_number.value for k in keys]
    vs = [k.get_value() for k in keys]
    def f(t):
        i = bisect.bisect_left(ts, t)
        if i <= 0:
            return vs[0]
        if i >= len(ts):
            return vs[-1]
        a = (t - ts[i - 1]) / max(1, ts[i] - ts[i - 1])
        return vs[i - 1] + (vs[i] - vs[i - 1]) * a
    return f, ts


for b in sub.get_bindings():
    if str(b.get_display_name()) != "LtBelicaPlayerCharacter":
        continue
    for t in b.get_tracks():
        if isinstance(t, unreal.MovieScene3DTransformTrack):
            chans = t.get_sections()[0].get_all_channels()
            fns = [chan_fn(c) for c in chans[:6]]
            print("KEYTIMES X first/last:", fns[0][1][:3], fns[0][1][-3:])
            for fr in range(556, 1742, 24):
                v = [fns[i][0](fr) for i in range(6)]
                print("F%d t=%.0fs loc=(%d,%d,%d) yaw=%d" % (fr, (fr - 556) / 24.0, v[0], v[1], v[2], v[5]))
