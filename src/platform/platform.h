// 平台相关接口：文本输入、字体、数据目录
#pragma once

#include <string>

namespace plat {
bool isAndroid();
// 是否使用系统输入法（Android / iOS 为 true，桌面端为 false）
bool hasNativeIme();
void showTextInput(const std::string& text, bool numeric);
void hideTextInput();
std::string fontPath();   // 中文字体文件路径
std::string dataDir();    // 应用数据目录
}
