# -*- coding: utf-8 -*-
"""
将灰盒关卡 L_DemoExtractionGraybox 中的玩法 Actor 复制到目标关卡（如 UtopianCityDemoMap）。

包含类型（类名子串匹配，含蓝图子类）：
  - PlayerStart
  - NavMeshBoundsVolume（导航烘焙范围；大地图可能需随后拉大或增建）
  - DemoExtractionZone
  - DemoLootContainer / DemoLootItem

说明：
  - AI 由 DemoGameMode 在运行时按 NavMesh 随机生成，关卡里通常无 AI Actor；复制后请在 BP_DemoGameMode 上把 AISpawnCenter / AISpawnRadius 调到粘贴区域附近。
  - 运行前修改 WORLD_OFFSET：在目标地图中选一块可玩地面，把该点世界坐标作为灰盒原点 (0,0,0) 的落点。

用法：UE 菜单 文件 -> 执行 Python 脚本 -> 选本文件。完成后在目标关卡 Ctrl+S，World Settings 选 BP_DemoGameMode，Build -> Build Paths（导航）并保存。
"""

import unreal

# 源 / 目标地图（软对象路径）
SOURCE_MAP = "/Game/Demo/Maps/L_DemoExtractionGraybox"
TARGET_MAP = "/Game/UtopianCity/Maps/UtopianCityDemoMap"

# 灰盒关卡中原点附近的布局，整体平移到目标世界的该位置（按你的城市场景修改！）
WORLD_OFFSET = unreal.Vector(0.0, 0.0, 100.0)


def _class_name(actor):
    try:
        return actor.get_class().get_name()
    except Exception:
        return ""


def _should_copy(actor):
    if not actor:
        return False
    cn = _class_name(actor)
    if not cn:
        return False
    skip = (
        "WorldSettings",
        "GameNetworkManager",
        "DefaultPhysicsVolume",
        "GameplayDebuggerPlayerManager",
        "AbstractNavMeshData",
        "RecastNavMesh",
        "Brush",
    )
    if cn in skip:
        return False
    if cn == "PlayerStart":
        return True
    if cn == "NavMeshBoundsVolume":
        return True
    if "DemoExtractionZone" in cn:
        return True
    if "DemoLootContainer" in cn:
        return True
    if "DemoLootItem" in cn:
        return True
    return False


def _get_transform(actor):
    loc = actor.get_actor_location()
    rot = actor.get_actor_rotation()
    scale = actor.get_actor_scale3d()
    return loc, rot, scale


def _apply_offset(loc, rot, scale):
    new_loc = unreal.Vector(loc.x + WORLD_OFFSET.x, loc.y + WORLD_OFFSET.y, loc.z + WORLD_OFFSET.z)
    return new_loc, rot, scale


def _main():
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    unreal.log("CopyGrayboxGameplay: loading source map: {}".format(SOURCE_MAP))
    unreal.EditorLoadingAndSavingUtils.load_map(SOURCE_MAP)

    world = unreal.EditorLevelLibrary.get_editor_world()
    if not world:
        unreal.log_error("CopyGrayboxGameplay: failed to get editor world after loading source.")
        return

    actors = eas.get_all_level_actors()
    to_copy = [a for a in actors if _should_copy(a)]

    if not to_copy:
        unreal.log_error(
            "CopyGrayboxGameplay: no matching actors. Open L_DemoExtractionGraybox and ensure "
            "PlayerStart / NavMeshBoundsVolume / DemoExtractionZone / DemoLoot* exist."
        )
        return

    # 序列化：类 + 变换 + 源 actor 引用在切关后会失效，只保存类路径与数值
    serialized = []
    sum_loc = unreal.Vector(0, 0, 0)
    for a in to_copy:
        loc, rot, scale = _get_transform(a)
        sum_loc = unreal.Vector(sum_loc.x + loc.x, sum_loc.y + loc.y, sum_loc.z + loc.z)
        cls = a.get_class()
        cls_path = cls.get_path_name()
        props_blob = {}
        cn = _class_name(a)
        if "DemoLootContainer" in cn:
            for prop in ("LootTable", "ItemDataTable", "SearchTime", "MinItems", "MaxItems"):
                try:
                    props_blob[prop] = a.get_editor_property(prop)
                except Exception:
                    pass
        elif "DemoLootItem" in cn:
            for prop in ("ItemID", "Quantity", "ItemDataTable", "bAutoPickup"):
                try:
                    props_blob[prop] = a.get_editor_property(prop)
                except Exception:
                    pass
        elif "DemoExtractionZone" in cn:
            for prop in ("ZoneName", "ExtractionTime", "bRequiresKey", "RequiredKeyItemID"):
                try:
                    props_blob[prop] = a.get_editor_property(prop)
                except Exception:
                    pass

        serialized.append(
            {
                "class_path": cls_path,
                "location": loc,
                "rotation": rot,
                "scale": scale,
                "props": props_blob,
            }
        )

    n = float(len(to_copy))
    avg = unreal.Vector(sum_loc.x / n, sum_loc.y / n, sum_loc.z / n)
    paste_center = unreal.Vector(avg.x + WORLD_OFFSET.x, avg.y + WORLD_OFFSET.y, avg.z + WORLD_OFFSET.z)
    unreal.log_warning(
        "CopyGrayboxGameplay: {} actors captured. 玩法区域在灰盒中平均位置约 ({:.0f},{:.0f},{:.0f})；"
        "粘贴后中心约 ({:.0f},{:.0f},{:.0f}) —— 可将 BP_DemoGameMode 的 AISpawnCenter 设为附近。".format(
            len(serialized),
            avg.x,
            avg.y,
            avg.z,
            paste_center.x,
            paste_center.y,
            paste_center.z,
        )
    )

    unreal.log("CopyGrayboxGameplay: loading target map: {}".format(TARGET_MAP))
    unreal.EditorLoadingAndSavingUtils.load_map(TARGET_MAP)

    world = unreal.EditorLevelLibrary.get_editor_world()
    if not world:
        unreal.log_error("CopyGrayboxGameplay: failed to get editor world after loading target.")
        return

    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    spawned = 0
    for entry in serialized:
        cls_path = entry["class_path"]
        loc, rot, scale = _apply_offset(entry["location"], entry["rotation"], entry["scale"])
        cls_path = cls_path.strip().strip("'\"")
        actor_class = None
        try:
            actor_class = unreal.load_object(None, cls_path)
        except Exception:
            pass
        if not actor_class:
            try:
                actor_class = unreal.load_class(None, cls_path)
            except Exception:
                pass
        if not actor_class:
            unreal.log_error("CopyGrayboxGameplay: could not load class: {}".format(cls_path))
            continue

        new_a = eas.spawn_actor_from_class(actor_class, loc, rot)
        if not new_a:
            unreal.log_error("CopyGrayboxGameplay: spawn failed for {}".format(cls_path))
            continue

        try:
            t = unreal.Transform(loc, rot, scale)
            eas.set_actor_transform(new_a, t)
        except Exception:
            pass

        for prop, val in entry["props"].items():
            try:
                new_a.set_editor_property(prop, val)
            except Exception:
                pass

        spawned += 1

    unreal.log_warning(
        "CopyGrayboxGameplay: spawned {} actors in target. 请：Ctrl+S 保存关卡；World Settings -> GameMode Override = BP_DemoGameMode；"
        " 对大地图调整 NavMeshBoundsVolume 覆盖范围后 Build 导航。".format(spawned)
    )


_main()
