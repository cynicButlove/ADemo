# =============================================================================
# 里程碑 5：危险行动灰盒关卡 — 在 UE 编辑器中执行本脚本
#
# 重要（避免崩溃）：
#   在同一脚本里「复制关卡 + load_map 自动切关」会触发 EditorServer 的
#   World Memory Leaks 断言并崩溃。因此必须分两步：
#
#   【第 1 次运行】仅当尚不存在灰盒关卡时：复制 FirstPersonMap →
#       L_DemoExtractionGraybox，然后立刻结束。请不要再点一次直到打开新图。
#   【手动】在内容浏览器双击打开 /Game/Demo/Maps/L_DemoExtractionGraybox
#   【第 2 次运行】在当前已打开的灰盒关卡里布置地板、Nav、容器、撤离点等。
#
# 若灰盒资产已存在，则每次运行都要求「当前编辑器打开的关卡」路径中包含
# L_DemoExtractionGraybox，否则会报错退出，避免误改 FirstPersonMap。
#
# 运行方式：文件 -> 执行 Python 脚本... -> 选中本文件
# =============================================================================

import math
import unreal

PREFIX = 'GrayboxM5_'
SOURCE_MAP = '/Game/FirstPerson/Maps/FirstPersonMap'
DEST_MAP = '/Game/Demo/Maps/L_DemoExtractionGraybox'
GRAYBOX_TOKEN = 'L_DemoExtractionGraybox'

GRAYBOX_ORIGIN = unreal.Vector(0.0, 0.0, 0.0)
FLOOR_Z = -80.0
PLAYER_START = unreal.Vector(0.0, 0.0, 100.0)


def _run_console(world, cmd):
    try:
        unreal.SystemLibrary.execute_console_command(world, cmd)
    except Exception:
        unreal.log_warning('控制台命令失败: ' + cmd)


def _load_class(path):
    c = unreal.load_class(None, path)
    if c is None:
        unreal.log_error('load_class 失败: ' + path)
    return c


def _ensure_demo_maps_folder():
    for folder in ('/Game/Demo', '/Game/Demo/Maps'):
        if not unreal.EditorAssetLibrary.does_directory_exist(folder):
            unreal.EditorAssetLibrary.make_directory(folder)


def _ensure_graybox_map_asset():
    """
    若目标关卡资产不存在则复制并返回 True（表示本帧应结束，禁止继续布置）。
    已存在则返回 False。
    """
    _ensure_demo_maps_folder()
    if unreal.EditorAssetLibrary.does_asset_exist(DEST_MAP):
        unreal.log('已存在 ' + DEST_MAP + '，跳过复制。')
        return False
    if not unreal.EditorAssetLibrary.does_asset_exist(SOURCE_MAP):
        raise RuntimeError('找不到源关卡: ' + SOURCE_MAP)
    ok = unreal.EditorAssetLibrary.duplicate_asset(SOURCE_MAP, DEST_MAP)
    if not ok:
        raise RuntimeError('复制关卡失败: ' + SOURCE_MAP + ' -> ' + DEST_MAP)
    unreal.log('已创建 ' + DEST_MAP)
    return True


def _get_editor_world():
    sub = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    return sub.get_editor_world()


def _current_world_is_graybox(world):
    if not world:
        return False
    try:
        path = world.get_path_name()
        if path and GRAYBOX_TOKEN in path:
            return True
    except Exception:
        pass
    return False


def _destroy_prefixed_actors(world):
    if not world:
        return
    actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
    for a in actors:
        try:
            lab = a.get_actor_label()
        except Exception:
            continue
        if isinstance(lab, str) and lab.startswith(PREFIX):
            unreal.EditorLevelLibrary.destroy_actor(a)


def _spawn_static_cube(name_suffix, loc, scale, mesh):
    cls = _load_class('/Script/Engine.StaticMeshActor')
    if not cls:
        return None
    a = unreal.EditorLevelLibrary.spawn_actor_from_class(cls, loc, unreal.Rotator(0, 0, 0))
    if not a:
        return None
    a.set_actor_label(PREFIX + name_suffix)
    a.set_actor_scale3d(scale)
    try:
        smc = a.get_editor_property('static_mesh_component')
        smc.set_editor_property('static_mesh', mesh)
        smc.set_editor_property('mobility', unreal.ComponentMobility.STATIC)
    except Exception:
        unreal.log_warning('StaticMeshActor 设置网格失败: ' + name_suffix)
    return a


def _spawn_nav_bounds(loc, scale):
    cls = _load_class('/Script/NavigationSystem.NavMeshBoundsVolume')
    if not cls:
        return
    a = unreal.EditorLevelLibrary.spawn_actor_from_class(cls, loc, unreal.Rotator(0, 0, 0))
    if a:
        a.set_actor_label(PREFIX + 'NavBounds')
        a.set_actor_scale3d(scale)


def _spawn_demo_actor(class_path, name_suffix, loc, rot_yaw=0.0):
    cls = _load_class(class_path)
    if not cls:
        return None
    a = unreal.EditorLevelLibrary.spawn_actor_from_class(
        cls, loc, unreal.Rotator(0, rot_yaw, 0))
    if a:
        a.set_actor_label(PREFIX + name_suffix)
    return a


def _ensure_directional_light(world):
    lights = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.DirectionalLight)
    if lights:
        return
    cls = _load_class('/Script/Engine.DirectionalLight')
    if not cls:
        return
    a = unreal.EditorLevelLibrary.spawn_actor_from_class(
        cls, unreal.Vector(0, 0, 8000), unreal.Rotator(-45, 35, 0))
    if a:
        a.set_actor_label(PREFIX + 'SunFill')
        try:
            lc = a.light_component
            lc.set_editor_property('intensity', 8.0)
        except Exception:
            pass


def _ensure_skylight_and_fog(world):
    skies = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SkyLight)
    if not skies:
        cls = _load_class('/Script/Engine.SkyLight')
        if cls:
            a = unreal.EditorLevelLibrary.spawn_actor_from_class(
                cls, unreal.Vector(0, 0, 500), unreal.Rotator(0, 0, 0))
            if a:
                a.set_actor_label(PREFIX + 'SkyLight')
                try:
                    sl = a.light_component
                    sl.set_editor_property('mobility', unreal.ComponentMobility.MOVABLE)
                    sl.set_editor_property('intensity', 1.0)
                except Exception:
                    pass
    fogs = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.ExponentialHeightFog)
    if not fogs:
        cls = _load_class('/Script/Engine.ExponentialHeightFog')
        if cls:
            a = unreal.EditorLevelLibrary.spawn_actor_from_class(
                cls, unreal.Vector(0, 0, -200), unreal.Rotator(0, 0, 0))
            if a:
                a.set_actor_label(PREFIX + 'HeightFog')
                try:
                    fc = a.fog_component
                    fc.set_editor_property('fog_density', 0.02)
                    fc.set_editor_property('fog_max_opacity', 0.6)
                except Exception:
                    pass


def _optional_ambient(world):
    for path in (
        '/Game/StarterContent/Audio/Starter_Birds01',
        '/Game/StarterContent/Audio/Starter_Wind05',
    ):
        if not unreal.EditorAssetLibrary.does_asset_exist(path):
            continue
        sound = unreal.EditorAssetLibrary.load_asset(path)
        if not sound:
            continue
        cls = _load_class('/Script/Engine.AmbientSound')
        if not cls:
            return
        a = unreal.EditorLevelLibrary.spawn_actor_from_class(
            cls, GRAYBOX_ORIGIN + unreal.Vector(0, 0, 200), unreal.Rotator(0, 0, 0))
        if not a:
            return
        a.set_actor_label(PREFIX + 'Ambient')
        try:
            ac = a.audio_component
            ac.set_editor_property('sound', sound)
            ac.set_editor_property('volume_multiplier', 0.35)
        except Exception:
            pass
        unreal.log('已放置环境音: ' + path)
        return
    unreal.log('未找到 StarterContent 环境声资产，可稍后手动添加 AmbientSound。')


def _move_player_starts(world):
    for ps in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.PlayerStart):
        ps.set_actor_location(PLAYER_START, False, False)


def _place_zones(mesh):
    cont_cls = '/Script/DemoClient.DemoLootContainer'
    ext_cls = '/Script/DemoClient.DemoExtractionZone'

    idx = 0
    for dx in (-400, 0, 400):
        for dy in (-400, 0, 400):
            loc = unreal.Vector(float(dx), float(dy), 40.0)
            _spawn_demo_actor(cont_cls, 'Crate_HV_%d' % idx, loc, float(idx * 40))
            idx += 1
    ring_r = 1800.0
    for i in range(8):
        ang = (i / 8.0) * math.pi * 2.0
        loc = unreal.Vector(
            math.cos(ang) * ring_r, math.sin(ang) * ring_r, 40.0)
        _spawn_demo_actor(cont_cls, 'Crate_Mid_%d' % i, loc, float(i * 30))
    for i in range(6):
        ang = (i / 6.0) * math.pi * 2.0
        r = 3200.0
        loc = unreal.Vector(math.cos(ang) * r, math.sin(ang) * r, 40.0)
        _spawn_demo_actor(cont_cls, 'Crate_Out_%d' % i, loc, float(i * 50))

    corners = (
        ('Ext_N', unreal.Vector(0, 3800, 20), 180.0, '北侧撤离点'),
        ('Ext_S', unreal.Vector(0, -3800, 20), 0.0, '南侧撤离点'),
        ('Ext_E', unreal.Vector(3800, 0, 20), -90.0, '东侧撤离点'),
        ('Ext_W', unreal.Vector(-3800, 0, 20), 90.0, '西侧撤离点'),
    )
    for key, loc, yaw, title in corners:
        z = _spawn_demo_actor(ext_cls, key, loc, yaw)
        if z:
            try:
                z.set_editor_property('zone_name', unreal.Text(title))
            except Exception:
                pass

    wall_z = unreal.Vector(0, 0, 250.0)
    wall_scale = unreal.Vector(1.2, 12.0, 5.0)
    span = 4000.0
    _spawn_static_cube(
        'Wall_N',
        unreal.Vector(0, span, wall_z.z) + GRAYBOX_ORIGIN,
        wall_scale, mesh)
    _spawn_static_cube(
        'Wall_S',
        unreal.Vector(0, -span, wall_z.z) + GRAYBOX_ORIGIN,
        wall_scale, mesh)
    _spawn_static_cube(
        'Wall_E',
        unreal.Vector(span, 0, wall_z.z) + GRAYBOX_ORIGIN,
        unreal.Vector(12.0, 1.2, 5.0), mesh)
    _spawn_static_cube(
        'Wall_W',
        unreal.Vector(-span, 0, wall_z.z) + GRAYBOX_ORIGIN,
        unreal.Vector(12.0, 1.2, 5.0), mesh)


def main():
    unreal.log('=== 里程碑5 灰盒关卡脚本 开始 ===')

    just_created = _ensure_graybox_map_asset()
    if just_created:
        unreal.log('=== 第 1 步完成 ===')
        unreal.log('请勿在同一时刻自动切关。请现在在「内容浏览器」中双击打开：')
        unreal.log('  ' + DEST_MAP)
        unreal.log('打开该关卡后，请再次执行本脚本以完成布置（地板、Nav、物资、撤离点等）。')
        return

    world = _get_editor_world()
    if not world:
        raise RuntimeError('无法获取编辑器世界。')

    if not _current_world_is_graybox(world):
        unreal.log_error(
            '当前打开的关卡不是灰盒图（路径中需包含 ' + GRAYBOX_TOKEN + '）。'
            '请打开 L_DemoExtractionGraybox 后再运行，以免误改其它地图。')
        unreal.log_error('若你尚未创建灰盒关卡，先运行一次本脚本仅做复制，再打开新关卡后运行第二次。')
        return

    _destroy_prefixed_actors(world)

    cube_mesh = unreal.EditorAssetLibrary.load_asset('/Engine/BasicShapes/Cube')
    if not cube_mesh:
        cube_mesh = unreal.EditorAssetLibrary.load_asset('/Game/LevelPrototyping/Meshes/SM_Cube')
    if not cube_mesh:
        raise RuntimeError('找不到用于地板的静态网格（Cube）。')

    _spawn_static_cube(
        'FloorMain',
        unreal.Vector(GRAYBOX_ORIGIN.x, GRAYBOX_ORIGIN.y, FLOOR_Z),
        unreal.Vector(85.0, 85.0, 0.25), cube_mesh)

    _spawn_nav_bounds(
        unreal.Vector(GRAYBOX_ORIGIN.x, GRAYBOX_ORIGIN.y, 0.0),
        unreal.Vector(95.0, 95.0, 12.0))

    _place_zones(cube_mesh)
    _move_player_starts(world)
    _ensure_directional_light(world)
    _ensure_skylight_and_fog(world)
    _optional_ambient(world)

    _run_console(world, 'RebuildNavigation')

    unreal.log('=== 布置完成。请按 Ctrl+S 保存当前关卡（不再调用 save_dirty_packages，避免编辑器包状态异常）。 ===')
    unreal.log('建议：在 BP_DemoGameMode 中将 AISpawnCenter=(0,0,100)、AISpawnRadius=3500、AIPatrolRadius=800。')
    unreal.log('可选：项目设置 -> 地图和模式 中将默认地图改为灰盒关卡。')


main()
