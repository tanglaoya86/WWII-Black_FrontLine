# RA2_PYandCPP_refactor
## 所以你写HTML干什么   先别搞README了
这是错误的因为HTML依赖于浏览器环境 除此之外RA2资源文件受到EA版权的保护
我查了下资源确实受版权保护，so我们名字不能叫这个了，资源也得全部重新弄
不然我们就要把它弄成mod
## 我们还是直接弄成独立游戏吧，直接写我们要的纳粹的那个（90%不侵权），名字我想一想待会改
里面的建造厂还可以叫建造厂，发电厂叫发电厂（通用名都可以）
但“光凌”“天启”这种专用名词不行
“超时空”这种可能侵权

换一下目录换成我这个我已经在写ASM优化了
.
├── assets
│   ├── audio
│   │   ├── ambient
│   │   ├── speech
│   │   └── ui
│   ├── fonts
│   ├── images
│   │   ├── effects
│   │   ├── terrain
│   │   ├── ui
│   │   └── units
│   └── music
├── CMakeLists.txt
├── docs
│   └── README.md
├── LICENSE
├── logs_and_chat
│   ├── chat   #聊天
│   ├── debug
│   └── runtime
├── README.md
├── src
│   ├── native
│   │   ├── asm
│   │   │   ├── a_star_core.asm
│   │   │   ├── common.asm
│   │   │   └── pixel_copy.asm
│   │   └── cpp
│   │       ├── astar
│   │       │   ├── astar_types.h
│   │       │   ├── astar.cpp
│   │       │   └── astar.h
│   │       ├── bindings
│   │       │   ├── ctypes_exports.cpp
│   │       │   └── pybind11_bindings.cpp
│   │       └── render
│   │           ├── color_convert.cpp
│   │           ├── pixel_blitter.cpp
│   │           └── pixel_blitter.h
│   └── python
│       ├── __init__.py
│       ├── engine
│       │   ├── __init__.py
│       │   ├── audio.py
│       │   ├── input.py
│       │   └── render.py
│       ├── formats
│       │   ├── __init__.py
│       │   ├── mix_parser.py
│       │   ├── pal_parser.py
│       │   └── shp_parser.py
│       └── game
│           ├── __init__.py
│           ├── ai.py
│           ├── buildings.py
│           ├── rules.py
│           └── units.py
├── tests
│   ├── astar
│   │   └── test_astar.py
│   └── native_iface
│       └── test_native.py
└── tools
    ├── build_native.py
    └── extract_assets.py

33 directories, 33 files

