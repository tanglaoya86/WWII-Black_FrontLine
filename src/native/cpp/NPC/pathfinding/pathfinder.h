// 建议从头到尾读一遍。
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
//   e) PathFinder_GetMapAsBinary()  — 将当前地图序列化为二进制，可用于保存或网络传输
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

// can_pass: 自定义可通过判断。返回 1 表示可通行，0 不可。
//   参数: x, y 为网格坐标，userdata 是注册时传入的指针
typedef int   (*CanPassFunc)(int x, int y, void* userdata);

// cost: 自定义额外移动代价（叠加在基础代价之上）。
//   参数: fromX,fromY 起始格子, toX,toY 目标格子, heightFrom,heightTo 高度值
//   返回: 额外代价（浮点数，可为负但会导致未定义行为）
typedef float (*CostFunc)(int fromX, int fromY, int toX, int toY,
                          float heightFrom, float heightTo, void* userdata);

// heuristic: 自定义启发距离估计。
//   参数: x1,y1 当前点, x2,y2 目标点, userdata 用户数据
//   返回: 估计距离值（必须非负且不大于实际代价，否则可能找不到最优路径）
typedef float (*HeuristicFunc)(int x1, int y1, int x2, int y2, void* userdata);

// ---------- C 接口 ----------
extern "C" {

// ===== 生命周期 =====

// 创建一个寻路器实例，返回句柄 (void*)。失败返回 NULL。
EXPORT void* PathFinder_Create();

// 销毁实例，释放所有资源。传入 NULL 则无操作。
EXPORT void  PathFinder_Destroy(void* handle);


// ===== 地图设置 =====

// 从三个独立数组加载地图。所有数组长度必须为 width*height，行优先。
//   heights:       高度值 (float 数组)
//   walkable:      可否通行 (int 数组，0=不可，非0=可)
//   costMultiplier: 代价乘数 (float 数组)，可传 NULL 表示全部为 1.0
EXPORT void  PathFinder_SetMap(void* handle,
                               int width, int height,
                               const float* heights,
                               const int* walkable,
                               const float* costMultiplier);

// 从结构化数组加载地图，允许用户自定义字段在结构体内的偏移。
// 适合已有复杂结构体（如包含高度、类型、代价等字段）的场景，无需拆分成独立数组。
//   width, height: 地图尺寸
//   data:         指向连续的结构体数组（行优先）的指针
//   structSize:   单个结构体的字节大小 (可用 sizeof(MyCell))
//   heightOffset: 高度字段在结构体中的字节偏移 (如 offsetof(MyCell, height))
//   walkableOffset: 可通过字段的字节偏移
//   costMulOffset:  代价乘数字段的字节偏移，若 <0 则所有格子代价乘数默认为1.0
// 注意：字段类型必须与内部一致（高度为 float，可通过为 int，代价为 float），否则结果未定义。
EXPORT void  PathFinder_SetMapFromStruct(void* handle,
                                         int width, int height,
                                         const void* data,
                                         int structSize,
                                         int heightOffset,
                                         int walkableOffset,
                                         int costMulOffset);

// 从文本文件加载地图。文件格式：
//   第一行： width height
//   之后 width*height 行，每行三个值： 高度 可通过(0/1) [代价乘数(可选)]
//   代价乘数如果省略则默认为 1.0。
//   注释行：以 '#' 开头的行为注释，会被忽略。
//   返回 1 表示成功，0 表示失败。
EXPORT int   PathFinder_LoadMapFromFile(void* handle, const char* filename);

// 将当前地图数据导出为二进制缓冲区（用于保存或网络传输）。
// 格式：前4字节为 width (int), 再4字节为 height (int), 之后是 width*height 个连续的结构：
//   float height; int walkable; float costMultiplier; (共12字节)
// 如果提供了 worldCoords，导出格式会变为：前4字节 width, 4字节 height, 4字节 flags (bit0=1表示含世界坐标)，
//   然后每个格子：height, walkable, costMultiplier, worldX, worldY (20字节)。
//   outSize: 输出缓冲区的大小。如果提供的 buffer 为 NULL，函数仅计算所需大小写入 *outSize 并返回 0。
//   buffer:  接收数据的缓冲区，若为 NULL 则只查询大小。
//   includeWorldCoords: 0=不导出世界坐标，1=导出
// 返回: 实际写入的字节数，若 buffer 太小则返回 -1。
EXPORT int   PathFinder_ExportMapToBuffer(void* handle,
                                          char* buffer,
                                          int* outSize,
                                          int includeWorldCoords);

// 将地图保存到二进制文件（调用 ExportMapToBuffer 后写入文件）。
//   filename: 文件路径
//   includeWorldCoords: 0/1
// 返回 1 成功，0 失败。
EXPORT int   PathFinder_SaveMapToFile(void* handle, const char* filename, int includeWorldCoords);


// ===== 动态修改格子（无需重建整张地图） =====

// 修改指定格子的高度值
EXPORT void  PathFinder_SetCellHeight(void* handle, int x, int y, float height);
// 修改指定格子的可通过性 (0/1)
EXPORT void  PathFinder_SetCellWalkable(void* handle, int x, int y, int walkable);
// 修改指定格子的代价乘数
EXPORT void  PathFinder_SetCellCostMultiplier(void* handle, int x, int y, float multiplier);


// ===== 起终点设置 =====

// 设置寻路起点
EXPORT void  PathFinder_SetStart(void* handle, int sx, int sy);

// 清空所有目标点
EXPORT void  PathFinder_ClearTargets(void* handle);
// 添加一个目标点（可多次调用添加多个）
EXPORT void  PathFinder_AddTarget(void* handle, int ex, int ey);
// 获取当前目标点数量
EXPORT int   PathFinder_GetTargetCount(void* handle);
// 获取第 index 个目标点的坐标，index 从 0 开始。成功返回 1，失败返回 0。
EXPORT int   PathFinder_GetTarget(void* handle, int index, int* x, int* y);


// ===== 寻路参数配置 =====

// 设置最大允许高度差（绝对值）。若相邻格子高度差超过此值，则该方向不可通行。
EXPORT void  PathFinder_SetMaxHeightDiff(void* handle, float maxDiff);

// 设置高度差软惩罚：每单位高度差的额外代价。
//   penaltyPerUnit: 每单位高度差增加的基础代价 (>=0)
//   uphillFactor:   上坡（目标高度>当前高度）时惩罚乘数 (>=0)
//   downhillFactor: 下坡时惩罚乘数 (>=0)
// 例如：penaltyPerUnit=0.5, uphillFactor=1.5 => 上坡时额外代价 = 0.5 * 高度差 * 1.5
EXPORT void  PathFinder_SetHeightPenalty(void* handle,
                                         float penaltyPerUnit,
                                         float uphillFactor,
                                         float downhillFactor);

// 允许对角线移动（4方向扩展为8方向）。若设置了自定义移动方向，此设置被忽略。
EXPORT void  PathFinder_SetAllowDiagonal(void* handle, int allow);

// 设置转向惩罚：每次改变移动方向时额外增加的代价。值越大路径越倾向于直线。
EXPORT void  PathFinder_SetTurnPenalty(void* handle, float penalty);

// 设置自定义移动方向，完全替代内置的4/8方向。
//   dx, dy, costs: 长度均为 count 的数组，分别表示 x 偏移、y 偏移、基础移动代价（如直线1.0，对角线1.414）
// 例如，只允许向正右和正下移动： dx={1,0}, dy={0,1}, costs={1.0,1.0}
EXPORT void  PathFinder_SetCustomMoveDirections(void* handle,
                                                const int* dx,
                                                const int* dy,
                                                const float* costs,
                                                int count);

// 注册自定义可通过判断回调 (can_pass)。
//   func: 函数指针，若为 NULL 则取消回调
//   userdata: 传递给回调的额外数据，可传 Python 对象指针等
EXPORT void  PathFinder_SetCustomCanPass(void* handle, CanPassFunc func, void* userdata);

// 注册自定义额外代价回调 (cost)。
EXPORT void  PathFinder_SetCustomCost(void* handle, CostFunc func, void* userdata);

// 注册自定义启发函数回调 (heuristic)。
EXPORT void  PathFinder_SetCustomHeuristic(void* handle, HeuristicFunc func, void* userdata);

// 设置每个格子的世界坐标（如经纬度或游戏世界位置），用于输出路径的世界坐标。
//   xCoords, yCoords: 长度必须为 width*height 的 float 数组，行优先。
EXPORT void  PathFinder_SetWorldCoords(void* handle, const float* xCoords, const float* yCoords);


// ===== 搜索控制 =====

// 设置最大搜索时间（秒）。当搜索耗时超过此值时会强行停止并返回失败。默认 -1 表示不限制。
EXPORT void  PathFinder_SetMaxSearchTime(void* handle, float seconds);

// 设置最大扩展节点数（迭代次数）。超过后停止搜索。默认 -1 表示不限制。
EXPORT void  PathFinder_SetMaxIterations(void* handle, int maxIter);


// ===== 执行寻路 =====

// 执行寻路。必须先设置地图、起点和至少一个目标点。
// 返回 1 表示找到路径，0 表示失败（无路径或超时等）。
EXPORT int   PathFinder_FindPath(void* handle);


// ===== 路径后处理 =====

// 对找到的路径进行平滑处理，去掉共线或无障碍直线可达的中间点。
//   maxAngleDeviation: 该参数暂未使用，保留以备扩展。实际使用直线可视检测。
// 注意：平滑会直接修改内部路径，之后获取路径点即为平滑后的结果。
EXPORT void  PathFinder_SmoothPath(void* handle, float maxAngleDeviation);


// ===== 结果提取 =====

// 获取路径上的节点数量（包括起点和终点）
EXPORT int   PathFinder_GetPathLength(void* handle);

// 获取第 index 个路径点的网格坐标 (x, y)。index 从 0 开始。
// 成功返回 1，失败（越界）返回 0。
EXPORT int   PathFinder_GetPathPoint(void* handle, int index, int* x, int* y);

// 一次性获取所有路径点的网格坐标。
//   outX, outY: 接收坐标的 int 数组，长度至少为路径长度
//   maxPoints:  数组能容纳的最大点数，必须 >= 路径长度，否则返回 -1。
// 成功返回实际写入的点数。
EXPORT int   PathFinder_GetAllPathGridPoints(void* handle, int* outX, int* outY, int maxPoints);

// 一次性获取所有路径点的世界坐标（需先调用 SetWorldCoords）。
// 参数同上，但数组类型为 float。若未设置世界坐标返回 -1。
EXPORT int   PathFinder_GetAllPathWorldPoints(void* handle, float* outX, float* outY, int maxPoints);

// 一次性获取所有路径点的高度值。
//   outHeights: 接收高度的 float 数组，长度至少为路径长度
//   maxPoints:  同前
// 返回实际点数，若 maxPoints 不足返回 -1。
EXPORT int   PathFinder_GetAllPathHeights(void* handle, float* outHeights, int maxPoints);

// 获取找到路径的总代价（移动代价总和）
EXPORT float PathFinder_GetPathCost(void* handle);

}

#endif // PATHFINDER_H
