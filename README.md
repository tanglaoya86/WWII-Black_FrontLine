## 我重新选了个方案，就是下面这个。重新建的模是ar196，现在只建了个下面那个浮的（还没精修完），图片是那个""捕获.PNG""
""



第 1 步：起形（MagicaVoxel，10 分钟）

打开 MagicaVoxel，用方块堆一个坦克大体块：车身、炮塔、炮管、履带

不用雕细节，只看剪影对不对

File → Export → OBJ，存到项目文件夹

不想用体素的话，Blender 里直接用一个 Cube 拉伸也行，跳过这步。

第 2 步：重拓扑（Blender，做干净低模）

导入 OBJ：File → Import → Wavefront (.obj)

用这些工具把模型收拾干净：

Tab 进编辑模式，X 删多余面

M 合并重叠顶点

加 Decimate 修改器 降面数，保留硬边块面感

目标：几百到一两千面，看着像积木但边缘干净

第 3 步：展 UV（关键，烘焙前必做）

编辑模式全选，U → Smart UV Project（方块模型用这个最快）

或手动 U → Unwrap 调一下，让每块装甲分开不重叠

记得 Ctrl+A 应用缩放，不然烘焙会歪 

第 4 步：雕高模 + 烘焙（Normal + AO）

A. 先做高模（复制一份低模）

复制模型，进 Sculpt Mode

加细节：铆钉、焊缝、装甲缝、掉漆边（别太细，像素渲染看不见）

命名：tank_high 和 tank_low

B. 烘焙（Cycles）

渲染属性切到 Cycles，然后：

着色编辑器给低模新建材质，加一个 Image Texture 节点

点“新建图片”，2048×2048，命名 tank_normal，色彩空间设 Non-Color（关键！）

选中这个图片节点（不用连到材质上）

3D 视图里：先选高模，再 Shift 选低模（低模是活动对象）

渲染属性 → Bake 面板：

Bake Type: Normal

勾 Selected to Active

Extrusion: 0.02~0.05

点 Bake

图片菜单 → Save As，存 PNG

同样流程再烘一张 AO（Bake Type 换 Ambient Occlusion，采样 128）

⚠️ 烘完一定要手动 Save，Blender 不会自动存贴图

第 5 步：程序化材质（不画贴图，用节点）

低模材质里：

Principled BSDF 的 Base Color 接纯色（铁锈红/军绿/金属灰）

Normal 接口接刚才烘的 Normal 贴图（中间加 Normal Map 节点）

Ambient Occlusion 节点把 AO 图乘进颜色

加战损（可选）：用 Noise Texture + ColorRamp 做锈斑遮罩

关高光：Roughness 拉到 0.8~1.0，Metallic 0~0.2

第 6 步：正交相机预渲染（出精灵帧）

加相机，属性里类型切 Orthographic（正交，无透视）

打光：

主光：右上 45°，暖白

补光：左侧冷蓝，弱

背光：后面紫色勾边（尤里味）

渲染设置：

分辨率 512×512（最后再压小）

采样 128

输出 PNG，透明背景

绕 Y 轴复制 8 个相机角度（每 45° 一个），或者用空物体旋转模型

动画：做 idle（轻微上下浮动）+ 炮塔转动，每段 8~12 帧

偷懒办法：写一个 Blender Python 脚本环拍 8 方向，一键渲染。需要的话我直接给你。

第 7 步：像素修图（LibreSprite / Krita）

把 Blender 渲的 PNG 序列拖进 LibreSprite

干三件事：

收边缘：用 1px 铅笔笔刷修锯齿

统一调色板：建一个 32~64 色的全局色板，所有帧锁死

拼精灵表：8 方向 × N 帧，排成一张大 PNG + 导出 JSON 坐标

第 8 步：进 Godot 跑起来

新建 Godot 项目，把精灵表拖进 res://assets/

用 AnimatedSprite2D 或 Sprite2D + AnimationPlayer

项目设置里：

Rendering → Textures → Default Filter: Nearest（整数倍缩放不糊）

关抗锯齿

加后处理着色器（限色 + 抖动）：

用 Viewport + 全屏 ColorRect 挂 shader

色阶压到 16~32 级，加 Bayer 4×4 抖动 

调色板用 Lospec 导出的 1 行 PNG 做最近邻匹配
