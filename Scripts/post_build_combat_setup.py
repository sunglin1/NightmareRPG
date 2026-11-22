# Run once after rebuilding the C++ module and restarting the editor:
#   python Scripts/ue_run.py Scripts/post_build_combat_setup.py
#
# 1. Registers GUIDs for the HealthBar / DamageVignette widgets that were added to
#    WBP_PlayerHUD by script (removes the WidgetVariableNameToGuidMap compile ensure).
# 2. Verifies Blueprint defaults don't override the new combat values
#    (Belica WeaponDamage 10 / MaxHealth 100, Whisper MaxHP 100 / AttackDamage 20).
# 3. Recompiles and saves the touched Blueprints.
import unreal

EAL = unreal.EditorAssetLibrary
BEL = unreal.BlueprintEditorLibrary

WBP_PATH = "/Game/Characters/ParagonLtBelica/Characters/Heroes/Belica/WBP_PlayerHUD"
BELICA_PATH = "/Game/Characters/ParagonLtBelica/Characters/Heroes/Belica/LtBelicaPlayerCharacter"
WHISPER_PATH = "/Game/Enemy/NormalEnemy_Whisper/BP_NightmareWhisper"

# --- 1. Widget GUIDs ---
wbp = unreal.load_asset(WBP_PATH)
added = unreal.NightmareEditorUtils.ensure_widget_variable_guids(wbp)
print(f"[HUD] registered GUIDs for {added} widget(s)")
BEL.compile_blueprint(wbp)
EAL.save_loaded_asset(wbp, only_if_is_dirty=False)

# --- 2. Blueprint default overrides ---
EXPECTED = {
    BELICA_PATH: {"WeaponDamage": 10.0, "MaxHealth": 100.0, "DamageCooldown": 1.2, "DeathRestartDelay": 1.5},
    WHISPER_PATH: {"MaxHP": 100.0, "AttackDamage": 20.0, "HeadshotMultiplier": 1.0},
}
for path, props in EXPECTED.items():
    bp = unreal.load_asset(path)
    cdo = unreal.get_default_object(bp.generated_class())
    changed = False
    for name, want in props.items():
        try:
            have = cdo.get_editor_property(name)
        except Exception as e:  # protected C++ props may not be script-readable
            print(f"[{bp.get_name()}] {name}: not readable from Python ({e}); check in Class Defaults")
            continue
        if abs(float(have) - want) > 1e-4:
            print(f"[{bp.get_name()}] {name} overridden to {have} -> resetting to {want}")
            cdo.set_editor_property(name, want)
            changed = True
        else:
            print(f"[{bp.get_name()}] {name} = {have} OK")
    BEL.compile_blueprint(bp)
    if changed:
        EAL.save_loaded_asset(bp, only_if_is_dirty=False)

print("Done. Check the Output Log for 'WidgetVariableNameToGuidMap' ensures (there should be none).")
