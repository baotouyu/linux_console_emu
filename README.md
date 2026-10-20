# Linux Console Emu

跑在 Linux 上的游戏机模拟器。第一台板是 RK3576（aarch64）。

步骤写在 [docs/rk3576-emulator-port-steps.md](docs/rk3576-emulator-port-steps.md)。交叉编译器用的是 Purple Pi OH2 SDK 里的 `aarch64-none-linux-gnu-gcc`。`./build.sh` 会先把 SDL 1.2 编成静态库，再编 `apps/hello` 和 `apps/sdl_fb`。

```
./build.sh
./build.sh clean
./build.sh help
```

上游参考是本机的 `miyoo_src`，整棵树不放进这个仓库。一次只迁一个模拟器进来。
