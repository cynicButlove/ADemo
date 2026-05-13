# -*- coding: utf-8 -*-
"""
先编译 C++（含 UDemoLobbyRootWidget），再在 UE 中执行本脚本：
创建 /Game/UI/Lobby/WBP_LobbyRoot，父类为 DemoLobbyRootWidget。
随后在 UMG 设计器中添加与 C++ 一致的命名控件（见下方列表），编译保存。
"""
import unreal

PKG = "/Game/UI/Lobby"
NAME = "WBP_LobbyRoot"
FULL = f"{PKG}/{NAME}"

REQUIRED_NAMES = (
    "EditableDisplayName",
    "ButtonLogin",
    "ButtonRegister",
    "PanelLogin",
    "PanelLobby",
    "ButtonStartMatch",
    "ButtonTabStash",
    "ButtonTabTrade",
    "ButtonAddStashDemo",
    "SwitcherLobbySection",
)


def main():
    unreal.EditorAssetLibrary.make_directory(PKG)
    parent = unreal.load_class(None, "/Script/DemoClient.DemoLobbyRootWidget")
    if not parent:
        unreal.log_error("CreateWBP_LobbyRoot: load_class DemoLobbyRootWidget failed. Compile C++ first.")
        return

    if unreal.EditorAssetLibrary.does_asset_exist(FULL):
        unreal.log_warning(f"CreateWBP_LobbyRoot: exists {FULL}, skip create.")
        return

    factory = unreal.WidgetBlueprintFactory()
    try:
        factory.set_editor_property("ParentClass", parent)
    except Exception:
        try:
            factory.parent_class = parent
        except Exception as e:
            unreal.log_error(f"CreateWBP_LobbyRoot: set ParentClass failed: {e}")
            return

    at = unreal.AssetToolsHelpers.get_asset_tools()
    at.create_asset(NAME, PKG, unreal.WidgetBlueprint.static_class(), factory)
    unreal.log_warning(
        "CreateWBP_LobbyRoot: 已创建 " + FULL + "。请在 Designer 中按 Hierarchy 添加控件并命名为: "
        + ", ".join(REQUIRED_NAMES)
        + "（SwitcherLobbySection 用 WidgetSwitcher，子页 0=仓库 1=交易行占位）。"
    )


main()
