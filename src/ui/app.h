// 应用框架：路由、全局状态、主界面外壳
#pragma once

#include "core/store.h"
#include "ui/ui.h"

#include <string>
#include <vector>

// 页面编号
enum Screen {
    ScHome = 0,
    ScBusiness,
    ScPartners,
    ScReports,
    ScMine,

    ScPhoneHome,
    ScPhoneIn,
    ScPhoneOut,
    ScPhoneList,
    ScPhoneDetail,
    ScPhoneReport,

    ScRepairHome,
    ScRepairNew,
    ScRepairDetail,
    ScRepairList,

    ScPartsHome,
    ScPartList,
    ScPartForm,
    ScPartStockForm,
    ScPartFlows,

    ScCardHome,
    ScCardTplForm,
    ScCardOpen,
    ScCardDetail,
    ScCardList,

    ScFinanceHome,
    ScFinanceAdd,
    ScFinanceFlows,
    ScReceivables,

    ScReportSales,
    ScReportProfit,
    ScReportStock,
    ScReportStaff,
    ScReportRepair,
    ScReportParts,

    ScPartnerDetail,
    ScPartnerForm,

    ScShopInfo,
    ScEmployees,
    ScEmployeeForm,
    ScAccounts,
    ScModels,
    ScData,
    ScAbout
};

struct Route {
    int screen = ScHome;
    int id = 0;
    int arg = 0;
};

struct App {
    Store store;

    int tab = 0;
    std::vector<Route> stack;
    std::string fontPath;
    std::string toastHint;
    float animTime = 0.0f;

    // ---------------- 表单状态 ----------------
    PhoneDev fPhone;        int fPhoneId = 0;
    Repair fRepair;         int fRepairId = 0;
    Part fPart;             int fPartId = 0;
    CardTpl fTpl;           int fTplId = 0;
    Partner fPartner;      int fPartnerId = 0;
    Employee fEmp;          int fEmpId = 0;
    Account fAcc;           int fAccId = 0;

    // 手机出库
    int fSellPhoneId = 0;
    std::string fSellPrice;
    std::string fSellPay = "微信";
    int fSellCustomer = 0;
    std::string fSellRemark;

    // 配件出入库 / 盘点
    int fStockPartId = 0;
    int fStockType = 1;
    std::string fStockQty = "1";
    std::string fStockPrice;
    int fStockPartner = 0;
    std::string fStockRemark;

    // 次卡
    int fCardTplId = 0;
    int fCardPartnerId = 0;
    std::string fUseTimes = "1";
    std::string fUseRemark;

    // 财务
    int fFinType = 1;
    std::string fFinCategory;
    std::string fFinAmount;
    std::string fFinAccount = "现金";
    std::string fFinRemark;

    // 欠款核销
    int fSettlePartner = 0;
    std::string fSettleAmount;
    std::string fSettleAccount = "现金";
    std::string fSettleRemark;

    // 列表筛选
    std::string sPhone, sRepair, sPart, sPartner, sCard, sFlow;
    int phoneFilter = 0;
    int repairFilter = 100;
    int partnerType = 1;
    int cardFilter = 0;

    // 报表日期
    int reportRange = 2;   // 0 今日 1 本周 2 本月 3 自定义
    std::string reportFrom;
    std::string reportTo;

    // 设置
    Shop fShop;
    std::string fShopName;   // 门店初始化输入
    std::string fEmpComm;
    bool fEmpActive = true;
    std::string fAccNew;
    std::string fModelKey;
    std::string fPhoneInPrice, fPhonePrep;
    std::string fRepairLabor, fRepairParts, fRepairTotal, fRepairPaid;
    std::string fTplTimes, fTplPrice, fTplDays;
    std::string fPartCost, fPartPrice, fPartStockQty, fPartWarn;
    bool confirmClear = false;
    bool confirmDemo = false;
    bool confirmDeletePhone = false;
    bool confirmDeletePart = false;
    bool confirmDeletePartner = false;
    bool confirmDeleteEmployee = false;
    bool confirmDeleteTpl = false;
    bool confirmReturnPhone = false;
    bool confirmCancelRepair = false;

    // 当前门店的登录员工
    int me = 1;

    // ---------------- 行为 ----------------
    void go(int screen, int id = 0, int arg = 0);
    void back();
    void resetTo(int screen);
    void draw(float dt);
    void onBackPressed();

    void openPhoneIn();
    void openSell();
    void openRepairNew();
    void openPartStock(int type);
    void openCardOpen();
    void openPartnerForm(int id);
    void openEmployeeForm(int id);
    void openPartForm(int id);
    void openCardTplForm(int id);

    std::string rangeFrom() const;
    std::string rangeTo() const;
    std::string rangeLabel() const;
    void applyRange();

    std::string employeeName(int id);
    std::string partnerName(int id);
};

// 字符串数值解析
inline double parseD(const std::string& s, double def = 0.0) {
    if (s.empty()) return def;
    try { return std::stod(s); } catch (...) { return def; }
}
inline int parseI(const std::string& s, int def = 0) {
    if (s.empty()) return def;
    try { return std::stoi(s); } catch (...) { return def; }
}

// 各页面绘制函数（在对应 screens_*.cpp 中实现）
void ScreenHome(App& a);
void ScreenBusiness(App& a);
void ScreenPartners(App& a);
void ScreenReports(App& a);
void ScreenMine(App& a);
void ScreenLogin(App& a);

void ScreenPhoneHome(App& a);
void ScreenPhoneIn(App& a);
void ScreenPhoneOut(App& a);
void ScreenPhoneList(App& a);
void ScreenPhoneDetail(App& a, int id);
void ScreenPhoneReport(App& a);

void ScreenRepairHome(App& a);
void ScreenRepairNew(App& a);
void ScreenRepairDetail(App& a, int id);
void ScreenRepairList(App& a);

void ScreenPartsHome(App& a);
void ScreenPartList(App& a);
void ScreenPartForm(App& a);
void ScreenPartStockForm(App& a);
void ScreenPartFlows(App& a);

void ScreenCardHome(App& a);
void ScreenCardTplForm(App& a);
void ScreenCardOpen(App& a);
void ScreenCardDetail(App& a, int id);
void ScreenCardList(App& a);

void ScreenFinanceHome(App& a);
void ScreenFinanceAdd(App& a);
void ScreenFinanceFlows(App& a);
void ScreenReceivables(App& a);

void ScreenReportSales(App& a);
void ScreenReportProfit(App& a);
void ScreenReportStock(App& a);
void ScreenReportStaff(App& a);
void ScreenReportRepair(App& a);
void ScreenReportParts(App& a);

void ScreenPartnerDetail(App& a, int id);
void ScreenPartnerForm(App& a);

void ScreenShopInfo(App& a);
void ScreenEmployees(App& a);
void ScreenEmployeeForm(App& a);
void ScreenAccounts(App& a);
void ScreenModels(App& a);
void ScreenData(App& a);
void ScreenAbout(App& a);
