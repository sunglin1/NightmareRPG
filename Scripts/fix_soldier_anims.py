# Fixes the retargeted SWAT_* anims (new assets only): restore Mixamo root scale 100 and normalise Hips translation.
import unreal

OUT = "/Game/Cinematics/SoldierAnims/"
NAMES = ["Walk_Fwd_Rifle_Ironsights", "Idle_Rifle_Hip", "Fire_Rifle_Hip", "Death_1", "Death_2", "Jog_Fwd_Rifle", "Hit_React_1"]
AL = unreal.AnimationLibrary
APE = unreal.AnimPoseExtensions
REF_HIPS_Z = 0.958

idle = unreal.load_asset(OUT + "SWAT_Idle_Rifle_Hip")
p0 = APE.get_anim_pose_at_time(idle, 0.0, unreal.AnimPoseEvaluationOptions())
k = REF_HIPS_Z / APE.get_bone_pose(p0, "Hips", unreal.AnimPoseSpaces.LOCAL).translation.z
print("hips factor", k)
if abs(k - 1.0) < 0.05:
    print("already fixed; nothing to do")
else:
    for n in NAMES:
        anim = unreal.load_asset(OUT + "SWAT_" + n)
        ctrl = anim.controller
        nf = AL.get_num_frames(anim) + 1
        hp, hr, hs, rp, rr, rs = [], [], [], [], [], []
        for i in range(nf):
            h, r = AL.get_bone_poses_for_frame(anim, ["Hips", "SWAT"], i, False)
            t = h.translation
            hp.append(unreal.Vector(t.x * k, t.y * k, t.z * k)); hr.append(h.rotation); hs.append(h.scale3d)
            rp.append(r.translation); rr.append(r.rotation); rs.append(unreal.Vector(100.0, 100.0, 100.0))
        ctrl.open_bracket(unreal.Text("Fix SWAT root scale"), True)
        try:
            for add in ("add_bone_curve", "add_bone_track"):
                if hasattr(ctrl, add):
                    try:
                        getattr(ctrl, add)("SWAT", True)
                    except Exception as e:
                        print(add, e)
                    break
            ctrl.set_bone_track_keys("Hips", hp, hr, hs, True)
            ctrl.set_bone_track_keys("SWAT", rp, rr, rs, True)
        finally:
            ctrl.close_bracket(True)
        unreal.EditorAssetLibrary.save_asset(OUT + "SWAT_" + n)
        print("fixed", n, nf, "frames")

pose = APE.get_anim_pose_at_time(idle, 0.0, unreal.AnimPoseEvaluationOptions())
for bn in ["SWAT", "Hips", "Head", "LeftFoot"]:
    w = APE.get_bone_pose(pose, bn, unreal.AnimPoseSpaces.WORLD)
    print("CHECK", bn, round(w.translation.z, 1), "scale", round(w.scale3d.x, 1))
