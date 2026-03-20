# Kiwi

Kiwi 是一款面向 chiplet interposer 的布局布线工具。  
输入一份配置（芯粒实例、IO、连接关系），输出硬件可用的 `controlbits` 文件。

## 功能概览

- 支持布局（可选）+ 布线的一体化流程
- 支持单模式布线（普通/增量）
- 支持双模式联合布线（multi-mode），单次运行输出两个模式的控制比特：
  - `controlbits_1.txt`
  - `controlbits_2.txt`

## 目录结构

- `source/`：核心源码
- `test/`：模块测试与回归测试
- `tools/`：辅助工具（如 `view2d`、`view3d`、`cobmap`）
- `resource/`：GUI 资源
- `document/`：项目文档

## 构建

项目使用 [xmake](https://github.com/xmake-io/xmake) 构建。

```bash
xmake build kiwi
```

## 快速使用

```bash
xmake run kiwi <input_folder> [OPTIONS]
```

常用参数：

- `-o, --output <OUTPUT_PATH>`：输出目录
- `-g, --gui`：GUI 模式
- `-h, --help`：帮助
- `-V, --version`：版本信息
- `-v, --verbose`：详细日志
- `-p, --placement`：布线前执行布局
- `-i, --incremental <mode>`：单模式增量布线（mode 为正整数）
- `-c, --compare <mode>`：与目标模式 controlbits 比较（配合增量模式使用）
- `-m, --multi-mode`：启用双模式联合布线
- `--mm-k <K>`：multi-mode 每对 net 的候选并行尝试数
- `--mm-threshold <EPS>`：multi-mode 迭代收敛阈值

## 输入与输出

### 输入

- 传入一个配置目录（包含项目所需的 JSON 配置）
- 双模式联合布线仍然只需一份配置目录，两个模式需求来自同一份连接配置

### 输出

- 单模式：输出 `controlbits_<mode>.txt`
- 双模式（`--multi-mode`）：输出 `controlbits_1.txt` 和 `controlbits_2.txt`

## 测试

### 模块测试

```bash
xmake build module_test
xmake run module_test [module|all]
```

### 回归测试

```bash
xmake build regression_test
xmake run regression_test
```

## 常用工具

- `view2d`：加载配置并执行 P&R，展示 2D 结果
- `view3d`：加载配置并执行 P&R，展示 3D 结果
- `cobmap`：COB 端口映射相关工具

可通过：

```bash
xmake build <tool>
xmake run <tool> [args]
```
