#ifndef PATHFINDER_H
#define PATHFINDER_H

#ifdef _WIN32
    #define EXPORT __declspec(dllexport)
#else
    #define EXPORT __attribute__((visibility("default")))
#endif

// 这些家伙是给 Python 回调预留的，你可以在 Python 里写函数，然后扔给 C++ 执行
typedef int (*CanPassFunc)(int x, int y, void* userdata);
typedef float (*CostFunc)(int fromX, int fromY, int toX, int toY, float heightFrom, float heightTo, void* userdata);
typedef float (*HeuristicFunc)(int x1, int y1, int x2, int y2, void* userdata);

extern "C" {

EXPORT void* PathFinder_Create();
EXPORT void  PathFinder_Destroy(void* handle);

// 把地图塞进去，heights、walkable、costMultiplier 全是一维数组，行优先
// costMultiplier 传 NULL 就全按 1.0 处理，别怕
EXPORT void  PathFinder_SetMap(void* handle,
                               int width, int height,
                               const float* heights,
                               const int* walkable,
                               const float* costMultiplier);

// 告诉它从哪儿走到哪儿
EXPORT void  PathFinder_SetStartEnd(void* handle, int sx, int sy, int ex, int ey);

// 硬性高度差门槛，超过这个数直接不让走
EXPORT void  PathFinder_SetMaxHeightDiff(void* handle, float maxDiff);

// 高度差：每单位高度差加多少代价，上坡/下坡再分别乘个系数
// 比如你想让上坡累死，就把 uphillFactor 设成 2.0，下坡轻松就设 0.5
EXPORT void  PathFinder_SetHeightPenalty(void* handle,
                                         float penaltyPerUnit,
                                         float uphillFactor,
                                         float downhillFactor);

// 是否允许对角线（如果你用了自定义方向，这个设置会被忽略）
EXPORT void  PathFinder_SetAllowDiagonal(void* handle, int allow);

// 自定义移动方向！完全替代默认的 4/8 方向。
// dx, dy, costs 都是长度为 count 的数组，分别存偏移量、偏移量和基础移动代价。
// 举个栗子：你想让单位只能像马一样跳，就传 [{2,1}, {1,2}, ...] 和对应的代价。
EXPORT void  PathFinder_SetCustomMoveDirections(void* handle,
                                                const int* dx,
                                                const int* dy,
                                                const float* costs,
                                                int count);

// 三个后门：挂你自己的逻辑，userdata 爱传啥传啥
EXPORT void  PathFinder_SetCustomCanPass(void* handle, CanPassFunc func, void* userdata);
EXPORT void  PathFinder_SetCustomCost(void* handle, CostFunc func, void* userdata);
EXPORT void  PathFinder_SetCustomHeuristic(void* handle, HeuristicFunc func, void* userdata);

//返回 1 表示找到了，0 表示洗洗睡吧
EXPORT int   PathFinder_FindPath(void* handle);

// 拿结果，先问长度，再按索引一个一个揪点
EXPORT int   PathFinder_GetPathLength(void* handle);
EXPORT int   PathFinder_GetPathPoint(void* handle, int index, int* x, int* y);

// 一个获取路径总代价的接口，省得你自己算
EXPORT float PathFinder_GetPathCost(void* handle);

}

#endif
