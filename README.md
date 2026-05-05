### 有用的是DLL就是src/native/expdll/ 下面的两个Dllchecker
# 32 位 / 64 位不匹配

我的代码里写死了只检查 64 位 PE：
```cpp
if (nth->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64)
    return -1;
```
但问题不在这个判断，而在于 DLL 本身的位数 必须和 Python 解释器的位数一致。

64 位 Python 只能加载 64 位 DLL
32 位 Python 只能加载 32 位 DLL

#  你就是缺少 Visual C++ 运行时库       but我用了github上的开源软件，检测依赖就是只缺那个文件，system32我也看了真的不缺

我的 DLL 使用了 C++ 标准库（std::string、std::vector、文件流等）如果编译时链接了动态运行时在未安装 Visual C++ Redistributable 的机器上会缺少 msvcp140.dll、vcruntime140.dll 等依赖。
你去下载VC++ Redist看一下
# 函数没找到
这是经典问题```cppextern "C" __declspec(dllexport) const char* CheckDllsA(const char* path);```
虽然 extern "C" 避免了 C++ 名字修饰，但在 32 位 下它会导出为 _CheckDllsA这和32 位 / 64 位不匹配向呼应但是我认为你应该去下载VC++ Redist

## 我裂了，它一直无法导入，查了一下又是没装运行库，看了一下一个不缺，又查说是要装Visual Studio，装了还是不行，啥情况，不然你搞成静态链接？我实在撑不住了，睡了

## 最后看了一下，缺libwinpthread-1.dll，就是DllChecker.dll是用MinGW（GCC的Windows版）编的意思，我还没转MinGW我装了就是，但总不可能要玩这个还得装个这玩意吧     
## 行行行我开/MT
## ai给的
告诉他：当前是动态链接 MinGW 运行时（libwinpthread-1.dll、libgcc_s_seh-1.dll、libstdc++-6.dll），请改成静态链接，这样生成的 DLL 不依赖任何外部运行时文件。
编译命令示例（MinGW）：这会让dll变成Hanson的 不然我还可以直接在编译 DLL 时选择静态链接运行时（多线程 (/MT)），把运行时打进 DLL 里

bash
g++ -shared -static -static-libgcc -static-libstdc++ -Wl,-Bstatic -lpthread -Wl,-Bdynamic -o DllChecker.dll DllChecker.cpp -lshlwapi -luser32


# 汇编重写这段Py代码的任何部分都极其不划算而且...你就只写了个窗口啊   难道我不就是先让你看看吗，也没让你重写啊。至于这个按钮的逻辑我好好研究一下，把其他几个页面最出来
# 按钮逻辑你知道WindowsAPI的那个WM和VK还有LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)吗
# (shout) 你倒是把Py代码直接放出来啊 我只帮你用汇编重写热点函数 dll已经打包好了 dll和exe文件都传上来了自己找 src/native/expdll/    ?这个exe有啥用，要和dll配套?还有我今天作业剩的多，要晚上才能继续写了，素材我催footer弄一下 这个exe就是扫描dll的 dll单独使用自己看我的源代码  这个汇编我看不懂，给了ai给我分析一下吧，它和我说checker_stub.asm对检查游戏文件完整性和游戏依赖没用，dll_scan.asm有点用但不检查本地文件，还有你要把这些玩意写成py的函数打包成dll再给我，不然我用不了，那个下面的几个b站教程有讲怎么做成函数和打包（但我看不懂也没仔细研究，毕竟这不是我的工作）
## 我从哪里开始写
## 许可证我一开始是无许可证咋搞
## 你把剧情在这里写一下（把休谟的设定取消，懒得解释是啥而且也不重要，还有连接的世界就不要说叫啥了防止侵权）
# Bro,你得去写代码        从哪个文件开始啊，还有我得把名字改了，那就要先把剧情弄好
# 在SRC\Python写代码啊 还有就是你自己注意素材文件转移       难道不在根目录搞个入口文件？
## 我已经帮你拿到许可了 其实我更建议GNUv3.0
## 我觉得用汇编的SSE指令集和SMID整一个渲染加速

这个剧情呢：
1946年，第二次世界大战的结局与历史记载截然不同。德国凭借先进的军事科技和战略决策，迫使苏联投降，但苏联残余力量与同盟国合作抵抗德国。三方势力进入漫长的僵持期，战争推动科技以前所未有的速度发展。1949年，希特勒因药物滥用去世，纳粹德国陷入内部分裂。苏联残余力量与同盟国联手发起反攻，1952年，就在这场持续将近14年的旷日大战胜利在望之际，德国拿出了足以改变世界格局的秘密武器——一个能够吞噬物质的“奇点”，这个“球”里装着一个平行世界，被放进去的人、物都会被同化，纯正的纳粹德国势力试图将整个世界装进去，但美国发现了它，研究了一个装置（忘了你说的啥  我就随口一说:无信者之矛），试图使用卫星发出攻击摧毁“球”，但没想到的是整个世界都被强大的能量摧毁，不过些许残留科技被留了下来，由于这个世界被几乎彻底摧毁，所以残余部分被旁边的平行世界吸收，这个世界就是即将发生二战的另一个世界，世界各地出现了原本世界本国当时的残留科技，各个国家开始吸收这种科技，最后发生了一场不一样的世界大战。。。

# ......难道不在根目录搞个入口文件？
# 你代码都没写哪来的入口文件

我先写啥啊，正在研究窗口，刚才看了一下说pygame最好，网上资源也多。还有这剧情可不可以啊

# 行 我先写NASM 标准:Microsoft x64 ABI 我设想的调用链为:NASM(obj) -> C\C++(extern) -> Python

## 我这个py发癫，按键盘上的键没用，得好好研究一下
```python
import os
os.environ['PYGAME_HIDE_SUPPORT_PROMPT'] = '1'   # 必须放在 import pygame 之前

import pygame as pg

pg.init()
window=pg.display.set_mode((800,600)) #创建窗口
pg.display.set_caption("RA2")#设置窗口名
script_dir = os.path.dirname(__file__)#获取脚本窗口
icon_path = os.path.join(script_dir, 'assets/images/ui/window.png')#设置路径
icon=pg.image.load(icon_path)#加载图片
pg.display.set_icon(icon)#设置窗口图标
running = True
info = pg.display.Info()  # 获取显示信息
screen_w, screen_h = info.current_w, info.current_h
clock = pg.time.Clock()  # 在循环前创建时钟对象
is_fullscreen = False

while running:    
    # 只要 running 是 True 就一直循环
    for event in pg.event.get():  # 把新发生的事件全处理一遍
        if event.type == pg.QUIT: # 如果点了关闭按钮
            running = False       # 就让循环结束
    # 检测按键按下
        if event.type == pg.KEYDOWN:
           print('检测到按键:', event.key)  # 诊断用，可以观察
           if event.key == pg.K_ESCAPE:
               running = False          # 按ESC退出
           elif event.key == pg.K_f:
               # 全屏切换示例
               print("1")
               is_fullscreen = not is_fullscreen
               if is_fullscreen:
                    window = pg.display.set_mode((info.current_w, info.current_h), pg.FULLSCREEN)
               else:
                    window = pg.display.set_mode((800, 600), pg.RESIZABLE)
                    pg.display.maximize()

    window.fill((0, 100, 200))      # 填充一个颜色
    pg.display.flip()               # 刷新画面
    clock.tick(60)                  # 限制 60 帧
pg.quit()
```
```
# 那还不如C++的getch那一堆
伪代码
```asm
extern getch
main:
    xor rcx,rcx
    call getch
```
听说你们Py有个ra2mix专注处理《红警2》资源文件（*.mix）

是，你直接用这个？（我创建目录的时候看见你留了有关mix的文件）
有人玩py和ra2的，还专门做了一个py库叫ra2mix。我还用的这个解包的红警，但是我不知道它有用有访问mix的功能

今天一天都在外面，但是看了一下py和c++的合作原理。明天可能也没多少时间      你瞅瞅是不是这个方案
https://www.bilibili.com/video/BV17y4y1W7BY/?spm_id_from=333.337.search-card.all.click
https://www.bilibili.com/video/BV1RZ4y1e7n1/?spm_id_from=333.337.search-card.all.click
https://www.bilibili.com/cheese/play/ss20832?query_from=0&search_id=9534612882170631179&search_query=py%E5%92%8Cc%2B%2B%E5%90%88%E4%BD%9C&csource=common_hpsearch_null_null&spm_id_from=333.337.search-card.all.click

## 我要写ui把音频图片之类的py用上就得先写个主程序main.py(虽然最后要改名)才行。我先写个加载动画，然后你把检查dll的py扩展写好，我在加上检查dll的过程？

我还看了一下，简直我草了。一个优质的坦克模型（现代的那种高清的红警mod      https://www.bilibili.com/video/BV1qv411r7Nc/?spm_id_from=333.337.search-card.all.click）就要一个熟练模型师干7个小时，就是熟悉的模型师做原版红警的那种糊画质模型都要将近2个小时（https://www.bilibili.com/video/BV1Dx4y1g7gj/?spm_id_from=333.337.search-card.all.click），ui贴图更难说，毕竟是脸面怎么也要画好，但就是一个金属光泽我们就要掉，我们这个学校不可能有很会搞模型和游戏贴图的，卒了，还是只有我会一点模型（只会一点，简易广州塔模型已经是我的极限了），模型边做边学我也干了吧，ui贴图我们拿鼠标搞不可能搞好，只有让footer弄个触控笔在手机上画了，不然最好找个有平板画画的（或者让footer搞个平板方便点？但他家有点穷，有点难为他，虽然他爸支持）。

## 那个qq录屏是刚研究出来的，窗口图标是那个window.png
# 我写检查dll然后WinVerifyTrust以及API Hook检查谁在卸载dll最好有C运行时的时候用atexit打断过程抓个密钥 不过你们Py的是dll还是lib 依赖什么要告诉我        你这个的意思我看了大概吧，如果你说是库的话那就是dll，前面给你视频链接了，过程都有讲
素材我不管 
# ui贴图好说你知道rc就行


# 所以说你可以开写了吗
这不是在写吗，今天晚上给你交个加载和主菜单的结构，但你没给我dll我还没发写检查
# bro,你还没给我热点函数我用NASM写什么东西 导出什么 而且检查应该是C++专精（我就是让你给我弄成dll的py库我直接调用） 因为C++可以直接操作数字签名 而且检查应该MD5哈希最好来一个异或偏移然后.bin
# dll配合思路:Py热点函数->NASM重写->COFF/extern语言交互)->Py交互
# 我写了个Py常见依赖检查程序并且上传了二进制文件（NASM写的只有12kb你可以去试一下）（哪个文件。。。要不你还是写成dll） 就是src\native\asm\dll_scan.exe
# 你看shellcode   看不懂思密达
```bash
\x31\xC0\x99\x58\x50\x83\xEC\x08\x31\xC9\x41\x8B\x14\x08\x80\xFA\xFF\x75\xF7\x80\x7C\x08\x01\x15\x75\xF0\xEB\x00\x8A\x5C\x08\x05\x80\xFB\x00\x74\x02\xEB\xE3\x31\xDB\x8B\x5C\x08\x02\x8B\x3B\xC1\xEF\x1C\x83\xFF\x07\x74\x02\xEB\xD1\x8B\x1B\x31\xD2\x66\xBA\x01\x10\x81\xC2\xFF\xEF\x00\x00\x29\xD3\x66\x89\xD3\x66\x8B\x03\x66\x3D\x4D\x5A\x75\xF2\x8B\x7B\x3C\x01\xDF\x8B\x7F\x78\x01\xDF\x8B\x7F\x0C\x01\xDF\x83\xC7\x04\x31\xC0\x50\x68\x2E\x64\x6C\x6C\x68\x45\x4C\x33\x32\x89\xE6\x89\xCA\xB9\x08\x00\x00\x00\xFC\xF3\xA6\x83\xF9\x00\x75\x85
```
## so我们要这个干啥  那啥我加载和主菜单的模板在ai的协助下完成了（我还没学那么多，大概只有三分之一是我写的），还没有素材就随便搞了点，排版也没来得及慢慢调，你先把要给我检查文件完整性的库dll给我写好，我再加上，还有这个热点函数，目前也没啥内容，最多的就是搞导入图片，配置窗口、字体，都有注释，你自己看吧，记得先把pygame装上，在根目录叫ra2.zip,解压了直接运行ra2.py就是了
