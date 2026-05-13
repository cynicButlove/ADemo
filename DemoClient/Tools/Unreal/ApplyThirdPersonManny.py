import unreal

MANNY_MESH_PATH = "/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"
MANNY_ANIM_BP_PATH = "/Game/Characters/Mannequins/Animations/ABP_Manny"

PLAYER_BP_PATH = "/Game/FirstPerson/Blueprints/BP_DemoCharacter"
AI_BP_PATH = "/Game/FirstPerson/Blueprints/BP_DemoAICharacter"


def log(message):
    unreal.log("[ApplyThirdPersonManny] {}".format(message))


def require_asset(path):
    asset = unreal.load_asset(path)
    if not asset:
        raise RuntimeError("Failed to load asset: {}".format(path))
    return asset


def get_mesh_component_from_blueprint(bp_path):
    blueprint = require_asset(bp_path)
    generated_class = unreal.BlueprintEditorLibrary.generated_class(blueprint)
    if not generated_class:
        raise RuntimeError("Blueprint has no generated class: {}".format(bp_path))

    cdo = unreal.get_default_object(generated_class)
    if not cdo:
        raise RuntimeError("Failed to resolve CDO for blueprint: {}".format(bp_path))

    mesh_component = None
    try:
        mesh_component = cdo.get_editor_property("mesh")
    except Exception:
        pass

    if not mesh_component:
        try:
            mesh_component = cdo.get_mesh()
        except Exception:
            pass

    if not mesh_component:
        raise RuntimeError("Failed to resolve mesh component for blueprint: {}".format(bp_path))

    return blueprint, mesh_component


def configure_mesh_component(mesh_component, skeletal_mesh, anim_class, owner_no_see):
    mesh_component.set_editor_property("skeletal_mesh_asset", skeletal_mesh)
    mesh_component.set_editor_property("animation_mode", unreal.AnimationMode.ANIMATION_BLUEPRINT)
    mesh_component.set_editor_property("anim_class", anim_class)
    mesh_component.set_editor_property("relative_location", unreal.Vector(0.0, 0.0, -90.0))
    mesh_component.set_editor_property("relative_rotation", unreal.Rotator(0.0, -90.0, 0.0))
    mesh_component.set_editor_property("relative_scale3d", unreal.Vector(1.0, 1.0, 1.0))
    mesh_component.set_editor_property("hidden_in_game", False)
    mesh_component.set_editor_property("visible", True)
    mesh_component.set_editor_property("cast_shadow", True)
    mesh_component.set_editor_property("owner_no_see", owner_no_see)
    mesh_component.set_editor_property("only_owner_see", False)


def configure_blueprint(bp_path, skeletal_mesh, anim_class, owner_no_see):
    blueprint, mesh_component = get_mesh_component_from_blueprint(bp_path)
    configure_mesh_component(mesh_component, skeletal_mesh, anim_class, owner_no_see)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    saved = unreal.EditorAssetLibrary.save_loaded_asset(blueprint, False)
    if not saved:
        raise RuntimeError("Failed to save blueprint: {}".format(bp_path))
    log("Configured {}".format(bp_path))


def main():
    skeletal_mesh = require_asset(MANNY_MESH_PATH)
    anim_blueprint = require_asset(MANNY_ANIM_BP_PATH)
    anim_class = unreal.BlueprintEditorLibrary.generated_class(anim_blueprint)
    if not anim_class:
        raise RuntimeError("Failed to resolve anim class: {}".format(MANNY_ANIM_BP_PATH))

    configure_blueprint(PLAYER_BP_PATH, skeletal_mesh, anim_class, True)
    configure_blueprint(AI_BP_PATH, skeletal_mesh, anim_class, False)
    unreal.EditorAssetLibrary.save_directory("/Game/Characters/Mannequins", False, True)
    log("Finished")


if __name__ == "__main__":
    main()
