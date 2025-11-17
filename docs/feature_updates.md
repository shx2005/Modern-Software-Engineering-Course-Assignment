# 功能更新记录

## 唐老鸭装扮系统
- `web/index.html` 中新增 `CartoonDuck`、`CostumePiece`、`Hat`、`Cloth`、`Costume` 五个类，通过面向对象的组合关系管理基础图层、帽子和服饰。
  - `CartoonDuck.refreshLayers()` 会根据当前 `Costume` 将 `/static/hat*.png` 与 `/static/cloth*.png` 同步到 `.duck-layer` 覆盖层，并通过 `classList` 控制显隐。
- 底部小鸭面板重构为 `duck-avatar` 容器，`duck-hat-layer`、`duck-cloth-layer` 叠加在 `duck-helper` 之上，所有图层均设置 `pointer-events: none` 保证与原有点击逻辑兼容。
- `static/hat1.png` ~ `hat3.png` 与 `static/cloth1.png` ~ `cloth3.png` 由脚本生成，提供基础的 PNG 装扮素材，与 OOP 模型按 `id` 一一对应。

## “设置”滑动窗口
- 新增 `duck-settings-button` 与 `duck-settings-panel`，按钮触发 `openDuckSettings()`，面板通过 `transform` 与 `duck-settings-overlay` 实现滑入/滑出交互。
- `renderCostumeOptions()` 根据帽子/服饰目录动态生成滚动轨道，`setSelectedCostume()` 负责：
  1. 更新当前选中 `id`；
  2. 调用 `CartoonDuck.applyHat/Cloth()`；
  3. 通过 `highlightSelection()` 设置 `.active`，形成滑动选择体验。
- 面板初始状态默认选中 `hatCatalog[0]`、`clothCatalog[0]`，并提供“无帽子”“无服饰”预设确保可回退至基础造型。

## 代码统计增强
- `include/backend/CodeStats.hpp`：
  - 用 `FunctionDetail`、`FunctionSummary` 替换原有 Python 专用结构，每个 `LanguageSummary` 持有独立的函数统计向量。
- `src/backend/CodeStats.cpp`：
  - `visitFile()` 将语言拆分为 `C`、`C++`、`C#`、`Java`、`Python` 五类。
  - `analyzePythonFile()`、`analyzeBraceLanguageFile()` 分别统计缩进风格与花括号风格函数长度，并写入 `LanguageSummary.functions`。
  - 分析结束后统一在 `analyze()` 中计算 `functionCount`、`min/max`、`average`、`median`。
- `src/backend/CodeStatsFacade.cpp`：
  - `analyzeCppOnly()` 汇总 C + C++；
  - `printLongestFunction`/`printShortestFunction` 迭代所有语言的 `FunctionDetail` 并输出中文描述。
- `src/frontend/WebServer.cpp`：
  - `buildCodeStatsJson`、CSV/JSON/XLSX 构建逻辑都追加函数统计字段；
  - `parseLanguages()` 支持 `c/cpp/csharp/java/python` 等别名；
  - `/print_longest_function` 与 `/print_shortest_function` 的提示信息改为“未检测到函数数据”。
- `web/index.html` 前端：
  - 语言复选框扩展为五种语言；
  - `updateStatsView()` 新增 `stats-function-table`，以表格展示每种语言的函数数量、最短、最长、平均与中位值；
  - `statsResults.dataset.languages` 直接记录返回的语言名称，供打印接口复用。

## 小鸭会话与 AI 接口
- `openDuckDialog()` 打开时默认显示“您好，有什么可以帮助你”，保持统一问候语。
- `submitDuckCommand()` 在非“红包雨”指令下调用 `invokeDuckAI()`，该函数会优先回调可选的 `window.duckAiHandler`，便于后续无缝接入真实 AI 服务；当前若无实现则提示“AI 接口尚未接入”。

## 本次迭代补充
- 将小鸭面板放大至 120px，并同步放大帽子/服饰图层及设置面板中的缩略图，确保装扮展示更加清晰。
- `duck-settings-button` 的定位与遮罩随之微调，移动端下限宽高也同步拉伸以保持比例。
- 默认将 `duck-hat-layer`、`duck-cloth-layer` 的 `src` 指向 `/static/hat1.png` 与 `/static/cloth1.png`，即使在 JS 未加载之前也能展示来自 `static/` 的贴图，真正做到“服饰使用 static 中 hat/cloth 替换”。
- `invokeDuckAI()` 现在按照 OpenAI Chat Completions 的请求格式组装 payload：当未提供自定义 `window.duckAiHandler` 时，会读取 `window.DUCK_OPENAI_*` 或 `DUCK_OPENAI_CONFIG` 中的 `apiKey/model/endpoint/systemPrompt`，并向 `https://api.openai.com/v1/chat/completions` 发送 `messages` 数组。未配置密钥时仍会回退到本地提示。

以上内容即本轮重构与功能扩展的核心逻辑，可配合 `web/index.html` 与 `src/backend` 目录代码进一步查看实现细节。
