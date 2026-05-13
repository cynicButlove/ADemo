import unreal

def check_asset_exists(path):
    return unreal.EditorAssetLibrary.does_asset_exist(path)

def load_asset(path):
    return unreal.EditorAssetLibrary.load_asset(path)

def get_bp_parent_class(bp_path):
    bp = load_asset(bp_path)
    if bp is None:
        return None
    gen_class = bp.generated_class()
    if gen_class:
        parent = gen_class.get_super_class()
        return parent.get_name() if parent else None
    return None

print("=" * 60)
print("  DemoClient Milestone 1 Configuration Verification")
print("=" * 60)

errors = []
warnings = []

bp_char_path = "/Game/FirstPerson/Blueprints/BP_DemoCharacter"
bp_gm_path = "/Game/FirstPerson/Blueprints/BP_DemoGameMode"
bp_weapon_path = "/Game/FirstPerson/Blueprints/BP_DemoWeapon_AR"

print("\n[1] Blueprint Classes")
for name, path, expected_parent in [
    ("BP_DemoCharacter", bp_char_path, "DemoCharacter"),
    ("BP_DemoGameMode", bp_gm_path, "DemoGameMode"),
    ("BP_DemoWeapon_AR", bp_weapon_path, "DemoWeaponBase"),
]:
    if check_asset_exists(path):
        parent = get_bp_parent_class(path)
        if parent and expected_parent in parent:
            print(f"  OK  {name} (parent: {parent})")
        else:
            msg = f"{name} parent class is '{parent}', expected '{expected_parent}'"
            errors.append(msg)
            print(f"  ERR {msg}")
    else:
        errors.append(f"{name} not found at {path}")
        print(f"  ERR {name} NOT FOUND at {path}")

print("\n[2] Input Actions")
ia_names = ["IA_Move", "IA_Look", "IA_Jump", "IA_Sprint", "IA_Crouch",
            "IA_Fire", "IA_ADS", "IA_Reload", "IA_Interact"]
for ia_name in ia_names:
    path = f"/Game/FirstPerson/Input/Actions/{ia_name}"
    if check_asset_exists(path):
        ia = load_asset(path)
        vt = ia.get_editor_property("value_type")
        print(f"  OK  {ia_name} (ValueType: {vt})")
        if ia_name in ["IA_Move", "IA_Look"] and "Axis2D" not in str(vt):
            warnings.append(f"{ia_name} should be Axis2D, got {vt}")
    else:
        errors.append(f"{ia_name} not found")
        print(f"  ERR {ia_name} NOT FOUND")

print("\n[3] Input Mapping Context")
imc_path = "/Game/FirstPerson/Input/Actions/IMC_DemoDefault"
if not check_asset_exists(imc_path):
    imc_path = "/Game/FirstPerson/Input/IMC_DemoDefault"
if check_asset_exists(imc_path):
    print(f"  OK  IMC_DemoDefault found at {imc_path}")
else:
    errors.append("IMC_DemoDefault not found")
    print("  ERR IMC_DemoDefault NOT FOUND")

print("\n[4] BP_DemoCharacter Properties")
bp_char = load_asset(bp_char_path)
if bp_char:
    cdo = bp_char.generated_class().get_default_object()
    props_to_check = {
        "DefaultMappingContext": "default_mapping_context",
        "MoveAction": "move_action",
        "LookAction": "look_action",
        "JumpAction": "jump_action",
        "SprintAction": "sprint_action",
        "CrouchAction": "crouch_action",
        "FireAction": "fire_action",
        "ADSAction": "ads_action",
        "ReloadAction": "reload_action",
        "DefaultWeaponClass": "default_weapon_class",
    }
    for display_name, prop_name in props_to_check.items():
        try:
            val = cdo.get_editor_property(prop_name)
            if val is not None:
                print(f"  OK  {display_name} = {val.get_name() if hasattr(val, 'get_name') else val}")
            else:
                errors.append(f"BP_DemoCharacter.{display_name} is NOT SET (None)")
                print(f"  ERR {display_name} = None (NOT SET!)")
        except Exception as e:
            warnings.append(f"Cannot read {display_name}: {e}")
            print(f"  WARN Cannot read {display_name}")

print("\n[5] GameMode Configuration")
gm_settings = unreal.GameMapsSettings.get_game_maps_settings()
default_gm = gm_settings.get_editor_property("global_default_game_mode")
if default_gm:
    gm_name = default_gm.get_name()
    if "DemoGameMode" in gm_name:
        print(f"  OK  GlobalDefaultGameMode = {gm_name}")
    else:
        errors.append(f"GameMode is '{gm_name}', expected BP_DemoGameMode")
        print(f"  ERR GameMode = {gm_name}, expected BP_DemoGameMode")
else:
    errors.append("GlobalDefaultGameMode is not set")
    print("  ERR GlobalDefaultGameMode NOT SET")

print("\n" + "=" * 60)
print("  SUMMARY")
print("=" * 60)
if not errors and not warnings:
    print("  ALL CHECKS PASSED! Ready to play!")
else:
    if errors:
        print(f"\n  {len(errors)} ERROR(s):")
        for e in errors:
            print(f"    - {e}")
    if warnings:
        print(f"\n  {len(warnings)} WARNING(s):")
        for w in warnings:
            print(f"    - {w}")
    print("\n  Please fix the errors above in the UE Editor.")
print("=" * 60)
