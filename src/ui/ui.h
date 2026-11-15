// 界面基础库：主题、字体、布局、常用控件、图标与图表
// 设计稿以 390 x 844 逻辑像素为基准，运行时按屏幕尺寸整体缩放。
#pragma once

#include "imgui.h"

#include <string>
#include <vector>

namespace ui {

// ---------------- 主题颜色 ----------------
extern ImU32 colBg;
extern ImU32 colCard;
extern ImU32 colPrimary;
extern ImU32 colPrimaryDark;
extern ImU32 colText;
extern ImU32 colSub;
extern ImU32 colLine;
extern ImU32 colSuccess;
extern ImU32 colWarn;
extern ImU32 colDanger;
extern ImU32 colWhite;
extern ImU32 colBlueSoft;
extern ImU32 colGreenSoft;
extern ImU32 colOrangeSoft;
extern ImU32 colRedSoft;

struct FontSet {
    ImFont* small = nullptr;
    ImFont* body = nullptr;
    ImFont* title = nullptr;
    ImFont* big = nullptr;
    ImFont* huge = nullptr;
};
extern FontSet fonts;

// 初始化字体与样式（scale 为整体缩放比例，fontPath 为中文字体路径）
// fontIndex：字体文件内的字体序号（.ttc 字体集合用，单字体文件填 0）
bool Init(float scale, const std::string& fontPath, int fontIndex = 0);

// 逻辑尺寸 -> 像素
float S(float v);
ImVec2 V(float x, float y);
float Scale();
float CanvasW();
float CanvasH();

// 每帧开始（处理输入法回调、提示消失等）
void Tick(float dt);
void Toast(const std::string& msg);
const std::string& ToastText();
float ToastAlpha();

// 输入法接口（Android 由 Java 回调；Windows 由 ImGui 键盘处理）
void OnImeText(const std::string& text);
void OnImeDone();
bool IsEditing();

// ---------------- 文字 ----------------
void Text(const std::string& s, ImU32 color, int size = 1);
void TextWrapped(const std::string& s, ImU32 color, int size = 1);
void TextCenter(const std::string& s, ImU32 color, int size = 1, float width = 0);
void Label(const std::string& s);   // 次要文字
float TextWidth(const std::string& s, int size = 1);

// ---------------- 布局 ----------------
void Gap(float h);
void Divider(float inset = 0);
void PageTitle(const std::string& s);       // 分区标题
void Hint(const std::string& s);            // 灰色提示
void FieldLabel(const std::string& s);      // 表单字段标签

// ---------------- 卡片 ----------------
void BeginCard(const char* id, float height);
void EndCard();
void BeginCardAuto(const char* id);   // 自动高度卡片（内容较少时可用）
void EndCardAuto();

// ---------------- 控件 ----------------
// 文本输入：Android 使用系统输入法，Windows 使用键盘
bool TextField(const char* id, std::string& value, const std::string& hint,
               bool numeric = false, bool integer = false);
// 多行文本
bool TextArea(const char* id, std::string& value, const std::string& hint);

bool PrimaryButton(const std::string& label, float height = 48);
bool SoftButton(const std::string& label, float height = 44);
bool DangerButton(const std::string& label, float height = 44);
bool TextButton(const std::string& label, ImU32 color = 0);

// 列表行：返回是否点击
bool ListRow(const std::string& title, const std::string& sub, const std::string& right,
             ImU32 rightColor = 0, const std::string& tag = "", ImU32 tagColor = 0,
             float height = 64);

// 键值一行
void KeyValueRow(const std::string& key, const std::string& value, ImU32 valueColor = 0);

// 标签
void Tag(const std::string& text, ImU32 color);
void StatusTag(int status, int module);   // 业务状态标签

// 空状态
void Empty(const std::string& text);

// 搜索框
bool SearchBar(const char* id, std::string& value, const std::string& hint = "搜索");

// 分段控件
int Segmented(const char* id, const std::vector<std::string>& items, int current);

// 选择器（点击后弹出底部选项）；返回 -1 表示未选择，>=0 为选中项
int Dropdown(const char* id, const std::string& value, const std::string& hint,
             const std::vector<std::string>& options);

// 日期选择（简化为弹窗选择年/月/日）
bool DateField(const char* id, std::string& date, const std::string& hint);

// 开关
bool Switch(const char* id, bool& value);

// 顶部栏（返回是否点击返回/右侧按钮）
void TopBar(const std::string& title, bool showBack, const std::string& action, bool& backClicked, bool& actionClicked);
// 底部标签栏
void BottomNav(int& tab);

// 弹窗
bool Confirm(const char* id, const std::string& title, const std::string& content,
             const std::string& okText = "确定", const std::string& cancelText = "取消");

// ---------------- 图标 ----------------
enum IconType {
    IconHome = 0, IconGrid, IconUsers, IconChart, IconUser,
    IconPhone, IconWrench, IconParts, IconCard, IconMoney,
    IconIn, IconOut, IconSearch, IconBack, IconPlus, IconTrash,
    IconArrow, IconCheck, IconWarn, IconBag, IconSetting
};
void Icon(IconType type, ImVec2 center, float size, ImU32 color);

// ---------------- 图表 ----------------
void BarChart(const std::vector<float>& values, const std::vector<std::string>& labels,
              float height, ImU32 color = 0);
void HBarRow(const std::string& label, float ratio, const std::string& value, ImU32 color = 0);

// ---------------- 滚动区域 ----------------
void BeginScroll(const char* id, float height);
void EndScroll();

// 通用工具
std::string Money(double v);       // 金额格式化，保留两位
std::string MoneyShort(double v);  // 大额缩写
std::string Num(double v);

} // namespace ui
