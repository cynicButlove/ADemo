# =============================================================================
# 如何在 UE 里运行本脚本（不要只输入文件名，否则会去 Engine/Binaries/Win64 找）
#
# 方式 A（推荐）：菜单 文件 -> 执行 Python 脚本... -> 浏览选中本文件
#
# 方式 B：在「输出日志」下方的 Python 输入框里粘贴下面一整行（按你的实际路径改盘符）：
# exec(open(r'd:\DemoProject\ADemo\DemoClient\Content\FirstPerson\SetupAI_请在UE中运行.py', encoding='utf-8').read())
#
# 方式 C：项目设置 -> 插件 -> Python -> Developer Mode -> 额外路径
#         添加：d:\DemoProject\ADemo\DemoClient\Content\FirstPerson
#         之后才可只输入：SetupAI_请在UE中运行.py
# =============================================================================

import unreal


def _new_key_type(outer, class_path):
    cls = unreal.load_class(None, class_path)
    if cls is None:
        raise RuntimeError('无法 load_class: ' + class_path)
    return unreal.new_object(cls, outer)


def _ensure_blackboard():
    path = '/Game/FirstPerson/AI/BB_DemoAI'
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.log('BB_DemoAI 已存在，跳过创建（避免资源占用冲突）。')
        return unreal.EditorAssetLibrary.load_asset(path)

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    bb_factory = unreal.BlackboardDataFactory()
    bb = asset_tools.create_asset('BB_DemoAI', '/Game/FirstPerson/AI', unreal.BlackboardData, bb_factory)
    if bb is None:
        raise RuntimeError('创建 BB_DemoAI 失败')

    keys = []

    entries = [
        ('TargetEnemy', '/Script/AIModule.BlackboardKeyType_Object'),
        ('LastKnownLocation', '/Script/AIModule.BlackboardKeyType_Vector'),
        ('PatrolLocation', '/Script/AIModule.BlackboardKeyType_Vector'),
        ('AIState', '/Script/AIModule.BlackboardKeyType_String'),
    ]
    for name, cls_path in entries:
        e = unreal.BlackboardEntry()
        e.set_editor_property('entry_name', name)
        e.set_editor_property('key_type', _new_key_type(bb, cls_path))
        keys.append(e)

    bb.set_editor_property('keys', keys)
    unreal.EditorAssetLibrary.save_asset(path)
    unreal.log('已创建 BB_DemoAI（4 个键）。')
    return bb


def _ensure_behavior_tree():
    """确保存在 BT 资产。不在此设置 blackboard_asset：UBehaviorTree 该字段未对 Python 暴露，
    会触发 set_editor_property 报错。关联黑板由控制台命令 Demo.RebuildBTAssets（C++）完成。"""
    path = '/Game/FirstPerson/AI/BT_DemoAI'
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.log('BT_DemoAI 已存在，跳过创建。')
        return unreal.EditorAssetLibrary.load_asset(path)

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    bt_factory = unreal.BehaviorTreeFactory()
    bt = asset_tools.create_asset('BT_DemoAI', '/Game/FirstPerson/AI', None, bt_factory)
    if bt is None:
        raise RuntimeError('创建 BT_DemoAI 失败')
    unreal.EditorAssetLibrary.save_asset(path)
    unreal.log('已创建 BT_DemoAI 占位资产（黑板将在 Demo.RebuildBTAssets 中绑定）。')
    return bt


def _ensure_blueprint(asset_name, folder, parent_class_path):
    path = folder + '/' + asset_name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.log(asset_name + ' 已存在，跳过。')
        return
    demo_cls = unreal.load_class(None, parent_class_path)
    if demo_cls is None:
        raise RuntimeError('无法 load_class: ' + parent_class_path)
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    bp_factory = unreal.BlueprintFactory()
    bp_factory.set_editor_property('parent_class', demo_cls)
    asset_tools.create_asset(asset_name, folder, None, bp_factory)
    unreal.EditorAssetLibrary.save_asset(path)
    unreal.log('已创建 ' + asset_name)


def _run_console(cmd):
    try:
        sub = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        world = sub.get_editor_world()
        if world:
            unreal.SystemLibrary.execute_console_command(world, cmd)
            return
    except Exception:
        pass
    unreal.log_warning('无法从 Python 执行控制台命令，请在输出日志输入: ' + cmd)


_ensure_blackboard()
_ensure_behavior_tree()

_ensure_blueprint('BP_DemoAICharacter', '/Game/FirstPerson/Blueprints', '/Script/DemoClient.DemoAICharacter')
_ensure_blueprint('BP_DemoAIController', '/Game/FirstPerson/Blueprints', '/Script/DemoClient.DemoAIController')

_run_console('Demo.RebuildBTAssets')

unreal.log('=== AI 资源准备完成 ===')
unreal.log('行为树图由 C++ 命令 Demo.RebuildBTAssets 生成（战斗->调查->巡逻）。若未自动执行，请在编辑器控制台手动输入该命令。')
unreal.log('也可通过 MCP user-unreal：editor_console_command 或 editor_run_python 触发。')
unreal.log('地图内需 NavMeshBoundsVolume 覆盖可走区域。')
