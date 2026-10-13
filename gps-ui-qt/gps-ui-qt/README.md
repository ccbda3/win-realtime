# gps-ui-qt 工程源目录

```
gps-ui-qt/
├── CMakeLists.txt      构建脚本（含 appicon.rc、静态文件复制）
├── appicon.rc          图标资源定义
├── favicon.ico         图标文件
├── 卫星.png             卫星面板素材
├── src/                源码目录 → 见 src/README.md
└── static/             地图前端 → 见 static/README.md
```

## 构建产物

- `gps-ui-qt.exe` — Qt6 图形界面主程序（含授权校验、四联图、地图）
- `gps-core.exe` — 命令行信号发生器

## 编译后处理

构建脚本会在编译完成后自动把 `static/` 复制到可执行文件同级目录：

```cmake
add_custom_command(TARGET gps-ui-qt POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/gps-ui-qt/static"
        "$<TARGET_FILE_DIR:gps-ui-qt>/static")
```

因此**运行时必须保证 `static/` 目录与 exe 同目录**，否则地图页面无法加载。
