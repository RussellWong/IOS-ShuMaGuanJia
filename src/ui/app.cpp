#include "ui/app.h"

#include "imgui.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>

// ===========================================================================
// 路由与状态
// ===========================================================================
void App::go(int screen, int id, int arg) {
    stack.push_back({screen, id, arg});
}

void App::back() {
    if (!stack.empty()) stack.pop_back();
}

void App::resetTo(int screen) {
    stack.clear();
    go(screen);
}

void App::onBackPressed() {
    if (!stack.empty()) back();
}

void App::openPhoneIn() {
    fPhone = PhoneDev();
    fPhoneId = 0;
    fPhone.condition = "99新";
    fPhone.storage = "128G";
    fPhoneInPrice.clear();
    fPhonePrep.clear();
    go(ScPhoneIn);
}

void App::openSell() {
    fSellPhoneId = 0;
    fSellPrice.clear();
    fSellPay = "微信";
    fSellCustomer = 0;
    fSellRemark.clear();
    go(ScPhoneOut);
}

void App::openRepairNew() {
    fRepair = Repair();
    fRepairId = 0;
    go(ScRepairNew);
}

void App::openPartStock(int type) {
    fStockPartId = 0;
    fStockType = type;
    fStockQty = "1";
    fStockPrice.clear();
    fStockPartner = 0;
    fStockRemark.clear();
    go(ScPartStockForm);
}

void App::openCardOpen() {
    fCardTplId = 0;
    fCardPartnerId = 0;
    go(ScCardOpen);
}

void App::openPartnerForm(int id) {
    if (id > 0) fPartner = store.partner(id);
    else {
        fPartner = Partner();
        fPartner.type = partnerType;
    }
    fPartnerId = id;
    go(ScPartnerForm);
}

void App::openEmployeeForm(int id) {
    if (id > 0) fEmp = store.employee(id);
    else {
        fEmp = Employee();
        fEmp.role = "销售";
        fEmp.active = 1;
        fEmp.commission = 30;
    }
    fEmpId = id;
    fEmpComm = ui::Num(fEmp.commission);
    fEmpActive = fEmp.active != 0;
    go(ScEmployeeForm);
}

void App::openPartForm(int id) {
    if (id > 0) fPart = store.part(id);
    else {
        fPart = Part();
        fPart.unit = "个";
        fPart.warn = 5;
    }
    fPartId = id;
    fPartCost = ui::Num(fPart.cost);
    fPartPrice = ui::Num(fPart.price);
    fPartStockQty = std::to_string(fPart.stock);
    fPartWarn = std::to_string(fPart.warn);
    go(ScPartForm);
}

void App::openCardTplForm(int id) {
    if (id > 0) fTpl = store.cardTpl(id);
    else {
        fTpl = CardTpl();
        fTpl.times = 10;
        fTpl.validDays = 365;
    }
    fTplId = id;
    fTplTimes = std::to_string(fTpl.times);
    fTplPrice = ui::Num(fTpl.price);
    fTplDays = std::to_string(fTpl.validDays);
    go(ScCardTplForm);
}

// ===========================================================================
// 日期范围
// ===========================================================================
void App::applyRange() {
    std::string today = Store::today();
    if (reportRange == 0) {
        reportFrom = today;
        reportTo = today;
    } else if (reportRange == 1) {
        std::tm tm{};
        int y, m, d;
        std::sscanf(today.c_str(), "%d-%d-%d", &y, &m, &d);
        tm.tm_year = y - 1900;
        tm.tm_mon = m - 1;
        tm.tm_mday = d;
        tm.tm_hour = 12;
        std::mktime(&tm);
        int wd = tm.tm_wday == 0 ? 7 : tm.tm_wday;   // 周一为 1
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", y, m, d - (wd - 1));
        reportFrom = buf;
        reportTo = today;
    } else if (reportRange == 2) {
        reportFrom = today.substr(0, 8) + "01";
        reportTo = today;
    }
    if (reportFrom.empty()) reportFrom = today;
    if (reportTo.empty()) reportTo = today;
}

std::string App::rangeFrom() const { return reportFrom.empty() ? Store::today() : reportFrom; }
std::string App::rangeTo() const { return reportTo.empty() ? Store::today() : reportTo; }
std::string App::rangeLabel() const {
    if (reportRange == 0) return "今日";
    if (reportRange == 1) return "本周";
    if (reportRange == 2) return "本月";
    return "自定义";
}

std::string App::employeeName(int id) {
    if (id <= 0) return "未指定";
    Employee e = store.employee(id);
    return e.id ? e.name : "未指定";
}

std::string App::partnerName(int id) {
    if (id <= 0) return "散客";
    Partner p = store.partner(id);
    return p.id ? p.name : "散客";
}

// ===========================================================================
// 页面标题
// ===========================================================================
static const char* screenTitle(int s) {
    switch (s) {
    case ScPhoneHome: return "手机业务";
    case ScPhoneIn: return "手机入库";
    case ScPhoneOut: return "手机出库";
    case ScPhoneList: return "设备库存";
    case ScPhoneDetail: return "设备详情";
    case ScPhoneReport: return "手机报表";
    case ScRepairHome: return "维修业务";
    case ScRepairNew: return "维修开单";
    case ScRepairDetail: return "维修单详情";
    case ScRepairList: return "维修单列表";
    case ScPartsHome: return "配件业务";
    case ScPartList: return "配件库存";
    case ScPartForm: return "配件档案";
    case ScPartStockForm: return "配件出入库";
    case ScPartFlows: return "库存流水";
    case ScCardHome: return "次卡业务";
    case ScCardTplForm: return "次卡模板";
    case ScCardOpen: return "开卡";
    case ScCardDetail: return "次卡详情";
    case ScCardList: return "会员次卡";
    case ScFinanceHome: return "财务中心";
    case ScFinanceAdd: return "日常收付款";
    case ScFinanceFlows: return "财务流水";
    case ScReceivables: return "应收应付";
    case ScReportSales: return "销售报表";
    case ScReportProfit: return "利润报表";
    case ScReportStock: return "库存报表";
    case ScReportStaff: return "员工业绩";
    case ScReportRepair: return "维修报表";
    case ScReportParts: return "配件销量";
    case ScPartnerDetail: return "往来单位";
    case ScPartnerForm: return "编辑往来单位";
    case ScShopInfo: return "门店信息";
    case ScEmployees: return "员工与权限";
    case ScEmployeeForm: return "员工信息";
    case ScAccounts: return "结算账户";
    case ScModels: return "机型资料库";
    case ScData: return "数据备份";
    case ScAbout: return "关于软件";
    default: return "数码管家";
    }
}

// ===========================================================================
// 首页
// ===========================================================================
static void QuickTile(App& a, ImVec2 origin, int col, int row, ui::IconType icon, const std::string& label,
                      ImU32 color, ImU32 soft, const char* id, int screen, int arg) {
    float tileW = ui::S(80), tileH = ui::S(72);
    float gapX = ui::S(9);
    float totalW = tileW * 4 + gapX * 3;
    // 以当前内容区左边界为基准居中，避免画布整体偏移时错位
    float startX = origin.x + (ui::CanvasW() * ui::Scale() - ui::S(32) - totalW) * 0.5f;
    ImVec2 p = ImVec2(startX + col * (tileW + gapX), origin.y + row * (tileH + ui::S(10)));
    ImGui::PushID(id);
    ImGui::SetCursorScreenPos(p);
    bool clicked = ImGui::InvisibleButton("##tile", ImVec2(tileW, tileH));
    ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x + tileW, p.y + tileH),
                                              IM_COL32(255, 255, 255, 255), ui::S(12));
    ui::Icon(icon, ImVec2(p.x + tileW * 0.5f, p.y + ui::S(24)), ui::S(17), color);
    ImVec2 ts = ui::fonts.small->CalcTextSizeA(ui::fonts.small->FontSize, FLT_MAX, 0, label.c_str());
    ImGui::GetWindowDrawList()->AddText(ui::fonts.small, ui::fonts.small->FontSize,
                                        ImVec2(p.x + (tileW - ts.x) * 0.5f, p.y + ui::S(45)),
                                        ui::colText, label.c_str());
    (void)soft;
    ImGui::PopID();
    if (clicked) {
        if (screen == ScPhoneIn) a.openPhoneIn();
        else if (screen == ScPhoneOut) a.openSell();
        else if (screen == ScRepairNew) a.openRepairNew();
        else if (screen == ScPartStockForm) a.openPartStock(arg);
        else if (screen == ScCardOpen) a.openCardOpen();
        else if (screen == ScPartnerForm) a.openPartnerForm(0);
        else a.go(screen, 0, arg);
    }
}

void ScreenHome(App& a) {
    DashboardStat st = a.store.dashboard();
    Shop shop = a.store.shop();
    if (shop.name.empty()) shop.name = "数码管家";

    // 渐变头部
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = ui::CanvasW() * ui::Scale() - ui::S(32);
    float h = ui::S(112);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilledMultiColor(p, ImVec2(p.x + w, p.y + h),
                                IM_COL32(47, 107, 255, 255), IM_COL32(78, 132, 255, 255),
                                IM_COL32(56, 116, 245, 255), IM_COL32(38, 92, 232, 255));
    dl->AddText(ui::fonts.body, ui::fonts.body->FontSize, ImVec2(p.x + ui::S(16), p.y + ui::S(14)),
                IM_COL32(220, 232, 255, 255), shop.name.c_str());
    dl->AddText(ui::fonts.small, ui::fonts.small->FontSize, ImVec2(p.x + ui::S(16), p.y + ui::S(36)),
                IM_COL32(200, 218, 255, 255), Store::today().c_str());
    dl->AddText(ui::fonts.small, ui::fonts.small->FontSize, ImVec2(p.x + ui::S(16), p.y + ui::S(56)),
                IM_COL32(205, 220, 255, 255), "今日销售额");
    std::string sales = "¥" + ui::Money(st.todaySales);
    dl->AddText(ui::fonts.big, ui::fonts.big->FontSize, ImVec2(p.x + ui::S(16), p.y + ui::S(72)),
                IM_COL32(255, 255, 255, 255), sales.c_str());
    std::string profit = "利润 ¥" + ui::Money(st.todayProfit);
    ImVec2 ps = ui::fonts.body->CalcTextSizeA(ui::fonts.body->FontSize, FLT_MAX, 0, profit.c_str());
    dl->AddText(ui::fonts.body, ui::fonts.body->FontSize,
                ImVec2(p.x + w - ps.x - ui::S(16), p.y + ui::S(80)),
                IM_COL32(226, 238, 255, 255), profit.c_str());
    std::string orders = "今日 " + std::to_string(st.todayOrders) + " 单";
    ImVec2 os = ui::fonts.small->CalcTextSizeA(ui::fonts.small->FontSize, FLT_MAX, 0, orders.c_str());
    dl->AddText(ui::fonts.small, ui::fonts.small->FontSize,
                ImVec2(p.x + w - os.x - ui::S(16), p.y + ui::S(56)),
                IM_COL32(200, 218, 255, 255), orders.c_str());
    ImGui::Dummy(ImVec2(0, h));
    ui::Gap(12);

    // 三个概览数字
    ImVec2 q = ImGui::GetCursorScreenPos();
    float qw = (w - ui::S(20)) / 3.0f;
    const char* labels[] = {"在库设备", "待处理维修", "库存预警"};
    std::string vals[] = {std::to_string(st.stockPhones), std::to_string(st.pendingRepairs),
                          std::to_string(st.warnParts)};
    ImU32 cols[] = {ui::colPrimary, ui::colWarn, ui::colDanger};
    for (int i = 0; i < 3; i++) {
        ImVec2 cp = ImVec2(q.x + i * (qw + ui::S(10)), q.y);
        dl->AddRectFilled(cp, ImVec2(cp.x + qw, cp.y + ui::S(64)), ui::colCard, ui::S(12));
        ImVec2 vs = ui::fonts.title->CalcTextSizeA(ui::fonts.title->FontSize, FLT_MAX, 0, vals[i].c_str());
        dl->AddText(ui::fonts.title, ui::fonts.title->FontSize,
                    ImVec2(cp.x + (qw - vs.x) * 0.5f, cp.y + ui::S(10)), cols[i], vals[i].c_str());
        ImVec2 ls = ui::fonts.small->CalcTextSizeA(ui::fonts.small->FontSize, FLT_MAX, 0, labels[i]);
        dl->AddText(ui::fonts.small, ui::fonts.small->FontSize,
                    ImVec2(cp.x + (qw - ls.x) * 0.5f, cp.y + ui::S(38)), ui::colSub, labels[i]);
    }
    ImGui::Dummy(ImVec2(0, ui::S(64)));
    ui::Gap(16);

    ui::Text("快捷操作", ui::colText, 2);
    ui::Gap(10);
    ImVec2 grid = ImGui::GetCursorScreenPos();
    QuickTile(a, grid, 0, 0, ui::IconIn, "手机入库", ui::colPrimary, ui::colBlueSoft, "qi1", ScPhoneIn, 0);
    QuickTile(a, grid, 1, 0, ui::IconOut, "手机出库", ui::colSuccess, ui::colGreenSoft, "qi2", ScPhoneOut, 0);
    QuickTile(a, grid, 2, 0, ui::IconWrench, "维修开单", ui::colWarn, ui::colOrangeSoft, "qi3", ScRepairNew, 0);
    QuickTile(a, grid, 3, 0, ui::IconPhone, "设备库存", IM_COL32(139, 92, 246, 255), ui::colBlueSoft, "qi4", ScPhoneList, 0);
    QuickTile(a, grid, 0, 1, ui::IconParts, "配件入库", ui::colPrimary, ui::colBlueSoft, "qi5", ScPartStockForm, 1);
    QuickTile(a, grid, 1, 1, ui::IconBag, "配件出库", ui::colSuccess, ui::colGreenSoft, "qi6", ScPartStockForm, 2);
    QuickTile(a, grid, 2, 1, ui::IconCard, "次卡开卡", IM_COL32(139, 92, 246, 255), ui::colBlueSoft, "qi7", ScCardOpen, 0);
    QuickTile(a, grid, 3, 1, ui::IconUsers, "新增客户", ui::colWarn, ui::colOrangeSoft, "qi8", ScPartnerForm, 0);
    ImGui::SetCursorScreenPos(ImVec2(grid.x, grid.y + ui::S(72) * 2 + ui::S(10)));
    ui::Gap(14);

    // 经营概览
    ui::Text("经营概览", ui::colText, 2);
    ui::Gap(8);
    ui::BeginCard("home_overview", 150);
    ui::KeyValueRow("在库成本金额", "¥" + ui::Money(st.stockValue));
    ui::KeyValueRow("客户应收", "¥" + ui::Money(st.receivable), st.receivable > 0 ? ui::colDanger : ui::colText);
    ui::KeyValueRow("往来单位", std::to_string(st.totalPartners) + " 家");
    ui::KeyValueRow("本月销售额", "¥" + ui::Money(a.store.salesSummary(a.rangeFrom(), a.rangeTo()).totalIncome));
    ui::EndCard();

    ui::Text("最近流水", ui::colText, 2);
    ui::Gap(8);
    auto flows = a.store.finances("", "", 5);
    if (flows.empty()) {
        ui::Empty("暂无流水，先录入一笔业务吧");
    } else {
        for (auto& f : flows) {
            std::string sub = f.category + " · " + f.createdAt.substr(5, 11);
            std::string amt = (f.type == 1 ? "+" : "-") + ui::Money(f.amount);
            if (ui::ListRow(f.remark.empty() ? f.category : f.remark, sub, amt,
                            f.type == 1 ? ui::colSuccess : ui::colDanger, "", 0, 62)) {
                a.go(ScFinanceFlows);
            }
        }
    }
    ui::Gap(20);
}

// ===========================================================================
// 业务
// ===========================================================================
static void ModuleCard(App& a, ui::IconType icon, const std::string& title, const std::string& sub,
                       const std::string& right, ImU32 color, int screen, const char* id) {
    ImGui::PushID(id);
    float w = ImGui::GetContentRegionAvail().x;
    float h = ui::S(82);
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##m", ImVec2(w, h));
    bool clicked = ImGui::IsItemDeactivated() && ImGui::IsItemHovered();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), ui::colCard, ui::S(12));
    dl->AddRectFilled(ImVec2(p.x + ui::S(14), p.y + ui::S(21)), ImVec2(p.x + ui::S(54), p.y + ui::S(61)),
                      ui::colBlueSoft, ui::S(10));
    ui::Icon(icon, ImVec2(p.x + ui::S(34), p.y + ui::S(41)), ui::S(15), color);
    dl->AddText(ui::fonts.title, ui::fonts.title->FontSize, ImVec2(p.x + ui::S(68), p.y + ui::S(18)),
                ui::colText, title.c_str());
    dl->AddText(ui::fonts.small, ui::fonts.small->FontSize, ImVec2(p.x + ui::S(68), p.y + ui::S(44)),
                ui::colSub, sub.c_str());
    if (!right.empty()) {
        ImVec2 rs = ui::fonts.body->CalcTextSizeA(ui::fonts.body->FontSize, FLT_MAX, 0, right.c_str());
        dl->AddText(ui::fonts.body, ui::fonts.body->FontSize,
                    ImVec2(p.x + w - rs.x - ui::S(14), p.y + (h - rs.y) * 0.5f), ui::colSub, right.c_str());
    }
    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + h + ui::S(10)));
    ImGui::PopID();
    if (clicked) a.go(screen);
}

void ScreenBusiness(App& a) {
    DashboardStat st = a.store.dashboard();
    ui::Gap(6);
    ui::Text("业务办理", ui::colText, 3);
    ui::Gap(4);
    ui::Hint("手机 · 维修 · 配件 · 次卡，一站式管理");
    ui::Gap(12);

    ModuleCard(a, ui::IconPhone, "手机业务", "一机一码 · 库龄 · 利润", std::to_string(st.stockPhones) + " 台在库",
               ui::colPrimary, ScPhoneHome, "m_phone");
    ModuleCard(a, ui::IconWrench, "维修业务", "开单 · 接单 · 维修 · 交付",
               std::to_string(st.pendingRepairs) + " 单待处理", ui::colWarn, ScRepairHome, "m_repair");
    ModuleCard(a, ui::IconParts, "配件业务", "档案 · 库存 · 流水",
               std::to_string(st.warnParts) + " 项预警", ui::colSuccess, ScPartsHome, "m_parts");
    ModuleCard(a, ui::IconCard, "次卡业务", "开卡 · 核销 · 会员",
               std::to_string(a.store.cards().size()) + " 张卡", IM_COL32(139, 92, 246, 255), ScCardHome, "m_card");
    ModuleCard(a, ui::IconMoney, "财务中心", "流水 · 账户 · 应收应付",
               "¥" + ui::Money(st.todaySales), ui::colDanger, ScFinanceHome, "m_fin");

    ui::Gap(10);
    ui::Text("常用功能", ui::colText, 2);
    ui::Gap(8);
    if (ui::ListRow("手机出库开单", "选择在库设备，录入售价与收款", "去开单", ui::colPrimary)) a.openSell();
    if (ui::ListRow("维修开单", "登记客户、机型与故障", "去开单", ui::colPrimary)) a.openRepairNew();
    if (ui::ListRow("配件入库", "采购入库，自动更新库存与财务", "去入库", ui::colPrimary)) a.openPartStock(1);
    if (ui::ListRow("次卡开卡", "选择模板与客户，快速开卡", "去开卡", ui::colPrimary)) a.openCardOpen();
    ui::Gap(20);
}

// ===========================================================================
// 往来单位
// ===========================================================================
void ScreenPartners(App& a) {
    ui::Gap(6);
    ui::Text("往来单位", ui::colText, 3);
    ui::Gap(10);
    ui::SearchBar("partner_search", a.sPartner, "搜索客户/供应商名称、电话");
    int seg = ui::Segmented("partner_seg", {"客户", "供应商"}, a.partnerType == 2 ? 1 : 0);
    if (seg == 0) a.partnerType = 1;
    else if (seg == 1) a.partnerType = 2;

    auto list = a.store.partners(a.partnerType, a.sPartner);
    if (list.empty()) {
        ui::Empty("暂无往来单位，点击下方按钮新增");
    } else {
        for (auto& p : list) {
            std::string sub = p.phone.empty() ? "未填写电话" : p.phone;
            std::string right;
            ImU32 rc = ui::colText;
            if (p.receivable > 0.009) {
                right = "应收 ¥" + ui::Money(p.receivable);
                rc = ui::colDanger;
            } else if (p.payable > 0.009) {
                right = "应付 ¥" + ui::Money(p.payable);
                rc = ui::colWarn;
            } else {
                right = p.type == 2 ? "供应商" : "客户";
                rc = ui::colSub;
            }
            if (ui::ListRow(p.name, sub, right, rc)) a.go(ScPartnerDetail, p.id);
        }
    }
    ui::Gap(10);
    if (ui::PrimaryButton("+ 新增往来单位")) a.openPartnerForm(0);
    ui::Gap(20);
}

// ===========================================================================
// 报表
// ===========================================================================
void ScreenReports(App& a) {
    a.applyRange();
    SalesSummary sum = a.store.salesSummary(a.rangeFrom(), a.rangeTo());
    ui::Gap(6);
    ui::Text("报表中心", ui::colText, 3);
    ui::Gap(10);

    ImGui::PushID("range");
    int seg = ui::Segmented("report_range", {"今日", "本周", "本月"}, a.reportRange > 2 ? 2 : a.reportRange);
    ImGui::PopID();
    if (seg >= 0 && seg != a.reportRange) {
        a.reportRange = seg;
        a.applyRange();
    }
    ui::BeginCard("report_sum", 116);
    ui::KeyValueRow("区间收入", "¥" + ui::Money(sum.totalIncome), ui::colText);
    ui::KeyValueRow("区间利润", "¥" + ui::Money(sum.totalProfit), ui::colSuccess);
    ui::KeyValueRow("区间单量", std::to_string(sum.phoneCount + sum.repairCount + sum.cardCount + sum.partQty) + " 笔");
    ui::EndCard();

    if (ui::ListRow("销售报表", "按天查看销售额走势", "查看")) a.go(ScReportSales);
    if (ui::ListRow("利润报表", "手机 / 维修 / 配件利润构成", "查看")) a.go(ScReportProfit);
    if (ui::ListRow("库存报表", "在库设备与配件库存金额", "查看")) a.go(ScReportStock);
    if (ui::ListRow("员工业绩", "维修工单与销售业绩排名", "查看")) a.go(ScReportStaff);
    if (ui::ListRow("维修报表", "维修单状态与收入统计", "查看")) a.go(ScReportRepair);
    if (ui::ListRow("配件销量", "配件销售排行与毛利", "查看")) a.go(ScReportParts);
    if (ui::ListRow("财务流水", "收入支出明细与账户余额", "查看")) a.go(ScFinanceFlows);
    if (ui::ListRow("应收应付", "客户欠款与供应商应付款", "查看")) a.go(ScReceivables);
    ui::Gap(20);
}

// ===========================================================================
// 我的
// ===========================================================================
void ScreenMine(App& a) {
    Shop shop = a.store.shop();
    if (shop.name.empty()) shop.name = "未设置门店";
    ui::Gap(6);
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = ui::CanvasW() * ui::Scale() - ui::S(32);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilledMultiColor(p, ImVec2(p.x + w, p.y + ui::S(92)),
                                IM_COL32(47, 107, 255, 255), IM_COL32(80, 134, 255, 255),
                                IM_COL32(46, 100, 235, 255), IM_COL32(35, 84, 220, 255));
    dl->AddCircleFilled(ImVec2(p.x + ui::S(42), p.y + ui::S(46)), ui::S(24), IM_COL32(255, 255, 255, 45));
    std::string initial = shop.name.substr(0, 3);
    ImVec2 is = ui::fonts.title->CalcTextSizeA(ui::fonts.title->FontSize, FLT_MAX, 0, initial.c_str());
    dl->AddText(ui::fonts.title, ui::fonts.title->FontSize,
                ImVec2(p.x + ui::S(42) - is.x * 0.5f, p.y + ui::S(46) - is.y * 0.5f),
                IM_COL32(255, 255, 255, 255), initial.c_str());
    dl->AddText(ui::fonts.title, ui::fonts.title->FontSize, ImVec2(p.x + ui::S(78), p.y + ui::S(26)),
                IM_COL32(255, 255, 255, 255), shop.name.c_str());
    std::string owner = shop.owner.empty() ? "管理员" : shop.owner;
    if (!shop.phone.empty()) owner += " · " + shop.phone;
    dl->AddText(ui::fonts.small, ui::fonts.small->FontSize, ImVec2(p.x + ui::S(78), p.y + ui::S(52)),
                IM_COL32(215, 229, 255, 255), owner.c_str());
    ImGui::Dummy(ImVec2(0, ui::S(92)));
    ui::Gap(14);

    if (ui::ListRow("门店信息", "名称、负责人、联系方式", "编辑")) a.go(ScShopInfo);
    if (ui::ListRow("员工与权限", "员工档案与工费提成", std::to_string(a.store.employees().size()) + " 人")) a.go(ScEmployees);
    if (ui::ListRow("结算账户", "现金 / 微信 / 支付宝 / 银行卡", "")) a.go(ScAccounts);
    if (ui::ListRow("机型资料库", "内置常见品牌与机型", std::to_string(a.store.models().size()) + " 条")) a.go(ScModels);
    if (ui::ListRow("数据管理", "备份数据、演示数据、清空数据", "")) a.go(ScData);
    if (ui::ListRow("关于软件", "数码管家 手机进销存管理系统", "v1.0.0")) a.go(ScAbout);
    ui::Gap(20);
}

// ===========================================================================
// 外壳
// ===========================================================================
static void BeginContent(float height) {
    ImVec2 base = ImGui::GetCursorScreenPos();
    float cw = ui::CanvasW() * ui::Scale();
    ImGui::SetCursorScreenPos(ImVec2(base.x + ui::S(16), base.y + ui::S(10)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::BeginChild("##content", ImVec2(cw - ui::S(32), height - ui::S(10)), false,
                      ImGuiWindowFlags_AlwaysVerticalScrollbar);
}

static void EndContent() {
    ImGui::EndChild();
    ImGui::PopStyleVar();
}

void App::draw(float dt) {
    ui::Tick(dt);
    animTime += dt;

    ImGuiIO& io = ImGui::GetIO();
    float scale = ui::Scale();
    float cw = ui::CanvasW() * scale;
    float ch = ui::CanvasH() * scale;
    float offX = (io.DisplaySize.x - cw) * 0.5f;
    float offY = (io.DisplaySize.y - ch) * 0.5f;

    ImGui::SetNextWindowPos(ImVec2(offX, offY));
    ImGui::SetNextWindowSize(ImVec2(cw, ch));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ui::colBg);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("##root", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
                 ImGuiWindowFlags_NoNavFocus);

    if (!store.initialized()) {
        BeginContent(ch);
        ScreenLogin(*this);
        EndContent();
        ImGui::End();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        return;
    }

    float navH = ui::S(62);
    bool pushed = !stack.empty();

    if (pushed) {
        bool backClicked = false, actionClicked = false;
        ui::TopBar(screenTitle(stack.back().screen), true, "", backClicked, actionClicked);
        if (backClicked) back();
        BeginContent(ch - ui::S(56));
    } else {
        BeginContent(ch - navH);
    }

    if (!stack.empty()) {
        Route r = stack.back();
        switch (r.screen) {
        case ScPhoneHome: ScreenPhoneHome(*this); break;
        case ScPhoneIn: ScreenPhoneIn(*this); break;
        case ScPhoneOut: ScreenPhoneOut(*this); break;
        case ScPhoneList: ScreenPhoneList(*this); break;
        case ScPhoneDetail: ScreenPhoneDetail(*this, r.id); break;
        case ScPhoneReport: ScreenPhoneReport(*this); break;
        case ScRepairHome: ScreenRepairHome(*this); break;
        case ScRepairNew: ScreenRepairNew(*this); break;
        case ScRepairDetail: ScreenRepairDetail(*this, r.id); break;
        case ScRepairList: ScreenRepairList(*this); break;
        case ScPartsHome: ScreenPartsHome(*this); break;
        case ScPartList: ScreenPartList(*this); break;
        case ScPartForm: ScreenPartForm(*this); break;
        case ScPartStockForm: ScreenPartStockForm(*this); break;
        case ScPartFlows: ScreenPartFlows(*this); break;
        case ScCardHome: ScreenCardHome(*this); break;
        case ScCardTplForm: ScreenCardTplForm(*this); break;
        case ScCardOpen: ScreenCardOpen(*this); break;
        case ScCardDetail: ScreenCardDetail(*this, r.id); break;
        case ScCardList: ScreenCardList(*this); break;
        case ScFinanceHome: ScreenFinanceHome(*this); break;
        case ScFinanceAdd: ScreenFinanceAdd(*this); break;
        case ScFinanceFlows: ScreenFinanceFlows(*this); break;
        case ScReceivables: ScreenReceivables(*this); break;
        case ScReportSales: ScreenReportSales(*this); break;
        case ScReportProfit: ScreenReportProfit(*this); break;
        case ScReportStock: ScreenReportStock(*this); break;
        case ScReportStaff: ScreenReportStaff(*this); break;
        case ScReportRepair: ScreenReportRepair(*this); break;
        case ScReportParts: ScreenReportParts(*this); break;
        case ScPartnerDetail: ScreenPartnerDetail(*this, r.id); break;
        case ScPartnerForm: ScreenPartnerForm(*this); break;
        case ScShopInfo: ScreenShopInfo(*this); break;
        case ScEmployees: ScreenEmployees(*this); break;
        case ScEmployeeForm: ScreenEmployeeForm(*this); break;
        case ScAccounts: ScreenAccounts(*this); break;
        case ScModels: ScreenModels(*this); break;
        case ScData: ScreenData(*this); break;
        case ScAbout: ScreenAbout(*this); break;
        default: break;
        }
    } else if (tab == 0) ScreenHome(*this);
    else if (tab == 1) ScreenBusiness(*this);
    else if (tab == 2) ScreenPartners(*this);
    else if (tab == 3) ScreenReports(*this);
    else ScreenMine(*this);

    EndContent();

    if (!stack.empty()) {
        // 返回后可能已经清空
        if (stack.empty()) {
            // 本帧不绘制导航，下一帧自动进入主页
        }
    }

    if (stack.empty()) {
        ImGui::SetCursorScreenPos(ImVec2(offX, offY + ch - navH));
        int changed = tab;
        ui::BottomNav(changed);
        if (changed != tab) {
            tab = changed;
            stack.clear();
        }
    }

    // 提示浮层
    if (ui::ToastAlpha() > 0.01f) {
        const std::string& msg = ui::ToastText();
        if (!msg.empty()) {
            float tw = ui::TextWidth(msg, 1);
            float bw = tw + ui::S(36);
            float bh = ui::S(38);
            ImVec2 tp = ImVec2(offX + (cw - bw) * 0.5f, offY + ch - ui::S(100));
            ImDrawList* dl = ImGui::GetWindowDrawList();
            float al = ui::ToastAlpha();
            dl->AddRectFilled(tp, ImVec2(tp.x + bw, tp.y + bh),
                              IM_COL32(30, 38, 52, (int)(225 * al)), ui::S(9));
            dl->AddText(ui::fonts.body, ui::fonts.body->FontSize,
                        ImVec2(tp.x + ui::S(18), tp.y + (bh - ui::fonts.body->FontSize) * 0.5f),
                        IM_COL32(255, 255, 255, (int)(255 * al)), msg.c_str());
        }
    }

    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}
