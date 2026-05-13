# -*- coding: utf-8 -*-
"""
构建 / 重建 WBP_LobbyRoot 控件树（名称对齐 UDemoLobbyRootWidget 的 BindWidgetOptional）。

【首次】
1) 编译 C++；类设置父类 = Demo Lobby Root Widget。
2) 设计器拖 Overlay 为根并命名为 RootOverlay。
3) 运行本脚本；编译并保存蓝图。

【仅更新登录区外观】可再次运行：会清空 PanelLogin 子控件后重建；PanelLobby 若已存在则保留。
"""
import unreal

ASSET = "/Game/UI/Lobby/WBP_LobbyRoot.WBP_LobbyRoot"
ASSET_SAVE = "/Game/UI/Lobby/WBP_LobbyRoot"


def _new(outer, cls, name):
    return unreal.new_object(cls, outer, name=str(name))


def _txt(s):
    return unreal.Text(str(s))


def _find_widget_tree(wbp):
    return unreal.find_object(wbp, "WidgetTree")


def _find_root_visual(wt):
    if not wt:
        return None
    r = unreal.find_object(wt, "RootWidget")
    if r:
        return r
    r = unreal.find_object(wt, "RootOverlay")
    if r:
        return r
    for obj in unreal.ObjectIterator(unreal.Widget):
        try:
            if obj and obj.get_outer() == wt:
                return obj
        except Exception:
            pass
    return None


def _add_to_overlay(overlay, widget):
    overlay.add_child(widget)
    slot = widget.slot
    if slot:
        try:
            slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
            slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_FILL)
        except Exception:
            pass


def _add_to_panel_root(root, widget):
    cname = root.get_class().get_name()
    if cname == "Overlay":
        _add_to_overlay(root, widget)
        return True
    try:
        root.add_child(widget)
        return True
    except Exception:
        unreal.log_error("BuildWBP_LobbyRootHierarchy: cannot add_child to root type " + cname)
        return False


def _center_on_overlay(overlay, widget):
    overlay.add_child(widget)
    slot = widget.slot
    if slot:
        try:
            slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER)
            slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
        except Exception:
            pass


def _ensure_parent_demo_lobby_root(wbp):
    want = unreal.load_class(None, "/Script/DemoClient.DemoLobbyRootWidget")
    if not want:
        unreal.log_error("BuildWBP_LobbyRootHierarchy: 未找到 C++ 类 DemoLobbyRootWidget，请先编译 DemoClient 模块。")
        return False
    wb = unreal.WidgetBlueprint.cast(wbp)
    if not wb:
        unreal.log_error("BuildWBP_LobbyRootHierarchy: 资产不是 WidgetBlueprint。")
        return False
    try:
        cur = wb.get_editor_property("ParentClass")
        if cur == want:
            return True
    except Exception:
        pass
    try:
        unreal.BlueprintEditorLibrary.reparent_blueprint(wb, want)
        unreal.log_warning("BuildWBP_LobbyRootHierarchy: 已将蓝图父类改为 DemoLobbyRootWidget，请编译。")
        return True
    except Exception as e:
        unreal.log_error(
            "BuildWBP_LobbyRootHierarchy: 自动改父类失败（"
            + str(e)
            + "）。请手动：类设置 → 父类 = DemoLobbyRootWidget。"
        )
        return False


def _try_compile_widget_blueprint(wbp):
    wb = unreal.WidgetBlueprint.cast(wbp)
    if not wb:
        return
    try:
        unreal.KismetSystemLibrary.compile_blueprint(wb)
    except Exception:
        try:
            unreal.BlueprintEditorLibrary.compile_blueprint(wb)
        except Exception:
            pass


def _font_size_only(widget, size):
    """
    只改字号，保留控件默认 Font（含项目/引擎的中文字体回退）。
    勿用空的 SlateFontInfo()+set_font，否则会丢掉 CJK 字形，出现豆腐块。
    """
    if not widget or size is None:
        return
    try:
        fi = widget.get_editor_property("Font")
        if fi is None:
            return
        fi.size = int(size)
        widget.set_editor_property("Font", fi)
    except Exception:
        pass


def _tb(wt, outer, name, text, size=14, color=None):
    t = _new(outer, unreal.TextBlock, name)
    try:
        t.set_text(_txt(text))
    except Exception:
        try:
            t.set_editor_property("Text", _txt(text))
        except Exception:
            pass
    _font_size_only(t, size)
    if color is not None:
        try:
            t.set_color_and_opacity(color)
        except Exception:
            pass
    return t


def _hint_et(wt, outer, name, hint, is_password=False):
    et = _new(outer, unreal.EditableTextBox, name)
    try:
        et.set_editor_property("HintText", _txt(hint))
    except Exception:
        pass
    try:
        et.set_is_password(is_password)
    except Exception:
        try:
            et.set_editor_property("IsPassword", is_password)
        except Exception:
            pass
    try:
        et.set_editor_property("MinDesiredWidth", 400.0)
    except Exception:
        pass
    return et


def _vb_pad_slot(widget, top=0, bottom=10, left=0, right=0):
    slot = widget.slot
    if slot:
        try:
            slot.set_padding(unreal.Margin(left=float(left), top=float(top), right=float(right), bottom=float(bottom)))
        except Exception:
            pass


def _sized_button(outer_tree, outer, name, label_text, min_h=50):
    sb = _new(outer, unreal.SizeBox, name + "_SizeWrap")
    try:
        sb.set_min_desired_height(float(min_h))
    except Exception:
        pass
    btn = _new(outer, unreal.Button, name)
    tb = _new(btn, unreal.TextBlock, "Label")
    try:
        tb.set_text(_txt(label_text))
    except Exception:
        try:
            tb.set_editor_property("Text", _txt(label_text))
        except Exception:
            pass
    _font_size_only(tb, 17)
    try:
        tb.set_color_and_opacity(unreal.LinearColor(0.95, 0.96, 0.98, 1.0))
    except Exception:
        pass
    btn.add_child(tb)
    sb.add_child(btn)
    return sb


def _build_login_panel(wt, root, vb_login):
    try:
        vb_login.clear_children()
    except Exception as e:
        unreal.log_error("BuildWBP_LobbyRootHierarchy: clear_children(PanelLogin) 失败: " + str(e))
        return

    c_title = unreal.LinearColor(0.98, 0.99, 1.0, 1.0)
    c_muted = unreal.LinearColor(0.55, 0.58, 0.65, 1.0)
    c_err = unreal.LinearColor(1.0, 0.35, 0.38, 1.0)
    c_label = unreal.LinearColor(0.72, 0.75, 0.82, 1.0)

    sb_outer = _new(wt, unreal.SizeBox, "LoginCardSize")
    try:
        sb_outer.set_width_override(460.0)
    except Exception:
        pass

    border = _new(wt, unreal.Border, "LoginCardBorder")
    try:
        border.set_brush_color(unreal.LinearColor(0.07, 0.08, 0.11, 0.94))
    except Exception:
        pass
    try:
        border.set_padding(unreal.Margin(28.0, 28.0, 28.0, 24.0))
    except Exception:
        pass

    inner = _new(wt, unreal.VerticalBox, "LoginInnerVB")

    t1 = _tb(wt, wt, "TextAuthTitle", "欢迎", 26, c_title)
    inner.add_child(t1)
    _vb_pad_slot(t1, 0, 6, 0, 0)

    t2 = _tb(wt, wt, "TextAuthSubtitle", "登录已有角色，或注册新昵称与密码（密码仅本地校验）", 13, c_muted)
    inner.add_child(t2)
    _vb_pad_slot(t2, 0, 22, 0, 0)

    la = _tb(wt, wt, "LblDisplayName", "显示名称", 13, c_label)
    inner.add_child(la)
    _vb_pad_slot(la, 0, 4, 0, 0)
    et_name = _hint_et(wt, wt, "EditableDisplayName", "输入游戏中显示的名称", False)
    inner.add_child(et_name)
    _vb_pad_slot(et_name, 0, 14, 0, 0)

    lb = _tb(wt, wt, "LblPassword", "密码", 13, c_label)
    inner.add_child(lb)
    _vb_pad_slot(lb, 0, 4, 0, 0)
    et_p = _hint_et(wt, wt, "EditablePassword", "至少 4 位", True)
    inner.add_child(et_p)
    _vb_pad_slot(et_p, 0, 14, 0, 0)

    lc = _tb(wt, wt, "LblPasswordConfirm", "确认密码（注册时）", 13, c_label)
    inner.add_child(lc)
    _vb_pad_slot(lc, 0, 4, 0, 0)
    et_c = _hint_et(wt, wt, "EditablePasswordConfirm", "再次输入密码", True)
    inner.add_child(et_c)
    _vb_pad_slot(et_c, 0, 18, 0, 0)

    hb = _new(wt, unreal.HorizontalBox, "AuthButtonRow")
    b_login = _sized_button(wt, wt, "ButtonLogin", "登录", 50)
    hb.add_child(b_login)
    ls = b_login.slot
    if ls:
        try:
            ls.set_padding(unreal.Margin(0, 0, 10, 0))
        except Exception:
            pass
    b_reg = _sized_button(wt, wt, "ButtonRegister", "注册并进入", 50)
    hb.add_child(b_reg)
    inner.add_child(hb)
    _vb_pad_slot(hb, 4, 12, 0, 0)

    terr = _tb(wt, wt, "TextAuthError", "", 13, c_err)
    try:
        terr.set_visibility(unreal.SlateVisibility.COLLAPSED)
    except Exception:
        pass
    inner.add_child(terr)
    _vb_pad_slot(terr, 0, 0, 0, 0)

    border.add_child(inner)
    sb_outer.add_child(border)
    vb_login.add_child(sb_outer)
    _vb_pad_slot(sb_outer, 0, 0, 0, 0)


def _build_lobby_panel(wt, root):
    vb_lobby = _new(wt, unreal.VerticalBox, "PanelLobby")
    try:
        vb_lobby.set_visibility(unreal.SlateVisibility.COLLAPSED)
    except Exception:
        pass

    hb_tabs = _new(wt, unreal.HorizontalBox, "TabRow")
    hb_tabs.add_child(_sized_button(wt, wt, "ButtonTabStash", "仓库", 40))
    hb_tabs.add_child(_sized_button(wt, wt, "ButtonTabTrade", "交易行", 40))
    vb_lobby.add_child(hb_tabs)
    _vb_pad_slot(hb_tabs, 0, 10, 0, 0)

    sw = _new(wt, unreal.WidgetSwitcher, "SwitcherLobbySection")
    t_stash = _tb(wt, wt, "TxtStashPlaceholder", "仓库占位（接 PlayerState）", 15, None)
    t_trade = _tb(wt, wt, "TxtTradePlaceholder", "交易行占位", 15, None)
    sw.add_child(t_stash)
    sw.add_child(t_trade)
    vb_lobby.add_child(sw)
    _vb_pad_slot(sw, 0, 12, 0, 0)

    btn_add = _sized_button(wt, wt, "ButtonAddStashDemo", "加测试物品", 44)
    vb_lobby.add_child(btn_add)
    _vb_pad_slot(btn_add, 0, 10, 0, 0)
    btn_match = _sized_button(wt, wt, "ButtonStartMatch", "开始匹配", 50)
    vb_lobby.add_child(btn_match)
    if not _add_to_panel_root(root, vb_lobby):
        return False
    return True


def main():
    wbp = unreal.load_asset(ASSET)
    if not wbp:
        unreal.log_error("BuildWBP_LobbyRootHierarchy: load failed " + ASSET)
        return

    _ensure_parent_demo_lobby_root(wbp)

    wt = _find_widget_tree(wbp)
    if not wt:
        unreal.log_error("BuildWBP_LobbyRootHierarchy: WidgetTree not found")
        return

    root = _find_root_visual(wt)
    if not root:
        unreal.log_error(
            "BuildWBP_LobbyRootHierarchy: 未找到根 Overlay。请先在设计器从控制板拖入 Overlay 到画布，"
            "并命名为 RootOverlay，保存后再运行本脚本。"
        )
        return

    vb_login = unreal.find_object(root, "PanelLogin")
    if not vb_login:
        vb_login = _new(wt, unreal.VerticalBox, "PanelLogin")
        _center_on_overlay(root, vb_login)
    else:
        unreal.log_warning("BuildWBP_LobbyRootHierarchy: 重建 PanelLogin 内控件。")
        ls = vb_login.slot
        if ls and root.get_class().get_name() == "Overlay":
            try:
                ls.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER)
                ls.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
            except Exception:
                pass

    _build_login_panel(wt, root, vb_login)

    if not unreal.find_object(root, "PanelLobby"):
        if not _build_lobby_panel(wt, root):
            return
    else:
        unreal.log("BuildWBP_LobbyRootHierarchy: 已存在 PanelLobby，跳过大厅区生成。")

    try:
        unreal.EditorAssetLibrary.save_asset(ASSET_SAVE)
    except Exception:
        try:
            unreal.EditorAssetLibrary.save_loaded_asset(wbp)
        except Exception:
            pass

    _try_compile_widget_blueprint(wbp)
    unreal.log_warning("BuildWBP_LobbyRootHierarchy: 完成。请 Compile / Save WBP_LobbyRoot。")


main()
