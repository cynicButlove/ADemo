# -*- coding: utf-8 -*-
"""
在已打开 DemoClient 工程的情况下：菜单 文件 -> 执行 Python 脚本 运行本文件。
将创建 /Game/UI 下目录与占位 Widget Blueprint（需在 UMG 设计器里摆控件与换图）。
若 MCP 已连接编辑器，也可由外部通过 editor_run_python 执行同等逻辑（脚本内勿依赖 MCP）。
"""
import unreal

PATHS = [
    "/Game/UI",
    "/Game/UI/Auth",
    "/Game/UI/Lobby",
    "/Game/UI/Textures/Auth",
    "/Game/UI/Textures/Lobby",
    "/Game/UI/Icons",
]

WIDGETS = [
    ("WBP_LoginRegister", "/Game/UI/Auth"),
    ("WBP_LobbyShell", "/Game/UI/Lobby"),
    ("WBP_StashPanel", "/Game/UI/Lobby"),
    ("WBP_TradePanel", "/Game/UI/Lobby"),
    ("WBP_MatchReadyBar", "/Game/UI/Lobby"),
]


def main():
    for p in PATHS:
        unreal.EditorAssetLibrary.make_directory(p)

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    wb_cls = unreal.WidgetBlueprint.static_class()
    factory = unreal.WidgetBlueprintFactory()

    for name, pkg in WIDGETS:
        full = f"{pkg}/{name}"
        if unreal.EditorAssetLibrary.does_asset_exist(full):
            unreal.log(f"SetupUIAuthLobby: skip existing {full}")
            continue
        asset_tools.create_asset(name, pkg, wb_cls, factory)
        unreal.log(f"SetupUIAuthLobby: created {full}")

    unreal.log_warning(
        "SetupUIAuthLobby: 请在各 WBP 的 Designer 中添加 Canvas/Overlay、按钮与 EditableText；"
        "背景图导入到 /Game/UI/Textures，图标到 /Game/UI/Icons 后在细节面板绑定。"
    )


main()
