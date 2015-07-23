﻿#include "LongUI.h"

// node->Attribute\((.+?)\)
// node.attribute($1).value()

#define LONGUI_D3D_DEBUG
#define LONGUI_RENDER_IN_UNSAFE_MODE

// CUIManager 初始化
auto LongUI::CUIManager::Initialize(IUIConfigure* config) noexcept->HRESULT {
    m_szLocaleName[0] = L'\0';
    ::memset(m_apWindows, 0, sizeof(m_apWindows));
    // 检查
    if (!config) {
#ifdef LONGUI_WITH_DEFAULT_CONFIG
        config = &m_config;
#else
        return E_INVALIDARG;
#endif
    }
    // 获取信息
    force_cast(this->configure) = config;
    // 获取资源加载器
    config->QueryInterface(LongUI_IID_PV_ARGS(m_pResourceLoader));
    // 获取脚本
    config->QueryInterface(LongUI_IID_PV_ARGS(force_cast(this->script)));
    // 本地字符集名称
    config->GetLocaleName(m_szLocaleName);
    // 初始化其他
    ZeroMemory(m_apTextRenderer, sizeof(m_apTextRenderer));
    ZeroMemory(m_apSystemBrushes, sizeof(m_apSystemBrushes));
    // 获取实例句柄
    auto hInstance = ::GetModuleHandleW(nullptr);
    // 注册窗口类 | CS_DBLCLKS
    WNDCLASSEX wcex = { 0 };
    {
        wcex.cbSize = sizeof(WNDCLASSEX);
        wcex.style = CS_HREDRAW | CS_VREDRAW;
        wcex.lpfnWndProc = CUIManager::WndProc;
        wcex.cbClsExtra = 0;
        wcex.cbWndExtra = sizeof(void*);
        wcex.hInstance = hInstance;
        wcex.hCursor = nullptr;
        wcex.hbrBackground = nullptr;
        wcex.lpszMenuName = nullptr;
        wcex.lpszClassName = L"LongUIWindow";
        auto hicon = LoadIconW(hInstance, MAKEINTRESOURCEW(1));
        wcex.hIcon = hicon;
        // 注册普通窗口
        ::RegisterClassExW(&wcex);
    }
   /*{
        wcex.cbWndExtra = 0;
        wcex.lpszClassName = L"LongUIManager";
        wcex.hIcon = nullptr;
        wcex.lpfnWndProc = CUIManager::InvisibleWndProc;
        // 注册不可见窗口
        ::RegisterClassExW(&wcex);
        // 创建窗口
        m_hInvisibleHosted = ::CreateWindowExW(
            0,
            L"LongUIManager", L"LongUI UIManager Invisible Hosted Window",
            0,
            0, 0, 0, 0,
            nullptr, nullptr,
            ::GetModuleHandleW(nullptr),
            this
            );
        ::ShowWindow(m_hInvisibleHosted, SW_SHOW);
    }*/
    HRESULT hr = S_OK;
    // 位图缓存
    if (SUCCEEDED(hr)) {
        m_pBitmap0Buffer = reinterpret_cast<uint8_t*>(LongUI::CtrlAlloc(
            sizeof(RGBQUAD) * LongUIDefaultBitmapSize * LongUIDefaultBitmapSize)
            );
        // 内存不足
        if (!m_pBitmap0Buffer) {
            hr = E_OUTOFMEMORY;
        }
    }
    // 加载控件模板
    if (SUCCEEDED(hr)) {
        m_cCountCtrlTemplate = 1;
        hr = this->load_template_string(this->configure->GetTemplateString());
    }
    // 资源数据缓存
    if (SUCCEEDED(hr)) {
        // 获取缓存数据
        auto get_buffer_length = [this]() {
            size_t buffer_length =
                sizeof(void*) * m_cCountBmp +
                sizeof(void*) * m_cCountBrs +
                sizeof(void*) * m_cCountTf +
                sizeof(pugi::xml_node) * m_cCountCtrlTemplate +
                (sizeof(HICON) + sizeof(LongUI::Meta)) * m_cCountMt;
            return buffer_length;
        };
        m_cCountBmp = m_cCountBrs = m_cCountTf = m_cCountMt = 1;
        // 检查资源缓存
        if (!m_pResourceBuffer) {
            // 查询资源数量
            if (m_pResourceLoader) {
                m_cCountBmp += static_cast<decltype(m_cCountBmp)>(m_pResourceLoader->GetResourceCount(IUIResourceLoader::Type_Bitmap));
                m_cCountBrs += static_cast<decltype(m_cCountBrs)>(m_pResourceLoader->GetResourceCount(IUIResourceLoader::Type_Brush));
                m_cCountTf += static_cast<decltype(m_cCountTf)>(m_pResourceLoader->GetResourceCount(IUIResourceLoader::Type_TextFormat));
                m_cCountMt += static_cast<decltype(m_cCountMt)>(m_pResourceLoader->GetResourceCount(IUIResourceLoader::Type_Meta));
            }
            // 申请内存
            m_pResourceBuffer = LongUI::CtrlAlloc(get_buffer_length());
        }
        // 修改资源
        if (m_pResourceBuffer) {
            ::memset(m_pResourceBuffer, 0, get_buffer_length());
            m_ppBitmaps = reinterpret_cast<decltype(m_ppBitmaps)>(m_pResourceBuffer);
            m_ppBrushes = reinterpret_cast<decltype(m_ppBrushes)>(m_ppBitmaps + m_cCountBmp);
            m_ppTextFormats = reinterpret_cast<decltype(m_ppTextFormats)>(m_ppBrushes + m_cCountBrs);
            m_pMetasBuffer = reinterpret_cast<decltype(m_pMetasBuffer)>(m_ppTextFormats + m_cCountTf);
            m_phMetaIcon = reinterpret_cast<decltype(m_phMetaIcon)>(m_pMetasBuffer + m_cCountMt);
            m_pTemplateNodes = reinterpret_cast<decltype(m_pTemplateNodes)>(m_phMetaIcon + m_cCountMt);
            // 初始化
            for (auto itr = m_pTemplateNodes; itr < m_pTemplateNodes + m_cCountCtrlTemplate; ++itr) {
                LongUI::CreateObject(*itr);
            }
        }
        // 内存不足
        else {
            hr = E_OUTOFMEMORY;
        }
    }
    // 创建D2D工厂
    if (SUCCEEDED(hr)) {
        D2D1_FACTORY_OPTIONS options = { D2D1_DEBUG_LEVEL_NONE };
#ifdef _DEBUG
        options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif
        hr = LongUI::Dll::D2D1CreateFactory(
            D2D1_FACTORY_TYPE_SINGLE_THREADED,
            IID_ID2D1Factory1,
            &options,
            reinterpret_cast<void**>(&m_pd2dFactory)
            );
    }
    // 创建 DirectWrite 工厂.
    if (SUCCEEDED(hr)) {
        hr = LongUI::Dll::DWriteCreateFactory(
            DWRITE_FACTORY_TYPE_SHARED,
            LongUI_IID_PV_ARGS_Ex(m_pDWriteFactory)
            );
    }
    // 创建帮助器
    if (SUCCEEDED(hr)) {
        hr = ::CoCreateInstance(
            CLSID_DragDropHelper,
            nullptr,
            CLSCTX_INPROC_SERVER,
            LongUI_IID_PV_ARGS(m_pDropTargetHelper)
            );
    }
    // 创建字体集
    if (SUCCEEDED(hr)) {
       // m_pFontCollection = config->CreateFontCollection();
        // 失败获取系统字体集
        if (!m_pFontCollection) {
            hr = m_pDWriteFactory->GetSystemFontCollection(&m_pFontCollection);
        }
    }
    // 注册渲染器
    if (SUCCEEDED(hr)) {
        // 普通渲染器
        if (this->RegisterTextRenderer(&m_normalTRenderer) != Type_NormalTextRenderer) {
            hr = E_FAIL;
        }
    }
    // 添加控件
    if (SUCCEEDED(hr)) {
        // 添加默认控件创建函数
        this->RegisterControl(UILabel::CreateControl, L"Label");
        this->RegisterControl(UIButton::CreateControl, L"Button");
        this->RegisterControl(UIVerticalLayout::CreateControl, L"VerticalLayout");
        this->RegisterControl(UIHorizontalLayout::CreateControl, L"HorizontalLayout");
        this->RegisterControl(UISlider::CreateControl, L"Slider");
        this->RegisterControl(UICheckBox::CreateControl, L"CheckBox");
        this->RegisterControl(UIRichEdit::CreateControl, L"RichEdit");
        this->RegisterControl(UIScrollBarA::CreateControl, L"ScrollBarA");
        this->RegisterControl(UIEditBasic::CreateControl, L"EditBasic");
        this->RegisterControl(UIEditBasic::CreateControl, L"Edit");
        // 添加自定义控件
        config->AddCustomControl();
    }
    // 创建资源
    if (SUCCEEDED(hr)) {
        hr = this->RecreateResources();
    }
    // 检查错误
    if (FAILED(hr)) {
        this->ShowError(hr);
    }
    else {
        this->do_creating_event(LongUI::CreateEventType::Type_Initialize);
    }
    return hr;
}


// CUIManager  反初始化
void LongUI::CUIManager::UnInitialize() noexcept {
    this->do_creating_event(LongUI::CreateEventType::Type_Uninitialize);
    // 释放文本渲染器
    for (auto& renderer : m_apTextRenderer) {
        ::SafeRelease(renderer);
    }
    // 释放公共设备无关资源
    {
        // 释放文本格式
        for (auto itr = m_ppTextFormats; itr != m_ppTextFormats + m_cCountTf; ++itr) {
            ::SafeRelease(*itr);
        }
        // 摧毁META图标
        for (auto itr = m_phMetaIcon; itr != m_phMetaIcon + m_cCountMt; ++itr) {
            if (*itr) {
                ::DestroyIcon(*itr);
                *itr = nullptr;
            }
        }
        // 控件模板
        for (auto itr = m_pTemplateNodes; itr != m_pTemplateNodes + m_cCountCtrlTemplate; ++itr) {
            if (*itr) {
                LongUI::DestoryObject(*itr);
            }
        }
    }
    // 释放设备相关资源
    this->discard_resources();
    // 释放资源
    ::SafeRelease(m_pFontCollection);
    ::SafeRelease(m_pDWriteFactory);
    ::SafeRelease(m_pDropTargetHelper);
    ::SafeRelease(m_pd2dFactory);
    // 释放内存
    if (m_pBitmap0Buffer) {
        LongUI::CtrlFree(m_pBitmap0Buffer);
        m_pBitmap0Buffer = nullptr;
    }
    // 释放脚本
    ::SafeRelease(force_cast(script));
    // 释放资源缓存
    if (m_pResourceBuffer) {
        LongUI::CtrlFree(m_pResourceBuffer);
        m_pResourceBuffer = nullptr;
    }
    // 释放读取器
    ::SafeRelease(m_pResourceLoader);
    // 释放配置
    ::SafeRelease(force_cast(this->configure));
    m_cCountMt = m_cCountTf = m_cCountBmp = m_cCountBrs = 0;
}

// 创建UI窗口
auto LongUI::CUIManager::CreateUIWindow(
    const char* xml, UIWindow* parent, CreateUIWindowCallBack call, void* user_data
    )noexcept->UIWindow* {
    assert(xml && call && "bad arguments");
    // 载入字符串
    auto code = m_docWindow.load_string(xml);
    // 检查错误
    if (code.status) {
        wchar_t buffer[LongUIStringBufferLength];
        buffer[LongUI::UTF8toWideChar(code.description(), buffer)] = L'\0';
        this->ShowError(buffer);
        return nullptr;
    }
    // 创建
    return this->CreateUIWindow(m_docWindow.first_child(), parent, call, user_data);
}

// 利用XML节点创建UI窗口
auto LongUI::CUIManager::CreateUIWindow(
    const pugi::xml_node node, UIWindow* parent, CreateUIWindowCallBack call, void* user_data
    ) noexcept -> UIWindow* {
    assert(node && call && "bad arguments");
    // 有效情况
    if (call && node) {
        // 创建窗口
        auto window = call(node, parent, user_data);
        // 查错
        assert(window); if (!window) return nullptr;
        // 重建资源
        auto hr = window->Recreate(m_pd2dDeviceContext);
        AssertHR(hr);
        // 创建控件树
        this->make_control_tree(window, node);
        // 完成创建
        LongUI::EventArgument arg; ::memset(&arg, 0, sizeof(arg));
        arg.sender = window;
        arg.event = LongUI::Event::Event_TreeBulidingFinished;
        window->DoEvent(arg);
        // 返回
        return window;
    }
    return nullptr;
}


// 创建事件
void LongUI::CUIManager::do_creating_event(CreateEventType type) noexcept {
    assert(type != LongUI::Type_CreateControl);
    try {
        for (const auto& pair : m_mapString2CreateFunction) {
            reinterpret_cast<CreateControlFunction>(pair.second)(type, LongUINullXMLNode);
        }
    }
    catch (...) {
        assert(!"some error");
    }
}


// CUIManager 创建控件树
// 默认消耗 64kb+, 导致栈(默认1~2M)溢出几率较低
void LongUI::CUIManager::make_control_tree(LongUI::UIWindow* window, pugi::xml_node node) noexcept {
    // 断言
    assert(window && node && "bad argument");
    // 添加窗口
    //add_control(window, node);
    // 队列 -- 顺序遍历树
    LongUI::FixedCirQueue<pugi::xml_node, LongUIMaxControlInited> xml_queue;
    LongUI::FixedCirQueue<UIContainer*, LongUIMaxControlInited> parents_queue;
    // 
    UIControl* now_control = nullptr;
    UIContainer* parent_node = window;
    // 唯一名称
    std::pair<CUIString, void*> control_name;
    // 遍历算法: 1.压入所有子节点 2.依次弹出 3.重复1
    while (true) {
        // 压入/入队 所有子节点
        node = node.first_child();
        while (node) {
            xml_queue.push(node);
            parents_queue.push(parent_node);
            node = node.next_sibling();
        }
    recheck:
        // 为空则退出
        if (xml_queue.empty()) break;
        // 弹出/出队 第一个节点
        node = *xml_queue.front;  xml_queue.pop();
        parent_node = *parents_queue.front; parents_queue.pop();
        // 根据名称创建控件
        if (!(now_control = this->CreateControl(node, nullptr))) {
            parent_node = nullptr;
#ifdef _DEBUG
            const char* node_name = node.name();
            UIManager << DL_Error << L" Control Class Not Found: " << node_name << LongUI::endl;
#endif
            continue;
        }
        // 添加到表
        if (UIControl::MakeString(node.attribute("name").value(), control_name.first)) {
            control_name.second = now_control;
            window->AddControl(control_name);
        }
        // 添加子节点
        parent_node->insert(parent_node->end(), now_control);
        // 设置节点为下次父节点
        parent_node = static_cast<decltype(parent_node)>(now_control);
        // 检查本控件是否需要XML子节点信息
        if (now_control->flags & Flag_ControlNeedFullXMLNode) {
            goto recheck;
        }
    }
}

// 获取创建控件函数指针
auto LongUI::CUIManager::GetCreateFunc(const char* class_name) noexcept -> CreateControlFunction {
    // 缓冲区
    wchar_t buffer[LongUIStringBufferLength];
    auto* __restrict itra = class_name; auto* __restrict itrb = buffer;
    // 类名一定是英文的
    for (; *itra; ++itra, ++itrb) {
        assert(*itra >= 0 && "bad name");
        *itrb = *itra;
    }
    // null 结尾字符串
    *itrb = L'\0';
    // 获取
    return this->GetCreateFunc(buffer, itra - class_name);
}

// 获取创建控件函数指针
auto LongUI::CUIManager::GetCreateFunc(const CUIString& name) noexcept -> CreateControlFunction {
    // 查找
    try {
        const auto itr = m_mapString2CreateFunction.find(name);
        if (itr != m_mapString2CreateFunction.end()) {
            return reinterpret_cast<CreateControlFunction>(itr->second);
        }
    }
    catch (...)  {

    }
    return nullptr;
}

/*
if (::PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
    // 两种方式退出 
    if (msg.message == WM_QUIT) {
        m_exitFlag = true;
        break;
    }
    ::TranslateMessage(&msg);
    ::DispatchMessageW(&msg);
}
*/


// 消息循环
void LongUI::CUIManager::Run() noexcept {
    MSG msg;
    // 创建渲染线程
    std::thread thread([]() noexcept {
        UIWindow* windows[LongUIMaxWindow]; uint32_t length = 0;
        // 不退出?
        while (!UIManager.m_exitFlag) {
            // 复制
            UIManager.Lock();
            length = UIManager.m_cCountWindow;
            // 没有窗口
            if (!length) { UIManager.Unlock(); ::Sleep(20); continue; }
            for (auto i = 0u; i < length; ++i) {
                windows[i] = static_cast<UIWindow*>(UIManager.m_apWindows[i]);
            }
            // 刷新窗口
            for (auto i = 0u; i < length; ++i) {
                windows[i]->Update();
                windows[i]->NextFrame();
                if (!windows[i]->IsRendered()) {
                    windows[i] = nullptr;
                }
            }
#ifdef LONGUI_RENDER_IN_UNSAFE_MODE
            UIManager.Unlock();
#endif
            UIWindow* waitvs_window = nullptr;
            // 渲染窗口
            for (auto i = 0u; i < length; ++i) {
                if (windows[i]) {
                    waitvs_window = windows[i];
                    windows[i]->RenderWindow();
                }
            }
#ifndef LONGUI_RENDER_IN_UNSAFE_MODE
            UIManager.Unlock();
#endif
            // 等待垂直同步
            UIManager.WaitVS(waitvs_window);
        }
        // 关闭不可见窗口
        //::DestroyWindow(UIManager.m_hInvisibleHosted);
    });
    // 消息响应
    while (::GetMessageW(&msg, nullptr, 0, 0)) {
        ::TranslateMessage(&msg);
        ::DispatchMessageW(&msg);
    }
    m_exitFlag = true;
    // 等待线程
    try {
        if (thread.joinable()) {
            thread.join();
        }
    }
    catch (...) {

    }
    // 尝试强行关闭
    if (m_cCountWindow) {
        UIWindow* windows[LongUIMaxWindow];
        ::memcpy(windows, m_apWindows, sizeof(m_apWindows));
        auto count = m_cCountWindow;
        // 清理窗口
        for (auto i = 0u; i < count; ++i) {
            windows[count - i - 1]->Cleanup();
        }
    }
    assert(!m_cCountWindow && "bad");
    m_cCountWindow = 0;
}

// 等待垂直同步
auto LongUI::CUIManager::WaitVS(UIWindow* window) noexcept ->void {
    // UIManager << DL_Hint << window << endl;
    // 直接等待
    if (window) {
        window->WaitVS();
        m_dwWaitVSCount = 0;
        m_dwWaitVSStartTime = 0;
    }
    // 粗略等待
    else {
        if (!m_dwWaitVSCount) {
            m_dwWaitVSStartTime = ::timeGetTime();
        }
        // 获取屏幕刷新率
        DEVMODEW mode = { 0 };
        ::EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &mode);
        ++m_dwWaitVSCount;
        auto end_time_of_sleep = m_dwWaitVSCount * 1000 / mode.dmDisplayFrequency + 1;
        end_time_of_sleep += m_dwWaitVSStartTime;
        do { ::Sleep(1); } while (::timeGetTime() < end_time_of_sleep);
    }
}

// 利用现有资源创建控件
auto LongUI::CUIManager::create_control(CreateControlFunction function, pugi::xml_node node, size_t id) noexcept -> UIControl * {
    // 检查参数W
    if (!function) {
        if (node) {
            function = this->GetCreateFunc(node.name());
        }
        else if (id) {

        }
        else {
            assert(!"ERROR!!");
            return nullptr;
        }
    }
    if (node) {
        id = static_cast<decltype(id)>(LongUI::AtoI(node.attribute("templateid").value()));
    }
    // 利用id查找模板控件
    if (id) {

    }
    assert(function && "bad idea");
    return function(CreateEventType::Type_CreateControl, node);
}



// 窗口过程函数
LRESULT LongUI::CUIManager::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) noexcept {
    LRESULT recode = 0;
    // 创建窗口时设置指针
    if (message == WM_CREATE)    {
        // 获取指针
        LongUI::UIWindow *pUIWindow = reinterpret_cast<LongUI::UIWindow*>(
            (reinterpret_cast<LPCREATESTRUCT>(lParam))->lpCreateParams
            );
        // 设置窗口指针
        ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, PtrToUlong(pUIWindow));
        // 创建完毕
        pUIWindow->OnCreated(hwnd);
        // 返回1
        recode = 1;
    }
    else {
        // 设置参数
        LongUI::EventArgument arg;
        // 系统消息
        arg.msg = message;  arg.sender = nullptr;
        // 世界鼠标坐标
        POINT pt; ::GetCursorPos(&pt); ::ScreenToClient(hwnd, &pt);
        arg.pt.x = static_cast<float>(pt.x); arg.pt.y = static_cast<float>(pt.y);
        // 参数
        arg.wParam_sys = wParam; arg.lParam_sys = lParam;
        arg.lr = 0;
        // 获取储存的指针
        auto* pUIWindow = reinterpret_cast<LongUI::UIWindow *>(static_cast<LONG_PTR>(
            ::GetWindowLongPtrW(hwnd, GWLP_USERDATA))
            );
        // 检查是否处理了
        bool wasHandled = false;
        //指针有效的情况
        if (pUIWindow) {
            AutoLocker;
            wasHandled = pUIWindow->DoEvent(arg);
        }
        // 默认处理
        recode = wasHandled ? arg.lr : ::DefWindowProcW(hwnd, message, wParam, lParam);
    }
    return recode;
}

// 获取主题颜色
auto LongUI::CUIManager::GetThemeColor(D2D1_COLOR_F& colorf) noexcept -> HRESULT {
    union { DWORD color; uint8_t argb[4]; };
    color = DWORD(-1); auto hr = S_OK; DWORD buffer_size = sizeof(DWORD);
    // 获取Colorization颜色
    ::RegGetValueA(
        HKEY_CURRENT_USER,
        "Software\\Microsoft\\Windows\\DWM", "ColorizationColor",
        RRF_RT_DWORD, nullptr, &color, &buffer_size
        );
    DWORD balance = 50; buffer_size = sizeof(DWORD);
    // 获取Colorization混合标准
    ::RegGetValueA(
        HKEY_CURRENT_USER,
        "Software\\Microsoft\\Windows\\DWM", "ColorizationColorBalance",
        RRF_RT_DWORD, nullptr, &balance, &buffer_size
        );
    {
        // 混合通道
        auto blend_channel = [](float ch1, float ch2, float prec) {
            register auto data = ch1 + (ch2 - ch1) * prec;
            return data > 1.f ? 1.f : (data < 0.f ? 0.f : data);
        };
        colorf.a = 1.f; auto prec = 1.f - float(balance) / 100.f;
        constexpr float basegrey = float(217) / 255.f;
        colorf.r = blend_channel(float(argb[2]) / 255.f, basegrey, prec);
        colorf.g = blend_channel(float(argb[1]) / 255.f, basegrey, prec);
        colorf.b = blend_channel(float(argb[0]) / 255.f, basegrey, prec);
    }
    return hr;
}

// 默认创建窗口
auto LongUI::CUIManager::CreateLongUIWIndow(pugi::xml_node node, UIWindow* parent, void* user_data) noexcept -> UIWindow * {
    UNREFERENCED_PARAMETER(user_data);
    auto ctrl = LongUI::UIControl::AllocRealControl<LongUI::UIWindow>(
        node, 
        [=](void* p) noexcept { new(p) UIWindow(node, parent); }
        );
    assert(ctrl);
    return static_cast<UIWindow*>(ctrl);
}


// 获取操作系统版本
namespace LongUI { auto GetWindowsVersion() noexcept->CUIManager::WindowsVersion; }

// CUIManager 构造函数
LongUI::CUIManager::CUIManager() noexcept : 
m_config(*this), 
version(LongUI::GetWindowsVersion()) {

}

// CUIManager 析构函数
LongUI::CUIManager::~CUIManager() noexcept {
    this->discard_resources();
}

// 获取控件 wchar_t指针
auto LongUI::CUIManager::
RegisterControl(CreateControlFunction func, const wchar_t* name) noexcept ->HRESULT {
    if (!name || !(*name)) return S_FALSE;
    // 超过了容器限制
    if (m_mapString2CreateFunction.size() >= LongUIMaxControlClass) {
        assert(!"out of sapce for control");
        return E_ABORT;
    }
    // 创建pair
    std::pair<LongUI::CUIString, CreateControlFunction> pair(name, func);
    HRESULT hr = S_OK;
    // 插入
    try {
        m_mapString2CreateFunction.insert(pair);
    }
    // 创建失败
    CATCH_HRESULT(hr);
    return hr;
}

// 显示错误代码
void LongUI::CUIManager::ShowError(HRESULT hr, const wchar_t* str_b) noexcept {
    wchar_t buffer[LongUIStringBufferLength];
    if (!::FormatMessageW(
        FORMAT_MESSAGE_FROM_SYSTEM,
        nullptr,  hr,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
        buffer,
        lengthof(buffer),
        nullptr)) {
        // 处理
        ::swprintf(
            buffer, LongUIStringBufferLength,
            L"Error! HRESULT Code: 0x%08X",
            hr
            );
    }
    // 错误
    this->ShowError(buffer, str_b);
}

// 创建LongUI的字体集: 本函数会进行I/O, 所以程序开始调用一次即可
auto LongUI::CUIManager::CreateLongUIFontCollection(
    IDWriteFactory* factory, const wchar_t * filename, const wchar_t * folder)
    noexcept -> IDWriteFontCollection* {
    // 字体文件枚举
    class LongUIFontFileEnumerator final : public ComStatic<QiList<IDWriteFontFileEnumerator>> {
    public:
        // 获取当前字体文件
        HRESULT STDMETHODCALLTYPE GetCurrentFontFile(IDWriteFontFile **ppFontFile) noexcept override  {
            if (!ppFontFile) return E_INVALIDARG;
            if (!m_pFilePath || !m_pFactory)  return E_FAIL;
            *ppFontFile = ::SafeAcquire(m_pCurFontFie);
            return m_pCurFontFie ? S_OK : E_FAIL;
        }
        // 移动到下一个文件
        HRESULT STDMETHODCALLTYPE MoveNext(BOOL *pHasCurrentFile) noexcept override {
            if (!pHasCurrentFile)return E_INVALIDARG;
            if (!m_pFilePath || !m_pFactory) return E_FAIL;
            HRESULT hr = S_OK;
            if (*pHasCurrentFile = *m_pFilePathNow) {
                ::SafeRelease(m_pCurFontFie);
                hr = m_pFactory->CreateFontFileReference(m_pFilePathNow, nullptr, &m_pCurFontFie);
                if (*pHasCurrentFile = SUCCEEDED(hr)) {
                    m_pFilePathNow += ::wcslen(m_pFilePathNow);
                    ++m_pFilePathNow;
                }
            }
            return hr;
        }
    public:
        // 构造函数
        LongUIFontFileEnumerator(IDWriteFactory* f) :m_pFactory(::SafeAcquire(f)) {}
        // 析构函数
        ~LongUIFontFileEnumerator() { ::SafeRelease(m_pCurFontFie); ::SafeRelease(m_pFactory); }
        // 初始化
        auto Initialize(const wchar_t* path) { m_pFilePathNow = m_pFilePath = path; };
    private:
        // 文件路径 连续字符串
        const wchar_t*              m_pFilePath = nullptr;
        // 当前文件路径
        const wchar_t*              m_pFilePathNow = nullptr;
        // 当前Direct Write Font File
        IDWriteFontFile*            m_pCurFontFie = nullptr;
        // DWrite 工厂
        IDWriteFactory*             m_pFactory;
    };
    // 字体文件载入器
    class LongUIFontCollectionLoader final : public ComStatic<QiList<IDWriteFontCollectionLoader>> {
    public:
        // 创建枚举器
        HRESULT STDMETHODCALLTYPE CreateEnumeratorFromKey(
            IDWriteFactory *pFactory,
            const void *collectionKey,
            UINT32 collectionKeySize,
            IDWriteFontFileEnumerator **ppFontFileEnumerator
            ) noexcept override {
            UNREFERENCED_PARAMETER(collectionKeySize);
            if (!pFactory || !ppFontFileEnumerator) return E_INVALIDARG;
            m_enumerator.LongUIFontFileEnumerator::~LongUIFontFileEnumerator();
            m_enumerator.LongUIFontFileEnumerator::LongUIFontFileEnumerator(pFactory);
            m_enumerator.Initialize(reinterpret_cast<const wchar_t*>(collectionKey));
            *ppFontFileEnumerator = &m_enumerator;
            return S_OK;
        }
    public:
        // 构造函数
        LongUIFontCollectionLoader() :m_enumerator(nullptr) {}
        // 析构函数
        ~LongUIFontCollectionLoader() = default;
    private:
        // 枚举器
        LongUIFontFileEnumerator        m_enumerator;
    };
    IDWriteFontCollection* collection = nullptr;
    constexpr size_t buffer_length = 256 * 256;
    // 申请足够的空间
    wchar_t* const buffer(new(std::nothrow) wchar_t[buffer_length]);
    if (buffer) {
        wchar_t* index = buffer; *buffer = 0;
        WIN32_FIND_DATA fileinfo;
        wchar_t file_name_path[MAX_PATH]; ::swprintf(file_name_path, MAX_PATH, L"%ls\\%ls", folder, filename);
        HANDLE hFile = ::FindFirstFileW(file_name_path, &fileinfo);
        DWORD errorcode = ::GetLastError();
        // 遍历文件
        while (hFile != INVALID_HANDLE_VALUE && errorcode != ERROR_NO_MORE_FILES) {
            ::swprintf(index, MAX_PATH, L"%ls\\%ls", folder, fileinfo.cFileName);
            index += ::wcslen(index) + 1; *index = 0;
            if (index + MAX_PATH >= buffer + buffer_length) {
                break;
            }
            ::FindNextFileW(hFile, &fileinfo);
            errorcode = ::GetLastError();
        }
        ::FindClose(hFile);
        // 当存在符合标准的文件时
        if (index != buffer) {
            LongUIFontCollectionLoader loader;
            factory->RegisterFontCollectionLoader(&loader);
            factory->CreateCustomFontCollection(
                &loader,
                buffer, reinterpret_cast<uint8_t*>(index) - reinterpret_cast<uint8_t*>(buffer),
                &collection
                );
            factory->UnregisterFontCollectionLoader(&loader);
        }
        delete[] buffer;
    }
    return collection;
}

// 从 文本格式创建几何
auto LongUI::CUIManager::CreateTextPathGeometry(
    IN const char32_t* utf32_string,
    IN size_t string_length,
    IN IDWriteTextFormat* format,
    IN ID2D1Factory* factory,
    IN OUT OPTIONAL IDWriteFontFace** _fontface,
    OUT ID2D1PathGeometry** geometry
    ) noexcept -> HRESULT {
    // 参数检查
    assert(utf32_string && string_length && format && geometry && factory && "bad arugments");
    if (!utf32_string || !string_length || !format || !geometry || !factory) return E_INVALIDARG;
    // 字体集
    IDWriteFontCollection* collection = nullptr;
    IDWriteFontFamily* family = nullptr;
    IDWriteFont* font = nullptr;
    IDWriteFontFace* fontface = nullptr;
    ID2D1PathGeometry* pathgeometry = nullptr;
    if (_fontface) fontface = ::SafeAcquire(*_fontface);
    // 字体名称缓存
    wchar_t fontname_buffer[MAX_PATH]; *fontname_buffer = 0;
    // 必要缓存
    uint16_t glyph_indices_buffer[1024];
    // 保证空间
    uint16_t* glyph_indices = string_length > lengthof(glyph_indices_buffer) ?
        new(std::nothrow) uint16_t[string_length * sizeof(uint16_t)] : glyph_indices_buffer;
    HRESULT hr = glyph_indices ? S_OK : E_OUTOFMEMORY;
    // 创建字形
    if (!fontface) {
        // 获取字体名称
        if (SUCCEEDED(hr)) {
            hr = format->GetFontFamilyName(fontname_buffer, MAX_PATH);
        }
        // 获取字体集
        if (SUCCEEDED(hr)) {
            hr = format->GetFontCollection(&collection);
        }
        // 查找索引
        uint32_t index = 0; BOOL exists = FALSE;
        if (SUCCEEDED(hr)) {
            hr = collection->FindFamilyName(fontname_buffer, &index, &exists);
        }
        // 获取字体族
        if (SUCCEEDED(hr)) {
            // 不存在也算错误
            if (exists) {
                hr = collection->GetFontFamily(index, &family);
            }
            else {
                hr = E_FAIL;
            }
        }
        // 获取字体
        if (SUCCEEDED(hr)) {
            hr = family->GetFirstMatchingFont(
                format->GetFontWeight(),
                format->GetFontStretch(),
                format->GetFontStyle(),
                &font
                );
        }
        // 创建字形
        if (SUCCEEDED(hr)) {
            hr = font->CreateFontFace(&fontface);
        }
    }
    // 创建几何
    if (SUCCEEDED(hr)) {
        hr = factory->CreatePathGeometry(&pathgeometry);
        ID2D1GeometrySink* sink = nullptr;
        // 打开Sink
        if (SUCCEEDED(hr)) {
            hr = pathgeometry->Open(&sink);
        }
        // 创建索引编号
        if (SUCCEEDED(hr)) {
            static_assert(sizeof(uint32_t) == sizeof(char32_t), "32 != 32 ?!");
            hr = fontface->GetGlyphIndices(
                reinterpret_cast<const uint32_t*>(utf32_string), string_length, glyph_indices
                );
        }
        // 创建轮廓路径几何
        if (SUCCEEDED(hr)) {
            hr = fontface->GetGlyphRunOutline(
                format->GetFontSize(),
                glyph_indices,
                nullptr, nullptr,
                string_length,
                true, true, sink
                );
        }
        // 关闭路径
        if (SUCCEEDED(hr)) {
            sink->Close();
        }
        ::SafeRelease(sink);
    }
    // 扫尾
    ::SafeRelease(collection);
    ::SafeRelease(family);
    ::SafeRelease(font);
    if (_fontface && !(*_fontface)) {
        *_fontface = fontface;
    }
    else {
        ::SafeRelease(fontface);
    }
    if (glyph_indices && glyph_indices != glyph_indices_buffer) {
        delete[] glyph_indices;
        glyph_indices = nullptr;
    }
    *geometry = pathgeometry;
#ifdef _DEBUG
    if (pathgeometry) {
        float float_var = 0.f;
        pathgeometry->ComputeLength(nullptr, &float_var);
        pathgeometry->ComputeArea(nullptr, &float_var);
        float_var = 0.f;
    }
#endif
    return hr;
}

// 利用几何体创建网格
auto LongUI::CUIManager::CreateMeshFromGeometry(ID2D1Geometry* geometry, ID2D1Mesh** mesh) noexcept -> HRESULT {
    UNREFERENCED_PARAMETER(geometry);
    UNREFERENCED_PARAMETER(mesh);
    return E_NOTIMPL;
}



// 转换为Core-Mode格式
auto LongUI::CUIManager::XMLToCoreFormat(const char* xml, wchar_t* core) noexcept -> bool {
    if (!xml || !core) return false;
    wchar_t buffer[LongUIStringBufferLength];
    *buffer = 0;
    return true;
}


// 注册文本渲染器
auto LongUI::CUIManager::RegisterTextRenderer(
    UIBasicTextRenderer* renderer) noexcept -> int32_t {
    if (m_uTextRenderCount == lengthof(m_apTextRenderer)) {
        return -1;
    }
    register const auto count = m_uTextRenderCount;
    m_apTextRenderer[count] = ::SafeAcquire(renderer);
    ++m_uTextRenderCount;
    return count;
}


// 创建0索引资源
auto LongUI::CUIManager::create_indexzero_resources() noexcept->HRESULT {
    assert(m_pResourceBuffer && "bad alloc");
    HRESULT hr = S_OK;
    // 索引0位图: 可MAP位图
    if (SUCCEEDED(hr)) {
        hr = m_pd2dDeviceContext->CreateBitmap(
            D2D1::SizeU(LongUIDefaultBitmapSize, LongUIDefaultBitmapSize),
            nullptr, LongUIDefaultBitmapSize * 4,
            D2D1::BitmapProperties1(
                static_cast<D2D1_BITMAP_OPTIONS>(LongUIDefaultBitmapOptions),
                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
                ),
            m_ppBitmaps + LongUIDefaultBitmapIndex
            );
    }
    // 索引0笔刷: 全控件共享用前写纯色笔刷
    if (SUCCEEDED(hr)) {
        ID2D1SolidColorBrush* brush = nullptr;
        D2D1_COLOR_F color = D2D1::ColorF(D2D1::ColorF::Black);
        hr = m_pd2dDeviceContext->CreateSolidColorBrush(&color, nullptr, &brush);
        m_ppBrushes[LongUICommonSolidColorBrushIndex] = ::SafeAcquire(brush);
        ::SafeRelease(brush);
    }
    // 索引0文本格式: 默认格式
    if (SUCCEEDED(hr)) {
        hr = m_pDWriteFactory->CreateTextFormat(
            LongUIDefaultTextFontName,
            m_pFontCollection,
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            LongUIDefaultTextFontSize,
            m_szLocaleName,
            m_ppTextFormats + LongUIDefaultTextFormatIndex
            );
    }
    // 设置
    if (SUCCEEDED(hr)) {
        m_ppTextFormats[LongUIDefaultTextFormatIndex]->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        m_ppTextFormats[LongUIDefaultTextFormatIndex]->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    }
    // 索引0META: 暂无
    if (SUCCEEDED(hr)) {

    }
    return hr;
}


// 载入模板字符串
auto LongUI::CUIManager::load_template_string(const char* str) noexcept->HRESULT {
    // 检查参数
    if (str && *str) {
        // 载入字符串
        auto code = m_docTemplate.load_string(str);
        if (code.status) {
            assert(!"load error");
            ::MessageBoxA(nullptr, code.description(), "<LongUI::CUIManager::load_template_string>: Failed to Parse/Load XML", MB_ICONERROR);
            return E_FAIL;
        }
        // 解析
        return E_NOTIMPL;
    }
    else {
        return S_FALSE;
    }
}


// UIManager 创建设备相关资源
auto LongUI::CUIManager::create_device_resources() noexcept ->HRESULT {
    // 检查渲染配置
    bool cpu_rendering = this->configure->IsRenderByCPU();
    // 待用适配器
    IDXGIAdapter1* ready2use = nullptr;
    // 枚举显示适配器
    if(!cpu_rendering) {
        IDXGIFactory1* temp_factory = nullptr;
        // 创建一个临时工程
        register auto hr = LongUI::Dll::CreateDXGIFactory1(IID_IDXGIFactory1, reinterpret_cast<void**>(&temp_factory));
        if (SUCCEEDED(hr)) {
            IDXGIAdapter1* apAdapters[256]; size_t adnum;
            // 枚举适配器
            for (adnum = 0; adnum < lengthof(apAdapters); ++adnum) {
                if (temp_factory->EnumAdapters1(adnum, apAdapters + adnum) == DXGI_ERROR_NOT_FOUND) {
                    break;
                }
            }
            // 选择适配器
            register auto index = this->configure->ChooseAdapter(apAdapters, adnum);
            if (index < adnum) {
                ready2use = ::SafeAcquire(apAdapters[index]);
            }
            // 释放适配器
            for (size_t i = 0; i < adnum; ++i) {
                ::SafeRelease(apAdapters[i]);
            }
        }
        ::SafeRelease(temp_factory);
    }
    // 创建设备资源
    register HRESULT hr /*= m_docResource.Error() ? E_FAIL :*/ S_OK;
    // 创建 D3D11设备与设备上下文 
    if (SUCCEEDED(hr)) {
        // D3D11 创建flag 
        // 一定要有D3D11_CREATE_DEVICE_BGRA_SUPPORT
        // 否则创建D2D设备上下文会失败
        UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if defined(_DEBUG) && defined(LONGUI_D3D_DEBUG)
        // Debug状态 有D3D DebugLayer就可以取消注释
        creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
        auto tmpflag = D3D11_CREATE_DEVICE_FLAG(creationFlags);
#endif
        D3D_FEATURE_LEVEL featureLevels[] = {
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0,
            D3D_FEATURE_LEVEL_9_3,
            D3D_FEATURE_LEVEL_9_2,
            D3D_FEATURE_LEVEL_9_1
        };
        // 创建设备
        hr = LongUI::Dll::D3D11CreateDevice(
            // 设置为渲染
            ready2use,
            // 根据情况选择类型
            cpu_rendering ? D3D_DRIVER_TYPE_WARP : 
                (ready2use ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE),
            // 没有软件接口
            nullptr,
            // 创建flag
            creationFlags,
            // 欲使用的特性等级列表
            featureLevels,
            // 特性等级列表长度
            lengthof(featureLevels),
            // SDK 版本
            D3D11_SDK_VERSION,
            // 返回的D3D11设备指针
            &m_pd3dDevice,
            // 返回的特性等级
            &m_featureLevel,
            // 返回的D3D11设备上下文指针
            &m_pd3dDeviceContext
            );
        // 检查
        if (FAILED(hr)) {
            UIManager << DL_Hint << L"Create D3D11 Device Failed,"
                L" Now, Try to Create In WARP Mode" << LongUI::endl;
        }
        // 创建失败则尝试使用软件
        if (FAILED(hr)) {
            hr = LongUI::Dll::D3D11CreateDevice(
                // 设置为渲染
                nullptr,
                // 根据情况选择类型
                D3D_DRIVER_TYPE_WARP,
                // 没有软件接口
                nullptr,
                // 创建flag
                creationFlags,
                // 欲使用的特性等级列表
                featureLevels,
                // 特性等级列表长度
                lengthof(featureLevels),
                // SDK 版本
                D3D11_SDK_VERSION,
                // 返回的D3D11设备指针
                &m_pd3dDevice,
                // 返回的特性等级
                &m_featureLevel,
                // 返回的D3D11设备上下文指针
                &m_pd3dDeviceContext
                );
        }
    }
#if defined(_DEBUG) && defined(LONGUI_D3D_DEBUG)
    // 创建 ID3D11Debug
    if (SUCCEEDED(hr)) {
        hr = m_pd3dDevice->QueryInterface(LongUI_IID_PV_ARGS(m_pd3dDebug));
    }
#endif
    // 创建 IDXGIDevice
    if (SUCCEEDED(hr)) {
        hr = m_pd3dDevice->QueryInterface(LongUI_IID_PV_ARGS(m_pDxgiDevice));
    }
    // 创建D2D设备
    if (SUCCEEDED(hr)) {
        hr = m_pd2dFactory->CreateDevice(m_pDxgiDevice, &m_pd2dDevice);
    }
    // 创建D2D设备上下文
    if (SUCCEEDED(hr)) {
        hr = m_pd2dDevice->CreateDeviceContext(
            D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
            &m_pd2dDeviceContext
            );
    }
    // 获取Dxgi适配器 可以获取该适配器信息
    if (SUCCEEDED(hr)) {
        // 顺带使用像素作为单位
        m_pd2dDeviceContext->SetUnitMode(D2D1_UNIT_MODE_PIXELS);
        hr = m_pDxgiDevice->GetAdapter(&m_pDxgiAdapter);
    }
#ifdef _DEBUG
    // 输出显卡信息
    if (SUCCEEDED(hr)) {
        DXGI_ADAPTER_DESC desc = { 0 }; 
        m_pDxgiAdapter->GetDesc(&desc);
        UIManager << DL_Log << desc << LongUI::endl;
    }
#endif
    // 获取Dxgi工厂
    if (SUCCEEDED(hr)) {
        hr = m_pDxgiAdapter->GetParent(LongUI_IID_PV_ARGS(m_pDxgiFactory));
    }
#ifdef LONGUI_VIDEO_IN_MF
    uint32_t token = 0;
    // 多线程
    if (SUCCEEDED(hr)) {
        ID3D10Multithread* mt = nullptr;
        hr = m_pd3dDevice->QueryInterface(IID_ID3D10Multithread, (void**)&mt);
        // 保护
        if (SUCCEEDED(hr)) {
            mt->SetMultithreadProtected(TRUE);
        }
        ::SafeRelease(mt);
    }
    // 设置MF
    if (SUCCEEDED(hr)) {
        hr = ::MFStartup(MF_VERSION);
    }
    // 创建MF Dxgi 设备管理器
    if (SUCCEEDED(hr)) {
        hr = ::MFCreateDXGIDeviceManager(&token, &m_pDXGIManager);
    }
    // 重置设备
    if (SUCCEEDED(hr)) {
        hr = m_pDXGIManager->ResetDevice(m_pd3dDevice, token);
    }
    // 创建MF媒体类工厂
    if (SUCCEEDED(hr)) {
        hr = CoCreateInstance(
            CLSID_MFMediaEngineClassFactory,
            nullptr,
            CLSCTX_INPROC_SERVER,
            LongUI_IID_PV_ARGS(m_pMediaEngineFactory)
            );
    }
#endif
    // 禁止 Alt + Enter 全屏
    /*if (SUCCEEDED(hr)) {
        hr = m_pDxgiFactory->MakeWindowAssociation(m_hwnd, DXGI_MWA_NO_WINDOW_CHANGES | DXGI_MWA_NO_ALT_ENTER);
    }*/
    // 创建系统笔刷
    if (SUCCEEDED(hr)) {
        hr = this->create_system_brushes();
    }
    // 创建资源描述资源
    if (SUCCEEDED(hr)) {
        hr = this->create_indexzero_resources();
    }
    ::SafeRelease(ready2use);
    // 事件
    if (SUCCEEDED(hr)) {
        this->do_creating_event(LongUI::CreateEventType::Type_Recreate);
    }
    // 设置文本渲染器数据
    if (SUCCEEDED(hr)) {
        for (uint32_t i = 0u; i < m_uTextRenderCount; ++i) {
            m_apTextRenderer[i]->SetNewRT(m_pd2dDeviceContext);
            m_apTextRenderer[i]->SetNewBrush(
                static_cast<ID2D1SolidColorBrush*>(m_ppBrushes[LongUICommonSolidColorBrushIndex])
                );
        }
        // 重建所有窗口
        for (auto itr = m_apWindows; itr < m_apWindows + m_cCountWindow; ++itr) {
            (*itr)->Recreate(m_pd2dDeviceContext);
        }
    }
    // 断言 HR
    AssertHR(hr);
    return hr;
}


// 创建系统笔刷
auto LongUI::CUIManager::create_system_brushes() noexcept -> HRESULT {
    HRESULT hr = S_OK;
    /*
    焦点: 0x3399FF 矩形描边, 并且内边有虚线矩形
        0. 禁用: 0xD9灰度 矩形描边; 中心 0xEF灰色
        1. 普通: 0xAC灰度 矩形描边; 中心 从上到下0xF0灰色到0xE5灰色渐变
        2. 移上: 0x7EB4EA 矩形描边; 中心 从上到下0xECF4FC到0xDCECFC渐变
        3. 按下: 0x569DE5 矩形描边; 中心 从上到下0xDAECFC到0xC4E0FC渐变
    */
    // 禁用
    if (SUCCEEDED(hr)) {
        hr = m_pd2dDeviceContext->CreateSolidColorBrush(
            D2D1::ColorF(0xEFEFEF),
            reinterpret_cast<ID2D1SolidColorBrush**>(m_apSystemBrushes + Status_Disabled)
            );
    }
    // 普通
    if (SUCCEEDED(hr)) {
        ID2D1GradientStopCollection* collection = nullptr;
        D2D1_GRADIENT_STOP stops[] = {
            { 0.f, D2D1::ColorF(0xF0F0F0) },
            { 1.f, D2D1::ColorF(0xE5E5E5) }
        };
        // 渐变关键点集
        if (SUCCEEDED(hr)) {
            hr = m_pd2dDeviceContext->CreateGradientStopCollection(
                stops, lengthof(stops), &collection
                );
        }
        // 创建笔刷
        if (SUCCEEDED(hr)) {
            hr = m_pd2dDeviceContext->CreateLinearGradientBrush(
                D2D1::LinearGradientBrushProperties(
                    D2D1::Point2F(), D2D1::Point2F(0.f, 1.f)
                    ),
                collection,
                reinterpret_cast<ID2D1LinearGradientBrush**>(m_apSystemBrushes + Status_Normal)
                );
        }
        ::SafeRelease(collection);
    }
    // 移上
    if (SUCCEEDED(hr)) {
        ID2D1GradientStopCollection* collection = nullptr;
        D2D1_GRADIENT_STOP stops[] = {
            { 0.f, D2D1::ColorF(0xECF4FC) },
            { 1.f, D2D1::ColorF(0xDCECFC) }
        };
        // 渐变关键点集
        if (SUCCEEDED(hr)) {
            hr = m_pd2dDeviceContext->CreateGradientStopCollection(
                stops, lengthof(stops), &collection
                );
        }
        // 创建笔刷
        if (SUCCEEDED(hr)) {
            hr = m_pd2dDeviceContext->CreateLinearGradientBrush(
                D2D1::LinearGradientBrushProperties(
                    D2D1::Point2F(), D2D1::Point2F(0.f, 1.f)
                    ),
                collection,
                reinterpret_cast<ID2D1LinearGradientBrush**>(m_apSystemBrushes + Status_Hover)
                );
        }
        ::SafeRelease(collection);
    }
    // 按下
    if (SUCCEEDED(hr)) {
        ID2D1GradientStopCollection* collection = nullptr;
        D2D1_GRADIENT_STOP stops[] = {
            { 0.f, D2D1::ColorF(0xDAECFC) } ,
            { 1.f, D2D1::ColorF(0xC4E0FC) } ,
        };
        // 渐变关键点集
        if (SUCCEEDED(hr)) {
            hr = m_pd2dDeviceContext->CreateGradientStopCollection(
                stops, lengthof(stops), &collection
                );
        }
        // 创建笔刷
        if (SUCCEEDED(hr)) {
            hr = m_pd2dDeviceContext->CreateLinearGradientBrush(
                D2D1::LinearGradientBrushProperties(
                    D2D1::Point2F(), D2D1::Point2F(0.f, 1.f)
                    ),
                collection,
                reinterpret_cast<ID2D1LinearGradientBrush**>(m_apSystemBrushes + Status_Pushed)
                );
        }
        ::SafeRelease(collection);
    }
    return hr;
}

// UIManager 丢弃
void LongUI::CUIManager::discard_resources() noexcept {
    // 释放系统笔刷
    for (auto& brush : m_apSystemBrushes) {
        ::SafeRelease(brush);
    }
    // 释放公共设备相关资源
    {
        // 释放 位图
        for (auto itr = m_ppBitmaps; itr != m_ppBitmaps + m_cCountBmp; ++itr) {
            ::SafeRelease(*itr);
        }
        // 释放 笔刷
        for (auto itr = m_ppBrushes; itr != m_ppBrushes + m_cCountBrs; ++itr) {
            ::SafeRelease(*itr);
        }
        // META
        for (auto itr = m_pMetasBuffer; itr != m_pMetasBuffer + m_cCountMt; ++itr) {
            LongUI::DestoryObject(*itr);
            itr->bitmap = nullptr;
        }
    }
    // 释放 设备
    ::SafeRelease(m_pDxgiFactory);
    ::SafeRelease(m_pd2dDeviceContext);
    ::SafeRelease(m_pd2dDevice);
    ::SafeRelease(m_pDxgiAdapter);
    ::SafeRelease(m_pDxgiDevice);
    ::SafeRelease(m_pd3dDevice);
    ::SafeRelease(m_pd3dDeviceContext);
#ifdef LONGUI_VIDEO_IN_MF
    ::SafeRelease(m_pDXGIManager);
    ::SafeRelease(m_pMediaEngineFactory);
    ::MFShutdown();
#endif
#ifdef _DEBUG
    if (m_pd3dDebug) {
        m_pd3dDebug->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);
    }
    ::SafeRelease(m_pd3dDebug);
#endif
}

// 获取位图
auto LongUI::CUIManager::GetBitmap(size_t index) noexcept ->ID2D1Bitmap1* {
    // 越界
    if (index >= m_cCountBmp) {
        UIManager << DL_Warning 
            << L"[index @ " << long(index)
            << L"]is out of range \t\tNow set to 0" 
            << LongUI::endl;
        index = 0;
    }
    auto bitmap = m_ppBitmaps[index];
    // 没有数据则载入
    if (!bitmap) {
        // 没有数据并且没有资源加载器则?
        assert(m_pResourceLoader);
        // 载入资源
        m_ppBitmaps[index] = static_cast<ID2D1Bitmap1*>(
            m_pResourceLoader->GetResourcePointer(m_pResourceLoader->Type_Bitmap, index - 1)
            );
        bitmap = m_ppBitmaps[index];
    }
    // 再没有数据则报错
    if (!bitmap) {
        UIManager << DL_Error << L"index @ " << long(index) << L"bitmap is null" << LongUI::endl;
    }
    return ::SafeAcquire(bitmap);
}

// 获取笔刷
auto LongUI::CUIManager::GetBrush(size_t index) noexcept -> ID2D1Brush* {
    // 越界
    if (index >= m_cCountBrs) {
        UIManager << DL_Warning
            << L"[index @ " << long(index)
            << L"]is out of range \t\tNow set to 0"
            << LongUI::endl;
        index = 0;
    }
    auto brush = m_ppBrushes[index];
    // 没有数据则载入
    if (!brush) {
        // 没有数据并且没有资源加载器则?
        assert(m_pResourceLoader);
        // 载入资源
        m_ppBrushes[index] = static_cast<ID2D1Brush*>(
            m_pResourceLoader->GetResourcePointer(m_pResourceLoader->Type_Brush, index - 1)
            );
        brush = m_ppBrushes[index];
    }
    // 再没有数据则报错
    if (!brush) {
        UIManager << DL_Error << L"index @ " << long(index) << L"brush is null" << LongUI::endl;
    }
    return ::SafeAcquire(brush);
}

// CUIManager 获取文本格式
auto LongUI::CUIManager::GetTextFormat(size_t index) noexcept ->IDWriteTextFormat* {
    // 越界
    if (index >= m_cCountTf) {
        UIManager << DL_Warning
            << L"[index @ " << long(index)
            << L"]is out of range \t\tNow set to 0"
            << LongUI::endl;
        index = 0;
    }
    auto format = m_ppTextFormats[index];
    // 没有数据则载入
    if (!format) {
        // 没有数据并且没有资源加载器则?
        assert(m_pResourceLoader);
        // 载入资源
        m_ppTextFormats[index] = static_cast<IDWriteTextFormat*>(
            m_pResourceLoader->GetResourcePointer(m_pResourceLoader->Type_TextFormat, index - 1)
            );
        format = m_ppTextFormats[index];
    }
    // 再没有数据则报错
    if (!format) {
        UIManager << DL_Error << L"index @ " << long(index) << L"text format is null" << LongUI::endl;
    }
    return ::SafeAcquire(format);
}

// 获取图元
void LongUI::CUIManager::GetMeta(size_t index, LongUI::Meta& meta) noexcept {
    // 越界
    if (index >= m_cCountMt) {
        UIManager << DL_Warning
            << L"[index @ " << long(index)
            << L"]is out of range \t\tNow set to 0"
            << LongUI::endl;
        index = 0;
        ZeroMemory(&meta, sizeof(meta));
        return;
    }
    meta = m_pMetasBuffer[index];
    // 没有位图数据则载入
    if (!meta.bitmap) {
        // 没有数据并且没有资源加载器则?
        assert(m_pResourceLoader);
        DeviceIndependentMeta meta_raw;
        ::ZeroMemory(&meta_raw, sizeof(meta_raw));
        // 载入资源
        m_pResourceLoader->GetMeta(index - 1, meta_raw);
        meta.interpolation = meta_raw.interpolation;
        meta.src_rect = meta_raw.src_rect;
        meta.rule = meta_raw.rule;
        meta.bitmap = this->GetBitmap(meta_raw.bitmap_index);
        // 减少计数
        if (meta.bitmap) {
            meta.bitmap->Release();
        }
    }
    // 再没有数据则报错
    if (!meta.bitmap) {
        UIManager << DL_Error << L"index @ " << long(index) << L"meta is null" << LongUI::endl;
    }
}

// 获取Meta的图标句柄
auto LongUI::CUIManager::GetMetaHICON(size_t index) noexcept -> HICON {
    // 越界
    if (index >= m_cCountMt) {
        UIManager << DL_Warning
            << L"[index @ " << long(index)
            << L"]is out of range \t\tNow set to 0"
            << LongUI::endl;
        index = 0;
    }
    // 有就直接返回
    if (m_phMetaIcon[index]) return m_phMetaIcon[index];
    LongUI::Meta meta; this->GetMeta(index, meta);
    assert(meta.bitmap);
    ID2D1Bitmap1* bitmap = this->GetBitmap(LongUIDefaultBitmapIndex);
    D2D1_RECT_U src_rect = {
        static_cast<uint32_t>(meta.src_rect.left),
        static_cast<uint32_t>(meta.src_rect.top),
        static_cast<uint32_t>(meta.src_rect.right),
        static_cast<uint32_t>(meta.src_rect.bottom)
    };
    HRESULT hr = S_OK;
    // 宽度不够?
    if (SUCCEEDED(hr)) {
        if (src_rect.right - src_rect.left > LongUIDefaultBitmapSize ||
            src_rect.bottom - src_rect.top > LongUIDefaultBitmapSize) {
            assert(!"width/height is too large");
            hr = E_FAIL;
        }
    }
    // 检查错误
    if (SUCCEEDED(hr)) {
        if (!(bitmap && meta.bitmap)) {
            hr = E_POINTER;
        }
    }
    // 复制数据
    if (SUCCEEDED(hr)) {
        hr = bitmap->CopyFromBitmap(nullptr, meta.bitmap, &src_rect);
    }
    // 映射数据
    if (SUCCEEDED(hr)) {
        D2D1_MAPPED_RECT mapped_rect = {
            LongUIDefaultBitmapSize * sizeof(RGBQUAD) ,
            m_pBitmap0Buffer
        };
        hr = bitmap->Map(D2D1_MAP_OPTIONS_READ, &mapped_rect);
    }
    // 取消映射
    if (SUCCEEDED(hr)) {
        hr = bitmap->Unmap();
    }
    // 转换数据
    HICON hAlphaIcon = nullptr;
    if (SUCCEEDED(hr)) {
        auto meta_width = src_rect.right - src_rect.left;
        auto meta_height = src_rect.bottom - src_rect.top;
#if 1
        BITMAPV5HEADER bi; ZeroMemory(&bi, sizeof(BITMAPV5HEADER));
        bi.bV5Size = sizeof(BITMAPV5HEADER);
        bi.bV5Width = meta_width;
        bi.bV5Height = meta_height;
        bi.bV5Planes = 1;
        bi.bV5BitCount = 32;
        bi.bV5Compression = BI_BITFIELDS;
        // 掩码填写
        bi.bV5RedMask = 0x00FF0000;
        bi.bV5GreenMask = 0x0000FF00;
        bi.bV5BlueMask = 0x000000FF;
        bi.bV5AlphaMask = 0xFF000000;
        HDC hdc = ::GetDC(nullptr);
        uint8_t* pTargetBuffer = nullptr;
        // 创建带Alpha通道DIB
        auto hBitmap = ::CreateDIBSection(
            hdc, reinterpret_cast<BITMAPINFO*>(&bi), DIB_RGB_COLORS,
            reinterpret_cast<void **>(&pTargetBuffer), nullptr,
            (DWORD)0
            );
        auto hMemDC = ::CreateCompatibleDC(hdc);
        ::ReleaseDC(nullptr, hdc);
        // 写入数据
        auto hOldBitmap = static_cast<HBITMAP*>(::SelectObject(hMemDC, hBitmap));
        ::PatBlt(hMemDC, 0, 0, meta_width, meta_height, WHITENESS);
        ::SelectObject(hMemDC, hOldBitmap);
        ::DeleteDC(hMemDC);
        // 创建掩码位图
        HBITMAP hMonoBitmap = ::CreateBitmap(meta_width, meta_height, 1, 1, nullptr);
        // 输入
        auto lpdwPixel = reinterpret_cast<DWORD*>(pTargetBuffer);
        for (auto y = 0u; y < meta_height; ++y) {
            auto src_buffer = m_pBitmap0Buffer + LongUIDefaultBitmapSize * sizeof(RGBQUAD) * y;
            for (auto x = 0u; x < meta_width; ++x) {
                *lpdwPixel = *src_buffer;
                src_buffer++;
                lpdwPixel++;
            }
        }
        // 填写
        ICONINFO ii;
        ii.fIcon = TRUE; ii.xHotspot = 0; ii.yHotspot = 0;
        ii.hbmMask = hMonoBitmap; ii.hbmColor = hBitmap;
        // 创建图标
        hAlphaIcon = ::CreateIconIndirect(&ii);
        ::DeleteObject(hBitmap);
        ::DeleteObject(hMonoBitmap);
#else
        assert(!"CreateIcon just AND & XOR, no alpha channel")
#endif
    }
    AssertHR(hr);
    ::SafeRelease(bitmap);
    return m_phMetaIcon[index] = hAlphaIcon;
}


// 格式化文字
/*
control char    C-Type      Infomation                                  StringInlineParamSupported
  
%%               [none]      As '%' Character(like %% in ::printf)                 ---
%a %A      [const wchar_t*] string add(like %S in ::printf)                Yes but no "," char 

%C              [float4*]    new font color range start                            Yes
%c              [uint32_t]   new font color range start, with alpha                Yes
!! color is also a drawing effect

%d %D         [IUnknown*]    new drawing effect range start                 Yes and Extensible

%S %S            [float]     new font size range start                             Yes

%n %N       [const wchar_t*] new font family name range start               Yes but No "," char

%h %H            [enum]      new font stretch range start                          Yes

%y %Y            [enum]      new font style range start                            Yes

%w %W            [enum]      new font weight range start                           Yes

%u %U            [BOOL]      new underline range start                          Yes(0 or 1)

%e %E            [BOOL]      new strikethrough range start                      Yes(0 or 1)

%i %I            [IDIO*]     new inline object range start                  Yes and Extensible

%] %}            [none]      end of the last range                                 ---

//  Unsupported
%f %F   [UNSPT]  [IDFC*]     new font collection range start                       ---
                                IDWriteFontCollection*

%t %T   [UNSPT]  [IDT*]      new inline object range start                         ---
                                IDWriteTypography*

%t %T   [UNSPT] [char_t*]    new locale name range start                           ---

FORMAT IN STRING
the va_list(ap) can be nullptr while string format
include the PARAMETERS,
using %p or %P to mark PARAMETERS start

*/

// 创建格式文本
auto LongUI::CUIManager::FormatTextCore(
    FormatTextConfig& config,
    const wchar_t* format,
    ...) noexcept->IDWriteTextLayout* {
    va_list ap;
    va_start(ap, format);
    return CUIManager::FormatTextCore(config, format, ap);
}

// find next param
template<typename T>
auto __fastcall FindNextToken(T* buffer, const wchar_t* stream, size_t token_num) {
    register wchar_t ch;
    while ((ch = *stream)) {
        ++stream;
        if (ch == L',' && !(--token_num)) {
            break;
        }
        *buffer = static_cast<T>(ch);
        ++buffer;
    }
    *buffer = 0;
    return stream;
}


#define CUIManager_GetNextTokenW(n) param = FindNextToken(param_buffer, param, n)
#define CUIManager_GetNextTokenA(n) param = FindNextToken(reinterpret_cast<char*>(param_buffer), param, n)


// 创建格式文本
// 效率本函数耗时参考:
// 包含释放数据(::SafeRelease(layout))
// 1. L"%cHello%], world!%p#FFFF0000"
// Debug    : 循环 1000000(一百万)次，耗时8750ms(精确到16ms)
// Release  : 循环 1000000(一百万)次，耗时3484ms(精确到16ms)
// 2. L"%cHello%], world!%cHello%], world!%p#FFFF0000, #FF00FF00"
// Debug    : 循环 1000000(一百万)次，耗时13922ms(精确到16ms)
// Release  : 循环 1000000(一百万)次，耗时 6812ms(精确到16ms)
// 结论: Release版每处理一个字符(包括格式与参数)平均消耗0.12微秒, Debug版加倍
// 假设: 60Hz每帧16ms 拿出8ms处理本函数, 可以处理6万6个字符
//一般论: 不可能每帧调用6万字, 一般可能每帧处理数百字符(忙碌时), 可以忽略不计

auto  LongUI::CUIManager::FormatTextCore(
    FormatTextConfig& config, const wchar_t* format, va_list ap
    ) noexcept->IDWriteTextLayout* {
    const wchar_t* param = nullptr;
    // 检查是否带参数
    if (!ap) {
        register auto format_param_tmp = format;
        register wchar_t ch;
        while (ch = *format_param_tmp) {
            if (ch == L'%') {
                ++format_param_tmp;
                ch = *format_param_tmp;
                if (ch == L'p' || ch == L'p') {
                    param = format_param_tmp + 1;
                    break;
                }
            }
            ++format_param_tmp;
        }
        assert(param && "ap set to nullptr, but none param found.");
    }
    // Range Type
    enum class R : size_t { N, W, Y, H, S, U, E, D, I };
    // Range Data
    struct RangeData {
        DWRITE_TEXT_RANGE       range;
        union {
            const wchar_t*      name;       // N
            DWRITE_FONT_WEIGHT  weight;     // W
            DWRITE_FONT_STYLE   style;      // Y
            DWRITE_FONT_STRETCH stretch;    // H
            float               size;       // S
            BOOL                underline;  // U
            BOOL                strikethr;  // E
            IUnknown*           draweffect; // D
            IDWriteInlineObject*inlineobj;  // I
            // ----------------------------
            D2D1_COLOR_F*       color;      // C
            uint32_t            u32;        // c
        };
        R                       range_type;
    } range_data;
    ::memset(&range_data, 0, sizeof(range_data));
    assert(format && "bad argument");
    IDWriteTextLayout* layout = nullptr;
    register UIColorEffect* tmp_color = nullptr;
    // 缓存字符串长度
    uint32_t string_length = 0;
    // 当前字符
    wchar_t ch;
    // 缓冲区索引
    wchar_t* buffer_index;
    // 参数缓冲区
    wchar_t param_buffer[256];
    // 缓冲区
    wchar_t buffer[LongUIStringBufferLength];
    // 缓冲区
    wchar_t fontname_buffer[LongUIStringBufferLength];
    auto fontname_buffer_index = fontname_buffer;
    // 使用栈
    FixedStack<RangeData, 1024> stack_check, statck_set;
    // 缓存起点
    buffer_index = buffer;
    // 便利
    while (ch = *format) {
        // 为%时, 检查下一字符
        if (ch == L'%' && (++format, ch = *format)) {
            switch (ch)
            {
            case L'%':
                // 添加%
                *buffer_index = L'%';
                ++buffer_index;
                ++string_length;
                break;
            case L'A': case L'a': // [A]dd string
                // 复制字符串
                {
                    register const wchar_t* i;
                    if (ap) {
                        i = va_arg(ap, const wchar_t*);
                    }
                    else {
                        CUIManager_GetNextTokenW(1);
                        i = param_buffer;
                    }
                    for (; *i; ++i) {
                        *buffer_index = *i;
                        ++string_length;
                        ++range_data.name;
                    }
                }
                break;
            case L'C': // [C]olor in float4
                // 浮点数组颜色开始标记: 
                range_data.range.startPosition = string_length;
                if (ap) {
                    range_data.color = va_arg(ap, D2D1_COLOR_F*);
                }
                range_data.range_type = R::D;
                // 动态创建颜色效果
                tmp_color = UIColorEffect::Create();
                assert(tmp_color && "C");
                // 从范围数据中获取
                if (ap) {
                    tmp_color->color = *range_data.color;
                }
                // 直接设置
                else {
                    CUIManager_GetNextTokenA(4);
                    UIControl::MakeColor(reinterpret_cast<char*>(param_buffer), tmp_color->color);
                }
                range_data.draweffect = tmp_color;
                stack_check.push(range_data);
                break;
            case L'c': // [C]olor in uint32
                // 32位颜色开始标记: 
                range_data.range.startPosition = string_length;
                if (ap) {
                    range_data.u32 = va_arg(ap, uint32_t);
                }
                range_data.range_type = R::D;
                // 动态创建颜色效果
                tmp_color = UIColorEffect::Create();
                assert(tmp_color && "c");
                if (ap) {
                    LongUI::UnpackTheColorARGB(range_data.u32, tmp_color->color);
                }
                else {
                    CUIManager_GetNextTokenA(1);
                    UIControl::MakeColor(reinterpret_cast<char*>(param_buffer), tmp_color->color);
                }
                range_data.draweffect = tmp_color;
                stack_check.push(range_data);
                break;
            case 'D': case 'd': // [D]rawing effect
                range_data.range.startPosition = string_length;
                if (ap) {
                    range_data.draweffect = va_arg(ap, IUnknown*);
                }
                else {
                    CUIManager_GetNextTokenW(1);
                    IUnknown* result = nullptr;
                    if (config.inline_handler) {
                        result = config.inline_handler(0, param_buffer);
                    }
                    range_data.draweffect = result;
                }
                range_data.range_type = R::D;
                stack_check.push(range_data);
                break;
            case 'E': case 'e': // strik[E]through
                range_data.range.startPosition = string_length;
                if (ap) {
                    range_data.strikethr = va_arg(ap, BOOL);
                }
                else {
                    CUIManager_GetNextTokenA(1);
                    range_data.strikethr = static_cast<BOOL>(
                        LongUI::AtoI(reinterpret_cast<char*>(param_buffer))
                        );
                }
                range_data.range_type = R::E;
                stack_check.push(range_data);
                break;
            case 'H': case 'h': // stretc[H]
                range_data.range.startPosition = string_length;
                if (ap) {
                    range_data.stretch = va_arg(ap, DWRITE_FONT_STRETCH);
                }
                else {
                    CUIManager_GetNextTokenA(1);
                    range_data.stretch = static_cast<DWRITE_FONT_STRETCH>(
                        LongUI::AtoI(reinterpret_cast<char*>(param_buffer))
                        );
                }
                range_data.range_type = R::H;
                stack_check.push(range_data);
                break;
            case 'I': case 'i': // [I]nline object
                range_data.range.startPosition = string_length;
                if (ap) {
                    range_data.inlineobj = va_arg(ap, IDWriteInlineObject*);
                }
                else {
                    CUIManager_GetNextTokenW(1);
                    IDWriteInlineObject* result = nullptr;
                    if (config.inline_handler) {
                        result = static_cast<IDWriteInlineObject*>(
                            config.inline_handler(0, param_buffer)
                            );
                    }
                    range_data.inlineobj = result;
                }
                range_data.range_type = R::I;
                stack_check.push(range_data);
                break;
            case 'N': case 'n': // family [N]ame
                range_data.range.startPosition = string_length;
                if (ap) {
                    range_data.name = va_arg(ap, const wchar_t*);
                }
                else {
                    // 复制字体名称 并去除前后空白
                    register wchar_t now_ch;
                    auto param_buffer_index = param_buffer;
                    wchar_t* last_firststart_while = nullptr;
                    const wchar_t* firststart_notwhile = nullptr;
                    bool nameless = true;
                    while (now_ch = *param_buffer) {
                        *fontname_buffer_index = now_ch;
                        if (nameless && (now_ch == L' ' || now_ch == L'\t')) {
                            last_firststart_while = fontname_buffer_index;
                            nameless = false;
                        }
                        else {
                            nameless = true;
                            if (!firststart_notwhile) {
                                param_buffer_index = fontname_buffer_index;
                            }
                        }
                        ++fontname_buffer_index;
                    }
                    *last_firststart_while = 0;
                    fontname_buffer_index = last_firststart_while + 1;
                    range_data.name = firststart_notwhile;
                }
                range_data.range_type = R::N;
                stack_check.push(range_data);
                break;
            case 'S': case 's': // [S]ize
                range_data.range.startPosition = string_length;
                if (ap) {
                    range_data.size = va_arg(ap, float);
                }
                else {
                    CUIManager_GetNextTokenA(1);
                    range_data.size = LongUI::AtoF(
                        reinterpret_cast<char*>(param_buffer)
                        );
                }
                range_data.range_type = R::S;
                stack_check.push(range_data);
                break;
            case 'U': case 'u': // [U]nderline
                range_data.range.startPosition = string_length;
                if (ap) {
                    range_data.underline = va_arg(ap, BOOL);
                }
                else {
                    CUIManager_GetNextTokenA(1);
                    range_data.underline = static_cast<BOOL>(
                        LongUI::AtoI(reinterpret_cast<char*>(param_buffer))
                        );
                }
                range_data.range_type = R::U;
                stack_check.push(range_data);
                break;
            case 'W': case 'w': // [W]eight
                range_data.range.startPosition = string_length;
                if (ap) {
                    range_data.weight = va_arg(ap, DWRITE_FONT_WEIGHT);
                }
                else {
                    CUIManager_GetNextTokenA(1);
                    range_data.weight = static_cast<DWRITE_FONT_WEIGHT>(
                        LongUI::AtoI(reinterpret_cast<char*>(param_buffer))
                        );
                }
                range_data.range_type = R::W;
                stack_check.push(range_data);
                break;
            case L'Y': case L'y': // st[Y]le
                range_data.range.startPosition = string_length;
                if (ap) {
                    range_data.style = va_arg(ap, DWRITE_FONT_STYLE);
                }
                else {
                    CUIManager_GetNextTokenA(1);
                    range_data.style = static_cast<DWRITE_FONT_STYLE>(
                        LongUI::AtoI(reinterpret_cast<char*>(param_buffer))
                        );
                }
                range_data.range_type = R::Y;
                stack_check.push(range_data);
                break;
            case L'P': case L'p': // end of main string, then, is the param
                goto force_break;
            case L']': case L'}': // end of all range type
                // 检查栈弹出
                stack_check.pop();
                // 计算长度
                stack_check.top->range.length = string_length - stack_check.top->range.startPosition;
                // 压入设置栈
                statck_set.push(*stack_check.top);
                break;
            }
        }
        // 添加
        else {
            *buffer_index = ch;
            ++buffer_index;
            ++string_length;
        }
        ++format;
    }
force_break:
    // 尾巴0
    *buffer_index = 0;
    // 计算长度
    assert(string_length < lengthof(buffer));
    // 计算需要长度
    config.text_length = string_length;
    register auto string_length_need = static_cast<uint32_t>(static_cast<float>(string_length + 1) * config.progress);
    LongUIClamp(string_length_need, 0, string_length);
    // 修正
    va_end(ap);
    // 创建布局
    if (config.dw_factory && SUCCEEDED(config.dw_factory->CreateTextLayout(
        buffer, string_length_need, config.text_format, config.width, config.height,&layout
        ))) {
        // 创建
        while (!statck_set.empty()) {
            statck_set.pop();
            // 检查进度(progress)范围 释放数据
            if (statck_set.top->range.startPosition 
                + statck_set.top->range.length > string_length_need) {
                if (statck_set.top->range_type == R::D || statck_set.top->range_type == R::I) {
                    ::SafeRelease(statck_set.top->draweffect);
                }
                continue;
            };
            // enum class R :size_t { N, W, Y, H, S, U, E, D, I };
            switch (statck_set.top->range_type)
            {
            case R::N:
                layout->SetFontFamilyName(statck_set.top->name, statck_set.top->range);
                break;
            case R::W:
                layout->SetFontWeight(statck_set.top->weight, statck_set.top->range);
                break;
            case R::Y:
                layout->SetFontStyle(statck_set.top->style, statck_set.top->range);
                break;
            case R::H:
                layout->SetFontStretch(statck_set.top->stretch, statck_set.top->range);
                break;
            case R::S:
                layout->SetFontSize(statck_set.top->size, statck_set.top->range);
                break;
            case R::U:
                layout->SetUnderline(statck_set.top->underline, statck_set.top->range);
                break;
            case R::E:
                layout->SetStrikethrough(statck_set.top->strikethr, statck_set.top->range);
                break;
            case R::D:
                layout->SetDrawingEffect(statck_set.top->draweffect, statck_set.top->range);
                break;
            case R::I:
                layout->SetInlineObject(statck_set.top->inlineobj, statck_set.top->range);
                break;
            }
        }
    }
    return layout;
}


// 添加窗口
void LongUI::CUIManager::RegisterWindow(UIWindow * wnd) noexcept {
    assert(wnd && "bad argument");
    // 检查剩余空间
    if (m_cCountWindow >= LongUIMaxWindow) {
        assert(!"ABORT! OUT OF SPACE! m_cCountWindow >= LongUIMaxWindow");
        return;
    }
    // 检查是否已经存在
#ifdef _DEBUG
    {
        auto endwindow = m_apWindows + m_cCountWindow;
        if (std::find(m_apWindows, endwindow, wnd) != endwindow) {
            assert(!"target window has been registered.");
        }
    }
#endif
    // 添加窗口
    m_apWindows[m_cCountWindow] = wnd; ++m_cCountWindow;
}

// 移出窗口
void LongUI::CUIManager::RemoveWindow(UIWindow * wnd, bool cleanup) noexcept {
    assert(m_cCountWindow); assert(wnd && "bad argument");
    // 清理?
    if (cleanup) {
        wnd->Cleanup();
#ifdef _DEBUG
        // 现在已经不再数组中了, 不过需要检查一下
        auto endwindow = m_apWindows + m_cCountWindow;
        if (std::find(m_apWindows, endwindow, wnd) != endwindow) {
            assert(!"remove window failed!");
        }
#endif
        return;
    }
    // 检查时是不是在本数组中
#ifdef _DEBUG
    {
        auto endwindow = m_apWindows + m_cCountWindow;
        if (std::find(m_apWindows, endwindow, wnd) == endwindow) {
            assert(!"target window not in windows array!");
            return;
        }
    }
#endif
    // 一次循环就搞定
    {
        const register auto count = m_cCountWindow;
        bool found = false;
        for (auto i = 0u; i < m_cCountWindow; ++i) {
            // 找到后, 后面的元素依次前移
            if (found) {
                m_apWindows[i] = m_apWindows[i + 1];
            }
            // 没找到就尝试
            else if(m_apWindows[i] == wnd) {
                found = true;
                m_apWindows[i] = m_apWindows[i + 1];
            }
        }
        assert(found && "window not found");
        --m_cCountWindow;
        m_cCountWindow[m_apWindows] = nullptr;
    }
}

// 是否以管理员权限运行
bool LongUI::CUIManager::IsRunAsAdministrator() noexcept {
    BOOL fIsRunAsAdmin = FALSE;
    PSID pAdministratorsGroup = nullptr;
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    ::AllocateAndInitializeSid(
        &NtAuthority,
        2,
        SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS,
        0, 0, 0, 0, 0, 0,
        &pAdministratorsGroup
    );
    // 成功? 清理资源
    if (pAdministratorsGroup) {
        ::CheckTokenMembership(nullptr, pAdministratorsGroup, &fIsRunAsAdmin);
        ::FreeSid(pAdministratorsGroup);
        pAdministratorsGroup = nullptr;
    }
    // 返回结果
    return fIsRunAsAdmin != 0;
}

// 提升权限
bool LongUI::CUIManager::TryElevateUACNow(const wchar_t* parameters, bool exit) noexcept {
    if (!CUIManager::IsRunAsAdministrator()) {
        wchar_t szPath[MAX_PATH];
        // 获取实例句柄
        if (::GetModuleFileNameW(NULL, szPath, ARRAYSIZE(szPath))) {
            // Launch itself as admin
            SHELLEXECUTEINFO sei = { 0 };
            sei.cbSize = sizeof(sei);
            sei.lpVerb = L"runas";
            sei.lpFile = szPath;
            sei.lpParameters = parameters;
            sei.hwnd = nullptr;
            sei.nShow = SW_NORMAL;
            // 执行
            if (!::ShellExecuteExW(&sei)) {
#ifdef _DEBUG
                DWORD dwError = ::GetLastError();
                assert(dwError == ERROR_CANCELLED && "anyelse?");
#endif
                return false;
            }
            else if(exit) {
                // 退出
                UIManager.Exit();
            }
        }
    }
    return true;
}

//#include <valarray>

#ifdef _DEBUG

// 换行刷新重载
auto LongUI::CUIManager::operator<<(const LongUI::EndL) noexcept ->CUIManager& {
    wchar_t chs[3] = { L'\r',L'\n', 0 }; 
    this->Output(m_lastLevel, chs);
    return *this;
}

auto LongUI::CUIManager::operator<<(const DXGI_ADAPTER_DESC& desc) noexcept->CUIManager& {
    wchar_t buffer[LongUIStringBufferLength];
    ::swprintf(
        buffer, LongUIStringBufferLength,
        L"Adapter:   { \n\t Description: %ls\n\t VendorId:0x%08X"
        L"\t\t DeviceId:0x%08X\n\t SubSysId:0x%08X\t\t Revision:0x%08X\n"
        L"\t DedicatedVideoMemory: %.3lfMB\n"
        L"\t DedicatedSystemMemory: %.3lfMB\n"
        L"\t SharedSystemMemory: %.3lfMB\n"
        L"\t AdapterLuid: 0x%08X--%08X\n }",
        desc.Description,
        desc.VendorId,
        desc.DeviceId,
        desc.SubSysId,
        desc.Revision,
        static_cast<double>(desc.DedicatedVideoMemory) / (1024.*1024.),
        static_cast<double>(desc.DedicatedSystemMemory) / (1024.*1024.),
        static_cast<double>(desc.SharedSystemMemory) / (1024.*1024.),
        desc.AdapterLuid.HighPart,
        desc.AdapterLuid.LowPart
        );
    this->OutputNoFlush(m_lastLevel, buffer);
    return *this;
}

auto LongUI::CUIManager::operator<<(const RectLTWH_F& rect) noexcept->CUIManager& {
    wchar_t buffer[LongUIStringBufferLength];
    ::swprintf(
        buffer, LongUIStringBufferLength,
        L"RECT_WH(%7.2f, %7.2f, %7.2f, %7.2f)",
        rect.left, rect.top, rect.width, rect.height
        );
    this->OutputNoFlush(m_lastLevel, buffer);
    return *this;
}

auto LongUI::CUIManager::operator<<(const D2D1_MATRIX_3X2_F& matrix) noexcept->CUIManager& {
    wchar_t buffer[LongUIStringBufferLength];
    ::swprintf(
        buffer, LongUIStringBufferLength,
        L"MATRIX(%7.2f, %7.2f, %7.2f, %7.2f, %7.2f, %7.2f)",
        matrix._11, matrix._12, 
        matrix._21, matrix._22, 
        matrix._31, matrix._32
        );
    this->OutputNoFlush(m_lastLevel, buffer);
    return *this;
}

auto LongUI::CUIManager::operator<<(const D2D1_RECT_F& rect) noexcept->CUIManager& {
    wchar_t buffer[LongUIStringBufferLength];
    ::swprintf(
        buffer, LongUIStringBufferLength,
        L"RECT_RB(%7.2f, %7.2f, %7.2f, %7.2f)",
        rect.left, rect.top, rect.right, rect.bottom
        );
    this->OutputNoFlush(m_lastLevel, buffer);
    return *this;
}

auto LongUI::CUIManager::operator<<(const D2D1_POINT_2F& pt) noexcept->CUIManager& {
    wchar_t buffer[LongUIStringBufferLength];
    ::swprintf(
        buffer, LongUIStringBufferLength,
        L"POINT(%7.2f, %7.2f)",
        pt.x, pt.y
        );
    this->OutputNoFlush(m_lastLevel, buffer);
    return *this;
}

// 输出UTF-8字符串 并刷新
void LongUI::CUIManager::Output(DebugStringLevel l, const char * s) noexcept {
    wchar_t buffer[LongUIStringBufferLength];
    buffer[LongUI::UTF8toWideChar(s, buffer)] = 0;
    this->Output(l, buffer);
}

// 输出UTF-8字符串
void LongUI::CUIManager::OutputNoFlush(DebugStringLevel l, const char * s) noexcept {
    wchar_t buffer[LongUIStringBufferLength];
    buffer[LongUI::UTF8toWideChar(s, buffer)] = 0;
    this->OutputNoFlush(l, buffer);
}

// 浮点重载
auto LongUI::CUIManager::operator<<(const float f) noexcept ->CUIManager&  {
    wchar_t buffer[LongUIStringBufferLength];
    ::swprintf(buffer, LongUIStringBufferLength, L"%f", f);
    this->OutputNoFlush(m_lastLevel, buffer);
    return *this;
}

// 控件
auto LongUI::CUIManager::operator<<(const UIControl* ctrl) noexcept ->CUIManager& {
    wchar_t buffer[LongUIStringBufferLength];
    if (ctrl) {
#if 1
        ::swprintf(
            buffer, LongUIStringBufferLength,
            L"[Control:%ls@%ls@0x%p] ",
            ctrl->GetNameStr(),
            ctrl->GetControlClassName(false),
            ctrl
            );
#else
        ::swprintf(
            buffer, LongUIStringBufferLength,
            L"[Control:%ls@0x%p] ",
            ctrl->GetNameStr(),
            ctrl
            );
#endif
    }
    else {
        ::swprintf(buffer, LongUIStringBufferLength, L"[Control:null] ");
    }
    this->OutputNoFlush(m_lastLevel, buffer);
    return *this;
}

// 整型重载
auto LongUI::CUIManager::operator<<(const long l) noexcept ->CUIManager& {
    wchar_t buffer[LongUIStringBufferLength];
    ::swprintf(buffer, LongUIStringBufferLength, L"%d", l);
    this->OutputNoFlush(m_lastLevel, buffer);
    return *this;
}

// 整型重载
auto LongUI::CUIManager::operator<<(const bool b) noexcept ->CUIManager& {
    this->OutputNoFlush(m_lastLevel, b ? "true" : "false");
    return *this;
}


// 调试输出
#define OutputDebug(a, b)\
void LongUI::CUIManager::a(const wchar_t* format, ...) noexcept {\
    wchar_t buffer[LongUIStringBufferLength];\
    va_list argList;\
    va_start(argList, format);\
    auto ret = ::vswprintf(buffer, LongUIStringBufferLength - 1, format, argList);\
    assert(ret < LongUIStringBufferLength);\
    buffer[ret] = 0;\
    va_end(argList);\
    this->Output(b, buffer);\
}

OutputDebug(OutputN, DLevel_None)
OutputDebug(OutputL, DLevel_Log)
OutputDebug(OutputH, DLevel_Hint)
OutputDebug(OutputW, DLevel_Warning)
OutputDebug(OutputE, DLevel_Error)
OutputDebug(OutputF, DLevel_Fatal)

#endif