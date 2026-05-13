# -*- coding: utf-8 -*-
"""
空大厅/登录关卡：/Game/UI/Maps/MAP_LobbyHub（无 FirstPerson 玩法几何，仅作 Listen Server 大厅 + UMG）。

运行：UE 菜单 文件 -> 执行 Python 脚本 -> 选本文件。
重新生成前请关闭该关卡（若当前打开的是 MAP_LobbyHub）。

MCP：编辑器已连接 user-unreal 时，可对 editor_run_python 执行与本脚本 main() 等价的代码（工具要求代码内不写 # 注释）。
"""
import unreal

PACKAGE = "/Game/UI/Maps"
ASSET_NAME = "MAP_LobbyHub"
FULL = PACKAGE + "/" + ASSET_NAME


def main():
    unreal.EditorAssetLibrary.make_directory(PACKAGE)
    if unreal.EditorAssetLibrary.does_asset_exist(FULL):
        if not unreal.EditorAssetLibrary.delete_asset(FULL):
            unreal.log_error(
                "CreateLobbyEmptyMap: 无法删除已存在的 "
                + FULL
                + "。请关闭该关卡后重试，或手动删除资产后再运行。"
            )
            return
    factory = unreal.WorldFactory()
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    world_asset = tools.create_asset(ASSET_NAME, PACKAGE, unreal.World, factory)
    if not world_asset:
        unreal.log_error("CreateLobbyEmptyMap: WorldFactory 创建失败。")
        return
    unreal.EditorAssetLibrary.save_loaded_asset(world_asset)
    unreal.EditorLoadingAndSavingUtils.load_map(FULL)
    sub = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = sub.get_editor_world()
    if world:
        ps_cls = unreal.load_class(None, "/Script/Engine.PlayerStart")
        if ps_cls:
            loc = unreal.Vector(0.0, 0.0, 300.0)
            rot = unreal.Rotator(0.0, 0.0, 0.0)
            unreal.EditorLevelLibrary.spawn_actor_from_class(world, ps_cls, loc, rot)
        else:
            unreal.log_error("CreateLobbyEmptyMap: 无法 load_class PlayerStart。")
        unreal.EditorLevelLibrary.save_current_level()
    unreal.log_warning(
        "CreateLobbyEmptyMap: 已生成并保存 "
        + FULL
        + "。请确认 DefaultEngine.ini 中默认地图已指向该关卡后重启编辑器或重新打开工程。"
    )


main()
