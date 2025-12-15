# 运行与考勤（MySQL）配置说明

本文档说明在本机如何准备数据库、编译并运行服务，使“考勤点名”功能可以通过 MySQL 持久化数据。

## 1. 环境准备

- 基本工具：
  - `clang++`, `make`
  - 浏览器（Chrome / Edge / Safari 均可）
- MySQL：
  - 已安装服务端和命令行客户端，例如：
    ```bash
    which mysql
    # 输出类似 /opt/homebrew/bin/mysql
    ```

## 2. 初始化考勤数据库

项目根目录下已提供初始化脚本 `sql/attendance_init.sql`，包含：

- 创建数据库 `attendance_db`
- 创建表：
  - `students(student_id, name)`
  - `attendance(id, student_id, date, status)`
- 插入两条示例学生数据（可自行扩展）

建议使用 Makefile 提供的目标来初始化：

```bash
# 按需修改 Makefile 顶部的 DB_USER / DB_PASSWORD / DB_HOST / DB_PORT
make db-init
```

`db-init` 目标本质等价于：

```bash
mysql -h$DB_HOST -P$DB_PORT -u$DB_USER [-p$DB_PASSWORD] < sql/attendance_init.sql
```

如果你使用非 root 账号或不同的数据库名，可以直接修改 `Makefile` 中的相关变量。

## 3. 运行服务

在项目根目录执行：

```bash
make run
```

该目标会先执行 `make db-init` 确保数据库存在，然后启动可执行文件 `bin/tank_red_envelope`。默认监听 `8080` 端口，可通过 `TANK_GAME_PORT` 环境变量或命令行参数修改。

浏览器访问：

```text
http://localhost:8080
```

即可看到“空域红包战线”页面、小鸭装扮、代码统计面板，以及已接入 MySQL 后端的基础考勤能力（通过 `/attendance/next` 和 `/attendance/mark` API）。

## 4. MySQL 连接参数（后端）

考勤仓库的 MySQL 连接信息通过环境变量配置（仅在使用 MySQL 版本时生效）：

- `ATTENDANCE_DB_HOST`：主机名，默认 `localhost`
- `ATTENDANCE_DB_PORT`：端口，默认 `3306`
- `ATTENDANCE_DB_USER`：用户名，默认 `root`
- `ATTENDANCE_DB_PASSWORD`：密码（强烈建议设置；未设置时默认为空字符串）
- `ATTENDANCE_DB_NAME`：数据库名，默认 `attendance_db`

示例：

```bash
export ATTENDANCE_DB_HOST=localhost
export ATTENDANCE_DB_PORT=3306
export ATTENDANCE_DB_USER=root
export ATTENDANCE_DB_PASSWORD=你的密码
export ATTENDANCE_DB_NAME=attendance_db
```

如果你看到类似报错：

```text
Access denied for user 'root'@'localhost' (using password: NO)
```

说明运行时没有读到密码环境变量。请设置 `ATTENDANCE_DB_PASSWORD`（或兼容变量 `MYSQL_PWD` / `DB_PASSWORD`）后再启动服务。

> 提示：Makefile 默认启用 MySQL（`WITH_MYSQL=1`）且不会回退到内存实现；若本机缺少 MySQL client 开发库，会在编译阶段直接报错。需要临时跳过 MySQL 编译可使用 `make WITH_MYSQL=0`（点名功能将不可用）。
