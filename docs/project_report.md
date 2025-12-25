# 项目阶段汇报

## 1 建模
### 1.1 场景建模
- 游戏以 2D 网格为空间基础，世界大小由 `GameConfig.worldWidth/worldHeight` 控制。
- 坦克与红包均占据离散网格坐标，前端用 `CELL_SIZE` 将整数坐标转换为像素坐标。
- HTTP 服务与浏览器前端构成客户端/服务器场景；前端每 450ms 轮询 `/state` 获取最新场景描述。

### 1.2 类建模
- **后端核心类**：
  - `backend::GameEngine`：维护 `Tank`、红包容器、时间与统计信息。
  - `backend::Tank`：负责移动、动量与边界约束，暴露精确坐标供碰撞检测。
  - `backend::RedEnvelope`：红包实体，包含 ID、面额、大小与收集半径。
  - `backend::CodeStatsAnalyzer`/`CodeStatsFacade`：实现语言级别统计与对外接口。
  - `frontend::WebServer`：封装 HTTP socket、静态资源与 API 分发。
- **前端主要对象**：
  - `gridState`：缓存 DOM、尺寸和红包动画状态。
  - `CartoonDuck` + `Costume` 系列类：管理鸭子装扮的 OOP 模型。

### 1.3 功能建模/行为建模
- **游戏行为**：开始游戏→轮询状态→玩家发送 `/move`/`/rain`/`/pause`，后端返回 HUD、红包与坦克位置。红包碰撞在 `GameEngine` 中判断并即时 `respawn`。
- **统计行为**：前端通过 `/codestats` 发送目录与语言选项，后端用 `CodeStatsAnalyzer` 遍历文件并返回 JSON；导出功能调用 `/codestats/export` 生成 CSV/JSON/XLSX。
- **装扮行为**：前端“设置”面板触发，选项变化时调用 `CartoonDuck.applyHat()/applyCloth()` 更新图层。
- **AI 对话行为**：点击小鸭→提交文本→若包含“红包雨”则直接触发 `/rain`；否则调用 `invokeDuckAI()`，优先走自定义 handler，或按 OpenAI Chat Completions 协议请求云端。

## 2 已完成工作
### 2.1 红包游戏
#### 2.1.1 已实现功能
- 后端：坦克移动、红包随机生成与重生、时间控制、暂停/恢复、红包雨（额外生成）、精准碰撞检测。
- 前端：HUD 显示、网格渲染、红包动画、操作按钮（开始/暂停/重置/红包雨）、总结面板。

#### 2.1.2 界面截图及说明
- HUD + 战场示意：`web/index.html` 渲染的主界面，展示剩余时间、红包数量/金额及网格。
- （可在报告中嵌入 `static` 下的截图，示例：`![主界面](static/duck2.jpg)`，此处保留说明以待贴图。）

#### 2.1.3 代码结构及说明
- 后端逻辑位于 `include/backend/*.hpp` 与 `src/backend/*.cpp`：
  - `GameEngine.cpp`：负责初始化、移动、红包碰撞与统计。
  - `Tank.cpp`：保存精确坐标与动量，暴露 `getExactX/Y`。
- 前端游戏逻辑集中在 `web/index.html` 的 `<script>` 区域（状态轮询、DOM 更新与动画）。

### 2.2 统计代码量
#### 2.2.1 已实现功能及测试结果
- 支持 C、C++、C#、Java、Python 五种语言的文件计数、代码行/空行/注释行、函数数量及函数长度统计。
- Python 使用缩进解析，花括号语言用线性扫描和括号深度分析函数定义。
- 前端勾选语言 + 输出选项 → 发送 `/codestats`，开发过程中通过实际项目目录测试，返回值正确驱动柱状 / 饼图渲染。

#### 2.2.2 界面截图及说明
- “统计代码量”弹窗（按钮位于右下）：输入目录、勾选语言、选择导出格式，展示结果图表。
- 可在 Markdown 中放置示例图，如 `![统计面板](static/duck3.jpg)`。

#### 2.2.3 代码结构及说明
- `backend/CodeStats.hpp/.cpp`：包含 `LanguageSummary`、`FunctionSummary` 与遍历实现。
- `backend/CodeStatsFacade.*`：面向外部接口，提供 `analyzeAll`、`printLongestFunction` 等。
- 前端对应模块位于 `web/index.html` 中 `stats-` 前缀的 DOM 与 JS 函数（`runStats`, `updateStatsView`, `downloadStatsReport` 等）。

### 2.3 小鸭扮靓
#### 2.3.1 已实现功能
- 默认加载 `/static/hat1.png` 与 `/static/cloth1.png`。
- “设置”滑窗提供帽子/服饰选项，可随时切换；`CartoonDuck` OOP 模型同步更新覆盖层。

#### 2.3.2 界面截图及说明
- 左下角小鸭按钮 + 设置抽屉，可展示 `web/index.html` 对应 UI（示例图：`![小鸭装扮设置](static/duck4.jpg)`）。

#### 2.3.3 代码结构及说明
- JS 类定义：`CostumePiece`, `Hat`, `Cloth`, `Costume`, `CartoonDuck`（位于 `web/index.html` 脚本中）。
- 静态资源：`static/hat1.png`~`hat3.png`, `static/cloth1.png`~`cloth3.png`。
- UI 组件：`duck-settings-panel`, `duck-hat-options`, `duck-cloth-options`。

### 2.4 AI对话
#### 2.4.1 已实现功能
- 对话框默认提示语“您好，有什么可以帮助你”。
- 识别“红包雨”命令，直接触发后端 `/rain`。
- 其他内容调用 `invokeDuckAI()`：
  - 优先执行 `window.duckAiHandler`（
  - 若未提供，则默认调用后端 `/duckai` 代理（后端读取服务器环境变量 `OPENAI_API_KEY`，并按 OpenAI Chat Completions 格式转发到阿里云百炼 DashScope 兼容模式）。

#### 2.4.2 界面截图及说明
- 对话框位于页面中央弹窗，含输入框、发送按钮、关闭按钮。
- 建议在 Markdown 中嵌入示意图（例如 `![AI 对话](static/duck.png)`）。

#### 2.4.3 代码结构及说明
- 前端：`openDuckDialog`, `submitDuckCommand`, `invokeDuckAI`（`web/index.html`）。
- 后端：`/rain`、`/duckai` 等 API 处理逻辑在 `src/frontend/WebServer.cpp` 中的 `handleApiRequest`。
- 配置：
  - 推荐：在运行服务前设置环境变量 `OPENAI_API_KEY`（填 DashScope API Key），前端将走 `/duckai` 代理。
  - 可选：在浏览器环境注入 `window.DUCK_OPENAI_CONFIG = { apiKey, model, endpoint }`，使前端直接调用 OpenAI 兼容接口（如 `https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions`，`model` 可填 `qwen-plus` / `qwen3-*`）。
