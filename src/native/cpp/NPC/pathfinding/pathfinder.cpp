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
    int enterDir  = -1;
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
          postProcessFunc_(nullptr),
          canPassUD_(nullptr), costUD_(nullptr), heuristicUD_(nullptr), postUD_(nullptr),
          useCustomDirections_(false), hasWorldCoords_(false),
          totalPathCost_(0.0f),
          maxSearchTime_(-1.0f), maxIterations_(-1),
          tacticalMode_(PF_MODE_DIRECT),
          followTX_(-1), followTY_(-1), followOX_(0), followOY_(0),
          unitSizeX_(1), unitSizeY_(1),
          terrainTypes_(nullptr), terrainCostTable_(nullptr), numTerrainTypes_(0),
          mapLoadFunc_(nullptr), mapLoadUD_(nullptr) {}

    ~PathFinder() { clearNodePool(); }

    // ---------- 地图加载 ----------
    void setMap(int w, int h, const float* heights, const int* walkable, const float* costMul) {
        width_ = w; height_ = h;
        size_t sz = w * h;
        heights_.assign(heights, heights + sz);
        baseWalkable_.assign(walkable, walkable + sz);
        if (costMul) costMultiplier_.assign(costMul, costMul + sz);
        else         costMultiplier_.assign(sz, 1.0f);
        dynamicBlocked_.assign(sz, 0);
        userData_.assign(sz, 0);
        path_.clear();
        totalPathCost_ = 0.0f;
    }

    void setMapFromStruct(int w, int h, const void* data, int structSize,
                          int hOff, int wOff, int cOff) {
        width_ = w; height_ = h;
        size_t sz = w * h;
        heights_.resize(sz);
        baseWalkable_.resize(sz);
        costMultiplier_.resize(sz);
        const char* base = static_cast<const char*>(data);
        for (int i = 0; i < (int)sz; ++i) {
            const char* item = base + i * structSize;
            heights_[i] = *reinterpret_cast<const float*>(item + hOff);
            baseWalkable_[i] = *reinterpret_cast<const int*>(item + wOff);
            costMultiplier_[i] = (cOff >= 0) ? *reinterpret_cast<const float*>(item + cOff) : 1.0f;
        }
        dynamicBlocked_.assign(sz, 0);
        userData_.assign(sz, 0);
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
        fgets(line, sizeof(line), f);
        for (int i = 0; i < w*h; ) {
            if (!fgets(line, sizeof(line), f)) break;
            if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
            float hgt; int wk; float cm = 1.0f;
            int read = sscanf(line, "%f %d %f", &hgt, &wk, &cm);
            if (read >= 2) {
                heights.push_back(hgt);
                walkable.push_back(wk);
                costMul.push_back(read == 3 ? cm : 1.0f);
                ++i;
            } else { fclose(f); return false; }
        }
        fclose(f);
        if ((int)heights.size() != w*h) return false;
        setMap(w, h, heights.data(), walkable.data(), costMul.data());
        return true;
    }

    // 自定义地图加载回调
    void setMapLoadCallback(MapLoadFunc func, void* ud) {
        mapLoadFunc_ = func;
        mapLoadUD_ = ud;
    }

    bool loadCustomMap(const char* filename) {
        if (!mapLoadFunc_) return false;
        return mapLoadFunc_(this, filename, mapLoadUD_) != 0;
    }

    void setDynamicWalkable(const int* blocked) {
        if (!blocked) { std::fill(dynamicBlocked_.begin(), dynamicBlocked_.end(), 0); return; }
        std::copy(blocked, blocked + width_ * height_, dynamicBlocked_.begin());
    }

    void setUnitSize(int sx, int sy) { unitSizeX_ = sx; unitSizeY_ = sy; }

    void setTerrainTypes(const int* types, const float* costTable, int numTypes) {
        terrainTypes_ = types;
        terrainCostTable_ = costTable;
        numTerrainTypes_ = numTypes;
    }

    // 自定义格子元素数据
    void setCellUserDataArray(const int* userData) {
        if (!userData || width_<=0 || height_<=0) return;
        std::copy(userData, userData + width_*height_, userData_.begin());
    }
    void setCellUserData(int x, int y, int data) {
        if (inBounds(x,y)) userData_[idx(x,y)] = data;
    }
    int getCellUserData(int x, int y) const {
        if (inBounds(x,y)) return userData_[idx(x,y)];
        return 0;
    }

    int exportMapToBuffer(char* buf, int* outSize, int inclWC) {
        int cellSize = sizeof(float)+sizeof(int)+sizeof(float);
        if (inclWC) cellSize += 2*sizeof(float);
        int headerSize = 2*sizeof(int);
        if (inclWC) headerSize += sizeof(int);
        int total = headerSize + width_*height_*cellSize;
        if (outSize) *outSize = total;
        if (!buf) return 0;
        char* p = buf;
        memcpy(p, &width_, sizeof(int)); p+=sizeof(int);
        memcpy(p, &height_, sizeof(int)); p+=sizeof(int);
        if (inclWC) { int flags=1; memcpy(p,&flags,sizeof(int)); p+=sizeof(int); }
        for (int i=0; i<width_*height_; ++i) {
            memcpy(p,&heights_[i],sizeof(float)); p+=sizeof(float);
            int wk = baseWalkable_[i] && dynamicBlocked_[i]==0 ? 1 : 0;
            memcpy(p,&wk,sizeof(int)); p+=sizeof(int);
            memcpy(p,&costMultiplier_[i],sizeof(float)); p+=sizeof(float);
            if (inclWC) {
                float wx = hasWorldCoords_ ? worldX_[i] : 0.0f;
                float wy = hasWorldCoords_ ? worldY_[i] : 0.0f;
                memcpy(p,&wx,sizeof(float)); p+=sizeof(float);
                memcpy(p,&wy,sizeof(float)); p+=sizeof(float);
            }
        }
        return total;
    }

    bool saveMapToFile(const char* fn, int inclWC) {
        int size=0;
        exportMapToBuffer(nullptr,&size,inclWC);
        std::vector<char> buf(size);
        int written = exportMapToBuffer(buf.data(),&size,inclWC);
        if (written<=0) return false;
        FILE* f = fopen(fn,"wb");
        if (!f) return false;
        fwrite(buf.data(),1,written,f);
        fclose(f);
        return true;
    }

    void setCellHeight(int x,int y,float h) { if(inBounds(x,y)) heights_[idx(x,y)]=h; }
    void setCellWalkable(int x,int y,int w) { if(inBounds(x,y)) baseWalkable_[idx(x,y)]=w; }
    void setCellCostMultiplier(int x,int y,float c) { if(inBounds(x,y)) costMultiplier_[idx(x,y)]=c; }

    void clearTargets() { targets_.clear(); }
    void addTarget(int x,int y) { if(inBounds(x,y)) targets_.push_back({x,y}); }
    void setStart(int sx,int sy) { startX_=sx; startY_=sy; }
    int  getTargetCount() const { return (int)targets_.size(); }
    bool getTarget(int i,int* x,int* y) const {
        if(i<0||i>=(int)targets_.size()) return false;
        *x=targets_[i].first; *y=targets_[i].second; return true;
    }

    void setMaxHeightDiff(float d) { maxHeightDiff_=d; }
    void setHeightPenalty(float p,float up,float dn) { heightPenaltyPerUnit_=p; uphillFactor_=up; downhillFactor_=dn; }
    void setAllowDiagonal(bool a) { allowDiagonal_=a; }
    void setTurnPenalty(float p) { turnPenalty_=p; }
    void setCustomMoveDirections(const int* dx,const int* dy,const float* costs,int cnt) {
        customDirs_.clear();
        for(int i=0;i<cnt;++i) customDirs_.push_back({dx[i],dy[i],costs[i]});
        useCustomDirections_=true;
    }

    void setCustomCanPass(CanPassFunc f,void* u) { canPassFunc_=f; canPassUD_=u; }
    void setCustomCost(CostFunc f,void* u) { costFunc_=f; costUD_=u; tacticalMode_=PF_MODE_DIRECT; }
    void setCustomHeuristic(HeuristicFunc f,void* u) { heuristicFunc_=f; heuristicUD_=u; }
    void setPathPostProcess(PathPostProcessFunc f,void* u) { postProcessFunc_=f; postUD_=u; }

    void setWorldCoords(const float* xc,const float* yc) {
        if(!xc||!yc||width_<=0||height_<=0) return;
        size_t sz=width_*height_;
        worldX_.assign(xc,xc+sz); worldY_.assign(yc,yc+sz);
        hasWorldCoords_=true;
    }

    void setTacticalMode(PathFinderMode mode) { tacticalMode_=mode; costFunc_=nullptr; costUD_=nullptr; }

    void setFollowTarget(int tx,int ty,int ox,int oy) {
        followTX_=tx; followTY_=ty; followOX_=ox; followOY_=oy;
        clearTargets(); int ttx=tx+ox, tty=ty+oy;
        if(inBounds(ttx,tty)) addTarget(ttx,tty);
    }

    void setMaxSearchTime(float s) { maxSearchTime_=s; }
    void setMaxIterations(int m) { maxIterations_=m; }

    bool findPath() {
        path_.clear(); totalPathCost_=0.0f;
        if(!sanityCheck()) return false;

        if(tacticalMode_==PF_MODE_FOLLOW) {
            clearTargets();
            int ttx=followTX_+followOX_, tty=followTY_+followOY_;
            if(inBounds(ttx,tty)) addTarget(ttx,tty); else return false;
        }

        initNodePool();
        std::priority_queue<std::pair<float,int>> open;
        int startIdx = idx(startX_,startY_);
        PathNode& startNode = nodePool_[startIdx];
        startNode.g=0; startNode.h=computeMinHeuristic(startX_,startY_);
        open.emplace(-startNode.f(),startIdx);

        std::vector<MoveDir> dirs;
        if(useCustomDirections_) dirs = customDirs_;
        else {
            const int dx8[8]={1,-1,0,0,1,-1,1,-1};
            const int dy8[8]={0,0,1,-1,1,1,-1,-1};
            const float cost8[8]={1,1,1,1,1.41421356f,1.41421356f,1.41421356f,1.41421356f};
            int dcount = allowDiagonal_ ? 8 : 4;
            for(int i=0;i<dcount;++i) dirs.push_back({dx8[i],dy8[i],cost8[i]});
        }

        auto startTime = std::chrono::steady_clock::now();
        int iter=0; bool found=false; int finalIdx=-1;

        while(!open.empty()) {
            if(maxSearchTime_>0.0f) {
                float el = std::chrono::duration<float>(std::chrono::steady_clock::now()-startTime).count();
                if(el>=maxSearchTime_) break;
            }
            if(maxIterations_>0 && iter>=maxIterations_) break;

            int curIdx = open.top().second; open.pop();
            PathNode& cur = nodePool_[curIdx];
            if(cur.closed) continue;
            cur.closed=true; iter++;

            if(isTarget(cur.x,cur.y)) { found=true; finalIdx=curIdx; break; }

            for(size_t d=0;d<dirs.size();++d) {
                int nx=cur.x+dirs[d].dx, ny=cur.y+dirs[d].dy;
                if(!inBounds(nx,ny)) continue;
                if(!isWalkable(nx,ny)) continue;
                if(canPassFunc_ && !canPassFunc_(nx,ny,canPassUD_)) continue;

                float hFrom=height(cur.x,cur.y), hTo=height(nx,ny);
                if(std::fabs(hTo-hFrom)>maxHeightDiff_) continue;

                if(!useCustomDirections_ && d>=4) {
                    int ax1=cur.x+dirs[d].dx, ay1=cur.y;
                    int ax2=cur.x, ay2=cur.y+dirs[d].dy;
                    if(!isWalkable(ax1,ay1)||!isWalkable(ax2,ay2)) continue;
                    if(canPassFunc_ && (!canPassFunc_(ax1,ay1,canPassUD_)||
                                        !canPassFunc_(ax2,ay2,canPassUD_))) continue;
                }

                if(unitSizeX_>1 || unitSizeY_>1) {
                    bool blocked=false;
                    for(int sy=0;sy<unitSizeY_ && !blocked;++sy)
                        for(int sx=0;sx<unitSizeX_ && !blocked;++sx)
                            if(!isWalkable(nx+sx,ny+sy)) blocked=true;
                    if(blocked) continue;
                }

                float moveCost = dirs[d].cost * costMultiplier(nx,ny);
                if(terrainTypes_ && terrainCostTable_) {
                    int t = terrainTypes_[idx(nx,ny)];
                    if(t>=0 && t<numTerrainTypes_) moveCost *= terrainCostTable_[t];
                }

                float diff = hTo-hFrom;
                float penalty = heightPenaltyPerUnit_*std::fabs(diff);
                if(diff>0) penalty*=uphillFactor_; else penalty*=downhillFactor_;
                moveCost+=penalty;

                if(turnPenalty_>0.0f && cur.enterDir>=0 && (int)d!=cur.enterDir)
                    moveCost+=turnPenalty_;

                if(costFunc_) moveCost+=costFunc_(cur.x,cur.y,nx,ny,hFrom,hTo,costUD_);

                float tg = cur.g+moveCost;
                int nbIdx = idx(nx,ny);
                PathNode& nb = nodePool_[nbIdx];
                if(tg < nb.g) {
                    nb.g=tg; nb.h=computeMinHeuristic(nx,ny);
                    nb.parentIdx=curIdx; nb.enterDir=(int)d;
                    open.emplace(-nb.f(),nbIdx);
                }
            }
        }

        if(found && finalIdx>=0) {
            rebuildPath(finalIdx);
            totalPathCost_ = nodePool_[finalIdx].g;
            if(postProcessFunc_) {
                int len = (int)path_.size();
                std::vector<int> xs(len), ys(len);
                for(int i=0;i<len;++i) { xs[i]=path_[i].first; ys[i]=path_[i].second; }
                postProcessFunc_(xs.data(), ys.data(), len, postUD_);
                path_.clear();
                for(int i=0;i<len;++i) path_.emplace_back(xs[i],ys[i]);
            }
        }
        return found;
    }

    bool isPathValid() const {
        if(path_.empty()) return false;
        if(path_.front().first!=startX_ || path_.front().second!=startY_) return false;
        if(tacticalMode_==PF_MODE_FOLLOW) {
            int ttx=followTX_+followOX_, tty=followTY_+followOY_;
            if(abs(path_.back().first-ttx)>1 || abs(path_.back().second-tty)>1) return false;
        } else {
            bool ok=false;
            for(auto& t:targets_) if(t.first==path_.back().first&&t.second==path_.back().second) { ok=true; break; }
            if(!ok) return false;
        }
        for(auto& p:path_) if(!isWalkable(p.first,p.second)) return false;
        return true;
    }

    void smoothPath(float) {
        if(path_.size()<3) return;
        std::vector<std::pair<int,int>> s; s.push_back(path_.front());
        size_t i=1;
        while(i<path_.size()-1) {
            size_t last=path_.size()-1;
            for(size_t k=i;k<path_.size();++k) {
                if(lineOfSight(s.back().first,s.back().second,path_[k].first,path_[k].second)) last=k;
                else break;
            }
            s.push_back(path_[last]); i=last+1;
        }
        s.push_back(path_.back()); path_=std::move(s);
    }

    int  getPathLength() const { return (int)path_.size(); }
    bool getPathPoint(int i,int* x,int* y) const {
        if(i<0||i>=(int)path_.size()) return false;
        *x=path_[i].first; *y=path_[i].second; return true;
    }
    int getAllPathGridPoints(int* x,int* y,int max) const {
        if(max<(int)path_.size()) return -1;
        for(size_t i=0;i<path_.size();++i) { x[i]=path_[i].first; y[i]=path_[i].second; }
        return (int)path_.size();
    }
    int getAllPathWorldPoints(float* x,float* y,int max) const {
        if(!hasWorldCoords_) return -1;
        if(max<(int)path_.size()) return -1;
        for(size_t i=0;i<path_.size();++i) {
            int id = idx(path_[i].first,path_[i].second);
            x[i]=worldX_[id]; y[i]=worldY_[id];
        }
        return (int)path_.size();
    }
    int getAllPathHeights(float* h,int max) const {
        if(max<(int)path_.size()) return -1;
        for(size_t i=0;i<path_.size();++i) h[i]=height(path_[i].first,path_[i].second);
        return (int)path_.size();
    }
    float getPathCost() const { return totalPathCost_; }
    bool getPathWorldPoint(int i,float* wx,float* wy) const {
        if(!hasWorldCoords_ || i<0 || i>=(int)path_.size()) return false;
        int id=idx(path_[i].first,path_[i].second); *wx=worldX_[id]; *wy=worldY_[id]; return true;
    }
    bool getPathHeight(int i,float* h) const {
        if(i<0||i>=(int)path_.size()) return false;
        *h=height(path_[i].first,path_[i].second); return true;
    }

private:
    int width_, height_;
    std::vector<float> heights_;
    std::vector<int>   baseWalkable_;
    std::vector<int>   dynamicBlocked_;   // 动态阻塞计数
    std::vector<float> costMultiplier_;   // 基础代价乘数
    std::vector<int>   userData_;         // 自定义格子元素数据

    float maxHeightDiff_;
    float heightPenaltyPerUnit_;
    float uphillFactor_, downhillFactor_;
    bool allowDiagonal_;
    float turnPenalty_;

    int startX_, startY_;
    std::vector<std::pair<int,int>> targets_;

    CanPassFunc canPassFunc_; void* canPassUD_;
    CostFunc    costFunc_;    void* costUD_;
    HeuristicFunc heuristicFunc_; void* heuristicUD_;
    PathPostProcessFunc postProcessFunc_; void* postUD_;

    bool useCustomDirections_;
    std::vector<MoveDir> customDirs_;

    std::vector<float> worldX_, worldY_;
    bool hasWorldCoords_;

    std::vector<PathNode> nodePool_;
    std::vector<std::pair<int,int>> path_;
    float totalPathCost_;

    float maxSearchTime_;
    int   maxIterations_;

    PathFinderMode tacticalMode_;
    int followTX_, followTY_, followOX_, followOY_;

    int unitSizeX_, unitSizeY_;
    const int* terrainTypes_;
    const float* terrainCostTable_;
    int numTerrainTypes_;

    MapLoadFunc mapLoadFunc_;
    void*       mapLoadUD_;

    int idx(int x,int y) const { return y*width_+x; }
    float height(int x,int y) const { return heights_[idx(x,y)]; }
    bool isWalkable(int x,int y) const {
        if(!inBounds(x,y)) return false;
        int i=idx(x,y);
        return baseWalkable_[i] && (dynamicBlocked_.empty() || dynamicBlocked_[i]==0);
    }
    float costMultiplier(int x,int y) const { return costMultiplier_[idx(x,y)]; }
    bool inBounds(int x,int y) const { return x>=0 && x<width_ && y>=0 && y<height_; }
    bool isTarget(int x,int y) const {
        for(auto& t:targets_) if(t.first==x && t.second==y) return true;
        return false;
    }

    float computeMinHeuristic(int x,int y) const {
        if(heuristicFunc_) {
            float best=std::numeric_limits<float>::max();
            for(auto& t:targets_) {
                float h=heuristicFunc_(x,y,t.first,t.second,heuristicUD_);
                if(h<best) best=h;
            }
            return best==std::numeric_limits<float>::max()?0.0f:best;
        }
        float best=std::numeric_limits<float>::max();
        for(auto& t:targets_) {
            int dx=std::abs(x-t.first), dy=std::abs(y-t.second);
            float h = (allowDiagonal_&&!useCustomDirections_)
                ? 1.0f*(dx+dy)+(1.41421356f-2.0f)*std::min(dx,dy) : (float)(dx+dy);
            if(h<best) best=h;
        }
        return best==std::numeric_limits<float>::max()?0.0f:best;
    }

    bool sanityCheck() const {
        if(width_<=0||height_<=0||heights_.empty()) return false;
        if(!inBounds(startX_,startY_)||!isWalkable(startX_,startY_)) return false;
        if(canPassFunc_&&!canPassFunc_(startX_,startY_,canPassUD_)) return false;
        if(targets_.empty()) return false;
        return true;
    }

    void initNodePool() {
        size_t total=width_*height_;
        nodePool_.clear(); nodePool_.resize(total);
        for(int y=0;y<height_;++y) for(int x=0;x<width_;++x) {
            int i=idx(x,y);
            nodePool_[i].x=x; nodePool_[i].y=y;
            nodePool_[i].g=std::numeric_limits<float>::infinity();
            nodePool_[i].h=0; nodePool_[i].parentIdx=-1;
            nodePool_[i].enterDir=-1; nodePool_[i].closed=false;
        }
    }
    void clearNodePool() { nodePool_.clear(); }

    void rebuildPath(int endIdx) {
        path_.clear();
        int cur=endIdx;
        while(cur!=-1) { path_.emplace_back(nodePool_[cur].x,nodePool_[cur].y); cur=nodePool_[cur].parentIdx; }
        std::reverse(path_.begin(),path_.end());
    }

    bool lineOfSight(int x1,int y1,int x2,int y2) {
        int dx=abs(x2-x1), dy=abs(y2-y1);
        int sx=x1<x2?1:-1, sy=y1<y2?1:-1;
        int err=dx-dy, x=x1, y=y1;
        while(true) {
            if(x==x2&&y==y2) break;
            int e2=err*2;
            if(e2>-dy) { err-=dy; x+=sx; }
            if(e2<dx)  { err+=dx; y+=sy; }
            if(x==x1&&y==y1) continue;
            if(!inBounds(x,y)) return false;
            if(!isWalkable(x,y)) return false;
            if(canPassFunc_&&!canPassFunc_(x,y,canPassUD_)) return false;
            if(std::fabs(height(x,y)-height(x1,y1))>maxHeightDiff_) return false;
        }
        return true;
    }
};

// ==================== C 接口 ====================
extern "C" {

EXPORT void* PathFinder_Create() { return new PathFinder(); }
EXPORT void  PathFinder_Destroy(void* h) { delete (PathFinder*)h; }

EXPORT void PathFinder_SetMap(void* h, int w, int hgt,
                              const float* heights, const int* walkable,
                              const float* costMul) {
    ((PathFinder*)h)->setMap(w, hgt, heights, walkable, costMul);
}
EXPORT void PathFinder_SetMapFromStruct(void* h, int w, int hgt,
                                        const void* data, int ss, int hoff, int woff, int coff) {
    ((PathFinder*)h)->setMapFromStruct(w, hgt, data, ss, hoff, woff, coff);
}
EXPORT int PathFinder_LoadMapFromFile(void* h, const char* fn) {
    return ((PathFinder*)h)->loadMapFromFile(fn) ? 1 : 0;
}
EXPORT void PathFinder_SetMapLoadCallback(void* h, MapLoadFunc func, void* ud) {
    ((PathFinder*)h)->setMapLoadCallback(func, ud);
}
EXPORT int PathFinder_LoadCustomMap(void* h, const char* fn) {
    return ((PathFinder*)h)->loadCustomMap(fn) ? 1 : 0;
}
EXPORT void PathFinder_SetDynamicWalkable(void* h, const int* blocked) {
    ((PathFinder*)h)->setDynamicWalkable(blocked);
}
EXPORT void PathFinder_SetUnitSize(void* h, int sx, int sy) {
    ((PathFinder*)h)->setUnitSize(sx, sy);
}
EXPORT void PathFinder_SetTerrainTypes(void* h, const int* types, const float* table, int n) {
    ((PathFinder*)h)->setTerrainTypes(types, table, n);
}
EXPORT void PathFinder_SetCellUserDataArray(void* h, const int* userData) {
    ((PathFinder*)h)->setCellUserDataArray(userData);
}
EXPORT void PathFinder_SetCellUserData(void* h, int x, int y, int data) {
    ((PathFinder*)h)->setCellUserData(x, y, data);
}
EXPORT int PathFinder_GetCellUserData(void* h, int x, int y) {
    return ((PathFinder*)h)->getCellUserData(x, y);
}
EXPORT int PathFinder_ExportMapToBuffer(void* h, char* b, int* sz, int wc) {
    return ((PathFinder*)h)->exportMapToBuffer(b, sz, wc);
}
EXPORT int PathFinder_SaveMapToFile(void* h, const char* fn, int wc) {
    return ((PathFinder*)h)->saveMapToFile(fn, wc) ? 1 : 0;
}
EXPORT void PathFinder_SetCellHeight(void* h, int x, int y, float v) { ((PathFinder*)h)->setCellHeight(x,y,v); }
EXPORT void PathFinder_SetCellWalkable(void* h, int x, int y, int v) { ((PathFinder*)h)->setCellWalkable(x,y,v); }
EXPORT void PathFinder_SetCellCostMultiplier(void* h, int x, int y, float v) { ((PathFinder*)h)->setCellCostMultiplier(x,y,v); }
EXPORT void PathFinder_SetStart(void* h, int x, int y) { ((PathFinder*)h)->setStart(x,y); }
EXPORT void PathFinder_ClearTargets(void* h) { ((PathFinder*)h)->clearTargets(); }
EXPORT void PathFinder_AddTarget(void* h, int x, int y) { ((PathFinder*)h)->addTarget(x,y); }
EXPORT int  PathFinder_GetTargetCount(void* h) { return ((PathFinder*)h)->getTargetCount(); }
EXPORT int  PathFinder_GetTarget(void* h, int i, int* x, int* y) { return ((PathFinder*)h)->getTarget(i,x,y)?1:0; }
EXPORT void PathFinder_SetMaxHeightDiff(void* h, float d) { ((PathFinder*)h)->setMaxHeightDiff(d); }
EXPORT void PathFinder_SetHeightPenalty(void* h, float p, float up, float dn) { ((PathFinder*)h)->setHeightPenalty(p,up,dn); }
EXPORT void PathFinder_SetAllowDiagonal(void* h, int a) { ((PathFinder*)h)->setAllowDiagonal(a!=0); }
EXPORT void PathFinder_SetTurnPenalty(void* h, float p) { ((PathFinder*)h)->setTurnPenalty(p); }
EXPORT void PathFinder_SetCustomMoveDirections(void* h, const int* dx, const int* dy, const float* cs, int cnt) {
    ((PathFinder*)h)->setCustomMoveDirections(dx,dy,cs,cnt);
}
EXPORT void PathFinder_SetCustomCanPass(void* h, CanPassFunc f, void* u) { ((PathFinder*)h)->setCustomCanPass(f,u); }
EXPORT void PathFinder_SetCustomCost(void* h, CostFunc f, void* u) { ((PathFinder*)h)->setCustomCost(f,u); }
EXPORT void PathFinder_SetCustomHeuristic(void* h, HeuristicFunc f, void* u) { ((PathFinder*)h)->setCustomHeuristic(f,u); }
EXPORT void PathFinder_SetPathPostProcess(void* h, PathPostProcessFunc f, void* u) { ((PathFinder*)h)->setPathPostProcess(f,u); }
EXPORT void PathFinder_SetWorldCoords(void* h, const float* xc, const float* yc) { ((PathFinder*)h)->setWorldCoords(xc,yc); }
EXPORT void PathFinder_SetTacticalMode(void* h, PathFinderMode m) { ((PathFinder*)h)->setTacticalMode(m); }
EXPORT void PathFinder_SetFollowTarget(void* h, int tx, int ty, int ox, int oy) { ((PathFinder*)h)->setFollowTarget(tx,ty,ox,oy); }
EXPORT void PathFinder_SetMaxSearchTime(void* h, float s) { ((PathFinder*)h)->setMaxSearchTime(s); }
EXPORT void PathFinder_SetMaxIterations(void* h, int m) { ((PathFinder*)h)->setMaxIterations(m); }
EXPORT int  PathFinder_FindPath(void* h) { return ((PathFinder*)h)->findPath()?1:0; }
EXPORT int  PathFinder_IsPathValid(void* h) { return ((PathFinder*)h)->isPathValid()?1:0; }
EXPORT void PathFinder_SmoothPath(void* h, float a) { ((PathFinder*)h)->smoothPath(a); }
EXPORT int  PathFinder_GetPathLength(void* h) { return ((PathFinder*)h)->getPathLength(); }
EXPORT int  PathFinder_GetPathPoint(void* h, int i, int* x, int* y) { return ((PathFinder*)h)->getPathPoint(i,x,y)?1:0; }
EXPORT int  PathFinder_GetAllPathGridPoints(void* h, int* x, int* y, int max) { return ((PathFinder*)h)->getAllPathGridPoints(x,y,max); }
EXPORT int  PathFinder_GetAllPathWorldPoints(void* h, float* x, float* y, int max) { return ((PathFinder*)h)->getAllPathWorldPoints(x,y,max); }
EXPORT int  PathFinder_GetAllPathHeights(void* h, float* hs, int max) { return ((PathFinder*)h)->getAllPathHeights(hs,max); }
EXPORT float PathFinder_GetPathCost(void* h) { return ((PathFinder*)h)->getPathCost(); }
EXPORT int  PathFinder_GetPathWorldPoint(void* h, int i, float* wx, float* wy) { return ((PathFinder*)h)->getPathWorldPoint(i,wx,wy)?1:0; }
EXPORT int  PathFinder_GetPathHeight(void* h, int i, float* hgt) { return ((PathFinder*)h)->getPathHeight(i,hgt)?1:0; }

}
