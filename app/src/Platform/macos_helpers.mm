#include <objc/runtime.h>
#include <objc/message.h>

// 设置 macOS 暗色标题栏 (类似 Qt Creator)
extern "C" void setDarkTitleBar(void *nsWindowId) {
    id nsWindow = reinterpret_cast<id (*)(id, SEL)>(objc_msgSend)(
        reinterpret_cast<id>(nsWindowId), sel_registerName("window"));
    if (!nsWindow) return;

    // 标题栏透明，透出窗口背景色
    reinterpret_cast<void (*)(id, SEL, int)>(objc_msgSend)(
        nsWindow, sel_registerName("setTitlebarAppearsTransparent:"), 1);

    // 去掉标题栏下方的分隔线
    reinterpret_cast<void (*)(id, SEL, long)>(objc_msgSend)(
        nsWindow, sel_registerName("setTitlebarSeparatorStyle:"), 0);

    // 窗口背景色 = 主题色
    Class nsColorClass = objc_getClass("NSColor");
    SEL colorSel = sel_registerName("colorWithCalibratedRed:green:blue:alpha:");
    id bgColor = reinterpret_cast<id (*)(id, SEL, double, double, double, double)>(objc_msgSend)(
        reinterpret_cast<id>(nsColorClass), colorSel,
        0.118, 0.118, 0.118, 1.0);  // #1E1E1E
    reinterpret_cast<void (*)(id, SEL, id)>(objc_msgSend)(
        nsWindow, sel_registerName("setBackgroundColor:"), bgColor);

    // 暗色外观
    Class appearanceClass = objc_getClass("NSAppearance");
    Class nsStringClass = objc_getClass("NSString");
    SEL strSel = sel_registerName("stringWithUTF8String:");
    id darkName = reinterpret_cast<id (*)(id, SEL, const char*)>(objc_msgSend)(
        reinterpret_cast<id>(nsStringClass), strSel, "NSAppearanceNameDarkAqua");
    id darkAppearance = reinterpret_cast<id (*)(id, SEL, id)>(objc_msgSend)(
        reinterpret_cast<id>(appearanceClass), sel_registerName("appearanceNamed:"), darkName);
    if (darkAppearance) {
        reinterpret_cast<void (*)(id, SEL, id)>(objc_msgSend)(
            nsWindow, sel_registerName("setAppearance:"), darkAppearance);
    }
}
