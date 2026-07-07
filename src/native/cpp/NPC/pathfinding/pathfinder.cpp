#include "pathfinder.h"
#include <vector>
#include <queue>
#include <cmath>
#include <limits>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <chrono>

struct PathNode {
    int x, y;
    float g, h;
    int parentIdx = -1;
    int enterDir  = -1;   // 从哪个方向进入的，-1 表示起点
    bool closed   = false;

    float f() const { return g + h; }
    bool operator<(const PathNode& other) const { return f() > other.f(); }
};

struct MoveDir {
    int dx, dy;
    float cost;
};

class PathFinder {
public:
    PathFinder()
        : width_(0), height_(0),
          maxHeightDiff_(0.0f), heightPenaltyPerUnit_(0.0f),
          uphillFactor_(1.0f), downhillFactor_(1.0f),
          allowDiagonal_(false), turnPenalty_(0.0f),
          startX_(-1), startY_(-1),
          canPassFunc_(nullptr), costFunc_(nullptr), heuristicFunc_(nullptr),
          canPassUserData_(nullptr), costUserData_(nullptr), heuristicUserData_(nullptr),
          useCustomDirections_(false), hasWorldCoords_(false),
          totalPathCost_(0.0f),
          maxSearchTime_(-1.0f), maxIterations_(-1) {}

    ~PathFinder() { clearNodePool(); }

    void setMap(int w, int h, const float* heights, const int* walkable, const float* costMul) {
        width_ = w; height_ = h;
        size_t sz = w * h;
        heights_.assign(heights, heights + sz);
        walkable_.assign(walkable, walkable + sz);
        if (costMul) costMultiplier_.assign(costMul, costMul + sz);
        else         costMultiplier_.assign(sz, 1.0f);
        path_.clear();
        totalPathCost_ = 0.0f;
    }

    void setMapFromStruct(int w, int h, const void* data, int structSize,
                          int heightOff, int walkableOff, int costMulOff) {
        width_ = w; height_ = h;
        size_t sz = w * h;
        heights_.resize(sz);
        walkable_.resize(sz);
        costMultiplier_.resize(sz);
        const char* base = static_cast<const char*>(data);
        for (int i = 0; i < (int)sz; ++i) {
            const char* item = base + i * structSize;
            heights_[i] = *reinterpret_cast<const float*>(item + heightOff);
            walkable_[i] = *reinterpret_cast<const int*>(item + walkableOff);
            costMultiplier_[i] = (costMulOff >= 0) 
                ? *reinterpret_cast<const float*>(item + costMulOff) : 1.0f;
        }
        path_.clear();
    }

    bool loadMapFromFile(const char* filename) {
        FILE* f = fopen(filename, "r");
        if (!f) return false;
        int w, h;
        if (fscanf(f, "%d %d", &w, &h) != 2) { fclose(f); return false; }

        std::vector<float> heights; heights.reserve(w*h);
        std::vector<int>   walkable; walkable.reserve(w*h);
        std::vector<float> costMul;  costMul.reserve(w*h);

        char line[256];
        fgets(line, sizeof(line), f); // 跳过尺寸行后的换行
        for (int i = 0; i < w*h; ) {
            if (!fgets(line, sizeof(line), f)) break;
            if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue; // 注释或空行
            float hgt;
            int wk;
            float cm = 1.0f;
            int read = sscanf(line, "%f %d %f", &hgt, &wk, &cm);
            if (read >= 2) {
                heights.push_back(hgt);
                walkable.push_back(wk);
                costMul.push_back(read == 3 ? cm : 1.0f);
                ++i;
            } else {
                fclose(f);
                return false;
            }
        }
        fclose(f);
        if ((int)heights.size() != w*h) return false;
        setMap(w, h, heights.data(), walkable.data(), costMul.data());
        return true;
    }

    int exportMapToBuffer(char* buffer, int* outSize, int includeWorldCoords) {
        int cellSize = sizeof(float) + sizeof(int) + sizeof(float); // 12
        if (includeWorldCoords) cellSize += 2 * sizeof(float);      // 20
        int headerSize = sizeof(int) * 2;
        if (includeWorldCoords) headerSize += sizeof(int);
        int totalSize = headerSize + width_ * height_ * cellSize;

        if (outSize) *outSize = totalSize;
        if (!buffer) return 0;

        char* ptr = buffer;
        memcpy(ptr, &width_, sizeof(int));  ptr += sizeof(int);
        memcpy(ptr, &height_, sizeof(int)); ptr += sizeof(int);
        if (includeWorldCoords) {
            int flags = 1;
            memcpy(ptr, &flags, sizeof(int)); ptr += sizeof(int);
        }

        for (int i = 0; i < width_*height_; ++i) {
            memcpy(ptr, &heights_[i], sizeof(float)); ptr += sizeof(float);
            memcpy(ptr, &walkable_[i], sizeof(int));  ptr += sizeof(int);
            memcpy(ptr, &costMultiplier_[i], sizeof(float)); ptr += sizeof(float);
            if (includeWorldCoords) {
                float wx = hasWorldCoords_ ? worldX_[i] : 0.0f;
                float wy = hasWorldCoords_ ? worldY_[i] : 0.0f;
                memcpy(ptr, &wx, sizeof(float)); ptr += sizeof(float);
                memcpy(ptr, &wy, sizeof(float)); ptr += sizeof(float);
            }
        }
        return totalSize;
    }

    bool saveMapToFile(const char* filename, int includeWorldCoords) {
        int size = 0;
        exportMapToBuffer(nullptr, &size, includeWorldCoords);
        std::vector<char> buf(size);
        int written = exportMapToBuffer(buf.data(), &size, includeWorldCoords);
        if (written <= 0) return false;
        FILE* f = fopen(filename, "wb");
        if (!f) return false;
        size_t n = fwrite(buf.data(), 1, written, f);
        fclose(f);
        return n == (size_t)written;
    }

    void setCellHeight(int x, int y, float h) { if (inBounds(x,y)) heights_[idx(x,y)] = h; }
    void setCellWalkable(int x, int y, int w) { if (inBounds(x,y)) walkable_[idx(x,y)] = w; }
    void setCellCostMultiplier(int x, int y, float c) { if (inBounds(x,y)) costMultiplier_[idx(x,y)] = c; }

    void clearTargets() { targets_.clear(); }
    void addTarget(int x, int y) { if (inBounds(x,y)) targets_.push_back({x,y}); }
    void setStart(int sx, int sy) { startX_ = sx; startY_ = sy; }
    int getTargetCount() const { return (int)targets_.size(); }
    bool getTarget(int i, int* x, int* y) const {
        if (i < 0 || i >= (int)targets_.size()) return false;
        *x = targets_[i].first; *y = targets_[i].second;
        return true;
    }

    void setMaxHeightDiff(float d) { maxHeightDiff_ = d; }
    void setHeightPenalty(float p, float up, float down) {
        heightPenaltyPerUnit_ = p; uphillFactor_ = up; downhillFactor_ = down;
    }
    void setAllowDiagonal(bool a) { allowDiagonal_ = a; }
    void setTurnPenalty(float p) { turnPenalty_ = p; }

    void setCustomMoveDirections(const int* dx, const int* dy, const float* costs, int count) {
        customDirs_.clear();
        for (int i = 0; i < count; ++i) customDirs_.push_back({dx[i], dy[i], costs[i]});
        useCustomDirections_ = true;
    }

    void setCustomCanPass(CanPassFunc f, void* u) { canPassFunc_ = f; canPassUserData_ = u; }
    void setCustomCost(CostFunc f, void* u) { costFunc_ = f; costUserData_ = u; }
    void setCustomHeuristic(HeuristicFunc f, void* u) { heuristicFunc_ = f; heuristicUserData_ = u; }

    void setWorldCoords(const float* xc, const float* yc) {
        if (!xc || !yc || width_<=0 || height_<=0) return;
        size_t sz = width_*height_;
        worldX_.assign(xc, xc+sz);
        worldY_.assign(yc, yc+sz);
        hasWorldCoords_ = true;
    }

    void setMaxSearchTime(float s) { maxSearchTime_ = s; }
    void setMaxIterations(int m) { maxIterations_ = m; }

    bool findPath() {
        path_.clear();
        totalPathCost_ = 0.0f;
        if (!sanityCheck()) return false;

        initNodePool();
        std::priority_queue<std::pair<float, int>> open;
        int startIdx = idx(startX_, startY_);
        PathNode& startNode = nodePool_[startIdx];
        startNode.g = 0;
        startNode.h = computeMinHeuristic(startX_, startY_);
        open.emplace(-startNode.f(), startIdx);

        std::vector<MoveDir> dirs;
        if (useCustomDirections_) {
            dirs = customDirs_;
        } else {
            const int dx8[8] = {1,-1,0,0,1,-1,1,-1};
            const int dy8[8] = {0,0,1,-1,1,1,-1,-1};
            const float cost8[8] = {1,1,1,1,1.41421356f,1.41421356f,1.41421356f,1.41421356f};
            int dcount = allowDiagonal_ ? 8 : 4;
            for (int i=0; i<dcount; ++i) dirs.push_back({dx8[i], dy8[i], cost8[i]});
        }

        auto startTime = std::chrono::steady_clock::now();
        int iter = 0;
        bool found = false;
        int finalNodeIdx = -1;

        while (!open.empty()) {
            if (maxSearchTime_ > 0.0f) {
                float elapsed = std::chrono::duration<float>(std::chrono::steady_clock::now() - startTime).count();
                if (elapsed >= maxSearchTime_) break;
            }
            if (maxIterations_ > 0 && iter >= maxIterations_) break;

            int curIdx = open.top().second; open.pop();
            PathNode& cur = nodePool_[curIdx];
            if (cur.closed) continue;
            cur.closed = true;
            iter++;

            if (isTarget(cur.x, cur.y)) {
                found = true; finalNodeIdx = curIdx; break;
            }

            for (size_t d = 0; d < dirs.size(); ++d) {
                int nx = cur.x + dirs[d].dx;
                int ny = cur.y + dirs[d].dy;
                if (!inBounds(nx,ny)) continue;
                if (!isWalkable(nx,ny)) continue;
                if (canPassFunc_ && !canPassFunc_(nx, ny, canPassUserData_)) continue;

                float hFrom = height(cur.x, cur.y);
                float hTo   = height(nx, ny);
                if (std::fabs(hTo - hFrom) > maxHeightDiff_) continue;

                // 内置对角线时防穿墙
                if (!useCustomDirections_ && d >= 4) {
                    int ax1 = cur.x + dirs[d].dx, ay1 = cur.y;
                    int ax2 = cur.x, ay2 = cur.y + dirs[d].dy;
                    if (!isWalkable(ax1,ay1) || !isWalkable(ax2,ay2)) continue;
                    if (canPassFunc_ && (!canPassFunc_(ax1,ay1,canPassUserData_) ||
                                         !canPassFunc_(ax2,ay2,canPassUserData_))) continue;
                }

                float moveCost = dirs[d].cost * costMultiplier(nx, ny);
                float diff = hTo - hFrom;
                float penalty = heightPenaltyPerUnit_ * std::fabs(diff);
                if (diff > 0) penalty *= uphillFactor_;
                else          penalty *= downhillFactor_;
                moveCost += penalty;

                if (turnPenalty_ > 0.0f && cur.enterDir >= 0 && (int)d != cur.enterDir)
                    moveCost += turnPenalty_;

                if (costFunc_)
                    moveCost += costFunc_(cur.x, cur.y, nx, ny, hFrom, hTo, costUserData_);

                float tentativeG = cur.g + moveCost;
                int nbIdx = idx(nx, ny);
                PathNode& nb = nodePool_[nbIdx];
                if (tentativeG < nb.g) {
                    nb.g = tentativeG;
                    nb.h = computeMinHeuristic(nx, ny);
                    nb.parentIdx = curIdx;
                    nb.enterDir = (int)d;
                    open.emplace(-nb.f(), nbIdx);
                }
            }
        }

        if (found && finalNodeIdx >= 0) {
            rebuildPath(finalNodeIdx);
            totalPathCost_ = nodePool_[finalNodeIdx].g;
        }
        return found;
    }

    void smoothPath(float /*maxAngle*/) {
        if (path_.size() < 3) return;
        std::vector<std::pair<int,int>> smoothed;
        smoothed.push_back(path_.front());
        size_t idx = 1;
        while (idx < path_.size() - 1) {
            size_t last = path_.size() - 1;
            for (size_t k = idx; k < path_.size(); ++k) {
                if (lineOfSight(smoothed.back().first, smoothed.back().second,
                                path_[k].first, path_[k].second)) {
                    last = k;
                } else break;
            }
            smoothed.push_back(path_[last]);
            idx = last + 1;
        }
        smoothed.push_back(path_.back());
        path_ = std::move(smoothed);
    }

    int getPathLength() const { return (int)path_.size(); }
    bool getPathPoint(int i, int* x, int* y) const {
        if (i<0 || i>=(int)path_.size()) return false;
        *x = path_[i].first; *y = path_[i].second; return true;
    }
    int getAllPathGridPoints(int* x, int* y, int max) const {
        if (max < (int)path_.size()) return -1;
        for (size_t i=0; i<path_.size(); ++i) { x[i]=path_[i].first; y[i]=path_[i].second; }
        return (int)path_.size();
    }
    int getAllPathWorldPoints(float* x, float* y, int max) const {
        if (!hasWorldCoords_) return -1;
        if (max < (int)path_.size()) return -1;
        for (size_t i=0; i<path_.size(); ++i) {
            int id = idx(path_[i].first, path_[i].second);
            x[i] = worldX_[id]; y[i] = worldY_[id];
        }
        return (int)path_.size();
    }
    int getAllPathHeights(float* h, int max) const {
        if (max < (int)path_.size()) return -1;
        for (size_t i=0; i<path_.size(); ++i) h[i] = height(path_[i].first, path_[i].second);
        return (int)path_.size();
    }
    float getPathCost() const { return totalPathCost_; }

private:
    int width_, height_;
    std::vector<float> heights_;
    std::vector<int>   walkable_;
    std::vector<float> costMultiplier_;

    float maxHeightDiff_;
    float heightPenaltyPerUnit_;
    float uphillFactor_, downhillFactor_;
    bool allowDiagonal_;
    float turnPenalty_;

    int startX_, startY_;
    std::vector<std::pair<int,int>> targets_;

    CanPassFunc canPassFunc_;
    CostFunc    costFunc_;
    HeuristicFunc heuristicFunc_;
    void *canPassUserData_, *costUserData_, *heuristicUserData_;

    bool useCustomDirections_;
    std::vector<MoveDir> customDirs_;

    std::vector<float> worldX_, worldY_;
    bool hasWorldCoords_;

    std::vector<PathNode> nodePool_;
    std::vector<std::pair<int,int>> path_;
    float totalPathCost_;

    float maxSearchTime_;
    int   maxIterations_;

    int idx(int x, int y) const { return y*width_+x; }
    float height(int x, int y) const { return heights_[idx(x,y)]; }
    bool isWalkable(int x, int y) const { return walkable_[idx(x,y)] != 0; }
    float costMultiplier(int x, int y) const { return costMultiplier_[idx(x,y)]; }
    bool inBounds(int x, int y) const { return x>=0 && x<width_ && y>=0 && y<height_; }
    bool isTarget(int x, int y) const {
        for (auto& t : targets_) if (t.first==x && t.second==y) return true;
        return false;
    }

    float computeMinHeuristic(int x, int y) const {
        if (heuristicFunc_) {
            float best = std::numeric_limits<float>::max();
            for (auto& t : targets_) {
                float h = heuristicFunc_(x, y, t.first, t.second, heuristicUserData_);
                if (h < best) best = h;
            }
            return best == std::numeric_limits<float>::max() ? 0.0f : best;
        }
        float best = std::numeric_limits<float>::max();
        for (auto& t : targets_) {
            int dx = std::abs(x - t.first), dy = std::abs(y - t.second);
            float h = (allowDiagonal_ && !useCustomDirections_) 
                ? 1.0f*(dx+dy) + (1.41421356f-2.0f)*std::min(dx,dy) 
                : (float)(dx+dy);
            if (h < best) best = h;
        }
        return best == std::numeric_limits<float>::max() ? 0.0f : best;
    }

    bool sanityCheck() const {
        if (width_<=0 || height_<=0 || heights_.empty()) return false;
        if (!inBounds(startX_,startY_) || !isWalkable(startX_,startY_)) return false;
        if (canPassFunc_ && !canPassFunc_(startX_,startY_,canPassUserData_)) return false;
        if (targets_.empty()) return false;
        return true;
    }

    void initNodePool() {
        size_t total = width_*height_;
        nodePool_.clear();
        nodePool_.resize(total);
        for (int y=0; y<height_; ++y) for (int x=0; x<width_; ++x) {
            int i = idx(x,y);
            nodePool_[i].x = x; nodePool_[i].y = y;
            nodePool_[i].g = std::numeric_limits<float>::infinity();
            nodePool_[i].h = 0;
            nodePool_[i].parentIdx = -1;
            nodePool_[i].enterDir = -1;
            nodePool_[i].closed = false;
        }
    }
    void clearNodePool() { nodePool_.clear(); }

    void rebuildPath(int endIdx) {
        path_.clear();
        int cur = endIdx;
        while (cur != -1) {
            path_.emplace_back(nodePool_[cur].x, nodePool_[cur].y);
            cur = nodePool_[cur].parentIdx;
        }
        std::reverse(path_.begin(), path_.end());
    }

    bool lineOfSight(int x1, int y1, int x2, int y2) {
        int dx = abs(x2-x1), dy = abs(y2-y1);
        int sx = x1<x2 ? 1 : -1, sy = y1<y2 ? 1 : -1;
        int err = dx-dy;
        int x=x1, y=y1;
        while (true) {
            if (x==x2 && y==y2) break;
            int e2 = err*2;
            if (e2 > -dy) { err -= dy; x += sx; }
            if (e2 < dx)  { err += dx; y += sy; }
            if (x==x1 && y==y1) continue;
            if (!inBounds(x,y)) return false;
            if (!isWalkable(x,y)) return false;
            if (canPassFunc_ && !canPassFunc_(x,y,canPassUserData_)) return false;
            if (std::fabs(height(x,y)-height(x1,y1)) > maxHeightDiff_) return false;
        }
        return true;
    }
};


extern "C" {

EXPORT void* PathFinder_Create() { return new PathFinder(); }
EXPORT void  PathFinder_Destroy(void* h) { delete static_cast<PathFinder*>(h); }

EXPORT void PathFinder_SetMap(void* h, int w, int hgt,
                              const float* heights, const int* walkable,
                              const float* costMul) {
    static_cast<PathFinder*>(h)->setMap(w, hgt, heights, walkable, costMul);
}

EXPORT void PathFinder_SetMapFromStruct(void* h, int w, int hgt,
                                        const void* data, int structSize,
                                        int hOff, int wOff, int cOff) {
    static_cast<PathFinder*>(h)->setMapFromStruct(w, hgt, data, structSize, hOff, wOff, cOff);
}

EXPORT int PathFinder_LoadMapFromFile(void* h, const char* fn) {
    return static_cast<PathFinder*>(h)->loadMapFromFile(fn) ? 1 : 0;
}

EXPORT int PathFinder_ExportMapToBuffer(void* h, char* buf, int* size, int incWC) {
    return static_cast<PathFinder*>(h)->exportMapToBuffer(buf, size, incWC);
}

EXPORT int PathFinder_SaveMapToFile(void* h, const char* fn, int incWC) {
    return static_cast<PathFinder*>(h)->saveMapToFile(fn, incWC) ? 1 : 0;
}

EXPORT void PathFinder_SetCellHeight(void* h, int x, int y, float v) {
    static_cast<PathFinder*>(h)->setCellHeight(x, y, v);
}
EXPORT void PathFinder_SetCellWalkable(void* h, int x, int y, int v) {
    static_cast<PathFinder*>(h)->setCellWalkable(x, y, v);
}
EXPORT void PathFinder_SetCellCostMultiplier(void* h, int x, int y, float v) {
    static_cast<PathFinder*>(h)->setCellCostMultiplier(x, y, v);
}

EXPORT void PathFinder_SetStart(void* h, int x, int y) { static_cast<PathFinder*>(h)->setStart(x, y); }
EXPORT void PathFinder_ClearTargets(void* h) { static_cast<PathFinder*>(h)->clearTargets(); }
EXPORT void PathFinder_AddTarget(void* h, int x, int y) { static_cast<PathFinder*>(h)->addTarget(x, y); }
EXPORT int  PathFinder_GetTargetCount(void* h) { return static_cast<PathFinder*>(h)->getTargetCount(); }
EXPORT int  PathFinder_GetTarget(void* h, int i, int* x, int* y) {
    return static_cast<PathFinder*>(h)->getTarget(i, x, y) ? 1 : 0;
}

EXPORT void PathFinder_SetMaxHeightDiff(void* h, float d) { static_cast<PathFinder*>(h)->setMaxHeightDiff(d); }
EXPORT void PathFinder_SetHeightPenalty(void* h, float p, float up, float dn) {
    static_cast<PathFinder*>(h)->setHeightPenalty(p, up, dn);
}
EXPORT void PathFinder_SetAllowDiagonal(void* h, int a) { static_cast<PathFinder*>(h)->setAllowDiagonal(a != 0); }
EXPORT void PathFinder_SetTurnPenalty(void* h, float p) { static_cast<PathFinder*>(h)->setTurnPenalty(p); }
EXPORT void PathFinder_SetCustomMoveDirections(void* h, const int* dx, const int* dy, const float* costs, int cnt) {
    static_cast<PathFinder*>(h)->setCustomMoveDirections(dx, dy, costs, cnt);
}
EXPORT void PathFinder_SetCustomCanPass(void* h, CanPassFunc f, void* u) { static_cast<PathFinder*>(h)->setCustomCanPass(f, u); }
EXPORT void PathFinder_SetCustomCost(void* h, CostFunc f, void* u) { static_cast<PathFinder*>(h)->setCustomCost(f, u); }
EXPORT void PathFinder_SetCustomHeuristic(void* h, HeuristicFunc f, void* u) { static_cast<PathFinder*>(h)->setCustomHeuristic(f, u); }
EXPORT void PathFinder_SetWorldCoords(void* h, const float* xc, const float* yc) {
    static_cast<PathFinder*>(h)->setWorldCoords(xc, yc);
}
EXPORT void PathFinder_SetMaxSearchTime(void* h, float s) { static_cast<PathFinder*>(h)->setMaxSearchTime(s); }
EXPORT void PathFinder_SetMaxIterations(void* h, int m) { static_cast<PathFinder*>(h)->setMaxIterations(m); }

EXPORT int PathFinder_FindPath(void* h) { return static_cast<PathFinder*>(h)->findPath() ? 1 : 0; }
EXPORT void PathFinder_SmoothPath(void* h, float a) { static_cast<PathFinder*>(h)->smoothPath(a); }

EXPORT int PathFinder_GetPathLength(void* h) { return static_cast<PathFinder*>(h)->getPathLength(); }
EXPORT int PathFinder_GetPathPoint(void* h, int i, int* x, int* y) {
    return static_cast<PathFinder*>(h)->getPathPoint(i, x, y) ? 1 : 0;
}
EXPORT int PathFinder_GetAllPathGridPoints(void* h, int* x, int* y, int max) {
    return static_cast<PathFinder*>(h)->getAllPathGridPoints(x, y, max);
}
EXPORT int PathFinder_GetAllPathWorldPoints(void* h, float* x, float* y, int max) {
    return static_cast<PathFinder*>(h)->getAllPathWorldPoints(x, y, max);
}
EXPORT int PathFinder_GetAllPathHeights(void* h, float* hs, int max) {
    return static_cast<PathFinder*>(h)->getAllPathHeights(hs, max);
}
EXPORT float PathFinder_GetPathCost(void* h) { return static_cast<PathFinder*>(h)->getPathCost(); }

}
