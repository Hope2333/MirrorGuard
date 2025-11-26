# MirrorGuard Makefile
VERSION = 0.1.0alpha
CC = gcc
CFLAGS = -Wall -Wextra -O2 -D_FORTIFY_SOURCE=2 -std=c99 -D_POSIX_C_SOURCE=200809L -DMIRRORGUARD_VERSION=\"$(VERSION)\" -I./include
LDFLAGS = -lcrypto -lpthread

# 源文件和目标文件
SRCDIR = src
INCDIR = include
BUILDDIR = build

SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(SOURCES:$(SRCDIR)/%.c=$(BUILDDIR)/%.o)
TARGET = mirrorguard

# 默认目标
.PHONY: all clean install uninstall debug test

all: $(TARGET)

# 编译目标
$(TARGET): $(OBJECTS)
	$(CC) -o $@ $^ $(LDFLAGS)
	@echo "✅ 编译完成: $(TARGET) (版本: $(VERSION))"

# 编译对象文件
$(BUILDDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# 调试版本
debug: CFLAGS += -g -DDEBUG -O0
debug: clean $(TARGET)
	@echo "🔍 调试版本编译完成 (版本: $(VERSION))"

# 清理
clean:
	rm -rf $(BUILDDIR)/*
	rm -f $(TARGET)
	@echo "🧹 清理完成"

# 安装
install: $(TARGET)
	sudo install -m 755 $(TARGET) /usr/local/bin/
	@echo "📦 安装完成: /usr/local/bin/$(TARGET) (版本: $(VERSION))"

# 卸载
uninstall:
	sudo rm -f /usr/local/bin/$(TARGET)
	@echo "🗑️ 卸载完成"

# 测试（如果有的话）
test:
	@echo "🧪 运行测试 (版本: $(VERSION))..."
	# 在这里添加测试命令

# 打包发布
package: $(TARGET)
	@mkdir -p package
	cp $(TARGET) package/
	cp README.md package/ 2>/dev/null || true
	tar -czf $(TARGET)-$(VERSION)-$(shell date +%Y%m%d).tar.gz -C package .
	@echo "📦 打包完成: $(TARGET)-$(VERSION)-$(shell date +%Y%m%d).tar.gz"

# 显示信息
info:
	@echo "🎯 MirrorGuard 项目信息"
	@echo "版本: $(VERSION)"
	@echo "源码目录: $(SRCDIR)"
	@echo "头文件目录: $(INCDIR)"
	@echo "构建目录: $(BUILDDIR)"
	@echo "目标文件: $(TARGET)"
	@echo "源文件: $(notdir $(SOURCES))"

# 帮助
help:
	@echo "🚀 MirrorGuard Makefile 帮助"
	@echo "版本: $(VERSION)"
	@echo ""
	@echo "可用目标:"
	@echo "  all       - 编译项目 (默认)"
	@echo "  debug     - 编译调试版本"
	@echo "  clean     - 清理构建文件"
	@echo "  install   - 安装到系统"
	@echo "  uninstall - 卸载程序"
	@echo "  test      - 迢行测试"
	@echo "  package   - 打包发布版本"
	@echo "  info      - 显示项目信息"
	@echo "  help      - 显示此帮助"
	@echo ""
	@echo "示例:"
	@echo "  make              # 编译项目"
	@echo "  make debug        # 编译调试版本"
	@echo "  make install      # 安装程序"
	@echo "  make clean        # 清理项目"
	@echo "  make VERSION=1.0.0 # 使用特定版本编译"

# 便捷别名
build: all
run: $(TARGET)
	./$(TARGET) --version
