import pygame
import sys

pygame.init()
SCREEN_WIDTH, SCREEN_HEIGHT = 1280, 720
screen = pygame.display.set_mode((SCREEN_WIDTH, SCREEN_HEIGHT))
pygame.display.set_caption("BL-gamed tools")
clock = pygame.time.Clock()

# 瓦片
TILE_WIDTH = 128    # 瓦片总宽度
TILE_HEIGHT = 64    # 瓦片总高度
HALF_TILE_W = TILE_WIDTH // 2
HALF_TILE_H = TILE_HEIGHT // 2

# 地图偏移（让地图居中显示）
MAP_OFFSET_X = SCREEN_WIDTH // 2 - HALF_TILE_W
MAP_OFFSET_Y = 100

def create_isometric_tile(color):
    """创建菱形等距地面瓦片"""
    tile = pygame.Surface((TILE_WIDTH, TILE_HEIGHT), pygame.SRCALPHA)
    points = [
        (HALF_TILE_W, 0),
        (TILE_WIDTH, HALF_TILE_H),
        (HALF_TILE_W, TILE_HEIGHT),
        (0, HALF_TILE_H)
    ]
    pygame.draw.polygon(tile, color, points)
    pygame.draw.polygon(tile, (0,0,0), points, 1)  # 黑色边框
    return tile

def create_unit(size, color):
    """创建单位/建筑贴图"""
    unit = pygame.Surface((TILE_WIDTH, TILE_HEIGHT*2), pygame.SRCALPHA)
    pygame.draw.rect(unit, color, (TILE_WIDTH//2 - size//2, TILE_HEIGHT, size, size))
    return unit

# 生成素材
grass_tile = create_isometric_tile((34, 139, 34))    # 草地
sand_tile = create_isometric_tile((210, 180, 140))   # 沙地
tank_unit = create_unit(40, (150, 0, 0))             # 坦克
building_unit = create_unit(80, (100, 100, 100))     # 建筑

def iso_to_screen(map_x, map_y):
    """游戏逻辑坐标 → 屏幕渲染坐标"""
    screen_x = MAP_OFFSET_X + (map_x - map_y) * HALF_TILE_W
    screen_y = MAP_OFFSET_Y + (map_x + map_y) * HALF_TILE_H
    return screen_x, screen_y

# 10x10地图（0=草地，1=沙地）
game_map = [
    [0,0,0,1,1,0,0,0,0,0],
    [0,0,1,1,1,1,0,0,0,0],
    [0,1,1,0,0,1,1,0,0,0],
    [0,1,0,0,0,0,1,0,0,0],
    [0,1,1,0,0,1,1,0,0,0],
    [0,0,1,1,1,1,0,0,0,0],
    [0,0,0,1,1,0,0,0,0,0],
    [0,0,0,0,0,0,0,0,0,0],
    [0,0,0,0,0,0,0,0,0,0],
    [0,0,0,0,0,0,0,0,0,0],
]

# 单位列表
units = [
    (2, 2, tank_unit),
    (5, 3, building_unit),
    (3, 6, tank_unit),
]


running = True
while running:
    screen.fill((0,0,0))  # 黑色背景
    # 事件处理
    for event in pygame.event.get():
        if event.type == pygame.QUIT:
            running = False
   
    for y in range(len(game_map)):
        for x in range(len(game_map[y])):
            tile_type = game_map[y][x]
            tile = grass_tile if tile_type == 0 else sand_tile
            sx, sy = iso_to_screen(x, y)
            screen.blit(tile, (sx, sy))

    sorted_units = sorted(units, key=lambda u: u[0] + u[1])
    for x, y, img in sorted_units:
        sx, sy = iso_to_screen(x, y)
        screen.blit(img, (sx, sy - TILE_HEIGHT))

    pygame.display.flip()
    clock.tick(60)

pygame.quit()
sys.exit()
