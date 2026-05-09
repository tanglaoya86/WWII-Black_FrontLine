import ctypes
import sys
import json
import os
from ctypes import wintypes

# 取消pg提示
os.environ['PYGAME_HIDE_SUPPORT_PROMPT'] = '1'  # 必须放在 import pygame 之前

import pygame as pg

dll_dir = os.path.join(os.path.dirname(__file__), "src/native/cpp/dll")
os.add_dll_directory(dll_dir)

dll_path = os.path.join(os.path.dirname(__file__), "src/native/cpp/dll/DllChecker.dll")
# 然后尝试加载
try:
    dll_checker = ctypes.CDLL(dll_path)
    print("加载成功")
except Exception as e:
    print("加载失败:", e)


# 指定游戏目录（例如当前目录，或者从外部传入）
game_dir = os.getcwd()


# 初始化pg
pg.init()

# 创建窗口为变量window
window = pg.display.set_mode((800, 600))

# 加载图片为变量icon
icon = pg.image.load('assets/images/ui/window.png')

# 设置名字和图标
pg.display.set_caption("二战异变")
pg.display.set_icon(icon)

# ////////////准备循环////////////

# 创建帧率控制对象
clock = pg.time.Clock()
# 坐标
x, y = 0, 0

# 帧率文本    转循环中

# 文本  f:文本拼接    .1f：保留一位小数    转循环中

# ---------- 字体（使用微软雅黑，解决中文乱码）----------
font_path = 'C:/Windows/Fonts/msyh.ttc'
if os.path.exists(font_path):
    font = pg.font.Font(font_path, 15)
else:
    font = pg.font.Font(None, 15)  # 回退，但中文会乱码
# ---------- 加载界面用的大号字体 ----------
title_font = pg.font.Font(font_path, 36) if os.path.exists(font_path) else pg.font.Font(None, 36)

# 渲染文本       转循环中


# 判断循环是否继续的变量
isRunning = True


# ========== 加载界面：三张图片（直接替换，不滚动） ==========
def load_and_scale_bg(filename, target_size):
    """加载图片并缩放到目标尺寸，若文件不存在则生成色块"""
    if os.path.exists(filename):
        img = pg.image.load(filename).convert()
        return pg.transform.scale(img, target_size)
    else:
        # 生成一个彩色替代图片，便于调试
        surf = pg.Surface(target_size)
        surf.fill((100, 100, 100))
        return surf


# 加载三张 loading 图片
loading_images = []
loading_paths = [
    'assets/images/ui/loading1.png',
    'assets/images/ui/loading2.png',
    'assets/images/ui/loading3.png'
]
window_size = (800, 600)

for path in loading_paths:
    loading_images.append(load_and_scale_bg(path, window_size))


# ========== 进度条绘制函数（圆角、细条、底部） ==========
def draw_progress_bar(progress, y_offset=0):
    """
    progress: 0.0 ~ 1.0
    y_offset: 相对于屏幕底部向上偏移的像素
    """
    bar_width = 600
    bar_height = 12
    bar_x = (window.get_width() - bar_width) // 2
    bar_y = window.get_height() - 40 - y_offset  # 底部上方40像素

    # 圆角背景（pygame 2.0.1+ 支持 border_radius）
    pg.draw.rect(window, (60, 60, 60), (bar_x, bar_y, bar_width, bar_height), border_radius=6)
    fill_w = int(bar_width * min(max(progress, 0.0), 1.0))
    if fill_w > 0:
        pg.draw.rect(window, (0, 200, 0), (bar_x, bar_y, fill_w, bar_height), border_radius=6)
    # 边框
    pg.draw.rect(window, (200, 200, 200), (bar_x, bar_y, bar_width, bar_height), 2, border_radius=6)

    # 显示百分比文字（中文）
    percent_text = font.render(f"加载中 {int(progress * 100)}%", True, 'yellow')
    text_rect = percent_text.get_rect(center=(window.get_width() // 2, bar_y - 20))
    window.blit(percent_text, text_rect)


# ========== 模拟加载过程（进度条从0%到100%，图片分段直接替换） ==========
total_steps = 300  # 增加步数，延长加载时间（约10秒）
for step in range(total_steps + 1):
    # 加载过程中不允许退出（不处理任何事件）
    # 如果你还是希望允许退出，可以取消下面代码块的注释，但根据要求已禁用
    # for ev in pg.event.get():
    #     if ev.type == pg.QUIT:
    #         pg.quit()
    #         exit()
    #     elif ev.type == pg.KEYDOWN:
    #         if ev.key == pg.K_ESCAPE:
    #             pg.quit()
    #             exit()

    progress = step / total_steps

    # 根据进度选择显示哪一张图片（直接替换）
    if progress < 0.33:
        current_img = loading_images[0]  # loading1.png
    elif progress < 0.66:
        current_img = loading_images[1]  # loading2.png
    else:
        current_img = loading_images[2]  # loading3.png

    # 绘制当前背景图片
    window.blit(current_img, (0, 0))

    # 绘制进度条
    draw_progress_bar(progress, y_offset=0)

    pg.display.flip()
    clock.tick(30)  # 控制加载动画速度

# 加载完成，进入主循环（游戏内容已修改为主菜单，仿红警2风格）
# ========== 加载完成 ==========

# ========== 主菜单初始化（新增部分） ==========
# 定义雷达区域（蓝色矩形，放在左侧）
radar_rect = pg.Rect(0, 0, 600, 600)  # 占据整个左侧区域，宽200

# 定义按钮属性（仿照红警2主菜单按钮排列，从上到下）
buttons = [
    {"text": "单人游戏",   "target_x": 610},   # 最终水平位置
    {"text": "局域网", "target_x": 610},
    {"text": "制作人员",     "target_x": 610},
    {"text": "选项",   "target_x": 610},
    {"text": "退出",     "target_x": 610}
]

# 按钮通用尺寸
BTN_WIDTH  = 180
BTN_HEIGHT = 40
# 按钮之间的垂直间距
BTN_GAP    = 15
# 第一个按钮距离顶部的偏移
BTN_TOP    = 120

# 计算每个按钮的矩形（垂直排列）
# 新增：单独确定退出按钮的y坐标（放置在屏幕最下方）
exit_btn_margin_bottom = 10  # 退出按钮距窗口下边缘的距离
for i, btn in enumerate(buttons):
    if btn["text"].startswith("退出"):
        # 退出按钮放在窗口底部
        btn_y = window.get_height() - BTN_HEIGHT - exit_btn_margin_bottom
    else:
        btn_y = BTN_TOP + i * (BTN_HEIGHT + BTN_GAP)
    # 初始时不绘制实际矩形，先设为占位（后续动画会更新）
    btn["y"] = btn_y
    # 动画开始时按钮在屏幕右侧之外
    btn["current_x"] = 850  # 屏幕宽度800，初始位置在右侧外
    btn["rect"] = pg.Rect(btn["current_x"], btn_y, BTN_WIDTH, BTN_HEIGHT)

# 入场动画控制
animation_start_time = pg.time.get_ticks()  # 记录动画开始时刻
ANIMATION_DURATION = 400  # 动画总时长（毫秒）
BUTTON_DELAY = 20       # 每个按钮延迟入场的时间（毫秒）
animation_finished = False  # 动画是否已完成

# 确认对话框状态
show_confirm = False

# 辅助函数：绘制一个按钮（带悬停高亮）
def draw_button(btn, mouse_pos):
    """根据按钮当前状态绘制红色矩形和文字，鼠标悬停时变亮"""
    # 基础颜色：深红色
    base_color = (180, 0, 0)
    # 悬停颜色：更亮的红色
    hover_color = (255, 50, 50)

    rect = btn["rect"]
    # 判断鼠标是否悬停在按钮上
    is_hover = rect.collidepoint(mouse_pos)
    color = hover_color if is_hover else base_color

    # 绘制按钮主体（红色矩形）
    pg.draw.rect(window, color, rect, border_radius=4)
    # 绘制按钮边框（白色细线）
    pg.draw.rect(window, (255, 255, 255), rect, 2, border_radius=4)

    # 绘制按钮文字（改为黄色）
    text_surf = font.render(btn["text"], True, (255, 255, 0))
    text_rect = text_surf.get_rect(center=rect.center)
    window.blit(text_surf, text_rect)

# 辅助函数：绘制退出确认对话框，并返回“是”和“否”按钮的矩形区域
def draw_confirm_dialog():
    """在屏幕中央绘制确认退出对话框，返回(是按钮矩形, 否按钮矩形)"""
    # 半透明黑色遮罩（覆盖整个屏幕）
    mask = pg.Surface((800, 600), pg.SRCALPHA)
    mask.fill((0, 0, 0, 180))  # 半透明黑色
    window.blit(mask, (0, 0))

    # 对话框背景白色矩形
    dlg_rect = pg.Rect(200, 200, 400, 200)
    pg.draw.rect(window, (240, 240, 240), dlg_rect, border_radius=8)
    pg.draw.rect(window, (100, 100, 100), dlg_rect, 3, border_radius=8)

    # 提示文字
    confirm_text = title_font.render("确认退出？", True, (0, 0, 0))
    confirm_rect = confirm_text.get_rect(center=(400, 250))
    window.blit(confirm_text, confirm_rect)

    # “是”按钮
    yes_rect = pg.Rect(240, 310, 120, 40)
    yes_hover = yes_rect.collidepoint(pg.mouse.get_pos())
    pg.draw.rect(window, (200, 50, 50) if yes_hover else (150, 0, 0), yes_rect, border_radius=4)
    pg.draw.rect(window, (255, 255, 255), yes_rect, 2, border_radius=4)
    yes_text = font.render("是", True, (255, 255, 255))
    window.blit(yes_text, yes_text.get_rect(center=yes_rect.center))

    # “否”按钮
    no_rect = pg.Rect(440, 310, 120, 40)
    no_hover = no_rect.collidepoint(pg.mouse.get_pos())
    pg.draw.rect(window, (200, 50, 50) if no_hover else (150, 0, 0), no_rect, border_radius=4)
    pg.draw.rect(window, (255, 255, 255), no_rect, 2, border_radius=4)
    no_text = font.render("否", True, (255, 255, 255))
    window.blit(no_text, no_text.get_rect(center=no_rect.center))

    return yes_rect, no_rect

# ////////////准备循环////////////

# 游戏主循环（已修改为主菜单，保留退出事件并添加菜单交互）
while isRunning:
    # 处理事件
    # 将事件处理计为ev
    for ev in pg.event.get():
        # 如果按下退出键
        if ev.type == pg.QUIT:
            # 退出
            isRunning = False
            break
        # 如果键盘被按下
        elif ev.type == pg.KEYDOWN:
            # 如果是esc
            if ev.key == pg.K_ESCAPE:
                # 切换退出确认对话框状态
                show_confirm = not show_confirm
        # 如果鼠标左键被按下（修正了之前可能出现的缩进问题）
        elif ev.type == pg.MOUSEBUTTONDOWN and ev.button == 1:
            mouse_pos = ev.pos
            if show_confirm:
                # 处于退出确认对话框，直接用写死的矩形位置判断（与draw_confirm_dialog内一致）
                yes_rect_check = pg.Rect(240, 310, 120, 40)
                no_rect_check = pg.Rect(440, 310, 120, 40)
                if yes_rect_check.collidepoint(mouse_pos):
                    isRunning = False
                    break
                elif no_rect_check.collidepoint(mouse_pos):
                    show_confirm = False
            else:
                # 不在确认对话框，检测菜单按钮点击
                for btn in buttons:
                    if btn["rect"].collidepoint(mouse_pos):
                        if btn["text"].startswith("退出"):
                            # 点击退出按钮，打开确认对话框
                            show_confirm = True
                        # 其他按钮无任何反应（但可检测，留空）
                        break  # 一次点击只处理一个按钮
        ''' #这样无法长按 或者要循环前加上pg.key.set_repeat(延迟,间隔)
            # 如果是w
            if ev.key == pg.K_DOWN:
                y += 5
        '''

    # ---------- 更新按钮入场动画 ----------
    if not animation_finished:
        current_time = pg.time.get_ticks()
        elapsed = current_time - animation_start_time

        # 检查总动画时长是否结束
        if elapsed >= ANIMATION_DURATION + (len(buttons)-1) * BUTTON_DELAY:
            # 动画结束，所有按钮强制设置到最终位置
            for btn in buttons:
                btn["current_x"] = btn["target_x"]
                btn["rect"].x = btn["target_x"]
            animation_finished = True
        else:
            # 逐个更新按钮位置
            for i, btn in enumerate(buttons):
                # 该按钮的延迟时间
                delay = i * BUTTON_DELAY
                if elapsed < delay:
                    # 还未到出场时间，保持在屏幕外
                    btn["current_x"] = 850
                else:
                    # 计算局部动画进度
                    local_elapsed = elapsed - delay
                    local_duration = ANIMATION_DURATION
                    progress = min(local_elapsed / local_duration, 1.0)
                    # 使用缓入缓出效果（简单的ease_out_cubic）
                    eased = 1 - (1 - progress) ** 3
                    start_x = 850
                    target_x = btn["target_x"]
                    btn["current_x"] = start_x + (target_x - start_x) * eased
                    btn["rect"].x = int(btn["current_x"])

    # ---------- 绘制菜单界面 ----------
    # 游戏画面背景纯黑
    window.fill('black')

    # 绘制蓝色雷达矩形（左侧）
    pg.draw.rect(window, (0, 0, 180), radar_rect)       # 深蓝色填充
    pg.draw.rect(window, (0, 150, 255), radar_rect, 3)  # 亮蓝色边框

    # 新增：在按钮区域上方绘制绿色矩形（占满右侧顶部直到第一个按钮上方）
    green_rect_x = 610  # 按钮区域起始x
    green_rect_y = 10
    green_rect_w = 180   # 右侧剩余宽度
    green_rect_h = 100  # 高度为按钮顶部偏移量
    pg.draw.rect(window, (0, 160, 0), (green_rect_x, green_rect_y, green_rect_w, green_rect_h))

    # 获取当前鼠标位置，用于按钮悬停效果
    mouse_pos = pg.mouse.get_pos()

    # //////////////绘图部分////////////////
    # 绘制所有主菜单按钮
    for btn in buttons:
        draw_button(btn, mouse_pos)

    # 如果需要显示退出确认对话框，覆盖在最上层
    if show_confirm:
        draw_confirm_dialog()

    # //////////////绘图部分////////////////
    # 更新窗口
    pg.display.update()

    # 设置帧率
    clock.tick(60)

    # 获取帧率
    # print('FPS:',clock.get_fps())

# 清理pg
pg.quit()