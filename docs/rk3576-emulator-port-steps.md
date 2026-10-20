# 把 miyoo_src 的模拟器迁到 RK3576 的步骤

本文只定顺序和每一步的验收。确认这份文档合理之后，再按里面的顺序改代码。一次只做一个模拟器，在板上看到结果再做下一个。

两个目录分开：

- 上游只读：`/home/yuwei/samba/yuwei_work/project/miyoo_src`。这里保持论坛上的原样，不在里面改编译器，也不把整棵树拷进本仓库。`jinyong` 一类目录带了大量游戏数据。
- 本仓库：`/home/yuwei/samba/yuwei_work/project/linux_console_emu`。开始某个模拟器时，只把它的源码目录从上游拷进来，然后在这里改、编译、记录结果。

已经定下的范围：

- 第一台机器是 RK3576，用户态是 aarch64 Linux（glibc）。
- 先迁模拟器并在板上验证。汇总配置文件和 LVGL 选单放在模拟器能跑起来之后。
- 以后再为 32 位 ARM 单独编一套。配置文件和界面源码两边共用，可执行文件按架构各编各的。
- 老 Miyoo 的 `/opt/miyoo`、`arm926ej-s`、`/dev/miyoo_fb0`、`/dev/miyoo_vir`、`/dev/mem` 不带到 RK3576 上。

「ARM 都能用」在这里的意思是：同一份源码能为目标板的 gcc 编过，显示和按键走 SDL，不访问 Miyoo 私有设备。有 aarch64 汇编就用；这棵旧树里没有的，就用 C 解释器。不把 ARM926 的汇编原样套到 RK3576 上。

## 板上怎么跑

RK3576 的系统库是 glibc。这些源码现在链的是 uClibc，所以 Miyoo 上编出来的程序不能直接拷过来。

依赖库在本仓库的 `third_party/` 里交叉编译，安装到 `deps/`，模拟器静态链接这些 `.a`。`libc`、`libpthread`、`libm`、`libdl` 继续动态链接板上的 glibc。第一版显示用 SDL 1.2 的 framebuffer（`/dev/fb0`），不接 Mali GPU，也不用 Buildroot。

编译在 PC 上做，用 Purple Pi OH2 SDK 里的 aarch64 Linux 工具链，见下一节。不再使用 `/opt/miyoo/bin/arm-linux-gcc`。

## 交叉编译器

用这一套，三元组是 `aarch64-none-linux-gnu`：

```
rk3576_purple_pi_oh2_linux_sdk/prebuilts/gcc/linux-x86/aarch64/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu
```

本机绝对路径是 `/home/yuwei/samba/yuwei_work/chip_sdk/Rockship/RK3576/rk3576_purple_pi_oh2_linux_sdk/prebuilts/gcc/linux-x86/aarch64/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu`。编译器是 `bin/aarch64-none-linux-gnu-gcc`。

同一目录下还有三套不参与模拟器编译：

| 目录 | 用途 |
|---|---|
| `aarch64/arm-gnu-toolchain-13.2.Rel1-x86_64-aarch64-none-elf` | 裸机，没有 Linux libc |
| `arm/gcc-arm-10.3-2021.07-x86_64-arm-none-linux-gnueabihf` | 32 位 ARM Linux，留到以后做 armhf |
| `arm/gcc-arm-none-eabi-10-2020-q4-major-x86_64-linux` | 32 位裸机 |

2026-10-19 在这台 x86_64 机器上验证过：

- `aarch64-none-linux-gnu-gcc` 版本是 10.3.1（GNU Toolchain for the A-profile Architecture 10.3-2021.07）。
- `-dumpmachine` 是 `aarch64-none-linux-gnu`。
- 用它编译的 hello 是 `ELF 64-bit LSB executable, ARM aarch64`，动态链接器是 `/lib/ld-linux-aarch64.so.1`。
- sysroot 里的 C 库是 glibc 2.33（`libc-2.33.so`）。
- 本机没有 `qemu-aarch64`，hello 还没有在 RK3576 板上跑过。

同一天用 CMake 又编过一次，产物同样是 `ARM aarch64`。入口是 `./build.sh`，它会带上默认的 `aarch64-none-linux-gnu` 工具链并调用 CMake。换工具链时设置 `TOOLCHAIN_ROOT`。`./build.sh clean` 删除 `build/`，`./build.sh help` 打印用法。

可执行文件在 `build/apps/hello/hello`。`build/` 不进仓库。

## 工程路径

```
build.sh
CMakeLists.txt
cmake/toolchain-aarch64-linux.cmake
third_party/build-sdl.sh
apps/hello/
apps/sdl_fb/
deps/                 编译产物，不进仓库
```

`./build.sh` 先用 `third_party/build-sdl.sh` 把 SDL 1.2.15 编成 `deps/lib/libSDL.a`，再编 `apps/`。源码包缓存在 `third_party/cache/`，也不进仓库。`./build.sh clean` 会删掉 `build/`、`deps/` 和 `third_party/src/`。

显示先试 `/dev/fb0`。打不开再试 `/dev/dri/card0`，用 DRM dumb buffer 把 SDL 画面放大到屏幕分辨率。两个设备都没有时，`SDL_Init` 失败，程序退出。不使用 Mali。`third_party/sdl-drm/SDL_drmvideo.c` 在编译 SDL 时打进静态库。

`apps/sdl_fb` 把 320×240 的表面涂成绿色并保持 5 秒。板上成功时打印 `sdl driver drm` 或 `sdl driver fbcon`。`apps/hello` 不链 SDL。

以后每个模拟器放在 `apps/<目录>`，并在顶层 `CMakeLists.txt` 里 `add_subdirectory`。链接时使用 `deps/lib/libSDL.a`。

后面每个模拟器都用这支 `aarch64-none-linux-gnu-gcc` 编。`file` 看到 `ARM aarch64` 才算编译这步通过。

## 这几步先不做

- `overclock`、`aclk_list`、`cclk_list`、`vclk_list`、`motor`、`calib`、`daemon`、`show_cpu`。它们读写 Miyoo 的时钟、framebuffer 和振动节点。
- `gmenunx`、`commander`、`gmu`、`flite`、`mpg123`、`libmikmod`、`libvorbis`。菜单以后由 LVGL 替换，播放器和音频库不是模拟器。
- `jinyong` 目录有 664MB 游戏数据，等对应程序能编过再碰数据。
- 第一版不要求满速，也不要求手柄映射和原掌机完全一致。先做到能进游戏画面、方向和两个确认键能用。

## 每个模拟器都按这 6 步做

1. 从 `miyoo_src/<目录>` 拷到本仓库 `apps/<目录>`。记下它现在的编译命令，以及写死的 `arm-linux-gcc`、`-march=armv5te`、`-mtune=arm926ej-s`、`/opt/miyoo`。
2. 改成上一节的 `aarch64-none-linux-gnu-gcc`，去掉上面这些标志。源文件里如果打开 `/dev/miyoo_*` 或 `/dev/mem`，改成 SDL 的画面、键盘和音频。
3. 在编译机上编过，确认生成的是 aarch64 程序（`file` 能看到 `ARM aarch64`）。
4. 拷到 RK3576，用一份你自己的 ROM 启动到标题或第一个画面。BIOS 类模拟器同时放上它要求的 BIOS。
5. 在文末的状态表写结果：通过、编译失败、或能开不能玩，并写上一句原因。
6. 停下来看这一条结果，再开始下一个目录。

ROM 和 BIOS 不放进这个仓库。`.gitignore` 已经忽略 `roms/` 和 `bios/`。

## 模拟器分三批

绑定情况是对着 `miyoo_src` 扫出来的。`/dev/miyoo` 表示源码直接打开掌机私有设备。`arm926` 表示编译参数锁在 ARM926 上。`dynarec` 表示有动态重编译，这棵树里的实现多半是 32 位 ARM。

### 第一批：C 和 SDL，没有掌机私有设备

这些目录的游戏逻辑可以在 aarch64 上编。主要工作是换编译器、换掉 SDL 的库路径。

| 顺序 | 目录 | 机器 | 先改哪里 |
|---|---|---|---|
| 1 | `sms` | Master System | `sms/Makefile` 里的 `arm-linux-gcc` 和 sysroot |
| 2 | `wqx` | 文曲星 NC1020 | `wqx/Makefile` |
| 3 | `fceux` | 红白机 | `fceux/Makefile` 第 222 行的 `-march=armv5te -mtune=arm926ej-s` |
| 4 | `gamebatte` | Game Boy / GBC | `gamebatte/build_miyoo.sh`，改成普通 SDL 构建 |
| 5 | `ohboy` | Game Boy / GBC | 前端是 SDL。目录里有 3 个汇编文件，编不过就先关掉这 3 个文件 |
| 6 | `gngeo` | Neo Geo | 目录里有 8 个汇编文件。先看它们是不是 x86；是的话 aarch64 用 C 路径 |

`sms` 放第一个，是因为它小，而且能把「拷源码、编译、拷到板子、进画面、记一笔」整条路走通。后面的都复用这条路。

### 第二批：带 32 位 ARM 加速，先走 C 路径

| 顺序 | 目录 | 机器 | 做法 |
|---|---|---|---|
| 7 | `snes9x4d` | 超任 | `dingux-sdl/Makefile` 换成普通 SDL。ARM 汇编编不过就用 C 核心 |
| 8 | `temper` | PC Engine | `temper/SDL/Makefile`。12 个汇编文件按同样办法处理 |
| 9 | `picodrive` | Mega Drive | 这棵树的 68000 快核是 Cyclone，只在 `__arm__` 下编译。aarch64 用已有的 C 解释器，`./configure` 不再传 `--platform=miyoo` |
| 10 | `dosbox` | DOS | 按 `README-MIYOO.txt` 的开关重配：`--host` 改成 aarch64，去掉 `-march=armv5te -mtune=arm926ej-s`。动态重编译在这棵树里没有 aarch64 后端时用解释核 |

### 第三批：动态重编译和 Miyoo 设备缠在一起

| 顺序 | 目录 | 机器 | 做法 |
|---|---|---|---|
| 11 | `gpsp` | GBA | `gpsp/main.c` 打开了 `/dev/miyoo_vir`。先去掉振动，保留 SDL。7 个汇编是 32 位 ARM 动态重编译，aarch64 上先用 C 核，速度不够再单独做 |
| 12 | `pcsx_rearmed` | PlayStation | 不要用 `--platform=miyoo`。`configure` 的默认平台是 `generic`，会走 SDL。`aarch64` 匹配不到 `arm*`，脚本会把动态重编译关掉，第一版用解释器加软件 GPU（`unai` 或 `peops`）。`frontend/plat_suniv.c` 整文件不参与这次构建 |

`pcsx_rearmed/include/arm_features.h` 里有空的 `__aarch64__` 分支，说明作者知道这个架构，但这份 `configure` 还没有把 aarch64 接进动态重编译。所以 PS1 能先跑起来，满速是后面的单独任务。

### 游戏移植，单独排期

模拟器三批都有结论之后，再按同样的 6 步做这些纯 C / SDL 移植：`ccdoom`、`wolf3d`、`hheretic`、`hhexen`、`rott`、`eduke32`、`openbor`、`sdlpal`、`cdogs`、`digger`、`liero`、`pang`、`mrdrillux`、`asciiportal`、`snowman`、`tombstone`。它们不是模拟器，不插进上面的顺序里。

## 汇总配置

模拟器状态表里至少有 3 个「通过」之后再写配置，避免格式被还没跑通的程序带着走。

文件放在 `launcher/emulators.conf`。用简单的段格式，方便以后 LVGL 和脚本一起读：

```
[sms]
name=Master System
binary=sms
rom_dir=roms/sms
rom_ext=sms,gg
args={rom}
workdir=.

[fceux]
name=NES
binary=fceux
rom_dir=roms/nes
rom_ext=nes,fds
args={rom}
workdir=.
```

每段只描述一件事：界面上的名字、可执行文件、ROM 目录、扩展名、启动参数。画面分辨率和按键不写进第一版，沿用该模拟器自己的 SDL 默认值。换板子时只改 `binary` 和 `rom_dir` 这类路径，段名保持不变。

## LVGL 选单

配置文件有 3 个能启动的条目之后再做界面。程序是一个独立的小可执行文件，不链进任何一个模拟器。

它做三件事：

1. 读 `launcher/emulators.conf`，在 320×240 或板上实际分辨率上列出 `name`。
2. 选中一项后，用该段的 `binary`、`args`、`workdir` 启动，并把 `{rom}` 换成用户选的 ROM。
3. 模拟器进程退出后回到列表。

显示用 LVGL 的 SDL 驱动，这样和模拟器一样走板上的 SDL，不单独写 DRM。按键沿用模拟器已经验证过的方向键和确认键。

## 状态表

每做完一个目录就填一行。结果只用这四个词：未开始、编译失败、能开不能玩、通过。

| 目录 | 批次 | 结果 | 说明 |
|---|---|---|---|
| sms | 1 | 未开始 | |
| wqx | 1 | 未开始 | |
| fceux | 1 | 未开始 | |
| gamebatte | 1 | 未开始 | |
| ohboy | 1 | 未开始 | |
| gngeo | 1 | 未开始 | |
| snes9x4d | 2 | 未开始 | |
| temper | 2 | 未开始 | |
| picodrive | 2 | 未开始 | |
| dosbox | 2 | 未开始 | |
| gpsp | 3 | 未开始 | |
| pcsx_rearmed | 3 | 未开始 | |

## 已经定下的编译方式

在 PC 上用 SDK 里的 `aarch64-none-linux-gnu-gcc` 交叉编译。工具链本身已经验证，见「交叉编译器」。

## 已经定下的库

SDL 1.2.15 放在本仓库编译，静态链接，显示走 framebuffer。GPU 这批不用。zlib、libpng、SDL_image 等库等第一个用到它们的模拟器再加。

下一步从 `sms` 开始，把它拷进 `apps/sms`，改成这套工具链和 `libSDL.a`。
