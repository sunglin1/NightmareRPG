# Builds /Game/Cinematics/Scene_1_09_Cinematic from Scene_1_09 following the 1-minute plan (202415140.pdf).
# Re-runnable: deletes and re-duplicates the target each run. Never edits Scene_1_09 or Belica's take/subsequence.
import bisect
import math
import unreal

SRC = "/Game/Cinematics/Takes/Takes_20261110/Scene_1_09"
DST = "/Game/Cinematics/Scene_1_09_Cinematic"
BELICA_SUB = "/Game/Cinematics/Takes/Takes_20261110/Scene_1_09_Subscenes/LtBelicaPlayerCharacter_Scene_1_09"
ANIM = "/Game/Cinematics/SoldierAnims/"
WANIM = "/Game/Enemy/Whisper/Animations/"
DOOR_CLOSE = "/Game/Abandoned_Bunker/Animation/Anim_Door_close"
START, TAKE_START, END = 316, 556, 1741   # 10 s pre-roll + recorded take (24 fps)

eal = unreal.EditorAssetLibrary
LSE = unreal.LevelSequenceEditorBlueprintLibrary
KI = unreal.MovieSceneKeyInterpolation

# ---------------------------------------------------------------- Belica path (read-only)
sub = unreal.load_asset(BELICA_SUB)
_path = None
for b in sub.get_bindings():
    if str(b.get_display_name()) == "LtBelicaPlayerCharacter":
        for t in b.get_tracks():
            if isinstance(t, unreal.MovieScene3DTransformTrack):
                ch = t.get_sections()[0].get_all_channels()
                _path = []
                for c in (ch[0], ch[1], ch[2]):
                    ks = c.get_keys()
                    _path.append(([k.get_time().frame_number.value for k in ks], [k.get_value() for k in ks]))


def _interp(ts, vs, t):
    i = bisect.bisect_left(ts, t)
    if i <= 0:
        return vs[0]
    if i >= len(ts):
        return vs[-1]
    a = (t - ts[i - 1]) / max(1, ts[i] - ts[i - 1])
    return vs[i - 1] + (vs[i] - vs[i - 1]) * a


def P(f):
    f = max(TAKE_START, min(END, f))
    return [_interp(ts, vs, f) for ts, vs in _path]


def heading(f, span=12, default=-135.0):
    a, b = P(f - span), P(f + span)
    dx, dy = b[0] - a[0], b[1] - a[1]
    if dx * dx + dy * dy < 25:
        return None
    return math.degrees(math.atan2(dy, dx))


def look(pos, tgt):
    dx, dy, dz = tgt[0] - pos[0], tgt[1] - pos[1], tgt[2] - pos[2]
    return [math.degrees(math.atan2(dz, math.hypot(dx, dy))), math.degrees(math.atan2(dy, dx))]


def lerp(a, b, t):
    return [a[i] + (b[i] - a[i]) * t for i in range(len(a))]


def unwrap(vals):
    out = []
    for v in vals:
        if out:
            while v - out[-1] > 180:
                v -= 360
            while v - out[-1] < -180:
                v += 360
        out.append(v)
    return out


# ---------------------------------------------------------------- copy (once) + reset in place
try:
    LSE.close_level_sequence()
except Exception:
    pass
if not eal.does_asset_exist(DST):
    eal.duplicate_asset(SRC, DST)
seq = unreal.load_asset(DST)
src_seq = unreal.load_asset(SRC)
SRC_CUT = [t for t in src_seq.get_tracks() if isinstance(t, unreal.MovieSceneCameraCutTrack)][0].get_sections()[0]
BELICA_CAM_ID = SRC_CUT.get_camera_binding_id()          # read-only from the original
for b in list(seq.get_bindings()):                       # bindings of the COPY only (cams, lights, teammates, whisper)
    b.remove()
for t in seq.get_tracks():
    if isinstance(t, unreal.MovieSceneCameraCutTrack):
        for s in list(t.get_sections()):
            t.remove_section(s)
    elif not isinstance(t, unreal.MovieSceneSubTrack):
        seq.remove_track(t)
print("reset copy: bindings", len(seq.get_bindings()), "tracks", [t.get_class().get_name() for t in seq.get_tracks()])
seq.set_playback_start(START)
seq.set_playback_end(END)
try:
    seq.set_view_range_start(START / 24.0 - 1)
    seq.set_view_range_end(END / 24.0 + 1)
    seq.set_work_range_start(START / 24.0)
    seq.set_work_range_end(END / 24.0)
except Exception:
    pass

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = {a.get_actor_label(): a for a in eas.get_all_level_actors()}


def fn(f):
    return unreal.FrameNumber(int(round(f)))


# ---------------------------------------------------------------- track helpers
def transform_section(binding):
    for t in binding.get_tracks():
        if isinstance(t, unreal.MovieScene3DTransformTrack):
            secs = t.get_sections()
            if secs:
                return secs[0]
    t = binding.add_track(unreal.MovieScene3DTransformTrack)
    s = t.add_section()
    s.set_range(START, END)
    return s


def key_transforms(binding, keys, interp=KI.AUTO):
    """keys: list of (frame, [x,y,z], [pitch,yaw,roll] or None, interp_override or None)"""
    s = transform_section(binding)
    ch = s.get_all_channels()
    yaws = unwrap([k[2][1] for k in keys if k[2] is not None])
    yi = 0
    for k in keys:
        f, loc, rot = k[0], k[1], k[2]
        it = k[3] if len(k) > 3 and k[3] is not None else interp
        for i in range(3):
            ch[i].add_key(fn(f), float(loc[i]), 0.0, unreal.MovieSceneTimeUnit.DISPLAY_RATE, it)
        if rot is not None:
            ch[3].add_key(fn(f), float(rot[2]), 0.0, unreal.MovieSceneTimeUnit.DISPLAY_RATE, it)   # roll
            ch[4].add_key(fn(f), float(rot[0]), 0.0, unreal.MovieSceneTimeUnit.DISPLAY_RATE, it)   # pitch
            ch[5].add_key(fn(f), float(yaws[yi]), 0.0, unreal.MovieSceneTimeUnit.DISPLAY_RATE, it)  # yaw
            yi += 1


def anim_track(binding):
    for t in binding.get_tracks():
        if isinstance(t, unreal.MovieSceneSkeletalAnimationTrack):
            return t
    return binding.add_track(unreal.MovieSceneSkeletalAnimationTrack)


def add_anim(binding, path, f0, f1, hold_last=False, rate=1.0, row=0):
    anim = unreal.load_asset(path)
    s = anim_track(binding).add_section()
    s.set_range(int(f0), int(f1))
    s.set_row_index(row)
    p = s.params
    p.animation = anim
    if hold_last:
        tick = seq.get_tick_resolution().numerator
        p.first_loop_start_frame_offset = unreal.FrameNumber(int(max(0.0, anim.get_play_length() - 0.05) * tick))
        rate = 0.001
    if rate != 1.0:
        pr = unreal.MovieSceneTimeWarpVariant()
        pr.set_fixed_play_rate(rate)
        p.play_rate = pr
    s.params = p
    return s


def visibility(binding, keys):
    """keys are (frame, visible). Written as an 'Actor Hidden In Game' (bHidden) track, so values are inverted."""
    t = binding.add_track(unreal.MovieSceneBoolTrack)
    t.set_property_name_and_path("bHidden", "bHidden")
    t.set_display_name(unreal.Text("Actor Hidden In Game"))
    s = t.add_section()
    s.set_range(START, END)
    c = s.get_all_channels()[0]
    c.set_default(False)
    for f, vis in keys:
        c.add_key(fn(f), not bool(vis))


def binding_id(b):
    for attempt in (lambda: seq.get_portable_binding_id(seq, b),
                    lambda: unreal.MovieSceneSequenceExtensions.get_portable_binding_id(seq, seq, b),
                    lambda: seq.get_binding_id(b),
                    lambda: seq.make_binding_id(b, unreal.MovieSceneObjectBindingSpace.LOCAL)):
        try:
            return attempt()
        except Exception:
            continue
    raise RuntimeError("cannot make binding id")


def spawn_camera(name, focal=24.0):
    b = seq.add_spawnable_from_class(unreal.CineCameraActor)
    b.set_display_name(name)
    tpl = b.get_object_template()
    cc = None
    for get in (lambda: tpl.get_editor_property("camera_component"),
                lambda: tpl.get_component_by_class(unreal.CineCameraComponent),
                lambda: tpl.get_cine_camera_component()):
        try:
            cc = get()
        except Exception:
            cc = None
        if cc:
            break
    if cc:
        fs = cc.get_editor_property("focus_settings")
        fs.focus_method = unreal.CameraFocusMethod.DISABLE
        cc.set_editor_property("focus_settings", fs)
        cc.set_editor_property("current_focal_length", focal)
    else:
        print("WARN no camera component on template", name)
    return b


def spawn_light(name, color, intensity, radius):
    b = seq.add_spawnable_from_class(unreal.PointLight)
    b.set_display_name(name)
    lc = b.get_object_template().get_editor_property("point_light_component")
    lc.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
    lc.set_editor_property("intensity", intensity)
    lc.set_editor_property("light_color", color)
    lc.set_editor_property("attenuation_radius", radius)
    return b


def static_cam(b, f0, f1, p0, p1, t0, t1=None, step=12):
    t1 = t1 or t0
    keys = []
    f = f0
    while True:
        a = (f - f0) / float(f1 - f0)
        pos = lerp(p0, p1, a)
        tg = lerp(t0, t1, a)
        pr = look(pos, tg)
        keys.append((f, pos, [pr[0], pr[1], 0.0]))
        if f >= f1:
            break
        f = min(f1, f + step)
    key_transforms(b, keys)


# ---------------------------------------------------------------- bindings for level actors
whisper = seq.add_possessable(actors["SK_Whisper"])
solA = seq.add_possessable(actors["sol_8_low_PhysicsAsset"]);  solA.set_display_name("Teammate_A (Alpha)")
solB = seq.add_possessable(actors["sol_8_low_PhysicsAsset2"]); solB.set_display_name("Teammate_B (Bravo)")
# Sequence-only emergency door placed in the bunker doorway (the level's SK_Door is left untouched)
door = seq.add_spawnable_from_class(unreal.SkeletalMeshActor)
door.set_display_name("CIN_EmergencyDoor")
_dsmc = door.get_object_template().get_editor_property("skeletal_mesh_component")
_dmesh = unreal.load_asset("/Game/Abandoned_Bunker/Mesh/SK_Door")
try:
    _dsmc.set_skeletal_mesh_asset(_dmesh)
except Exception:
    _dsmc.set_editor_property("skeletal_mesh_asset", _dmesh)
placed_belica = None
for a in eas.get_all_level_actors():
    if a.get_class().get_name() == "LtBelicaPlayerCharacter_C":
        placed_belica = a
if placed_belica:
    pb = seq.add_possessable(placed_belica)
    pb.set_display_name("Belica_LevelPlaced (hidden)")
    visibility(pb, [(START, False)])

MESH_YAW = -90.0          # SkeletalMeshActors face +Y: actor yaw = heading - 90
FLOOR = -98.0             # Belica capsule-centre Z -> mesh-root Z offset


def ground(p):
    return [p[0], p[1], p[2] + FLOOR]


def right_of(h):
    r = math.radians(h - 90)
    return [math.cos(r), math.sin(r)]


# ---------------------------------------------------------------- Teammates
def follow_keys(delay, side, stop_frame, f_end, step=6):
    keys, last_h = [], -135.0
    for f in range(START, f_end + 1, step):
        src = min(max(f - delay, TAKE_START), stop_frame)
        h = heading(src) if f - delay >= TAKE_START else None
        last_h = h if h is not None else last_h
        p = ground(P(src))
        r = right_of(last_h)
        p[0] += r[0] * side; p[1] += r[1] * side
        keys.append((f, p, [0, last_h + MESH_YAW, 0], KI.LINEAR))
    return keys, last_h


# Teammate A (Alpha): follows 1.25 s behind, right side; repositions, fires, gets taken at 27 s
kA, hA = follow_keys(30, 90, 880, 1084)
fireA = [200.0, 1950.0, -219.0]
kA += [(1110, fireA, [0, 90 + MESH_YAW, 0], KI.LINEAR), (1185, fireA, [0, 90 + MESH_YAW, 0], KI.LINEAR)]
# Teammate B (Bravo): follows 2.5 s behind, left side; snatched in the dark at 17 s
kB, hB = follow_keys(60, -90, 820, 970)
posB = kB[-1][1]

# Whisper path
W_HIDE = [-500.0, 3800.0, -219.0]
w_emerge0 = [posB[0] - 260, posB[1] + 380, -219.0]
w_grab = [posB[0] - 60, posB[1] + 110, -219.0]
w_drag_end = [-380.0, 4400.0, -219.0]
w_fight0 = [-100.0, 2950.0, -219.0]
w_attack = [185.0, 2130.0, -219.0]
w_drop = [-120.0, 2650.0, -219.0]
wk = [(START, W_HIDE, [0, 0, 0], KI.CONSTANT),
      (964, w_emerge0, [0, look(w_emerge0, posB)[1] + MESH_YAW, 0], KI.LINEAR),
      (988, w_grab, [0, look(w_grab, posB)[1] + MESH_YAW, 0], KI.LINEAR),
      (1012, w_grab, [0, look(w_grab, posB)[1] + MESH_YAW, 0], KI.LINEAR),
      (1083, w_drag_end, [0, look(w_drag_end, posB)[1] + MESH_YAW, 0], KI.CONSTANT),
      (1084, w_fight0, [0, -90 + MESH_YAW, 0], KI.LINEAR),
      (1164, w_attack, [0, look(w_attack, fireA)[1] + MESH_YAW, 0], KI.LINEAR),
      (1205, w_attack, [0, look(w_attack, fireA)[1] + MESH_YAW, 0], KI.LINEAR),
      (1300, w_drop, [0, -90 + MESH_YAW, 0], KI.LINEAR)]
# chase: from the drop point, run down Belica's path ~2 s behind her, stop at the door
for f in range(1324, 1621, 6):
    if f < 1400:
        tgt = ground(P(1352))
        p = lerp(w_drop, tgt, (f - 1324) / 76.0)
        h = look(w_drop, tgt)[1]
    else:
        p = ground(P(f - 48))
        h = heading(f - 48) or -110
    wk.append((f, p, [0, h + MESH_YAW, 0], KI.LINEAR))
door_stop = ground(P(1566))
wk.append((1660, door_stop, [0, (heading(1560) or -125) + MESH_YAW, 0], KI.LINEAR))
key_transforms(whisper, wk)

# Bodies dragged with the Whisper
def body_follow(start_f, end_f, w_from, w_to, base, yaw):
    out = []
    for i in range(0, 7):
        a = i / 6.0
        w = lerp(w_from, w_to, a)
        d = [base[0] - w_from[0], base[1] - w_from[1]]
        out.append((start_f + (end_f - start_f) * a, [w[0] + d[0] * 0.6, w[1] + d[1] * 0.6, base[2]], [0, yaw, 0], KI.LINEAR))
    return out

kB += [(1012, posB, [0, kB[-1][2][1], 0], KI.LINEAR)] + body_follow(1012, 1083, w_grab, w_drag_end, posB, kB[-1][2][1])
kA += body_follow(1205, 1300, w_attack, w_drop, fireA, 90 + MESH_YAW)
key_transforms(solA, kA)
key_transforms(solB, kB)

# Teammate animations (retargeted starter-pack rifle anims)
add_anim(solA, ANIM + "SWAT_Idle_Rifle_Hip", START, 604)
add_anim(solA, ANIM + "SWAT_Jog_Fwd_Rifle", 604, 910)
add_anim(solA, ANIM + "SWAT_Idle_Rifle_Hip", 910, 1084)
add_anim(solA, ANIM + "SWAT_Jog_Fwd_Rifle", 1084, 1110)
add_anim(solA, ANIM + "SWAT_Fire_Rifle_Hip", 1110, 1180)
add_anim(solA, ANIM + "SWAT_Death_1", 1180, 1180 + int(unreal.load_asset(ANIM + "SWAT_Death_1").get_play_length() * 24))
add_anim(solA, ANIM + "SWAT_Death_1", 1180 + int(unreal.load_asset(ANIM + "SWAT_Death_1").get_play_length() * 24), END, hold_last=True)
visibility(solA, [(START, True), (1310, False)])

add_anim(solB, ANIM + "SWAT_Idle_Rifle_Hip", START, 640)
add_anim(solB, ANIM + "SWAT_Jog_Fwd_Rifle", 640, 880)
add_anim(solB, ANIM + "SWAT_Idle_Rifle_Hip", 880, 994)
d2 = int(unreal.load_asset(ANIM + "SWAT_Death_2").get_play_length() * 24)
add_anim(solB, ANIM + "SWAT_Death_2", 994, 994 + d2)
add_anim(solB, ANIM + "SWAT_Death_2", 994 + d2, END, hold_last=True)
visibility(solB, [(START, True), (1090, False)])

# Whisper animations (keeps the existing Attack2 section at 1164-1205)
for p, f0, f1 in [("Anim_Whisper_Idle1", START, 964), ("Anim_Whisper_Walk1", 964, 988), ("Anim_Whisper_Attack1", 988, 1012),
                  ("Anim_Whisper_Walk_Back", 1012, 1084), ("Anim_Whisper_Run", 1084, 1110),
                  ("Anim_Whisper_Taking_Damage1", 1110, 1137), ("Anim_Whisper_Taking_Damage2", 1137, 1164), ("Anim_Whisper_Attack2", 1164, 1205),
                  ("Anim_Whisper_Walk_Back", 1205, 1300), ("Anim_Whisper_Idle2", 1300, 1324),
                  ("Anim_Whisper_Run", 1324, 1615), ("Anim_Whisper_Attack1", 1615, END)]:
    add_anim(whisper, WANIM + p, f0, f1)
visibility(whisper, [(START, False), (964, True), (1650, False)])

# Door: emergency shutter slams at 44 s, then holds closed
DOOR_SCALE, DOOR_YAW = 1.35, -108.9
door_center = [-1292.0, -4134.0]
ry = math.radians(DOOR_YAW)
door_root = [door_center[0] + 30 * DOOR_SCALE * math.cos(ry), door_center[1] + 30 * DOOR_SCALE * math.sin(ry), -178.0]
key_transforms(door, [(START, door_root, [0, DOOR_YAW, 0])])
dch = transform_section(door).get_all_channels()
for i in (6, 7, 8):
    dch[i].add_key(fn(START), DOOR_SCALE)
dl = int(unreal.load_asset(DOOR_CLOSE).get_play_length() * 24)
add_anim(door, DOOR_CLOSE, START, 1600, rate=0.0001)               # holds the open pose before the slam
add_anim(door, DOOR_CLOSE, 1600, 1600 + dl)
add_anim(door, DOOR_CLOSE, 1600 + dl, END, hold_last=True)
visibility(door, [(START, False), (1588, True)])

# ---------------------------------------------------------------- Lights
muzzle = spawn_light("FX_MuzzleFlash_Alpha", unreal.Color(120, 200, 255, 255), 120.0, 1200.0)  # BGRA
mpos = [fireA[0] + 10, fireA[1] + 80, fireA[2] + 150]
key_transforms(muzzle, [(START, mpos, [0, 0, 0])])
vk = [(START, False)]
for f in range(1110, 1178, 4):
    vk += [(f, True), (f + 1, False)]
visibility(muzzle, vk)

red = spawn_light("FX_EmergencyLight", unreal.Color(0, 0, 255, 255), 2.5, 1100.0)
key_transforms(red, [(START, [-1330.0, -4060.0, 200.0], [0, 0, 0])])
vk = [(START, False)]
for f in range(1590, END, 16):
    vk += [(f, True), (f + 9, False)]
visibility(red, vk)

# ---------------------------------------------------------------- Cameras
cams = []
# 0-5 s  wide establishing: forest hiding the tunnel entrance
c = spawn_camera("Cam00_Establish", 20.0)
static_cam(c, START, 436, [3700, 8300, 650], [3300, 7700, 480], [900, 5000, -50], [1100, 5200, -100])
cams.append((c, START, 436))
# 5-10 s team waiting at the entrance (radio: "Alpha, Bravo, moving in")
c = spawn_camera("Cam01_TeamLowAngle", 28.0)
static_cam(c, 436, TAKE_START, [2400, 6300, -140], [2340, 6240, -140], [1960, 5870, -80])
cams.append((c, 436, TAKE_START))
# 10-16 s follow behind the team toward the tunnel
c = spawn_camera("Cam02_FollowTeam", 24.0)
keys = []
for f in range(TAKE_START, 701, 6):
    h = heading(f) or -135.0
    p = P(f); r = math.radians(h)
    pos = [p[0] - math.cos(r) * 430, p[1] - math.sin(r) * 430, p[2] + 140]
    tg = [p[0] + math.cos(r) * 500, p[1] + math.sin(r) * 500, p[2] + 30]
    pr = look(pos, tg)
    keys.append((f, pos, [pr[0], pr[1], 0]))
key_transforms(c, keys)
cams.append((c, TAKE_START, 700))
# 16-25 s front-high, near-static pan following them into the tunnel
c = spawn_camera("Cam03_TunnelFrontHigh", 22.0)
keys = []
for f in range(700, 917, 8):
    pos = lerp([235, 2210, 60], [220, 2170, 55], (f - 700) / 216.0)
    p = P(f); pr = look(pos, [p[0], p[1], p[2] + 40])
    keys.append((f, pos, [pr[0], pr[1], 0]))
key_transforms(c, keys)
cams.append((c, 700, 916))
# 25-32 s over-the-shoulder POV back up the tunnel: Bravo is dragged into the dark
c = spawn_camera("Cam04_ShoulderPOV", 30.0)
keys = []
for i, f in enumerate(range(916, 1085, 4)):
    j = math.sin(i * 1.7) * 1.2
    pos = [-45 + j, 2120, -25 + math.cos(i * 1.3) * 1.0]
    pr = look(pos, [posB[0] - 40, posB[1] + 120, -120])
    keys.append((f, pos, [pr[0] + j * 0.4, pr[1] + j * 0.5, j * 0.6]))
key_transforms(c, keys)
cams.append((c, 916, 1084))
# 32-37 s the fight: gunfire doesn't stop it
c = spawn_camera("Cam05_FightWide", 20.0)
static_cam(c, 1084, 1204, [210, 740, 70], [200, 840, 65], [60, 2000, -90], [150, 2050, -110], step=6)
cams.append((c, 1084, 1204))
# 37-42 s Alpha is taken
c = spawn_camera("Cam06_AlphaTaken", 28.0)
keys = []
for f in range(1204, 1325, 6):
    a = (f - 1204) / 120.0
    pos = lerp([-140, 1180, -40], [-110, 1260, -30], a)
    w = lerp(w_attack, w_drop, min(1.0, max(0.0, (f - 1205) / 95.0)))
    pr = look(pos, [w[0], w[1], w[2] + 110])
    keys.append((f, pos, [pr[0], pr[1], 0]))
key_transforms(c, keys)
cams.append((c, 1204, 1324))
# 42-53 s chase down the tunnel, camera running ahead of Belica
c = spawn_camera("Cam07_Chase", 22.0)
keys = []
for f in range(1324, 1589, 4):
    a = P(f + 20); p = P(f)
    pos = [a[0], a[1], a[2] + 70]
    pr = look(pos, [p[0], p[1], p[2] + 30])
    keys.append((f, pos, [pr[0], pr[1], 0]))
key_transforms(c, keys)
cams.append((c, 1324, 1588))
# 53-56 s locked-off frame on the emergency door as it slams
c = spawn_camera("Cam08_DoorSlam", 24.0)
static_cam(c, 1588, 1660, [-1690, -4740, 20], [-1690, -4740, 20], [-1390, -3870, 140])
cams.append((c, 1588, 1660))

# ---------------------------------------------------------------- Camera cuts
cut_track = [t for t in seq.get_tracks() if isinstance(t, unreal.MovieSceneCameraCutTrack)][0]
final = cut_track.add_section()   # final beat: Belica's own gameplay camera (bridge to play)
final.set_range(1660, END)
final.set_camera_binding_id(BELICA_CAM_ID)
for cb, f0, f1 in cams:
    s = cut_track.add_section()
    s.set_range(int(f0), int(f1))
    s.set_camera_binding_id(binding_id(cb))

eal.save_asset(DST)
LSE.open_level_sequence(seq)
print("BUILT", DST, "frames", START, END, "cams", len(cams))
