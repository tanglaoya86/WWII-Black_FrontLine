// ============================================================================
// PathFinder.h —— 为决策树系统深度定制的 A* 寻路库 (C 接口)
// ============================================================================
//
// 核心概念：
//   地图由 width × height 个格子组成，每个格子包含：
//     高度 (height)        - float，地形海拔
//     可否通行 (walkable)  - int，0=不可通过，非0=可通过
//     代价乘数 (costMul)   - float，经过该格子的基础移动代价乘数，默认1.0
//     世界坐标 (wx, wy)    - float，可选，每个格子在游戏世界中的实际位置
//
//   寻路使用的 A* 算法支持：
//     - 自定义移动方向（替代默认的4/8方向）
//     - 高度差硬限制、软惩罚（上坡/下坡分别加权）
//     - 转向惩罚（使路径尽量保持直线）
//     - 多目标点（自动选择最近一个）
//     - 自定义可通过判断、自定义移动代价、自定义启发函数（可通过Python回调注入）
//     - 搜索时间/迭代次数限制，防止卡死
//     - 路径平滑（去掉不必要的中间点）
//     - 地图动态更新（单个格子属性修改，无需重建地图）
//     - 地图导入导出（支持文本文件、结构化二进制数据、自定义布局）
//     - 自定义地形类型与代价表
//     - 动态障碍层（友军、残骸等实时阻塞）
//     - 单位体积过滤（大型单位不会卡在窄缝）
//     - 自定义格子元素数据（供回调查询任意附加信息）
//     - 内置战术移动模式（掩护、潜行、撤退、侦察、跟随）
//     - 路径后处理回调
//     - 路径有效性检查与缓存
//
// 基本使用流程：
//   1. 创建实例：handle = PathFinder_Create()
//   2. 设置地图：PathFinder_SetMap() 或 PathFinder_LoadMapFromFile() 等
//   3. 设置起点：PathFinder_SetStart()
//   4. 添加目标：PathFinder_ClearTargets() + PathFinder_AddTarget()
//   5. 配置规则：高度差、惩罚、方向等（可选）
//   6. 执行寻路：PathFinder_FindPath()
//   7. 获取结果：PathFinder_GetPathLength() + 批量获取函数
//   8. 销毁实例：PathFinder_Destroy()
//
// 地图加载方式：
//   a) PathFinder_SetMap()          — 分别传入高度、walkable、costMul 三个独立数组
//   b) PathFinder_SetMapFromStruct()— 传入结构化数组，可自定义字段偏移
//   c) PathFinder_LoadMapFromFile() — 从文本文件加载（格式见实现文件注释）
//   d) 动态修改单个格子：PathFinder_SetCellHeight/SetCellWalkable/SetCellCostMultiplier
//   e) PathFinder_LoadCustomMap()   — 通过注册的自定义回调加载任意格式地图
//
// 注意事项：
//   - 所有坐标均为整数网格坐标，左上角为 (0,0)，x 向右，y 向下
//   - 数组均为行优先存储：index = y * width + x
//   - 回调函数均为 cdecl 调用约定，Python 端需使用 CFUNCTYPE
//   - 线程安全：单实例不支持多线程并发，但可创建多个实例分别使用
//
// ============================================================================

#ifndef PATHFINDER_H
#define PATHFINDER_H

#ifdef _WIN32
    #define EXPORT __declspec(dllexport)
#else
    #define EXPORT __attribute__((visibility("default")))
#endif

// ---------- 回调函数类型 ----------

typedef int   (*CanPassFunc)(int x, int y, void* userdata);
typedef float (*CostFunc)(int fromX, int fromY, int toX, int toY,
                          float heightFrom, float heightTo, void* userdata);
typedef float (*HeuristicFunc)(int x1, int y1, int x2, int y2, void* userdata);
typedef void  (*PathPostProcessFunc)(int* x, int* y, int count, void* userdata);
typedef int   (*MapLoadFunc)(void* handle, const char* filename, void* userdata);

// ---------- 内置战术移动模式 ----------
typedef enum {
    PF_MODE_DIRECT = 0,   // 最短路径
    PF_MODE_COVER,        // 偏好掩体
    PF_MODE_STEALTH,      // 极力隐蔽
    PF_MODE_RETREAT,      // 快速撤退
    PF_MODE_RECON,        // 侦察，偏好高地
    PF_MODE_FOLLOW        // 跟随，需配合 SetFollowTarget
} PathFinderMode;

extern "C" {

// ==================== 生命周期 ====================

EXPORT void* PathFinder_Create(void);
EXPORT void  PathFinder_Destroy(void* handle);

// ==================== 地图设置 ====================

// 从三个独立数组加载地图（行优先）。costMultiplier 可传 NULL 默认为 1.0。
EXPORT void  PathFinder_SetMap(void* handle,
                               int width, int height,
                               const float* heights,
                               const int* walkable,
                               const float* costMultiplier);

// 从结构化数组加载地图，通过偏移量提取字段。costMulOff < 0 则全部默认为 1.0。
EXPORT void  PathFinder_SetMapFromStruct(void* handle,
                                         int w, int h,
                                         const void* data,
                                         int structSize,
                                         int heightOff,
                                         int walkableOff,
                                         int costMulOff);

// 从内置文本格式文件加载地图。成功返回 1，失败返回 0。
EXPORT int   PathFinder_LoadMapFromFile(void* handle, const char* filename);

// 注册自定义地图加载回调，供 LoadCustomMap 使用。
EXPORT void  PathFinder_SetMapLoadCallback(void* handle,
                                           MapLoadFunc func,
                                           void* userdata);

// 使用已注册的回调加载自定义地图。成功返回 1，失败返回 0。
EXPORT int   PathFinder_LoadCustomMap(void* handle, const char* filename);

// ==================== 动态地图数据 ====================

// 设置动态障碍层（阻塞计数数组）。>0 视为不可通过。传 NULL 清除。
EXPORT void  PathFinder_SetDynamicWalkable(void* handle,
                                           const int* blockedCount);

// 设置单位占用格子数（默认 1x1）。大体积单位会检查矩形区域是否完全可通过。
EXPORT void  PathFinder_SetUnitSize(void* handle, int sizeX, int sizeY);

// 设置地形类型及每种类型的代价乘数表。最终代价 = 基础乘数 * 类型乘数。
EXPORT void  PathFinder_SetTerrainTypes(void* handle,
                                        const int* types,
                                        const float* costTable,
                                        int numTypes);

// 为每个格子附加一个整数用户数据，可在回调中通过 GetCellUserData 查询。
EXPORT void  PathFinder_SetCellUserDataArray(void* handle, const int* userData);
EXPORT void  PathFinder_SetCellUserData(void* handle, int x, int y, int data);
EXPORT int   PathFinder_GetCellUserData(void* handle, int x, int y);

// ==================== 地图导出 ====================

// 将当前地图（含动态障碍）序列化到二进制缓冲区。先传 NULL 查询所需大小。
EXPORT int   PathFinder_ExportMapToBuffer(void* handle,
                                          char* buffer,
                                          int* outSize,
                                          int includeWorldCoords);

// 保存地图为二进制文件。includeWorldCoords 非 0 则同时写入世界坐标。
EXPORT int   PathFinder_SaveMapToFile(void* handle,
                                      const char* filename,
                                      int includeWorldCoords);

// ==================== 动态修改单个格子 ====================

EXPORT void  PathFinder_SetCellHeight(void* handle, int x, int y, float height);
EXPORT void  PathFinder_SetCellWalkable(void* handle, int x, int y, int walkable);
EXPORT void  PathFinder_SetCellCostMultiplier(void* handle, int x, int y, float multiplier);

// ==================== 起终点与多目标 ====================

EXPORT void  PathFinder_SetStart(void* handle, int sx, int sy);
EXPORT void  PathFinder_ClearTargets(void* handle);
EXPORT void  PathFinder_AddTarget(void* handle, int ex, int ey);
EXPORT int   PathFinder_GetTargetCount(void* handle);
EXPORT int   PathFinder_GetTarget(void* handle, int index, int* x, int* y);

// ==================== 寻路参数配置 ====================

// 最大允许高度差（绝对值）。超过则不可通行。
EXPORT void  PathFinder_SetMaxHeightDiff(void* handle, float maxDiff);

// 高度差软惩罚：额外代价 = 基础惩罚 × |高度差| × 方向系数。
EXPORT void  PathFinder_SetHeightPenalty(void* handle,
                                         float penaltyPerUnit,
                                         float uphillFactor,
                                         float downhillFactor);

// 是否允许对角线移动（开启后为 8 方向）。若已设置自定义方向则忽略。
EXPORT void  PathFinder_SetAllowDiagonal(void* handle, int allow);

// 转向惩罚：每次改变移动方向时额外增加的代价。
EXPORT void  PathFinder_SetTurnPenalty(void* handle, float penalty);

// 完全自定义移动方向集（dx, dy, costs 数组，长度一致）。
EXPORT void  PathFinder_SetCustomMoveDirections(void* handle,
                                                const int* dx,
                                                const int* dy,
                                                const float* costs,
                                                int count);

// ==================== 注入自定义规则 ====================

// 注册自定义可通过判断回调。传 NULL 取消。
EXPORT void  PathFinder_SetCustomCanPass(void* handle,
                                         CanPassFunc func, void* userdata);

// 注册自定义额外代价回调（会覆盖内置战术模式的效果）。
EXPORT void  PathFinder_SetCustomCost(void* handle,
                                      CostFunc func, void* userdata);

// 注册自定义启发式函数。传 NULL 恢复默认（对角线/曼哈顿）。
EXPORT void  PathFinder_SetCustomHeuristic(void* handle,
                                           HeuristicFunc func, void* userdata);

// 注册路径后处理回调。每次寻路成功后调用，可修改路径点。
EXPORT void  PathFinder_SetPathPostProcess(void* handle,
                                           PathPostProcessFunc func, void* userdata);

// ==================== 世界坐标 ====================

// 为每个格子设置世界坐标。设置后可直接获取路径的世界坐标。
EXPORT void  PathFinder_SetWorldCoords(void* handle,
                                       const float* xCoords,
                                       const float* yCoords);

// ==================== 战术移动模式 ====================

// 选择内置战术移动模式（会被 SetCustomCost 覆盖）。
EXPORT void  PathFinder_SetTacticalMode(void* handle, PathFinderMode mode);

// ==================== 跟随模式 ====================

// 设置跟随目标及其期望偏移（用于 PF_MODE_FOLLOW）。每次寻路会自动更新终点。
EXPORT void  PathFinder_SetFollowTarget(void* handle,
                                        int targetX, int targetY,
                                        int offsetX, int offsetY);

// ==================== 搜索限制 ====================

EXPORT void  PathFinder_SetMaxSearchTime(void* handle, float seconds);
EXPORT void  PathFinder_SetMaxIterations(void* handle, int maxIter);

// ==================== 执行寻路 ====================

// 启动寻路。返回 1 表示找到路径，0 表示失败。
EXPORT int   PathFinder_FindPath(void* handle);

// 检查当前路径是否仍有效（起点/终点变化或出现新障碍）。
EXPORT int   PathFinder_IsPathValid(void* handle);

// 对当前路径进行平滑，移除不必要的中间点。
EXPORT void  PathFinder_SmoothPath(void* handle, float maxAngleDeviation);

// ==================== 结果提取 ====================

EXPORT int   PathFinder_GetPathLength(void* handle);
EXPORT int   PathFinder_GetPathPoint(void* handle, int index, int* x, int* y);
EXPORT int   PathFinder_GetAllPathGridPoints(void* handle,
                                             int* outX, int* outY,
                                             int maxPoints);
EXPORT int   PathFinder_GetAllPathWorldPoints(void* handle,
                                              float* outX, float* outY,
                                              int maxPoints);
EXPORT int   PathFinder_GetAllPathHeights(void* handle,
                                          float* outHeights,
                                          int maxPoints);
EXPORT float PathFinder_GetPathCost(void* handle);
EXPORT int   PathFinder_GetPathWorldPoint(void* handle, int index,
                                          float* wx, float* wy);
EXPORT int   PathFinder_GetPathHeight(void* handle, int index, float* height);

} // extern "C"

#endif // PATHFINDER_H
