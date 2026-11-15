#include "ui/ui.h"
#include "platform/platform.h"
#include "ui/glyphs.h"

#include "imgui_internal.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>

namespace ui {

ImU32 colBg = 0;
ImU32 colCard = 0;
ImU32 colPrimary = 0;
ImU32 colPrimaryDark = 0;
ImU32 colText = 0;
ImU32 colSub = 0;
ImU32 colLine = 0;
ImU32 colSuccess = 0;
ImU32 colWarn = 0;
ImU32 colDanger = 0;
ImU32 colWhite = 0;
ImU32 colBlueSoft = 0;
ImU32 colGreenSoft = 0;
ImU32 colOrangeSoft = 0;
ImU32 colRedSoft = 0;

FontSet fonts;

static float g_scale = 1.0f;
static float g_cw = 390.0f;
static float g_ch = 844.0f;

static std::string g_toast;
static float g_toastTimer = 0.0f;

// 输入法编辑会话
static bool g_editing = false;
static std::string g_editId;
static std::string g_editText;
static bool g_editNumeric = false;
static std::map<std::string, std::string> g_committed;

// 下拉选项弹窗结果
static std::map<std::string, int> g_dropResult;
static std::string g_dropActive;

// 自动高度卡片状态
struct CardState {
    bool autoH = false;
    ImVec2 start;
    float width = 0;
    float height = 0;
    float top = 0;
    ImDrawList* dl = nullptr;
    int channels = 0;
};
static std::vector<CardState> g_cards;

static ImU32 alpha(ImU32 c, float a) { return (c & 0x00FFFFFF) | ((ImU32)(255 * a) << 24); }

float S(float v) { return v * g_scale; }
ImVec2 V(float x, float y) { return ImVec2(x * g_scale, y * g_scale); }
float Scale() { return g_scale; }
float CanvasW() { return g_cw; }
float CanvasH() { return g_ch; }
void SetCanvas(float w, float h) {
    if (w > 0.0f) g_cw = w;
    if (h > 0.0f) g_ch = h;
}

// ===========================================================================
// 初始化
// ===========================================================================
bool Init(float scale, const std::string& fontPath, int fontIndex) {
    g_scale = scale;
    g_cw = 390.0f;
    g_ch = 844.0f;

    colBg = IM_COL32(244, 246, 250, 255);
    colCard = IM_COL32(255, 255, 255, 255);
    colPrimary = IM_COL32(47, 107, 255, 255);
    colPrimaryDark = IM_COL32(29, 78, 216, 255);
    colText = IM_COL32(27, 34, 48, 255);
    colSub = IM_COL32(122, 133, 152, 255);
    colLine = IM_COL32(235, 239, 246, 255);
    colSuccess = IM_COL32(22, 163, 74, 255);
    colWarn = IM_COL32(245, 158, 11, 255);
    colDanger = IM_COL32(239, 68, 68, 255);
    colWhite = IM_COL32(255, 255, 255, 255);
    colBlueSoft = IM_COL32(232, 240, 255, 255);
    colGreenSoft = IM_COL32(226, 248, 234, 255);
    colOrangeSoft = IM_COL32(255, 243, 224, 255);
    colRedSoft = IM_COL32(255, 233, 233, 255);

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;

    ImGuiStyle& st = ImGui::GetStyle();
    st.WindowPadding = ImVec2(0, 0);
    st.FramePadding = ImVec2(0, 0);
    st.ItemSpacing = ImVec2(0, 0);
    st.ItemInnerSpacing = ImVec2(0, 0);
    st.ScrollbarSize = S(4);
    st.ScrollbarRounding = S(2);
    st.WindowBorderSize = 0;
    st.ChildBorderSize = 0;
    st.PopupBorderSize = 0;
    st.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
    st.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.80f, 0.83f, 0.87f, 0.70f);
    st.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.72f, 0.76f, 0.82f, 0.85f);
    st.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.63f, 0.68f, 0.75f, 0.95f);

    io.Fonts->Clear();
    ImFontConfig cfg;
    cfg.FontNo = fontIndex;   // .ttc 字体集合索引
    cfg.OversampleH = 2;
    cfg.OversampleV = 1;
    cfg.PixelSnapH = false;
    cfg.RasterizerMultiply = 1.0f;

    ImVector<ImWchar> ranges;
    ImFontGlyphRangesBuilder builder;
    builder.AddRanges(io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
    builder.AddText("¥￥·—…、。，！？：；“”‘’（）【】《》1234567890,.+-/%");
    builder.AddText(kUiGlyphs);
    builder.BuildRanges(&ranges);

    auto load = [&](float px) -> ImFont* {
        ImFont* f = nullptr;
        if (!fontPath.empty()) {
            f = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), px * g_scale, &cfg,
                                             ranges.Size ? ranges.Data : nullptr);
        }
        if (!f) {
            ImFontConfig def;
            def.SizePixels = px * g_scale;
            f = io.Fonts->AddFontDefault(&def);
        }
        return f;
    };

    fonts.small = load(11.5f);
    fonts.body = load(14.5f);
    fonts.title = load(17.5f);
    fonts.big = load(22.0f);
    fonts.huge = load(30.0f);
    io.Fonts->Build();
    io.FontDefault = fonts.body;
    return fonts.body != nullptr;
}

void Tick(float dt) {
    if (g_toastTimer > 0) {
        g_toastTimer -= dt;
        if (g_toastTimer <= 0) g_toast.clear();
    }
}

void Toast(const std::string& msg) {
    g_toast = msg;
    g_toastTimer = 2.2f;
}

const std::string& ToastText() { return g_toast; }
float ToastAlpha() {
    if (g_toastTimer <= 0) return 0.0f;
    if (g_toastTimer > 1.8f) return (2.2f - g_toastTimer) / 0.4f;
    if (g_toastTimer < 0.4f) return g_toastTimer / 0.4f;
    return 1.0f;
}

// ===========================================================================
// 输入法
// ===========================================================================
bool IsEditing() { return g_editing; }

void OnImeText(const std::string& text) {
    if (!g_editing) return;
    std::string t = text;
    if (g_editNumeric) {
        std::string filtered;
        int dots = 0;
        for (char c : t) {
            if (c >= '0' && c <= '9') filtered.push_back(c);
            else if (c == '.' && dots == 0) { filtered.push_back(c); dots++; }
        }
        t = filtered;
    }
    g_editText = t;
}

void OnImeDone() {
    if (g_editing) {
        g_committed[g_editId] = g_editText;
        g_editing = false;
    }
    g_editText.clear();
}

// ===========================================================================
// 基础绘制
// ===========================================================================
static void drawText(ImFont* font, ImVec2 pos, ImU32 color, const std::string& s) {
    ImGui::GetWindowDrawList()->AddText(font, font->FontSize, pos, color, s.c_str());
}

void Text(const std::string& s, ImU32 color, int size) {
    ImFont* f = size <= 0 ? fonts.small : (size == 1 ? fonts.body : (size == 2 ? fonts.title : (size == 3 ? fonts.big : fonts.huge)));
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddText(f, f->FontSize, p, color ? color : colText, s.c_str());
    ImGui::Dummy(ImVec2(0, f->FontSize));
}

void TextWrapped(const std::string& s, ImU32 color, int size) {
    ImFont* f = size <= 0 ? fonts.small : (size == 1 ? fonts.body : (size == 2 ? fonts.title : fonts.big));
    ImGui::PushTextWrapPos(ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x);
    ImGui::PushStyleColor(ImGuiCol_Text, color ? color : colText);
    ImGui::PushFont(f);
    ImGui::TextWrapped("%s", s.c_str());
    ImGui::PopFont();
    ImGui::PopStyleColor();
    ImGui::PopTextWrapPos();
}

void TextCenter(const std::string& s, ImU32 color, int size, float width) {
    ImFont* f = size <= 0 ? fonts.small : (size == 1 ? fonts.body : (size == 2 ? fonts.title : (size == 3 ? fonts.big : fonts.huge)));
    if (width <= 0) width = ImGui::GetContentRegionAvail().x;
    float tw = f->CalcTextSizeA(f->FontSize, FLT_MAX, 0, s.c_str()).x;
    ImVec2 p = ImGui::GetCursorScreenPos();
    p.x += (width - tw) * 0.5f;
    ImGui::GetWindowDrawList()->AddText(f, f->FontSize, p, color ? color : colText, s.c_str());
    ImGui::Dummy(ImVec2(0, f->FontSize));
}

void Label(const std::string& s) { Text(s, colSub, 0); }

float TextWidth(const std::string& s, int size) {
    ImFont* f = size <= 0 ? fonts.small : (size == 1 ? fonts.body : (size == 2 ? fonts.title : (size == 3 ? fonts.big : fonts.huge)));
    return f->CalcTextSizeA(f->FontSize, FLT_MAX, 0, s.c_str()).x;
}

void Gap(float h) { ImGui::Dummy(ImVec2(0, S(h))); }

void Divider(float inset) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    ImGui::GetWindowDrawList()->AddLine(ImVec2(p.x + S(inset), p.y), ImVec2(p.x + w, p.y), colLine, 1.0f);
    ImGui::Dummy(ImVec2(0, 1));
}

void PageTitle(const std::string& s) {
    Gap(10);
    ImGui::Indent(S(14));
    Text(s, colText, 2);
    ImGui::Unindent(S(14));
    Gap(6);
}

void Hint(const std::string& s) {
    ImGui::Indent(S(14));
    Text(s, colSub, 0);
    ImGui::Unindent(S(14));
}

void FieldLabel(const std::string& s) {
    ImGui::Indent(S(14));
    Text(s, colSub, 0);
    ImGui::Unindent(S(14));
    Gap(4);
}

// ===========================================================================
// 卡片
// ===========================================================================
void BeginCard(const char* id, float height) {
    ImGui::PushID(id);
    CardState st;
    st.start = ImGui::GetCursorScreenPos();
    st.width = ImGui::GetContentRegionAvail().x;
    st.height = S(height);
    st.autoH = false;
    // 卡片背景先画，内容画在其上
    ImGui::GetWindowDrawList()->AddRectFilled(st.start, ImVec2(st.start.x + st.width, st.start.y + st.height),
                                              colCard, S(12));
    // 用子窗口限定卡内宽度，保证右侧对齐的数值不会跑出卡片
    ImGui::SetCursorScreenPos(ImVec2(st.start.x + S(14), st.start.y + S(12)));
    ImGui::BeginChild("##card", ImVec2(st.width - S(28), st.height - S(24)), false,
                      ImGuiWindowFlags_NoScrollbar);
    g_cards.push_back(st);
}

void EndCard() {
    if (g_cards.empty()) return;
    CardState st = g_cards.back();
    g_cards.pop_back();
    ImGui::EndChild();
    ImGui::SetCursorScreenPos(ImVec2(st.start.x, st.start.y + st.height + S(10)));
    ImGui::PopID();
}

void BeginCardAuto(const char* id) {
    ImGui::PushID(id);
    CardState st;
    st.autoH = true;
    st.start = ImGui::GetCursorScreenPos();
    st.width = ImGui::GetContentRegionAvail().x;
    st.top = st.start.y + S(12);
    st.dl = ImGui::GetWindowDrawList();
    st.dl->ChannelsSplit(2);
    st.dl->ChannelsSetCurrent(1);
    ImGui::SetCursorScreenPos(ImVec2(st.start.x + S(14), st.start.y + S(12)));
    ImGui::BeginChild("##card", ImVec2(st.width - S(28), 0), ImGuiChildFlags_AutoResizeY,
                      ImGuiWindowFlags_NoScrollbar);
    g_cards.push_back(st);
}

void EndCardAuto() {
    if (g_cards.empty()) return;
    CardState st = g_cards.back();
    g_cards.pop_back();
    ImGui::EndChild();
    float height = ImGui::GetItemRectSize().y + S(24);
    st.dl->ChannelsSetCurrent(0);
    st.dl->AddRectFilled(st.start, ImVec2(st.start.x + st.width, st.start.y + height), colCard, S(12));
    st.dl->ChannelsMerge();
    ImGui::SetCursorScreenPos(ImVec2(st.start.x, st.start.y + height + S(10)));
    ImGui::PopID();
}

// ===========================================================================
// 输入框
// ===========================================================================
static bool g_fieldClicked = false;

static void popUtf8(std::string& s) {
    if (s.empty()) return;
    size_t i = s.size() - 1;
    while (i > 0 && (s[i] & 0xC0) == 0x80) i--;
    s.erase(i);
}

static void appendUtf16(std::string& out, unsigned int c) {
    if (c < 0x80) {
        out.push_back((char)c);
    } else if (c < 0x800) {
        out.push_back((char)(0xC0 | (c >> 6)));
        out.push_back((char)(0x80 | (c & 0x3F)));
    } else {
        out.push_back((char)(0xE0 | (c >> 12)));
        out.push_back((char)(0x80 | ((c >> 6) & 0x3F)));
        out.push_back((char)(0x80 | (c & 0x3F)));
    }
}

// 桌面端：直接读取键盘字符与退格键（Android / iOS 由系统输入法回调）
static void pollDesktopInput(bool numeric) {
    if (plat::hasNativeIme()) return;
    ImGuiIO& io = ImGui::GetIO();
    for (int i = 0; i < io.InputQueueCharacters.Size; i++) {
        unsigned int c = (unsigned int)io.InputQueueCharacters[i];
        if (c == 0) continue;
        if (numeric) {
            if (!((c >= '0' && c <= '9') || c == '.')) continue;
            if (c == '.' && g_editText.find('.') != std::string::npos) continue;
        }
        appendUtf16(g_editText, c);
    }
    io.InputQueueCharacters.resize(0);
    if (ImGui::IsKeyPressed(ImGuiKey_Backspace)) popUtf8(g_editText);
    if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)) OnImeDone();
}

bool TextField(const char* id, std::string& value, const std::string& hint,
               bool numeric, bool integer) {
    bool changed = false;
    auto it = g_committed.find(id);
    if (it != g_committed.end()) {
        value = it->second;
        g_committed.erase(it);
        changed = true;
    }

    ImGui::PushID(id);
    float w = ImGui::GetContentRegionAvail().x;
    float h = S(46);
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    bool active = g_editing && g_editId == id;

    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), IM_COL32(247, 248, 251, 255), S(9));
    dl->AddRect(p, ImVec2(p.x + w, p.y + h), active ? colPrimary : colLine, S(9), 0, active ? 1.6f : 1.0f);

    ImGui::SetCursorScreenPos(p);
    ImGui::PushID("hit");
    bool clicked = ImGui::InvisibleButton("##hit", ImVec2(w, h));
    ImGui::PopID();

    if (active) {
        pollDesktopInput(numeric || integer);
        // 边输入边同步，避免用户未点“完成”就切换页面导致丢值
        if (value != g_editText) {
            value = g_editText;
            changed = true;
        }
    }

    std::string show = active ? g_editText : value;
    ImVec2 tp = ImVec2(p.x + S(13), p.y + (h - fonts.body->FontSize) * 0.5f);
    if (show.empty() && !active) {
        dl->AddText(fonts.body, fonts.body->FontSize, tp, IM_COL32(168, 176, 190, 255), hint.c_str());
    } else {
        dl->AddText(fonts.body, fonts.body->FontSize, tp, colText, show.c_str());
        if (active) {
            float tw = fonts.body->CalcTextSizeA(fonts.body->FontSize, FLT_MAX, 0, show.c_str()).x;
            float cx = std::min(tp.x + tw + S(2), p.x + w - S(6));
            dl->AddLine(ImVec2(cx, tp.y + S(2)), ImVec2(cx, tp.y + fonts.body->FontSize), colPrimary, S(1.5f));
        }
    }

    if (clicked) {
        if (g_editing && g_editId != id) OnImeDone();
        g_editing = true;
        g_editId = id;
        g_editText = value;
        g_editNumeric = numeric || integer;
        if (plat::hasNativeIme()) plat::showTextInput(value, numeric || integer);
    }

    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + h + S(10)));
    ImGui::PopID();

    // 实时回写（桌面端与输入法均可用）
    if (active) value = g_editText;
    return changed;
}

bool TextArea(const char* id, std::string& value, const std::string& hint) {
    ImGui::PushID(id);
    float w = ImGui::GetContentRegionAvail().x;
    float h = S(88);
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    bool active = g_editing && g_editId == id;
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), IM_COL32(247, 248, 251, 255), S(9));
    dl->AddRect(p, ImVec2(p.x + w, p.y + h), active ? colPrimary : colLine, S(9), 0, active ? 1.6f : 1.0f);
    ImGui::SetCursorScreenPos(p);
    ImGui::PushID("hit");
    bool clicked = ImGui::InvisibleButton("##hit", ImVec2(w, h));
    ImGui::PopID();
    if (active) pollDesktopInput(false);
    std::string show = active ? g_editText : value;
    if (show.empty() && !active) {
        dl->AddText(fonts.body, fonts.body->FontSize, ImVec2(p.x + S(13), p.y + S(12)),
                    IM_COL32(168, 176, 190, 255), hint.c_str());
    } else {
        ImGui::PushTextWrapPos(p.x + w - S(12));
        dl->AddText(fonts.body, fonts.body->FontSize, ImVec2(p.x + S(13), p.y + S(12)), colText,
                    show.c_str(), nullptr, w - S(24));
        ImGui::PopTextWrapPos();
    }
    if (clicked && !g_editing) {
        g_editing = true;
        g_editId = id;
        g_editText = value;
        g_editNumeric = false;
        if (plat::hasNativeIme()) plat::showTextInput(value, false);
    }
    if (active) value = g_editText;
    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + h + S(10)));
    ImGui::PopID();
    return false;
}

// ===========================================================================
// 按钮
// ===========================================================================
static bool drawButton(const char* id, const std::string& label, float height,
                       ImU32 bg, ImU32 fg, bool outline) {
    ImGui::PushID(id);
    float w = ImGui::GetContentRegionAvail().x;
    float h = S(height);
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##btn", ImVec2(w, h));
    bool hovered = ImGui::IsItemHovered();
    bool held = ImGui::IsItemActive();
    bool clicked = ImGui::IsItemDeactivated() && hovered;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImU32 fill = bg;
    if (held) fill = alpha(bg, 0.82f);
    else if (hovered) fill = alpha(bg, 0.92f);
    if (outline) {
        dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), IM_COL32(255, 255, 255, 255), S(10));
        dl->AddRect(p, ImVec2(p.x + w, p.y + h), colLine, S(10), 0, 1.0f);
    } else {
        dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), fill, S(10));
    }
    ImVec2 ts = fonts.title->CalcTextSizeA(fonts.title->FontSize, FLT_MAX, 0, label.c_str());
    dl->AddText(fonts.title, fonts.title->FontSize,
                ImVec2(p.x + (w - ts.x) * 0.5f, p.y + (h - ts.y) * 0.5f),
                outline ? fg : IM_COL32(255, 255, 255, 255), label.c_str());
    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + h + S(12)));
    ImGui::PopID();
    if (clicked && g_editing) {
        // 点按任意按钮即结束当前输入，收起键盘
        OnImeDone();
        if (plat::hasNativeIme()) plat::hideTextInput();
    }
    return clicked;
}

bool PrimaryButton(const std::string& label, float height) {
    return drawButton(label.c_str(), label, height, colPrimary, colPrimary, false);
}

bool SoftButton(const std::string& label, float height) {
    return drawButton((std::string("soft") + label).c_str(), label, height, colCard, colText, true);
}

bool DangerButton(const std::string& label, float height) {
    return drawButton((std::string("danger") + label).c_str(), label, height, colDanger, colDanger, false);
}

bool TextButton(const std::string& label, ImU32 color) {
    ImGui::PushID(label.c_str());
    float tw = TextWidth(label, 1);
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##tb", ImVec2(tw + S(4), S(24)));
    bool clicked = ImGui::IsItemDeactivated() && ImGui::IsItemHovered();
    ImGui::GetWindowDrawList()->AddText(fonts.body, fonts.body->FontSize, ImVec2(p.x, p.y + S(2)),
                                        color ? color : colPrimary, label.c_str());
    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + S(26)));
    ImGui::PopID();
    return clicked;
}

// ===========================================================================
// 列表行
// ===========================================================================
bool ListRow(const std::string& title, const std::string& sub, const std::string& right,
             ImU32 rightColor, const std::string& tag, ImU32 tagColor, float height) {
    ImGui::PushID(title.c_str());
    ImGui::PushID(sub.c_str());
    ImGui::PushID(right.c_str());
    float w = ImGui::GetContentRegionAvail().x;
    float h = S(height);
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##row", ImVec2(w, h));
    bool clicked = ImGui::IsItemDeactivated() && ImGui::IsItemHovered();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), colCard, S(10));

    dl->AddText(fonts.title, fonts.title->FontSize, ImVec2(p.x + S(14), p.y + S(11)),
                colText, title.c_str());
    if (!sub.empty()) {
        dl->AddText(fonts.small, fonts.small->FontSize, ImVec2(p.x + S(14), p.y + S(36)),
                    colSub, sub.c_str());
    }
    float rightX = p.x + w - S(14);
    if (!tag.empty()) {
        ImVec2 ts = fonts.small->CalcTextSizeA(fonts.small->FontSize, FLT_MAX, 0, tag.c_str());
        ImU32 tc = tagColor ? tagColor : colPrimary;
        ImVec2 bp = ImVec2(rightX - ts.x - S(16), p.y + S(14));
        dl->AddRectFilled(bp, ImVec2(bp.x + ts.x + S(16), bp.y + S(20)), alpha(tc, 0.12f), S(10));
        dl->AddText(fonts.small, fonts.small->FontSize, ImVec2(bp.x + S(8), bp.y + S(3)), tc, tag.c_str());
        rightX = bp.x - S(8);
    }
    if (!right.empty()) {
        ImVec2 rs = fonts.body->CalcTextSizeA(fonts.body->FontSize, FLT_MAX, 0, right.c_str());
        dl->AddText(fonts.body, fonts.body->FontSize,
                    ImVec2(rightX - rs.x, p.y + (h - rs.y) * 0.5f),
                    rightColor ? rightColor : colText, right.c_str());
    }
    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + h + S(8)));
    ImGui::PopID();
    ImGui::PopID();
    ImGui::PopID();
    return clicked;
}

void KeyValueRow(const std::string& key, const std::string& value, ImU32 valueColor) {
    ImGui::PushID(key.c_str());
    float w = ImGui::GetContentRegionAvail().x;
    float h = S(34);
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddText(fonts.body, fonts.body->FontSize, ImVec2(p.x, p.y + S(8)), colSub, key.c_str());
    ImVec2 vs = fonts.body->CalcTextSizeA(fonts.body->FontSize, FLT_MAX, 0, value.c_str());
    dl->AddText(fonts.body, fonts.body->FontSize,
                ImVec2(p.x + w - vs.x, p.y + S(8)), valueColor ? valueColor : colText, value.c_str());
    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + h));
    ImGui::PopID();
}

void Tag(const std::string& text, ImU32 color) {
    ImGui::PushID(text.c_str());
    ImVec2 ts = fonts.small->CalcTextSizeA(fonts.small->FontSize, FLT_MAX, 0, text.c_str());
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = ts.x + S(16);
    ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x + w, p.y + S(20)), alpha(color, 0.12f), S(10));
    ImGui::GetWindowDrawList()->AddText(fonts.small, fonts.small->FontSize,
                                        ImVec2(p.x + S(8), p.y + S(3)), color, text.c_str());
    ImGui::SetCursorScreenPos(ImVec2(p.x + w + S(6), p.y));
    ImGui::PopID();
}

void StatusTag(int status, int module) {
    // module: 0 手机 1 维修 2 配件流水 3 次卡 4 财务
    if (module == 0) {
        if (status == 0) Tag("在库", colPrimary);
        else if (status == 1) Tag("已售", colSuccess);
        else Tag("已退货", colSub);
    } else if (module == 1) {
        const char* names[] = {"待接单", "维修中", "待交付", "已交付", "已取消"};
        ImU32 colors[] = {colWarn, colPrimary, IM_COL32(139, 92, 246, 255), colSuccess, colSub};
        int s = status < 0 || status > 4 ? 4 : status;
        Tag(names[s], colors[s]);
    } else if (module == 2) {
        const char* names[] = {"", "入库", "出库", "调整"};
        ImU32 colors[] = {colSub, colPrimary, colWarn, colSub};
        int s = status < 0 || status > 3 ? 0 : status;
        Tag(names[s], colors[s]);
    } else if (module == 3) {
        if (status == 1) Tag("正常", colSuccess);
        else if (status == 2) Tag("已用完", colSub);
        else Tag("已过期", colSub);
    } else {
        if (status == 1) Tag("收入", colSuccess);
        else Tag("支出", colDanger);
    }
}

void Empty(const std::string& text) {
    Gap(60);
    TextCenter(text, colSub, 0);
    Gap(60);
}

// ===========================================================================
// 搜索框
// ===========================================================================
bool SearchBar(const char* id, std::string& value, const std::string& hint) {
    ImGui::PushID(id);
    float w = ImGui::GetContentRegionAvail().x;
    float h = S(40);
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    bool active = g_editing && g_editId == id;
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), IM_COL32(255, 255, 255, 255), S(20));
    dl->AddRect(p, ImVec2(p.x + w, p.y + h), active ? colPrimary : colLine, S(20), 0, 1.0f);
    Icon(IconSearch, ImVec2(p.x + S(18), p.y + h * 0.5f), S(15), colSub);
    ImGui::SetCursorScreenPos(p);
    ImGui::PushID("hit");
    bool clicked = ImGui::InvisibleButton("##hit", ImVec2(w, h));
    ImGui::PopID();
    if (active) pollDesktopInput(false);
    std::string show = active ? g_editText : value;
    if (show.empty() && !active) {
        dl->AddText(fonts.body, fonts.body->FontSize,
                    ImVec2(p.x + S(32), p.y + (h - fonts.body->FontSize) * 0.5f),
                    IM_COL32(168, 176, 190, 255), hint.c_str());
    } else {
        dl->AddText(fonts.body, fonts.body->FontSize,
                    ImVec2(p.x + S(32), p.y + (h - fonts.body->FontSize) * 0.5f), colText, show.c_str());
    }
    if (clicked && !g_editing) {
        g_editing = true;
        g_editId = id;
        g_editText = value;
        g_editNumeric = false;
        plat::showTextInput(value, false);
    }
    if (g_editing && g_editId == id && value != g_editText) {
        value = g_editText;
    }
    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + h + S(10)));
    ImGui::PopID();
    return false;
}

// ===========================================================================
// 分段控件
// ===========================================================================
int Segmented(const char* id, const std::vector<std::string>& items, int current) {
    ImGui::PushID(id);
    float w = ImGui::GetContentRegionAvail().x;
    float h = S(36);
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), IM_COL32(240, 242, 247, 255), S(9));
    int result = -1;
    float segW = w / (float)items.size();
    for (int i = 0; i < (int)items.size(); i++) {
        ImGui::PushID(i);
        ImGui::SetCursorScreenPos(ImVec2(p.x + segW * i, p.y));
        ImGui::InvisibleButton("##seg", ImVec2(segW, h));
        if (ImGui::IsItemDeactivated() && ImGui::IsItemHovered()) result = i;
        ImGui::PopID();
        if (i == current) {
            dl->AddRectFilled(ImVec2(p.x + segW * i + S(3), p.y + S(3)),
                              ImVec2(p.x + segW * (i + 1) - S(3), p.y + h - S(3)), colCard, S(7));
        }
        ImVec2 ts = fonts.body->CalcTextSizeA(fonts.body->FontSize, FLT_MAX, 0, items[i].c_str());
        dl->AddText(fonts.body, fonts.body->FontSize,
                    ImVec2(p.x + segW * i + (segW - ts.x) * 0.5f, p.y + (h - ts.y) * 0.5f),
                    i == current ? colText : colSub, items[i].c_str());
    }
    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + h + S(10)));
    ImGui::PopID();
    return result;
}

// ===========================================================================
// 下拉选择
// ===========================================================================
int Dropdown(const char* id, const std::string& value, const std::string& hint,
             const std::vector<std::string>& options) {
    int result = -1;
    auto it = g_dropResult.find(id);
    if (it != g_dropResult.end()) {
        result = it->second;
        g_dropResult.erase(it);
    }

    ImGui::PushID(id);
    float w = ImGui::GetContentRegionAvail().x;
    float h = S(46);
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), IM_COL32(247, 248, 251, 255), S(9));
    dl->AddRect(p, ImVec2(p.x + w, p.y + h), colLine, S(9));
    if (value.empty()) {
        dl->AddText(fonts.body, fonts.body->FontSize,
                    ImVec2(p.x + S(13), p.y + (h - fonts.body->FontSize) * 0.5f),
                    IM_COL32(168, 176, 190, 255), hint.c_str());
    } else {
        dl->AddText(fonts.body, fonts.body->FontSize,
                    ImVec2(p.x + S(13), p.y + (h - fonts.body->FontSize) * 0.5f), colText, value.c_str());
    }
    Icon(IconArrow, ImVec2(p.x + w - S(18), p.y + h * 0.5f), S(7), colSub);
    ImGui::SetCursorScreenPos(p);
    if (ImGui::InvisibleButton("##dd", ImVec2(w, h)) && !options.empty()) {
        g_dropActive = id;
        ImGui::OpenPopup("##drop_popup");
    }
    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + h + S(10)));

    // 弹窗内容（位于视口中央，便于触摸选择）
    ImGui::SetNextWindowSize(ImVec2(S(300), 0), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(S(16), S(14)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, S(14));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, S(4)));
    ImGui::PushStyleColor(ImGuiCol_PopupBg, colCard);
    if (ImGui::BeginPopupModal("##drop_popup", nullptr,
                               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize |
                               ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::PushStyleColor(ImGuiCol_Text, colSub);
        ImGui::TextUnformatted("请选择");
        ImGui::PopStyleColor();
        ImGui::Dummy(ImVec2(0, S(6)));
        ImGui::BeginChild("##opts", ImVec2(S(268), S(360)), false);
        for (int i = 0; i < (int)options.size(); i++) {
            ImGui::PushID(i);
            bool selected = options[i] == value;
            ImGui::PushStyleColor(ImGuiCol_Button, selected ? alpha(colPrimary, 0.10f) : IM_COL32(246, 247, 250, 255));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, alpha(colPrimary, 0.16f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, alpha(colPrimary, 0.22f));
            ImGui::PushStyleColor(ImGuiCol_Text, selected ? colPrimary : colText);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, S(9));
            if (ImGui::Button(options[i].c_str(), ImVec2(S(250), S(44)))) {
                g_dropResult[g_dropActive] = i;
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(4);
            ImGui::PopID();
        }
        ImGui::EndChild();
        ImGui::Dummy(ImVec2(0, S(8)));
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(240, 242, 247, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(232, 235, 242, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(224, 228, 238, 255));
        ImGui::PushStyleColor(ImGuiCol_Text, colSub);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, S(10));
        if (ImGui::Button("取消", ImVec2(S(268), S(42)))) ImGui::CloseCurrentPopup();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(4);
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
    ImGui::PopID();
    return result;
}

bool DateField(const char* id, std::string& date, const std::string& hint) {
    return TextField(id, date, hint);
}

// ===========================================================================
// 开关
// ===========================================================================
bool Switch(const char* id, bool& value) {
    ImGui::PushID(id);
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = S(46), h = S(26);
    ImGui::InvisibleButton("##sw", ImVec2(w, h));
    bool clicked = ImGui::IsItemDeactivated() && ImGui::IsItemHovered();
    if (clicked) value = !value;
    ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x + w, p.y + h),
                                              value ? colPrimary : IM_COL32(210, 216, 226, 255), h * 0.5f);
    float cx = value ? p.x + w - h * 0.5f : p.x + h * 0.5f;
    ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(cx, p.y + h * 0.5f), h * 0.5f - S(2), colWhite);
    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + h));
    ImGui::PopID();
    return clicked;
}

// ===========================================================================
// 顶部栏与底部导航
// ===========================================================================
void TopBar(const std::string& title, bool showBack, const std::string& action,
            bool& backClicked, bool& actionClicked) {
    backClicked = false;
    actionClicked = false;
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = CanvasW() * g_scale;
    float h = S(56);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), colWhite);
    if (showBack) {
        ImGui::SetCursorScreenPos(ImVec2(p.x + S(8), p.y + S(12)));
        if (ImGui::InvisibleButton("##back", ImVec2(S(32), S(32)))) backClicked = true;
        Icon(IconBack, ImVec2(p.x + S(24), p.y + h * 0.5f), S(18), colText);
    }
    ImVec2 ts = fonts.title->CalcTextSizeA(fonts.title->FontSize, FLT_MAX, 0, title.c_str());
    dl->AddText(fonts.title, fonts.title->FontSize,
                ImVec2(p.x + (w - ts.x) * 0.5f, p.y + (h - ts.y) * 0.5f), colText, title.c_str());
    if (!action.empty()) {
        ImVec2 as = fonts.body->CalcTextSizeA(fonts.body->FontSize, FLT_MAX, 0, action.c_str());
        ImGui::SetCursorScreenPos(ImVec2(p.x + w - as.x - S(16), p.y + S(14)));
        if (ImGui::InvisibleButton("##act", ImVec2(as.x + S(8), S(28)))) actionClicked = true;
        dl->AddText(fonts.body, fonts.body->FontSize,
                    ImVec2(p.x + w - as.x - S(12), p.y + (h - as.y) * 0.5f), colPrimary, action.c_str());
    }
    dl->AddLine(ImVec2(p.x, p.y + h), ImVec2(p.x + w, p.y + h), colLine, 1.0f);
    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + h));
}

void BottomNav(int& tab) {
    const char* names[] = {"首页", "业务", "往来", "报表", "我的"};
    IconType icons[] = {IconHome, IconGrid, IconUsers, IconChart, IconUser};
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = CanvasW() * g_scale;
    float h = S(62);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), colWhite);
    dl->AddLine(p, ImVec2(p.x + w, p.y), colLine, 1.0f);
    float seg = w / 5.0f;
    for (int i = 0; i < 5; i++) {
        ImGui::PushID(i);
        ImGui::SetCursorScreenPos(ImVec2(p.x + seg * i, p.y));
        if (ImGui::InvisibleButton("##tab", ImVec2(seg, h))) tab = i;
        ImGui::PopID();
        ImU32 color = (i == tab) ? colPrimary : colSub;
        Icon(icons[i], ImVec2(p.x + seg * i + seg * 0.5f, p.y + S(20)), S(19), color);
        ImVec2 ts = fonts.small->CalcTextSizeA(fonts.small->FontSize, FLT_MAX, 0, names[i]);
        dl->AddText(fonts.small, fonts.small->FontSize,
                    ImVec2(p.x + seg * i + (seg - ts.x) * 0.5f, p.y + S(38)), color, names[i]);
    }
    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + h));
}

// ===========================================================================
// 弹窗
// ===========================================================================
bool Confirm(const char* id, const std::string& title, const std::string& content,
             const std::string& okText, const std::string& cancelText) {
    bool ok = false;
    if (!ImGui::IsPopupOpen(id)) ImGui::OpenPopup(id);
    ImGui::SetNextWindowSize(ImVec2(S(300), 0), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(S(20), S(18)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, S(14));
    ImGui::PushStyleColor(ImGuiCol_PopupBg, colCard);
    if (ImGui::BeginPopupModal(id, nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::PushFont(fonts.title);
        ImGui::PushStyleColor(ImGuiCol_Text, colText);
        ImGui::TextUnformatted(title.c_str());
        ImGui::PopStyleColor();
        ImGui::PopFont();
        ImGui::Dummy(ImVec2(0, S(6)));
        ImGui::PushTextWrapPos(S(260));
        ImGui::PushStyleColor(ImGuiCol_Text, colSub);
        ImGui::TextUnformatted(content.c_str());
        ImGui::PopStyleColor();
        ImGui::PopTextWrapPos();
        ImGui::Dummy(ImVec2(0, S(16)));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, S(10));
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(240, 242, 247, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(232, 235, 242, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(224, 228, 238, 255));
        ImGui::PushStyleColor(ImGuiCol_Text, colSub);
        if (ImGui::Button(cancelText.c_str(), ImVec2(S(125), S(42)))) ImGui::CloseCurrentPopup();
        ImGui::PopStyleColor(4);
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, colPrimary);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(84, 138, 255, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(37, 90, 220, 255));
        ImGui::PushStyleColor(ImGuiCol_Text, colWhite);
        if (ImGui::Button(okText.c_str(), ImVec2(S(125), S(42)))) {
            ok = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar();
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
    return ok;
}

// ===========================================================================
// 图标（用几何图形绘制，避免依赖图标字体）
// ===========================================================================
void Icon(IconType type, ImVec2 c, float s, ImU32 color) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float t = std::max(1.0f, s * 0.11f);
    switch (type) {
    case IconHome: {
        dl->AddTriangleFilled(ImVec2(c.x, c.y - s * 0.9f), ImVec2(c.x - s, c.y - s * 0.1f),
                              ImVec2(c.x + s, c.y - s * 0.1f), color);
        dl->AddRectFilled(ImVec2(c.x - s * 0.72f, c.y - s * 0.15f), ImVec2(c.x + s * 0.72f, c.y + s * 0.85f),
                          color, s * 0.12f);
        break;
    }
    case IconGrid: {
        float q = s * 0.38f;
        for (int i = 0; i < 4; i++) {
            float x = c.x + (i % 2 ? q + s * 0.1f : -q - s * 0.1f);
            float y = c.y + (i / 2 ? q + s * 0.1f : -q - s * 0.1f);
            dl->AddRectFilled(ImVec2(x - q * 0.85f, y - q * 0.85f), ImVec2(x + q * 0.85f, y + q * 0.85f),
                              color, s * 0.1f);
        }
        break;
    }
    case IconUsers: {
        dl->AddCircleFilled(ImVec2(c.x - s * 0.25f, c.y - s * 0.35f), s * 0.38f, color, 24);
        dl->AddCircleFilled(ImVec2(c.x + s * 0.45f, c.y - s * 0.15f), s * 0.28f, color, 24);
        dl->PathClear();
        dl->PathArcTo(ImVec2(c.x - s * 0.2f, c.y + s * 0.85f), s * 0.72f, 3.1416f, 6.2832f, 20);
        dl->PathStroke(color, 0, s * 0.26f);
        break;
    }
    case IconChart: {
        dl->AddRectFilled(ImVec2(c.x - s * 0.85f, c.y + s * 0.05f), ImVec2(c.x - s * 0.35f, c.y + s * 0.85f), color, s * 0.08f);
        dl->AddRectFilled(ImVec2(c.x - s * 0.22f, c.y - s * 0.45f), ImVec2(c.x + s * 0.28f, c.y + s * 0.85f), color, s * 0.08f);
        dl->AddRectFilled(ImVec2(c.x + s * 0.45f, c.y - s * 0.85f), ImVec2(c.x + s * 0.9f, c.y + s * 0.85f), color, s * 0.08f);
        break;
    }
    case IconUser: {
        dl->AddCircleFilled(ImVec2(c.x, c.y - s * 0.42f), s * 0.4f, color, 24);
        dl->PathClear();
        dl->PathArcTo(ImVec2(c.x, c.y + s * 0.95f), s * 0.78f, 3.1416f, 6.2832f, 24);
        dl->PathStroke(color, 0, s * 0.26f);
        break;
    }
    case IconPhone: {
        dl->AddRectFilled(ImVec2(c.x - s * 0.55f, c.y - s * 0.95f), ImVec2(c.x + s * 0.55f, c.y + s * 0.95f),
                          color, s * 0.22f);
        break;
    }
    case IconWrench: {
        dl->AddLine(ImVec2(c.x - s * 0.75f, c.y + s * 0.75f), ImVec2(c.x + s * 0.45f, c.y - s * 0.45f), color, t * 2.2f);
        dl->AddCircleFilled(ImVec2(c.x + s * 0.55f, c.y - s * 0.6f), s * 0.36f, color, 18);
        break;
    }
    case IconParts: {
        dl->AddRectFilled(ImVec2(c.x - s * 0.85f, c.y - s * 0.6f), ImVec2(c.x + s * 0.85f, c.y + s * 0.75f), color, s * 0.12f);
        dl->AddLine(ImVec2(c.x - s * 0.85f, c.y - s * 0.1f), ImVec2(c.x + s * 0.85f, c.y - s * 0.1f), alpha(color, 0.4f), t);
        break;
    }
    case IconCard: {
        dl->AddRectFilled(ImVec2(c.x - s * 0.95f, c.y - s * 0.6f), ImVec2(c.x + s * 0.95f, c.y + s * 0.6f), color, s * 0.12f);
        dl->AddLine(ImVec2(c.x - s * 0.95f, c.y - s * 0.15f), ImVec2(c.x + s * 0.95f, c.y - s * 0.15f), alpha(color, 0.45f), t);
        break;
    }
    case IconMoney: {
        dl->AddCircleFilled(c, s * 0.9f, color, 28);
        dl->AddText(fonts.small, fonts.small->FontSize * (s * 1.1f / fonts.small->FontSize),
                    ImVec2(c.x - s * 0.3f, c.y - s * 0.6f), IM_COL32(255, 255, 255, 255), "¥");
        break;
    }
    case IconIn: {
        dl->AddLine(ImVec2(c.x, c.y - s * 0.85f), ImVec2(c.x, c.y + s * 0.5f), color, t * 1.8f);
        dl->AddTriangleFilled(ImVec2(c.x - s * 0.45f, c.y + s * 0.3f), ImVec2(c.x + s * 0.45f, c.y + s * 0.3f),
                              ImVec2(c.x, c.y + s * 0.9f), color);
        dl->AddLine(ImVec2(c.x - s * 0.85f, c.y - s * 0.85f), ImVec2(c.x + s * 0.85f, c.y - s * 0.85f), color, t);
        break;
    }
    case IconOut: {
        dl->AddLine(ImVec2(c.x, c.y + s * 0.85f), ImVec2(c.x, c.y - s * 0.5f), color, t * 1.8f);
        dl->AddTriangleFilled(ImVec2(c.x - s * 0.45f, c.y - s * 0.3f), ImVec2(c.x + s * 0.45f, c.y - s * 0.3f),
                              ImVec2(c.x, c.y - s * 0.9f), color);
        dl->AddLine(ImVec2(c.x - s * 0.85f, c.y + s * 0.85f), ImVec2(c.x + s * 0.85f, c.y + s * 0.85f), color, t);
        break;
    }
    case IconSearch: {
        dl->AddCircle(c, s * 0.6f, color, 20, t * 1.5f);
        dl->AddLine(ImVec2(c.x + s * 0.42f, c.y + s * 0.42f), ImVec2(c.x + s * 0.9f, c.y + s * 0.9f), color, t * 1.6f);
        break;
    }
    case IconBack: {
        dl->AddLine(ImVec2(c.x + s * 0.5f, c.y - s * 0.8f), ImVec2(c.x - s * 0.35f, c.y), color, t * 2.0f);
        dl->AddLine(ImVec2(c.x - s * 0.35f, c.y), ImVec2(c.x + s * 0.5f, c.y + s * 0.8f), color, t * 2.0f);
        break;
    }
    case IconPlus: {
        dl->AddLine(ImVec2(c.x - s * 0.7f, c.y), ImVec2(c.x + s * 0.7f, c.y), color, t * 2.0f);
        dl->AddLine(ImVec2(c.x, c.y - s * 0.7f), ImVec2(c.x, c.y + s * 0.7f), color, t * 2.0f);
        break;
    }
    case IconTrash: {
        dl->AddRectFilled(ImVec2(c.x - s * 0.55f, c.y - s * 0.25f), ImVec2(c.x + s * 0.55f, c.y + s * 0.8f), color, s * 0.1f);
        dl->AddLine(ImVec2(c.x - s * 0.8f, c.y - s * 0.4f), ImVec2(c.x + s * 0.8f, c.y - s * 0.4f), color, t * 1.5f);
        break;
    }
    case IconArrow: {
        dl->AddLine(ImVec2(c.x - s * 0.35f, c.y - s * 0.6f), ImVec2(c.x + s * 0.3f, c.y), color, t * 1.6f);
        dl->AddLine(ImVec2(c.x + s * 0.3f, c.y), ImVec2(c.x - s * 0.35f, c.y + s * 0.6f), color, t * 1.6f);
        break;
    }
    case IconCheck: {
        dl->AddLine(ImVec2(c.x - s * 0.7f, c.y), ImVec2(c.x - s * 0.1f, c.y + s * 0.6f), color, t * 1.8f);
        dl->AddLine(ImVec2(c.x - s * 0.1f, c.y + s * 0.6f), ImVec2(c.x + s * 0.75f, c.y - s * 0.65f), color, t * 1.8f);
        break;
    }
    case IconWarn: {
        dl->AddTriangleFilled(ImVec2(c.x, c.y - s * 0.85f), ImVec2(c.x - s * 0.9f, c.y + s * 0.75f),
                              ImVec2(c.x + s * 0.9f, c.y + s * 0.75f), color);
        break;
    }
    case IconBag: {
        dl->AddRectFilled(ImVec2(c.x - s * 0.75f, c.y - s * 0.35f), ImVec2(c.x + s * 0.75f, c.y + s * 0.85f), color, s * 0.14f);
        break;
    }
    case IconSetting: {
        dl->AddCircleFilled(c, s * 0.42f, color, 18);
        for (int i = 0; i < 8; i++) {
            float a = i * 3.14159f / 4.0f;
            dl->AddLine(ImVec2(c.x + std::cos(a) * s * 0.55f, c.y + std::sin(a) * s * 0.55f),
                        ImVec2(c.x + std::cos(a) * s * 0.9f, c.y + std::sin(a) * s * 0.9f), color, t * 1.6f);
        }
        break;
    }
    }
}

// ===========================================================================
// 图表
// ===========================================================================
void BarChart(const std::vector<float>& values, const std::vector<std::string>& labels,
              float height, ImU32 color) {
    if (values.empty()) {
        Empty("暂无数据");
        return;
    }
    ImU32 c = color ? color : colPrimary;
    float w = ImGui::GetContentRegionAvail().x;
    float h = S(height);
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float labelH = S(18);
    float chartH = h - labelH;
    float maxV = 0.01f;
    for (float v : values) maxV = std::max(maxV, v);
    // 参考线
    for (int i = 0; i <= 3; i++) {
        float y = p.y + chartH * i / 3.0f;
        dl->AddLine(ImVec2(p.x, y), ImVec2(p.x + w, y), alpha(colLine, 0.8f), 1.0f);
    }
    int n = (int)values.size();
    float gap = S(4);
    float barW = std::max(S(3), (w - gap * (n - 1)) / std::max(1, n));
    int labelStep = std::max(1, n / 7);
    for (int i = 0; i < n; i++) {
        float bh = chartH * (values[i] / maxV);
        float x = p.x + i * (barW + gap);
        dl->AddRectFilled(ImVec2(x, p.y + chartH - bh), ImVec2(x + barW, p.y + chartH), c, S(2));
        if (i % labelStep == 0 && i < (int)labels.size()) {
            std::string lb = labels[i];
            if (lb.size() >= 10) lb = lb.substr(5);
            ImVec2 ts = fonts.small->CalcTextSizeA(fonts.small->FontSize, FLT_MAX, 0, lb.c_str());
            dl->AddText(fonts.small, fonts.small->FontSize,
                        ImVec2(x + (barW - ts.x) * 0.5f, p.y + chartH + S(4)), colSub, lb.c_str());
        }
    }
    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + h + S(8)));
}

void HBarRow(const std::string& label, float ratio, const std::string& value, ImU32 color) {
    ImGui::PushID(label.c_str());
    float w = ImGui::GetContentRegionAvail().x;
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddText(fonts.body, fonts.body->FontSize, p, colText, label.c_str());
    ImVec2 vs = fonts.body->CalcTextSizeA(fonts.body->FontSize, FLT_MAX, 0, value.c_str());
    dl->AddText(fonts.body, fonts.body->FontSize, ImVec2(p.x + w - vs.x, p.y), colSub, value.c_str());
    float y = p.y + fonts.body->FontSize + S(6);
    dl->AddRectFilled(ImVec2(p.x, y), ImVec2(p.x + w, y + S(7)), IM_COL32(240, 242, 247, 255), S(3.5f));
    dl->AddRectFilled(ImVec2(p.x, y), ImVec2(p.x + std::max(S(3), w * std::min(1.0f, ratio)), y + S(7)),
                      color ? color : colPrimary, S(3.5f));
    ImGui::SetCursorScreenPos(ImVec2(p.x, y + S(7) + S(14)));
    ImGui::PopID();
}

// ===========================================================================
// 滚动区域
// ===========================================================================
void BeginScroll(const char* id, float height) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::BeginChild(id, ImVec2(CanvasW() * g_scale, S(height)), false,
                      ImGuiWindowFlags_AlwaysVerticalScrollbar);
    ImGui::Dummy(ImVec2(0, 0));
}

void EndScroll() {
    ImGui::EndChild();
    ImGui::PopStyleVar();
}

// ===========================================================================
// 工具
// ===========================================================================
std::string Money(double v) {
    char buf[64];
    bool neg = v < 0;
    if (neg) v = -v;
    std::snprintf(buf, sizeof(buf), "%.2f", v);
    std::string s(buf);
    size_t dot = s.find('.');
    std::string ip = s.substr(0, dot);
    std::string dp = s.substr(dot);
    std::string out;
    int cnt = 0;
    for (int i = (int)ip.size() - 1; i >= 0; i--) {
        out.push_back(ip[i]);
        if (++cnt == 3 && i > 0) {
            out.push_back(',');
            cnt = 0;
        }
    }
    std::reverse(out.begin(), out.end());
    return (neg ? "-" : "") + out + dp;
}

std::string MoneyShort(double v) {
    char buf[64];
    if (std::fabs(v) >= 10000) {
        std::snprintf(buf, sizeof(buf), "%.2f万", v / 10000.0);
    } else {
        std::snprintf(buf, sizeof(buf), "%.0f", v);
    }
    return buf;
}

std::string Num(double v) {
    char buf[64];
    if (v == (double)(long long)v) std::snprintf(buf, sizeof(buf), "%lld", (long long)v);
    else std::snprintf(buf, sizeof(buf), "%.2f", v);
    return buf;
}

} // namespace ui
