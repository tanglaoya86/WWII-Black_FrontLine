# 自适应部分是ai写的，这个注释我已经尽力了，看不懂后面再补


import os

# 取消控制台输出
os.environ['PYGAME_HIDE_SUPPORT_PROMPT'] = '1'
import pygame
import ctypes
import sys
import json

# ======================= 路径设置 =======================
current_dir = os.path.dirname(os.path.abspath(__file__))
dll_dir = os.path.join(current_dir, "src","native", "cpp", "dll")
image_dir = os.path.join(current_dir, 'assets/images/ui')
os.add_dll_directory(dll_dir)
font_dir = os.path.join(current_dir, 'assets/fonts')

# 文件对比文档
dlldata_path = os.path.join(dll_dir, "dlldata.json")
with open(dlldata_path, "r", encoding="utf-8") as f:
    CORRECT_DLL_SIZE = json.load(f)

# ======================= 屏幕自适应核心 =======================
user32 = ctypes.windll.user32
screen_width = user32.GetSystemMetrics(0)
screen_height = user32.GetSystemMetrics(1)

# 基准分辨率：1366*768
BASE_W = 1366
BASE_H = 768

# 等比例缩放系数
scale = min(screen_width / BASE_W, screen_height / BASE_H)

# 自适应函数
def ad(val):
    return int(val * scale)


# ======================= Pygame 初始化 =======================
pygame.init()
screen = pygame.display.set_mode((screen_width, screen_height), pygame.FULLSCREEN)
pygame.display.set_caption("二战：黑色前线")
icon = pygame.image.load(os.path.join(image_dir, "window.png"))
progress_bar = pygame.image.load(os.path.join(image_dir, "progress_bar.png")).convert_alpha()
progress_bar = pygame.transform.scale(progress_bar, (screen_width, ad(100)))
pygame.display.set_icon(icon)

# 自适应字体
font = pygame.font.Font(os.path.join(font_dir, 'NotoSansSC-Regular.ttf'), ad(22))
clock = pygame.time.Clock()


# ======================= DLL检查 =======================
def check_dlls():
    dll_files = ["DllChecker.dll", "libwinpthread-1.dll"]
    result = []
    for name in dll_files:
        path = os.path.join(dll_dir, name)
        exists = os.path.exists(path)
        size = os.path.getsize(path) if exists else 0
        result.append({"name": name, "exists": exists, "file_size": str(size)})
    return result


# ======================= 加载界面（自适应）=======================
def loading_screen():
    dll_list = check_dlls()
    total = len(dll_list)
    for i, dll in enumerate(dll_list):
        progress = (i + 1) / total
        percent = int(progress * 100)
        screen.fill((0, 0, 0))

        check_text = font.render(f"检查：{dll['name']}", True, 'red')
        screen.blit(check_text, (ad(10), screen_height - ad(130)))

        progress_text = font.render(f"加载 {percent}%", True, 'red')
        screen.blit(progress_bar, (0, screen_height - ad(100)))
        screen.blit(progress_text, (screen_width - ad(160), screen_height - ad(50)))

        pygame.display.flip()
        clock.tick(3)


# ======================= 主菜单 =======================
def main_menu():       #按钮位置
    btn_w = ad(280)
    btn_h = ad(60)
    btn_gap = ad(20)
    btn_start_y = ad(190)

    buttons = [
        {"name": "单人游戏", "y": btn_start_y + 0 * (btn_h + btn_gap)},
        {"name": "局域网", "y": btn_start_y + 1 * (btn_h + btn_gap)},
        {"name": "制作人员", "y": btn_start_y + 2 * (btn_h + btn_gap)},
        {"name": "选项", "y": btn_start_y + 3 * (btn_h + btn_gap)},
        {"name": "退出", "y": screen_height - btn_h - ad(10)},
    ]

# 按钮左对齐
    for btn in buttons:
        btn["x"] = screen_width + ad(50)
        btn["rect"] = pygame.Rect(btn["x"], btn["y"], btn_w, btn_h)

    start_time = pygame.time.get_ticks()
    show_exit_confirm = False


    # 主循环
    while True:
        mouse_pos = pygame.mouse.get_pos()
        # 几个退出的判断
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                pygame.quit()
                sys.exit()
            if event.type == pygame.MOUSEBUTTONDOWN and event.button == 1:
                if show_exit_confirm:
                    yes_rect = pygame.Rect(screen_width // 2 - ad(160), screen_height // 2 + ad(10), ad(120), ad(40))
                    no_rect = pygame.Rect(screen_width // 2 + ad(40), screen_height // 2 + ad(10), ad(120), ad(40))
                    if yes_rect.collidepoint(mouse_pos):
                        pygame.quit()
                        sys.exit()
                    if no_rect.collidepoint(mouse_pos):
                        show_exit_confirm = False
                else:
                    for btn in buttons:
                        if btn["rect"].collidepoint(mouse_pos) and btn["name"] == "退出":
                            show_exit_confirm = True

        time_now = pygame.time.get_ticks()
        elapsed = time_now - start_time

        # 动画速度自适应
        base_speed = 600
        anim_speed = base_speed / scale

        for idx, btn in enumerate(buttons):
            delay = idx * ad(20)
            if elapsed < delay:
                btn["x"] = screen_width + ad(50)
            else:
                t = min((elapsed - delay) / anim_speed, 1.0)
                btn["x"] = screen_width - ad(360) + (ad(50) + screen_width * (1 - t))
            btn["rect"].x = int(btn["x"])

        # 绘制的主菜单矩形
        screen.fill((0, 0, 0))
        pygame.draw.rect(screen, (0, 0, 180), (0, 0, screen_width * 0.75, screen_height))
        pygame.draw.rect(screen, (0, 150, 255), (0, 0, screen_width * 0.75, screen_height), ad(3))
        pygame.draw.rect(screen, (0, 160, 0), (screen_width - ad(310), ad(15), ad(280), ad(150)))


        # 剩下的几个确认退出按钮
        for btn in buttons:
            hover = btn["rect"].collidepoint(mouse_pos)
            color = (255, 60, 60) if hover else (180, 0, 0)
            pygame.draw.rect(screen, color, btn["rect"], border_radius=ad(4))
            pygame.draw.rect(screen, (255, 255, 255), btn["rect"], ad(2), border_radius=ad(4))
            text = font.render(btn["name"], True, (255, 255, 0))
            screen.blit(text, text.get_rect(center=btn["rect"].center))

        if show_exit_confirm:
            mask = pygame.Surface((screen_width, screen_height), pygame.SRCALPHA)
            mask.fill((0, 0, 0, 180))
            screen.blit(mask, (0, 0))

            dialog = pygame.Rect(screen_width // 2 - ad(200), screen_height // 2 - ad(100), ad(400), ad(200))
            pygame.draw.rect(screen, (240, 240, 240), dialog, border_radius=ad(8))
            pygame.draw.rect(screen, (100, 100, 100), dialog, ad(3), border_radius=ad(8))

            title = font.render("确认退出游戏？", True, (0, 0, 0))
            screen.blit(title, title.get_rect(center=(screen_width // 2, screen_height // 2 - ad(60))))

            yes_r = pygame.Rect(screen_width // 2 - ad(160), screen_height // 2 + ad(10), ad(120), ad(40))
            no_r = pygame.Rect(screen_width // 2 + ad(40), screen_height // 2 + ad(10), ad(120), ad(40))

            pygame.draw.rect(screen, (200, 50, 50) if yes_r.collidepoint(mouse_pos) else (150, 0, 0), yes_r,
                             border_radius=ad(4))
            pygame.draw.rect(screen, (255, 255, 255), yes_r, ad(2), border_radius=ad(4))
            pygame.draw.rect(screen, (200, 50, 50) if no_r.collidepoint(mouse_pos) else (150, 0, 0), no_r,
                             border_radius=ad(4))
            pygame.draw.rect(screen, (255, 255, 255), no_r, ad(2), border_radius=ad(4))

            screen.blit(font.render("是", True, (255, 255, 255)),
                        font.render("是", True, (255, 255, 255)).get_rect(center=yes_r.center))
            screen.blit(font.render("否", True, (255, 255, 255)),
                        font.render("否", True, (255, 255, 255)).get_rect(center=no_r.center))

        pygame.display.flip()
        clock.tick(60)


# ======================= 启动 =======================
if __name__ == "__main__":
    loading_screen()
    main_menu()
pygame.quit()