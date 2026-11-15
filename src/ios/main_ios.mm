// iOS 原生入口：UIApplication + CAEAGLLayer(OpenGL ES 2.0) + ImGui
// 与 Android / Windows 版共用 src/core（业务）与 src/ui（界面）全部代码。
//
// 说明：
//   1. 画面使用 EAGL 自建帧缓冲，渲染循环由 CADisplayLink 驱动（60fps）。
//   2. 触控事件转换为 ImGui 鼠标事件，拖动等效于滚轮滚动（与 Android 一致）。
//   3. 中文输入借助一个透明 UITextField 唤起系统输入法，文本实时回传给 ui::OnImeText。

#import <UIKit/UIKit.h>
#import <QuartzCore/QuartzCore.h>
#import <OpenGLES/ES2/gl.h>
#import <OpenGLES/EAGLDrawable.h>

#include "platform/platform.h"
#include "ui/app.h"

#include "imgui.h"
#include "imgui_impl_opengl3.h"

#include <cmath>
#include <cstdio>
#include <string>

// ===========================================================================
// 全局状态
// ===========================================================================
static App g_app;

@class DMViewController;

static DMViewController* g_vc = nil;    // 平台接口（输入法）回调入口
static float g_scale = 1.0f;            // 逻辑缩放比例
static std::string g_fontPath;          // 实际使用的中文字体
static int g_fontIndex = 0;             // .ttc 字体集合内的序号

// ===========================================================================
// 视图控制器接口
// ===========================================================================
@interface DMViewController : UIViewController <UITextFieldDelegate>
- (void)showKeyboard:(NSString*)text numeric:(BOOL)numeric;
- (void)hideKeyboard;
@end

// ===========================================================================
// 平台接口
// ===========================================================================
namespace plat {

bool isAndroid() { return false; }
bool hasNativeIme() { return true; }

void showTextInput(const std::string& text, bool numeric) {
    if (!g_vc) return;
    NSString* s = [NSString stringWithUTF8String:text.c_str()];
    [g_vc showKeyboard:(s ? s : @"") numeric:numeric];
}

void hideTextInput() {
    if (g_vc) [g_vc hideKeyboard];
}

std::string dataDir() {
    static std::string dir;
    if (dir.empty()) {
        @autoreleasepool {
            NSArray<NSString*>* paths =
                NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
            NSString* p = paths.count > 0 ? paths[0] : NSTemporaryDirectory();
            const char* u8 = p.UTF8String;
            dir = u8 ? u8 : "/tmp";
        }
    }
    return dir;
}

std::string fontPath() { return g_fontPath; }

} // namespace plat

// ===========================================================================
// 中文字体选择
// iOS 系统字体多为 .ttc 集合，需逐个尝试集合内序号并校验是否真的含汉字。
// ===========================================================================
static const char* kFontCandidates[] = {
    "/System/Library/Fonts/LanguageSupport/PingFang.ttc",
    "/System/Library/Fonts/PingFang.ttc",
    "/System/Library/Fonts/LanguageSupport/Hiragino Sans GB.ttc",
    "/System/Library/Fonts/STHeiti Light.ttc",
    "/System/Library/Fonts/STHeiti Medium.ttc",
    "/System/Library/Fonts/LanguageSupport/Heiti SC.ttc",
};

static bool fileExists(const char* path) {
    NSString* p = [NSString stringWithUTF8String:path];
    if (!p) return false;
    return [[NSFileManager defaultManager] fileExistsAtPath:p];
}

static bool hasChineseGlyphs() {
    ImFont* f = ui::fonts.body;
    if (!f) return false;
    return f->FindGlyphNoFallback((ImWchar)0x4E2D) != nullptr;   // “中”
}

// 读取 .ttc 集合内的字体数量（非集合字体返回 1）。
// 用于避免传入越界序号触发 ImGui 的字体加载断言。
static int ttcFontCount(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return 1;
    unsigned char h[12] = {0};
    size_t n = fread(h, 1, sizeof(h), f);
    fclose(f);
    if (n < 12) return 1;
    if (!(h[0] == 't' && h[1] == 't' && h[2] == 'c' && h[3] == 'f')) return 1;
    unsigned int num = ((unsigned int)h[8] << 24) | ((unsigned int)h[9] << 16) |
                       ((unsigned int)h[10] << 8) | (unsigned int)h[11];
    if (num == 0 || num > 64) return 1;
    return (int)num;
}

static void initFonts(float scale) {
    for (const char* path : kFontCandidates) {
        if (!fileExists(path)) continue;
        int count = ttcFontCount(path);
        if (count > 8) count = 8;
        for (int idx = 0; idx < count; ++idx) {
            ui::Init(scale, path, idx);
            if (hasChineseGlyphs()) {
                g_fontPath = path;
                g_fontIndex = idx;
                return;
            }
        }
    }
    // 全部失败时退回 ImGui 内置字体（中文会显示为方块，但不会崩溃）
    g_fontPath.clear();
    g_fontIndex = 0;
    ui::Init(scale, "", 0);
}

// ===========================================================================
// OpenGL 视图（提供 CAEAGLLayer）
// ===========================================================================
@interface DMGLView : UIView
@end

@implementation DMGLView
+ (Class)layerClass { return [CAEAGLLayer class]; }
@end

// ===========================================================================
// 主视图控制器
// ===========================================================================
@interface DMViewController () {
    GLuint _framebuffer;
    GLuint _renderbuffer;
    GLint _backingW;
    GLint _backingH;
    BOOL _glReady;
    BOOL _imguiReady;
    CGSize _fbBounds;
    CGFloat _contentScale;
    CFTimeInterval _lastTime;

    BOOL _touching;
    BOOL _dragging;
    CGPoint _downPt;
    CGPoint _lastPt;
    UITouch* _activeTouch;
}
@property (nonatomic, strong) EAGLContext* context;
@property (nonatomic, strong) CADisplayLink* displayLink;
@property (nonatomic, strong) UITextField* imeField;

- (BOOL)createFramebuffer;
- (void)destroyFramebuffer;
- (void)recreateFramebuffer;
- (void)setupImeField;
- (BOOL)startEngine;
- (CGRect)contentRectPoints;
- (CGRect)contentRectPixels;
- (CGPoint)imguiPoint:(CGPoint)p;
- (void)finishTouch:(NSSet<UITouch*>*)touches;
- (void)appWillResignActive;
- (void)appDidBecomeActive;
- (void)imeChanged:(UITextField*)sender;
- (void)imeDoneTapped;
@end

@implementation DMViewController

// ---------------- 生命周期 ----------------
- (void)viewDidLoad {
    [super viewDidLoad];

    DMGLView* glView = [[DMGLView alloc] initWithFrame:[UIScreen mainScreen].bounds];
    glView.backgroundColor = [UIColor blackColor];
    glView.opaque = YES;
    glView.multipleTouchEnabled = NO;
    self.view = glView;

    CAEAGLLayer* layer = (CAEAGLLayer*)glView.layer;
    layer.opaque = YES;
    layer.drawsAsynchronously = NO;
    layer.drawableProperties = @{
        kEAGLDrawablePropertyRetainedBacking : @NO,
        kEAGLDrawablePropertyColorFormat : kEAGLColorFormatRGBA8
    };

    self.context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
    if (!self.context) {
        NSLog(@"[数码管家] 无法创建 OpenGL ES 2 上下文");
        return;
    }
    [EAGLContext setCurrentContext:self.context];
    _glReady = [self createFramebuffer];

    [self setupImeField];

    self.displayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(onFrame:)];
    self.displayLink.preferredFramesPerSecond = 60;
    [self.displayLink addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];

    NSNotificationCenter* nc = [NSNotificationCenter defaultCenter];
    [nc addObserver:self
           selector:@selector(appWillResignActive)
               name:UIApplicationWillResignActiveNotification
             object:nil];
    [nc addObserver:self
           selector:@selector(appDidBecomeActive)
               name:UIApplicationDidBecomeActiveNotification
             object:nil];
}

- (void)dealloc {
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    [self.displayLink invalidate];
}

- (BOOL)prefersStatusBarHidden { return YES; }
- (BOOL)prefersHomeIndicatorAutoHidden { return NO; }
- (UIInterfaceOrientationMask)supportedInterfaceOrientations {
    return UIInterfaceOrientationMaskPortrait;
}

- (void)viewDidLayoutSubviews {
    [super viewDidLayoutSubviews];
    if (!self.context) return;
    _contentScale = self.view.contentScaleFactor;
    if (!CGSizeEqualToSize(_fbBounds, self.view.bounds.size)) {
        [self recreateFramebuffer];
    }
}

- (void)appWillResignActive {
    self.displayLink.paused = YES;
    if (ui::IsEditing()) ui::OnImeDone();
    [self hideKeyboard];
}

- (void)appDidBecomeActive {
    _fbBounds = CGSizeZero;   // 后台可能回收绘制缓冲区，回前台强制重建
    self.displayLink.paused = NO;
}

// ---------------- OpenGL 帧缓冲 ----------------
- (BOOL)createFramebuffer {
    [EAGLContext setCurrentContext:self.context];
    glGenFramebuffers(1, &_framebuffer);
    glGenRenderbuffers(1, &_renderbuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, _framebuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, _renderbuffer);

    if (![self.context renderbufferStorage:GL_RENDERBUFFER
                             fromDrawable:(CAEAGLLayer*)self.view.layer]) {
        NSLog(@"[数码管家] renderbufferStorage 失败");
        return NO;
    }
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, _renderbuffer);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &_backingW);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &_backingH);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        NSLog(@"[数码管家] 帧缓冲不完整");
        return NO;
    }
    _fbBounds = self.view.bounds.size;
    return YES;
}

- (void)destroyFramebuffer {
    if (_framebuffer) { glDeleteFramebuffers(1, &_framebuffer); _framebuffer = 0; }
    if (_renderbuffer) { glDeleteRenderbuffers(1, &_renderbuffer); _renderbuffer = 0; }
}

- (void)recreateFramebuffer {
    [EAGLContext setCurrentContext:self.context];
    [self destroyFramebuffer];
    _glReady = [self createFramebuffer];
}

// 安全区域内的可用区域（避开刘海与底部横条），单位为逻辑点（point）
- (CGRect)contentRectPoints {
    CGRect b = self.view.bounds;
    UIEdgeInsets in = self.view.safeAreaInsets;
    CGFloat x = in.left, y = in.top;
    CGFloat w = b.size.width - in.left - in.right;
    CGFloat h = b.size.height - in.top - in.bottom;
    if (w < 1.0f) { w = b.size.width; x = 0; }
    if (h < 1.0f) { h = b.size.height; y = 0; }
    return CGRectMake(x, y, w, h);
}

// 同上，但换算为物理像素。ImGui 的 DisplaySize 使用像素，
// 这样字体按真实像素光栅化，画面才不会发虚（与 Android 版一致）。
- (CGRect)contentRectPixels {
    CGRect c = [self contentRectPoints];
    CGFloat sf = _contentScale > 0 ? _contentScale : self.view.contentScaleFactor;
    return CGRectMake(c.origin.x * sf, c.origin.y * sf, c.size.width * sf, c.size.height * sf);
}

// ---------------- 渲染循环 ----------------
- (void)onFrame:(CADisplayLink*)link {
    if (!self.context) return;
    if (!CGSizeEqualToSize(_fbBounds, self.view.bounds.size)) {
        [self recreateFramebuffer];
        if (!_glReady) return;
    }
    if (!_glReady) return;

    [EAGLContext setCurrentContext:self.context];
    glBindFramebuffer(GL_FRAMEBUFFER, _framebuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, _renderbuffer);

    if (!_imguiReady) {
        if (![self startEngine]) return;
    }

    CFTimeInterval now = link.timestamp;
    float dt = _lastTime > 0 ? (float)(now - _lastTime) : (1.0f / 60.0f);
    _lastTime = now;
    if (dt <= 0.0f) dt = 1.0f / 60.0f;
    if (dt > 0.1f) dt = 0.1f;

    CGRect px = [self contentRectPixels];
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)px.size.width, (float)px.size.height);
    io.DeltaTime = dt;

    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();
    g_app.draw(dt);
    ImGui::Render();

    glViewport((GLint)lround(px.origin.x),
               (GLint)lround((CGFloat)_backingH - px.origin.y - px.size.height),
               (GLsizei)lround(px.size.width),
               (GLsizei)lround(px.size.height));
    glClearColor(0.95f, 0.96f, 0.98f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    [self.context presentRenderbuffer:GL_RENDERBUFFER];
}

- (BOOL)startEngine {
    if (_contentScale <= 0) _contentScale = self.view.contentScaleFactor;
    CGRect px = [self contentRectPixels];
    if (px.size.width < 1.0f || px.size.height < 1.0f) return NO;

    // 以屏宽为基准缩放，画布高度按屏幕实际比例延展：
    // 全面屏（19.5:9）、16:9（SE / 8 Plus）、动态岛机型都能铺满且不留黑边。
    float scale = (float)px.size.width / 390.0f;
    if (scale < 1.0f) scale = 1.0f;
    g_scale = scale;

    float canvasH = (float)px.size.height / scale;
    if (canvasH < 600.0f) canvasH = 600.0f;
    if (canvasH > 1200.0f) canvasH = 1200.0f;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.DisplaySize = ImVec2((float)px.size.width, (float)px.size.height);

    initFonts(scale);                    // Init 内部会把画布重置为 390x844
    ui::SetCanvas(390.0f, canvasH);      // 必须在其后设置实际画布高度
    ImGui_ImplOpenGL3_Init("#version 100");

    g_app.fontPath = g_fontPath;
    g_app.store.init(plat::dataDir() + "/shumaguanjia.db");
    g_app.applyRange();

    _imguiReady = YES;
    NSLog(@"[数码管家] 启动 %dx%d px, scale=%.2f, 字体=%s[%d], 数据目录=%s",
          (int)px.size.width, (int)px.size.height, scale,
          g_fontPath.c_str(), g_fontIndex, plat::dataDir().c_str());
    return YES;
}

// ---------------- 输入法（透明 UITextField 桥接） ----------------
- (void)setupImeField {
    UITextField* tf = [[UITextField alloc] initWithFrame:CGRectMake(-200, -200, 4, 4)];
    tf.delegate = self;
    tf.alpha = 0.02f;
    tf.backgroundColor = [UIColor clearColor];
    tf.textColor = [UIColor clearColor];
    tf.tintColor = [UIColor clearColor];
    tf.autocorrectionType = UITextAutocorrectionTypeNo;
    tf.autocapitalizationType = UITextAutocapitalizationTypeNone;
    tf.spellCheckingType = UITextSpellCheckingTypeNo;
    tf.textContentType = nil;
    [tf addTarget:self
           action:@selector(imeChanged:)
     forControlEvents:UIControlEventEditingChanged];

    // 数字键盘没有回车键，统一加一条“完成”工具条
    UIToolbar* bar = [[UIToolbar alloc] initWithFrame:CGRectMake(0, 0, 320, 44)];
    UIBarButtonItem* flex =
        [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemFlexibleSpace
                                                      target:nil
                                                      action:nil];
    UIBarButtonItem* done = [[UIBarButtonItem alloc] initWithTitle:@"完成"
                                                            style:UIBarButtonItemStyleDone
                                                           target:self
                                                           action:@selector(imeDoneTapped)];
    bar.items = @[ flex, done ];
    [bar sizeToFit];
    tf.inputAccessoryView = bar;

    [self.view addSubview:tf];
    self.imeField = tf;
}

- (void)showKeyboard:(NSString*)text numeric:(BOOL)numeric {
    if (!self.imeField) return;
    self.imeField.keyboardType = numeric ? UIKeyboardTypeDecimalPad : UIKeyboardTypeDefault;
    self.imeField.text = text ? text : @"";
    if (![self.imeField isFirstResponder]) [self.imeField becomeFirstResponder];

    UITextPosition* end = [self.imeField endOfDocument];
    if (end) self.imeField.selectedTextRange = [self.imeField textRangeFromPosition:end toPosition:end];
}

- (void)hideKeyboard {
    [self.imeField resignFirstResponder];
}

- (void)imeChanged:(UITextField*)sender {
    const char* s = sender.text.UTF8String;
    ui::OnImeText(s ? s : "");
}

- (void)imeDoneTapped {
    ui::OnImeDone();
    [self hideKeyboard];
}

- (BOOL)textFieldShouldReturn:(UITextField*)tf {
    [self imeDoneTapped];
    return NO;
}

- (void)textFieldDidEndEditing:(UITextField*)tf {
    ui::OnImeDone();
}

// ---------------- 触控 ----------------
- (CGPoint)imguiPoint:(CGPoint)p {
    CGRect c = [self contentRectPoints];
    CGFloat sf = _contentScale > 0 ? _contentScale : self.view.contentScaleFactor;
    return CGPointMake((p.x - c.origin.x) * sf, (p.y - c.origin.y) * sf);
}

- (void)touchesBegan:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    if (!_imguiReady || _activeTouch) return;
    UITouch* t = [touches anyObject];
    if (!t) return;
    _activeTouch = t;
    _touching = YES;
    _dragging = NO;

    CGPoint p = [self imguiPoint:[t locationInView:self.view]];
    _downPt = _lastPt = p;

    ImGuiIO& io = ImGui::GetIO();
    io.AddMousePosEvent((float)p.x, (float)p.y);
    io.AddMouseButtonEvent(0, true);
}

- (void)touchesMoved:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    if (!_imguiReady || !_activeTouch) return;

    BOOL mine = NO;
    for (UITouch* t in touches) {
        if (t == _activeTouch) { mine = YES; break; }
    }
    if (!mine) return;

    CGPoint p = [self imguiPoint:[_activeTouch locationInView:self.view]];
    ImGuiIO& io = ImGui::GetIO();

    if (!_dragging) {
        float sf = _contentScale > 0 ? (float)_contentScale : 1.0f;
        float dx = (float)(p.x - _downPt.x);
        float dy = (float)(p.y - _downPt.y);
        float limit = 10.0f * sf;
        if (dx * dx + dy * dy > limit * limit) _dragging = YES;
    }

    io.AddMousePosEvent((float)p.x, (float)p.y);
    if (_dragging) {
        float dy = (float)(p.y - _lastPt.y);
        float unit = 5.0f * 14.5f * g_scale;
        if (unit > 1.0f) io.AddMouseWheelEvent(0.0f, -dy / unit);
    }
    _lastPt = p;
}

- (void)touchesEnded:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    [self finishTouch:touches];
}

- (void)touchesCancelled:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    [self finishTouch:touches];
}

- (void)finishTouch:(NSSet<UITouch*>*)touches {
    if (!_imguiReady) return;
    BOOL mine = NO;
    for (UITouch* t in touches) {
        if (t == _activeTouch) { mine = YES; break; }
    }
    if (!mine && _activeTouch) return;   // 抬起的不是当前手指

    CGPoint p = _activeTouch ? [self imguiPoint:[_activeTouch locationInView:self.view]] : _lastPt;
    ImGuiIO& io = ImGui::GetIO();
    io.AddMousePosEvent((float)p.x, (float)p.y);
    io.AddMouseButtonEvent(0, false);

    _activeTouch = nil;
    _touching = NO;
    _dragging = NO;
}

@end

// ===========================================================================
// 应用入口
// ===========================================================================
@interface DMAppDelegate : UIResponder <UIApplicationDelegate>
@property (nonatomic, strong) UIWindow* window;
@end

@implementation DMAppDelegate

- (BOOL)application:(UIApplication*)application
    didFinishLaunchingWithOptions:(NSDictionary*)launchOptions {
    self.window = [[UIWindow alloc] initWithFrame:[UIScreen mainScreen].bounds];
    DMViewController* vc = [[DMViewController alloc] init];
    g_vc = vc;
    self.window.rootViewController = vc;
    [self.window makeKeyAndVisible];
    return YES;
}

@end

int main(int argc, char* argv[]) {
    @autoreleasepool {
        return UIApplicationMain(argc, argv, nil, NSStringFromClass([DMAppDelegate class]));
    }
}
