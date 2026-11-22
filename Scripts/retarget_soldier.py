# Creates NEW assets only: IK rigs, retargeter, and retargeted copies of starter-pack rifle anims for the SWAT soldier.
import unreal

OUT = "/Game/Cinematics/SoldierAnims"
SRC_MESH = "/Game/_Reference/AnimStarterPack/UE4_Mannequin/Mesh/SK_Mannequin"
TGT_MESH = "/Game/Characters/swat/source/sol_8_low"
ANIMS = ["Walk_Fwd_Rifle_Ironsights", "Idle_Rifle_Hip", "Fire_Rifle_Hip", "Death_1", "Death_2", "Jog_Fwd_Rifle", "Hit_React_1"]
ASP = "/Game/_Reference/AnimStarterPack/"

eal = unreal.EditorAssetLibrary
at = unreal.AssetToolsHelpers.get_asset_tools()
src_mesh = unreal.load_asset(SRC_MESH)
tgt_mesh = unreal.load_asset(TGT_MESH)


def make_rig(name, mesh):
    path = OUT + "/" + name
    if eal.does_asset_exist(path):
        return unreal.load_asset(path)
    rig = at.create_asset(name, OUT, unreal.IKRigDefinition, unreal.IKRigDefinitionFactory())
    c = unreal.IKRigController.get_controller(rig)
    c.set_skeletal_mesh(mesh)
    print(name, "auto retarget def:", c.apply_auto_generated_retarget_definition())
    print(name, "auto fbik:", c.apply_auto_fbik())
    print(name, "chains:", [str(ch.chain_name) for ch in c.get_retarget_chains()])
    eal.save_asset(path)
    return rig


src_rig = make_rig("IK_ASP_Mannequin", src_mesh)
tgt_rig = make_rig("IK_SWAT_Soldier", tgt_mesh)

rtg_path = OUT + "/RTG_ASP_to_SWAT"
if eal.does_asset_exist(rtg_path):
    rtg = unreal.load_asset(rtg_path)
else:
    rtg = at.create_asset("RTG_ASP_to_SWAT", OUT, unreal.IKRetargeter, unreal.IKRetargetFactory())
    rc = unreal.IKRetargeterController.get_controller(rtg)
    S, T = unreal.RetargetSourceOrTarget.SOURCE, unreal.RetargetSourceOrTarget.TARGET
    rc.set_ik_rig(S, src_rig)
    rc.set_ik_rig(T, tgt_rig)
    rc.set_preview_mesh(S, src_mesh)
    rc.set_preview_mesh(T, tgt_mesh)
    try:
        rc.add_default_ops()
    except Exception as e:
        print("add_default_ops:", e)
    try:
        rc.assign_ik_rig_to_all_ops(S, src_rig); rc.assign_ik_rig_to_all_ops(T, tgt_rig)
    except Exception as e:
        print("assign rigs:", e)
    try:
        rc.auto_map_chains(unreal.AutoMapChainType.FUZZY, True)
    except Exception as e:
        print("auto_map_chains:", e)
    # Mixamo target is T-pose, UE4 mannequin is A-pose: align target retarget pose to source
    try:
        rc.auto_align_all_bones(T)
    except Exception as e:
        print("auto_align:", e)
    try:
        rc.snap_bone_to_ground("RightToeBase", T)
    except Exception as e:
        print("snap:", e)
    eal.save_asset(rtg_path)

ar = unreal.AssetRegistryHelpers.get_asset_registry()
todo = []
for n in ANIMS:
    if eal.does_asset_exist(OUT + "/SWAT_" + n):
        continue
    hits = [a for a in ar.get_assets_by_path(ASP, recursive=True) if str(a.asset_name) == n]
    if hits:
        todo.append(hits[0])
if todo:
    res = unreal.IKRetargetBatchOperation.duplicate_and_retarget(
        todo, src_mesh, tgt_mesh, rtg, search="", replace="", prefix="SWAT_", suffix="",
        target_path=OUT, use_source_path=False, include_referenced_assets=False, overwrite_existing_files=False)
    for a in res:
        print("RETARGETED", a.package_name)
for n in ANIMS:
    print(n, eal.does_asset_exist(OUT + "/SWAT_" + n))
eal.save_directory(OUT)
