#!/usr/bin/env bash
# ===========================================================================
# 数码管家 iOS 打包脚本（必须在 macOS + Xcode 上运行）
#
# 用法：
#   1) 只生成未签名安装包（无法直接装到 iPhone，需后续签名）：
#        ./build_ios.sh
#   2) 用你自己的开发者账号自动签名，产出可直接安装的 IPA：
#        TEAM_ID=你的10位TeamID ./build_ios.sh
#      TeamID 查询：登录 https://developer.apple.com/account 首页即可看到；
#      或执行 security find-identity -v -p codesigning 查看证书名。
#   3) 已手动导出描述文件时，可指定证书与描述文件签名：
#        SIGN_IDENTITY="Apple Development: 你的名字 (XXXXXXXXXX)" \
#        PROFILE="$HOME/Downloads/数码管家.mobileprovision" ./build_ios.sh
#
# 产物：dist/数码管家.ipa（已签名）或 dist/数码管家-未签名.ipa
# ===========================================================================
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
BUILD="$ROOT/build/ios"
DIST="$ROOT/dist"
APP_NAME="shumaguanjia"
EXEC_NAME="shumaguanjia"
TEAM_ID="${TEAM_ID:-}"
SIGN_IDENTITY="${SIGN_IDENTITY:-}"
PROFILE="${PROFILE:-}"

say() { printf '\033[36m== %s ==\033[0m\n' "$*"; }

# ---------------- 0. 环境检查 ----------------
[ "$(uname -s)" = "Darwin" ] || { echo "错误：iOS 编译只能在 macOS 上进行"; exit 1; }
command -v cmake >/dev/null 2>&1 || { echo "错误：未安装 cmake（brew install cmake）"; exit 1; }
command -v xcodebuild >/dev/null 2>&1 || { echo "错误：未安装 Xcode"; exit 1; }
xcodebuild -version

# ---------------- 1. 配置工程 ----------------
say "1/4 生成 Xcode 工程"
CMAKE_ARGS=(
  -S "$ROOT" -B "$BUILD" -G Xcode
  -DCMAKE_SYSTEM_NAME=iOS
  -DCMAKE_OSX_ARCHITECTURES=arm64
  -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0
)
if [ -n "$TEAM_ID" ]; then
  CMAKE_ARGS+=(
    -DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM="$TEAM_ID"
    -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGN_STYLE=Automatic
  )
fi
cmake "${CMAKE_ARGS[@]}"

# ---------------- 2. 编译 ----------------
say "2/4 编译 Release 版本"
PROJ="$BUILD/shumaguanjia.xcodeproj"
if [ -n "$TEAM_ID" ]; then
  # 使用你的开发者账号自动签名（首次会自动注册 App ID 并生成描述文件）
  xcodebuild -project "$PROJ" -target "$APP_NAME" -configuration Release \
    -sdk iphoneos -allowProvisioningUpdates \
    DEVELOPMENT_TEAM="$TEAM_ID" CODE_SIGN_STYLE=Automatic build
else
  # 不签名编译：可产出 .app / IPA 包体，但装不进非越狱设备
  xcodebuild -project "$PROJ" -target "$APP_NAME" -configuration Release \
    -sdk iphoneos CODE_SIGNING_ALLOWED=NO CODE_SIGNING_REQUIRED=NO build
fi

APP_PATH="$(find "$BUILD" -name "$EXEC_NAME.app" -type d | head -n 1)"
[ -n "$APP_PATH" ] || { echo "错误：未找到 $EXEC_NAME.app"; exit 1; }
echo "App: $APP_PATH"

# ---------------- 3. 可选：手动签名 ----------------
if [ -n "$SIGN_IDENTITY" ] && [ -n "$PROFILE" ]; then
  say "3/4 使用证书手动签名"
  cp "$PROFILE" "$APP_PATH/embedded.mobileprovision"
  TMP_ENT="$(mktemp /tmp/dm_entitlements.XXXX.plist)"
  security cms -D -i "$PROFILE" > "$TMP_ENT.full"
  /usr/libexec/PlistBuddy -x -c 'Print :Entitlements' "$TMP_ENT.full" > "$TMP_ENT"
  codesign -f -s "$SIGN_IDENTITY" --timestamp=none --entitlements "$TMP_ENT" "$APP_PATH"
  codesign -v --verbose "$APP_PATH"
  rm -f "$TMP_ENT" "$TMP_ENT.full"
else
  say "3/4 跳过签名（未指定 SIGN_IDENTITY / PROFILE）"
fi

# ---------------- 4. 打成 IPA ----------------
say "4/4 打包 IPA"
mkdir -p "$DIST"
STAGE="$(mktemp -d /tmp/dm_ipa.XXXXXX)"
mkdir -p "$STAGE/Payload"
cp -R "$APP_PATH" "$STAGE/Payload/"

if [ -n "$TEAM_ID" ] || { [ -n "$SIGN_IDENTITY" ] && [ -n "$PROFILE" ]; }; then
  OUT="$DIST/数码管家.ipa"
else
  OUT="$DIST/数码管家-未签名.ipa"
fi
rm -f "$OUT"
( cd "$STAGE" && zip -qry "$OUT" Payload )
rm -rf "$STAGE"

echo
say "完成：$OUT"
echo "大小：$(du -h "$OUT" | cut -f1)"
if [ -z "$TEAM_ID" ] && [ -z "$SIGN_IDENTITY" ]; then
  cat <<'TIP'

注意：这是【未签名】安装包，不能直接装到 iPhone。
签名/安装的三条路（任选其一）：
  1. 用 Xcode 直接运行：open build/ios/*.xcodeproj
     → 选中 shumaguanjia → Signing & Capabilities → 勾选 Automatically manage signing
     → 选择你的 Apple ID（Team）→ 连接 iPhone 后点 Run。
     免费 Apple ID 签名的 App 7 天后过期，需重新运行一次。
  2. 本脚本加 TEAM_ID 重新打包：TEAM_ID=XXXXXXXXXX ./build_ios.sh
  3. 用 AltStore / Sideloadly 等工具，拿这个 IPA + 你的 Apple ID 在电脑上自签后安装。
TIP
fi
