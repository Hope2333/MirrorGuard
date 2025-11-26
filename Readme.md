# MirrorGuard 🛡️

[![License](https://img.shields.io/badge/license-GPL--3.0-blue.svg)](LICENSE)
[![Build Status](https://img.shields.io/badge/build-passing-brightgreen)](https://github.com/Hope2333/MirrorGuard/actions)
[![Version](https://img.shields.io/badge/version-0.1.0alpha-ff69b4)](https://github.com/Hope2333/MirrorGuard)
[![OpenSSL](https://img.shields.io/badge/OpenSSL-3.0%2B-ff69b4)](https://www.openssl.org/)

> **"数据的完整性，是数字世界的基石"**  
> —— MirrorGuard 0.1.0alpha

---

## 🌟 概览

**MirrorGuard** 是一个企业级的文件完整性校验工具，专为关键数据镜像验证而设计。它采用多层验证机制，结合现代密码学技术，确保您的数据镜像在传输、存储和同步过程中保持完整性和一致性。无论是大规模备份系统、分布式存储集群还是合规性关键环境，MirrorGuard 都能提供可靠的数据完整性保障。

---

## ✨ 核心特性

- 🔐 **强加密验证**：使用 OpenSSL 3.0+ EVP 接口，SHA-256 哈希算法，确保验证过程的安全性
- ⚡ **高性能处理**：多线程架构，智能内存管理，支持 TB 级数据验证
- 📊 **细粒度报告**：彩色终端输出，详细统计信息，实时进度追踪
- 🛡️ **安全第一**：路径规范化，路径遍历防护，符号链接安全处理
- 🔍 **多维度验证**：
  - 多源清单生成
  - 镜像完整性验证
  - 清单差异比较
  - 直接目录对比
- 📈 **TUI 模式**：5 种 TUI 模式，提供丰富的交互界面
- 🌐 **跨平台兼容**：POSIX 兼容，Linux/Unix 系统完美支持
- 📝 **灵活配置**：丰富的命令行选项，过滤规则，日志定制

---

## 🏗️ 技术架构

MirrorGuard 采用模块化设计架构，各组件协同工作，构建了一个健壮且可扩展的验证系统。

### 设计原则
- **模块化**：每个功能组件高度解耦，便于维护和扩展
- **安全性**：防御性编程，输入验证，内存安全
- **性能优化**：O(n log n) 算法，批量 I/O 操作，智能缓存
- **可观察性**：详细日志，性能统计，进度报告
- **健壮性**：异常处理，信号安全，资源清理

---

## 📁 项目文件详解

### 🧩 核心配置模块

#### `config.h` & `config.c`
**职责**：全局配置管理和初始化  
**关键功能**：
- 定义配置结构体和全局变量
- 信号处理机制（SIGINT, SIGTERM）
- 参数解析和验证
- 资源清理和内存管理

### 🗃️ 数据结构模块

#### `data_structs.h` & `data_structs.c`
**职责**：定义核心数据结构和内存管理  
**关键结构**：
- `FileInfo`：存储文件路径、哈希、大小、修改时间
- `FileList`：动态文件列表，支持线程安全操作
- `FileStatus`/`CompareResult`：枚举类型，标准化状态码

### 🧭 路径处理模块

#### `path_utils.h` & `path_utils.c`
**职责**：路径规范化和安全验证  
**关键功能**：
- 路径标准化（处理 `.`、`..`、重复分隔符）
- 安全路径验证（防范路径遍历攻击）
- 智能文件过滤（包含/排除模式，隐藏文件处理）

### 📄 文件操作模块

#### `file_utils.h` & `file_utils.c`
**职责**：文件哈希计算和验证  
**关键功能**：
- SHA-256 哈希计算（OpenSSL 3.0+ EVP 接口）
- 单文件验证逻辑
- 大文件分块处理（64KB 缓冲区）
- 性能统计（处理字节）

### 🔍 目录扫描模块

#### `directory_scan.h` & `directory_scan.c`
**职责**：递归目录遍历和文件收集  
**关键功能**：
- 递归目录扫描
- 符号链接安全处理
- 文件类型过滤
- 路径安全检查

### ✅ 验证核心模块

#### `verification.h` & `verification.c`
**职责**：核心验证逻辑实现  
**关键功能**：
- 多源清单生成
- 镜像完整性验证
- 额外文件检测
- 详细验证报告

### 🔄 比较分析模块

#### `comparison.h` & `comparison.c`
**职责**：清单和目录比较  
**关键功能**：
- 清单文件差异分析
- 直接目录内容比较
- 详细差异报告
- 一致性验证

### 📝 日志记录模块

#### `logging.h` & `logging.c`
**职责**：统一日志记录和输出  
**关键功能**：
- 彩色日志输出
- 多级日志过滤
- 时间戳格式化
- 日志文件支持

### 🖥️ TUI 界面模块

#### `tui.h` & `tui.c`
**职责**：终端用户界面  
**关键功能**：
- 5 种 TUI 模式
- 实时进度显示
- 交互控制
- 美观界面

### 🚀 主程序入口

#### `main.c`
**职责**：程序入口和流程控制  
**关键功能**：
- 命令行参数解析
- 操作模式分发
- 性能统计
- 帮助/版本信息

### ⚙️ 构建系统

#### `Makefile`
**职责**：项目构建和安装  
**关键功能**：
- 依赖管理
- 多目标构建（release/debug）
- 安装/卸载支持
- 清理支持

---

## 🚀 安装指南

### 依赖项
```bash
sudo apt-get update
sudo apt-get install build-essential libssl-dev
```

### 从源码构建
```bash
git clone https://github.com/Hope2333/MirrorGuard.git
cd MirrorGuard
make
sudo make install
```

### 验证安装
```bash
mirrorguard --version
# 应输出: MirrorGuard v0.1.0alpha
```

---

## 📖 使用示例

### 1. 生成多源清单
```bash
# 从两个源目录生成清单
mirrorguard -x '.tmp' -x '.cache' \
            -g /data/src1 /data/src2 \
            backup_manifest.sha256
```

### 2. 验证镜像完整性
```bash
# 验证镜像目录
mirrorguard -q -v /backup/mirror backup_manifest.sha256
```

### 3. 比较两个清单文件
```bash
# 比较清单差异
mirrorguard -c manifest_v1.sha256 manifest_v2.sha256
```

### 4. 直接比较两个目录
```bash
# 直接目录对比
mirrorguard -d /data/source1 /data/source2
```

### 5. 启用 TUI 模式
```bash
# 启用富文本 TUI
mirrorguard --tui=4 -g /data/source1 manifest.sha256
```

### 6. 短参数组合
```bash
# 短参数合并使用
mirrorguard -qv -g /data/source1 manifest.sha256  # 安静 + 详细输出
mirrorguard -np -v /mirror manifest.sha256        # 模拟 + 进度
```

---

## ⚙️ 高级配置

### TUI 模式说明
- `--tui=0`：无 TUI（默认）
- `--tui=1`：简单 TUI - 基本进度显示
- `--tui=2`：高级 TUI - 彩色界面，交互功能
- `--tui=3`：极简 TUI - 最小化显示
- `--tui=4`：富文本 TUI - 美观的彩色界面
- `--tui=5`：调试 TUI - 显示内部状态信息

### 环境变量支持
```bash
export MIRRORGUARD_THREADS=16
export MIRRORGUARD_LOG_LEVEL=DEBUG
export MIRRORGUARD_EXCLUDE=".tmp,.cache"
```

---

## 🚄 性能优化

### 大数据集建议
```bash
# 1. 增加线程数（根据CPU核心数）
mirrorguard --threads 16 ...

# 2. 禁用额外检查（仅验证清单中的文件）
mirrorguard -e ...

# 3. 启用进度显示，了解处理状态
mirrorguard -p ...

# 4. 禁用递归（仅处理顶层文件）
mirrorguard -r ...
```

---

## 🤝 贡献指南

### 开发环境设置
```bash
git clone https://github.com/Hope2333/MirrorGuard.git
cd MirrorGuard
make debug
```

### 贡献流程
1. Fork 项目仓库
2. 创建特性分支 (`git checkout -b feature/your-feature`)
3. 提交更改 (`git commit -am 'Add some feature'`)
4. 推送到分支 (`git push origin feature/your-feature`)
5. 创建 Pull Request

### 代码规范
- 遵循 Linux 内核编码风格
- 函数长度不超过 100 行
- 每个函数单一职责
- 详细注释关键算法
- 100% 内存泄漏检测（Valgrind）

---

## 📜 许可证

本项目采用 **GNU General Public License v3.0** 许可证。

```
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
```

---

## 🙏 致谢

- **OpenSSL 团队** - 提供强大的加密基础
- **Linux 内核团队** - 系统调用和 POSIX 兼容性
- **早期测试用户** - 企业存储团队和备份系统管理员
- **开源社区** - 代码审查和功能建议

---

## 📞 支持与联系

- **项目地址**: [GitHub Repository](https://github.com/Hope2333/MirrorGuard)
- **问题报告**: [GitHub Issues](https://github.com/Hope2333/MirrorGuard/issues)
- **功能请求**: [GitHub Discussions](https://github.com/Hope2333/MirrorGuard/discussions)

---

> **"在数据的世界里，完整性就是真理"**  
> —— 幽零小喵 个人开发团队

[![Star on GitHub](https://img.shields.io/github/stars/Hope2333/MirrorGuard?style=social)](https://github.com/Hope2333/MirrorGuard)  
**给项目加星支持我们！** ⭐

```bash
# 快速开始
git clone https://github.com/Hope2333/MirrorGuard.git
cd MirrorGuard && make && sudo make install
mirrorguard --help
```
