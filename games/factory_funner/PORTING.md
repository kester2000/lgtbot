# FactoryFunner → LGTBot 移植记录

来源：用户提供的 `D:/code/FactoryFunner`。

- `src/data.js` → `machines.h`：48 台机器的编号、名称、收入、端口及可选颜色，逐项通过导入脚本生成。更新方式：`node games/factory_funner/import_data.mjs <源项目目录>`。
- `src/boards.js` → `engine.h`：保留六种 A 面规则用于源引擎对照；实际多人对局按用户要求统一使用青色 A 面，包括其墙壁位置。
- `src/engine.js`、`src/routing.js` → `engine.h`：连通分量、颜色交集、流量、机器依赖循环、最终布局计费、固定旧机器、路径铺管。
- `src/multiplayer-rules.js`、`room-service.mjs` → `match.h` / `mygame.cc`：2–6 人、八轮、原子抢选、首末标记、随机分配、限时施工、冻结确认及同步结算。
- `assets/game/Toke_Factory_Funner_Machine_Tiles_41_a0s0.png` → `icon.png`。游戏设计 Corné van Moorsel，美术归原权利人所有。未将这些素材宣称为本项目原创或重新授予许可证。

插件运行不依赖源目录、Node.js、网页服务器、浏览器或账户系统。`games/CMakeLists.txt` 会自动发现目录并生成 `factory_funner`、`test_game_factory_funner`、`run_game_factory_funner`。

## 聊天适配

采用 LGTBot 房间、玩家身份和私聊。保留实时抢选；不额外启用轮流选机变体。每轮有准备阶段，10 秒后自动翻牌；不使用网页三秒动画倒计时。施工期间各人独立编辑，所有提交才统一公开分数。超时有效方案冻结后有 20 秒确认，期间禁止修改。

网页的游客大厅、房主转移、2 分钟无操作 / 15 秒断连判定、浏览器存档和刷新恢复属于网页服务功能，交由 LGTBot 自身的房间及生命周期处理；本插件没有另外复制 HTTP 服务。显式退出保留已确认提交，否则弃建，之后不再发牌。机器人占位玩家选择不抢并弃建，用于框架自动推进测试，不作为有策略的 AI。

原网页用管型+旋转+镜像表示管道；本插件直接存六条边的位集。原有 11 种管型经过旋转和镜像恰好覆盖所有 2–6 边集合，因此不需要记管件名称或镜像指令。`连` 自动选择接口，交叉管道仍各自独立。

`方案` 将分号分隔的多步施工合为原子操作：全部成功才替换草稿，并整体进入撤销历史。任何一步失败都不改变局面。此命令只接受施工，不允许嵌套方案或混入提交。

图像采用原版棋盘、48 台机器和储罐 PNG，结合 SVG 管道、坐标和端口标记，再通过 LGTBot HTML/Markdown → Qt → PNG 流程发送。使用 SVG 1.1 的 `xlink:href` 和绝对 `file:///` URL，兼容 Windows Qt5 WebKit。提供 `文字` 完整文本回退。每轮准备与施工开始时将全部玩家工厂按两列拼接群发；个人编辑回复原会话，结算仅群发已结算布局；未显示其他玩家尚未结算的草稿或未来暗牌。

## 验证

标准构建：重新运行本项目 CMake 配置后，构建 `factory_funner test_game_factory_funner run_game_factory_funner`，运行对应 CTest。核心规则另有不依赖 GTest 的测试入口：

```sh
g++ -std=c++23 -I. games/factory_funner/engine_test.cc -o build/factory_funner_engine_test
./build/factory_funner_engine_test
```

可用 `node games/factory_funner/parity_test.mjs <源项目目录> <核心测试程序路径>` 比较所有机器在六种板面、全部空格、六个方向上的相邻储罐方案与源引擎的合法性、费用和奖励。源目录仅是开发验证时的可选依赖。

## 本次验证结果（2026-10-06）

- Ubuntu GCC 15.2，C++23：插件动态库及框架测试程序编译成功。
- 9 个框架集成用例通过，包括 2/6 人八轮、抢选互斥、首末费用、施工锁定、冻结保留、退出及批量施工。
- `run_game_factory_funner --repeat=6` 自动对局通过。
- 独立规则测试 1,934 项检查通过；同时启用 AddressSanitizer 和 UndefinedBehaviorSanitizer 未报告错误。
- 与源 JavaScript 引擎对照的 84,672 组相邻储罐摆放案例全部一致（48 台机器 × 6 个板面 × 49 个候选坐标 × 6 个方向），其中墙壁或越界等非法摆放也应一致拒绝。这不是对所有可能管网的穷举证明。
- 初版 SVG 曾通过 Edge 无头渲染；当前原版素材画面另已通过 Windows Qt5 WebKit 实际 PNG 渲染、六人模拟器完整图片回调及人工画面检查。尚未接入真实聊天平台。

Windows 构建使用原生 MSYS2 MinGW64 GCC 16.2 / Ninja，输出目录为 `D:/code/lgtbot/build/windows`，工具链位于 `build/msys64`。已清理之前的 WSL 编译进程及 `build` 内的 Linux 构建产物，第三方子模块已初始化。Windows 下独立规则测试 1,934 项、源引擎对照 84,672 项均通过。

构建启用核心、全部游戏、工具、测试、Qt5 WebKit 图像及 SQLite，按照项目 Windows 文档关闭 glog。麻将依赖在 Windows GCC 下关闭推测性虚函数调用优化，避免引入未使用的宿主 `extern_get_*` 回调引用；附带的 Python 扩展保持启用。`third_party/markdown2image/src/main.cc` 的 flag 验证器改为 `gflags::RegisterFlagValidator`，兼容当前 gflags。

重新配置脚本为 `build/windows-configure.sh`。从 PowerShell 运行 Windows 程序时，将 `D:/code/lgtbot/build/msys64/mingw64/bin` 加入当前进程 PATH，以便找到编译器运行库、Qt 等依赖。
Windows 完整构建已于 2026-10-06 成功结束：66 个 `plugins/<game>/libgame.dll`、`libbot_core.dll`、模拟器及全部工具/测试目标生成，麻将 Python 模块可导入并创建 Table。模拟器游戏列表实际载入 66 款游戏，包含“工厂欢乐多”。

Windows CTest 扩展检查运行 85 个目标，81 个通过。工厂欢乐多三个测试目标（9 个框架用例、六轮自动对局、1,934 项规则检查）均通过；四个失败目标为 `test_game_laser_chess`、`test_game_sync_mahjong`、`test_alchemist`、`test_laser_chess`，分别触发标准库的越界或空 optional 断言，未作为本次编译任务扩大修改其他游戏。日志：`build/windows-tests.log`。

主要产物：`build/windows/libbot_core.dll`、`build/windows/simulator.exe`、`build/windows/plugins/factory_funner/libgame.dll`。`build/windows/启动模拟器.cmd` 设置当前进程 DLL 路径并启动模拟器。完整构建日志为 `build/windows-build-resume4.log`。
## 原版图片与模拟器预览（2026-10-06）

从源项目复制 93 张 `assets/game/*a0s0.png`（约 9.8 MB）及 `provenance.json` 到 `games/factory_funner/resource`，原版素材不作图像修改。资源随 CMake 复制到 `build/windows/plugins/factory_funner/resource`，运行时不再依赖源目录。管道本来就是网页版生成的图形，这里按实际六边接口绘制。

工厂图使用原版 965×880 棋盘坐标。机器按 `30 + rot×60` 度、储罐按 `90 + rot×60` 度旋转；坐标放在最上层，防止被素材遮挡。图含玩家、轮次、资金、费用、组件编号和接口。牌池及机器查询也发送原版图片。

模拟器默认启用 `.lgtbot_image` 图片输出；首次图片消息自动在浏览器打开 `.lgtbot_image/preview.html`，分别展示各玩家及群聊最新图片，每两秒刷新。`--open_images=false` 可关闭自动打开浏览器，`--image_path` 可改输出位置。修复 Windows 渲染器含空格路径的引用和失败返回；核心图片文件使用时间戳加原子序号，防止多人图片被同一线程输出覆盖。

最终 Windows 验证：核心、10 个游戏框架用例、六轮自动对局、1,945 项规则/画面结构检查全部通过；六种原版棋盘分别通过真实 Qt PNG 渲染。六人真实模拟器对局检验独立 PNG、全部玩家预览面板及含空格目录，日志为 `build/windows-factory-images-integration.log`；示例为 `build/factory_image_samples/player_1.png`。源码渲染入口为 `view.h`，游戏接入为 `mygame.cc`，本地预览为 `tools/image_preview.h`。

每轮开始总览采用紧凑的两列布局：两人一行，六人三行，群发单张 PNG，标注每位玩家和资金。总览只在准备阶段生成，当时没有未公开施工草稿。施工开始将本轮机器、首末抢费用及全部工厂拼接总览统一发到群里，不主动私聊。

游戏内帮助现直接包含准备、抢选、施工、批量方案、修改、撤销/重做、查询、提交及超时确认的操作说明与示例。真实模拟器验证了 帮助 文字 和默认帮助 PNG；每轮准备仅群发单张拼接总览，没有私聊版图。

当前主动私聊仅限施工超时后的冻结确认提醒。真实六人对局验证准备、施工及结算全程只有群聊消息，施工总览包含各人本轮机器与费用；10 张群聊图片成功生成。玩家主动私聊请求仍正常回复。

## Windows 子进程空闲中断修复（2026-10-06）

长时复现确认：旧版游戏与配置子进程在约 120 秒后因输入读取错误退出。Windows 下 reproc 将标准输入输出重定向到回环 TCP 套接字，实测返回 Windows 121 / Winsock 10060 超时，原代码将其视为输入结束。

Windows 现改用 `bot_core/subprocess_windows.cc` 中的原生匿名管道和 `CreateProcessW`，仅继承子进程需要的输入、输出、错误句柄，避免 TCP 通信路径的超时；其他平台保留 reproc。帧协议保持不变。回归脚本为 `scripts/check_factory_funner_ipc.py`，覆盖机器摆放、撤销、重复安装错误、超过 120 秒的空闲输入、恢复施工查询及配置子进程复用。运行示例：`python scripts/check_factory_funner_ipc.py --build-dir build/windows --idle-seconds 180`。
