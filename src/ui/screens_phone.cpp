// 手机业务：入库、出库、库存、详情、报表
#include "ui/app.h"

#include "imgui.h"

#include <algorithm>
#include <cstdio>

// ---------------------------------------------------------------------------
// 公共小工具
// ---------------------------------------------------------------------------
static std::string devTitle(const PhoneDev& d) {
    std::string s = d.brand;
    if (!d.model.empty()) s += " " + d.model;
    if (!d.storage.empty()) s += " " + d.storage;
    if (s.empty()) s = "未命名设备";
    return s;
}

static std::vector<std::string> brandOptions(App& a) {
    std::vector<std::string> b = a.store.brands();
    if (b.empty()) b = {"苹果", "华为", "小米", "OPPO", "vivo", "荣耀", "三星", "一加", "realme"};
    return b;
}

static std::vector<std::string> partnerOptions(App& a, int type, std::vector<int>& ids) {
    ids.clear();
    ids.push_back(0);
    std::vector<std::string> names;
    names.push_back("不指定");
    for (auto& p : a.store.partners(type)) {
        ids.push_back(p.id);
        names.push_back(p.name);
    }
    return names;
}

// 在库成本 + 预计毛利
static void stockStats(App& a, int& count, double& cost, double& expect) {
    count = 0;
    cost = 0;
    expect = 0;
    for (auto& d : a.store.phones(0, "", 5000)) {
        count++;
        cost += d.inPrice + d.prepCost;
        expect += (d.inPrice + d.prepCost) * 0.12;
    }
}

// ---------------------------------------------------------------------------
// 手机业务首页
// ---------------------------------------------------------------------------
void ScreenPhoneHome(App& a) {
    int count = 0;
    double cost = 0, expect = 0;
    stockStats(a, count, cost, expect);

    ui::Gap(4);
    ui::BeginCardAuto("ph_head");
    ui::KeyValueRow("在库设备", std::to_string(count) + " 台");
    ui::KeyValueRow("库存成本", "¥" + ui::Money(cost));
    ui::KeyValueRow("预计毛利", "¥" + ui::Money(expect), ui::colSuccess);
    ui::EndCardAuto();

    ui::Gap(6);
    if (ui::PrimaryButton("手机入库", 46)) a.openPhoneIn();
    if (ui::SoftButton("手机出库开单", 46)) a.openSell();
    if (ui::SoftButton("查看全部库存", 46)) a.go(ScPhoneList);
    if (ui::SoftButton("手机经营报表", 46)) a.go(ScPhoneReport);

    ui::Gap(10);
    ui::Text("最近入库", ui::colText, 2);
    ui::Gap(8);
    auto list = a.store.phones(-1, "", 6);
    if (list.empty()) {
        ui::Empty("还没有设备，点击「手机入库」开始");
    } else {
        for (auto& d : list) {
            std::string sub = d.code + " · " + (d.imei.empty() ? "无串号" : d.imei);
            std::string right = "¥" + ui::Money(d.inPrice);
            if (ui::ListRow(devTitle(d), sub, right, ui::colSub, "", 0, 62)) a.go(ScPhoneDetail, d.id);
        }
    }
    ui::Gap(20);
}

// ---------------------------------------------------------------------------
// 手机入库 / 编辑
// ---------------------------------------------------------------------------
void ScreenPhoneIn(App& a) {
    ui::Gap(2);
    ui::FieldLabel("品牌");
    {
        std::vector<std::string> brands = brandOptions(a);
        int sel = ui::Dropdown("pin_brand", a.fPhone.brand, "选择品牌", brands);
        if (sel >= 0) a.fPhone.brand = brands[sel];
    }
    ui::FieldLabel("型号");
    ui::TextField("pin_model", a.fPhone.model, "例如 iPhone 15 Pro");
    ui::FieldLabel("容量");
    {
        std::vector<std::string> caps = {"64G", "128G", "256G", "512G", "1T"};
        int sel = ui::Dropdown("pin_cap", a.fPhone.storage, "选择容量", caps);
        if (sel >= 0) a.fPhone.storage = caps[sel];
    }
    ui::FieldLabel("颜色");
    ui::TextField("pin_color", a.fPhone.color, "例如 黑色");
    ui::FieldLabel("成色");
    {
        std::vector<std::string> cond = {"全新", "99新", "95新", "9成新", "8成新", "7成新"};
        int sel = ui::Dropdown("pin_cond", a.fPhone.condition, "选择成色", cond);
        if (sel >= 0) a.fPhone.condition = cond[sel];
    }
    ui::FieldLabel("串号 / IMEI（一机一码）");
    ui::TextField("pin_imei", a.fPhone.imei, "扫描或输入 IMEI、SN", true);
    ui::FieldLabel("入库价（元）");
    ui::TextField("pin_price", a.fPhoneInPrice, "0.00", true);
    ui::FieldLabel("整备成本（元）");
    ui::TextField("pin_prep", a.fPhonePrep, "0.00", true);
    ui::FieldLabel("入库供应商");
    {
        std::vector<int> ids;
        std::vector<std::string> names = partnerOptions(a, 2, ids);
        int cur = 0;
        for (int i = 0; i < (int)ids.size(); i++) if (ids[i] == a.fPhone.inSupplier) cur = i;
        int sel = ui::Dropdown("pin_supplier", names[cur], "选择供应商", names);
        if (sel >= 0) a.fPhone.inSupplier = ids[sel];
    }
    ui::FieldLabel("备注");
    ui::TextField("pin_remark", a.fPhone.remark, "选填");

    ui::Gap(14);
    if (ui::PrimaryButton(a.fPhoneId > 0 ? "保存修改" : "确认入库")) {
        if (a.fPhone.brand.empty() && a.fPhone.model.empty()) {
            ui::Toast("请至少填写品牌或型号");
        } else if (a.fPhone.imei.empty()) {
            ui::Toast("请填写串号，一机一码");
        } else if (a.store.phones(-1, a.fPhone.imei, 5).size() > 0 && a.fPhoneId == 0) {
            ui::Toast("该串号已存在，请勿重复入库");
        } else {
            a.fPhone.inPrice = parseD(a.fPhoneInPrice, 0);
            a.fPhone.prepCost = parseD(a.fPhonePrep, 0);
            a.fPhone.inOperator = a.me;
            if (a.fPhoneId > 0) {
                a.store.updatePhone(a.fPhone);
                ui::Toast("已保存");
            } else {
                int id = a.store.addPhoneIn(a.fPhone);
                ui::Toast("入库成功，编码 " + a.fPhone.code);
                a.fPhoneId = id;
            }
            a.back();
        }
    }
    if (a.fPhoneId > 0) {
        ui::Gap(6);
        if (ui::SoftButton("查看设备详情")) a.go(ScPhoneDetail, a.fPhoneId);
    }
    ui::Gap(30);
}

// ---------------------------------------------------------------------------
// 手机出库
// ---------------------------------------------------------------------------
void ScreenPhoneOut(App& a) {
    if (a.fSellPhoneId <= 0) {
        ui::Gap(2);
        ui::Hint("请选择一台在库设备进行出库开单");
        ui::Gap(8);
        ui::SearchBar("sell_search", a.sPhone, "搜索品牌/型号/串号");
        auto list = a.store.phones(0, a.sPhone, 200);
        if (list.empty()) {
            ui::Empty("没有可出库的设备");
        } else {
            for (auto& d : list) {
                std::string sub = d.code + " · 成本 ¥" + ui::Money(d.inPrice + d.prepCost);
                if (ui::ListRow(devTitle(d), sub, d.condition, ui::colSub, "", 0, 62)) {
                    a.fSellPhoneId = d.id;
                    a.fSellPrice = ui::Num(d.inPrice + d.prepCost);
                }
            }
        }
        ui::Gap(24);
        return;
    }

    PhoneDev d = a.store.phone(a.fSellPhoneId);
    if (d.id == 0 || d.status != 0) {
        ui::Toast("该设备已不在库");
        a.fSellPhoneId = 0;
        return;
    }

    ui::Gap(2);
    ui::BeginCardAuto("sell_dev");
    ui::KeyValueRow("设备", devTitle(d));
    ui::KeyValueRow("编码", d.code);
    ui::KeyValueRow("成色", d.condition);
    ui::KeyValueRow("成本", "¥" + ui::Money(d.inPrice + d.prepCost));
    ui::EndCardAuto();

    ui::Gap(2);
    ui::FieldLabel("销售价（元）");
    ui::TextField("sell_price", a.fSellPrice, "0.00", true);
    ui::FieldLabel("收款方式");
    {
        std::vector<std::string> pays = {"现金", "微信", "支付宝", "银行卡", "分期", "其他"};
        int sel = ui::Dropdown("sell_pay", a.fSellPay, "选择收款方式", pays);
        if (sel >= 0) a.fSellPay = pays[sel];
    }
    ui::FieldLabel("客户");
    {
        std::vector<int> ids;
        std::vector<std::string> names = partnerOptions(a, 1, ids);
        int cur = 0;
        for (int i = 0; i < (int)ids.size(); i++) if (ids[i] == a.fSellCustomer) cur = i;
        int sel = ui::Dropdown("sell_cust", names[cur], "选择客户（可选）", names);
        if (sel >= 0) a.fSellCustomer = ids[sel];
    }
    ui::FieldLabel("备注");
    ui::TextField("sell_remark", a.fSellRemark, "选填");

    ui::Gap(14);
    if (ui::PrimaryButton("确认出库")) {
        double price = parseD(a.fSellPrice, 0);
        if (price <= 0) {
            ui::Toast("请输入销售价");
        } else {
            a.store.sellPhone(d.id, price, a.fSellCustomer, a.fSellPay, a.me);
            double profit = price - d.inPrice - d.prepCost;
            ui::Toast("出库成功，毛利 ¥" + ui::Money(profit));
            a.fSellPhoneId = 0;
            a.back();
        }
    }
    ui::Gap(6);
    if (ui::SoftButton("重新选择设备")) {
        a.fSellPhoneId = 0;
        a.fSellPrice.clear();
    }
    ui::Gap(24);
}

// ---------------------------------------------------------------------------
// 设备库存
// ---------------------------------------------------------------------------
void ScreenPhoneList(App& a) {
    ui::Gap(2);
    ui::SearchBar("phone_search", a.sPhone, "搜索品牌/型号/串号/编码");
    int seg = ui::Segmented("phone_seg", {"全部", "在库", "已售"}, a.phoneFilter);
    if (seg >= 0) a.phoneFilter = seg;

    int status = -1;
    if (a.phoneFilter == 1) status = 0;
    else if (a.phoneFilter == 2) status = 1;

    auto list = a.store.phones(status, a.sPhone, 500);
    if (list.empty()) {
        ui::Empty("暂无设备");
    } else {
        for (auto& d : list) {
            std::string sub = d.code + " · " + d.condition;
            std::string right;
            ImU32 rc = ui::colSub;
            if (d.status == 0) {
                right = "成本 ¥" + ui::Money(d.inPrice + d.prepCost);
            } else if (d.status == 1) {
                right = "售价 ¥" + ui::Money(d.outPrice);
                rc = ui::colSuccess;
            } else {
                right = "已退货";
                rc = ui::colDanger;
            }
            ImGui::PushID(d.id);
            if (ui::ListRow(devTitle(d), sub, right, rc, "", 0, 64)) a.go(ScPhoneDetail, d.id);
            ImGui::PopID();
        }
    }
    ui::Gap(10);
    if (ui::PrimaryButton("+ 手机入库")) a.openPhoneIn();
    ui::Gap(24);
}

// ---------------------------------------------------------------------------
// 设备详情
// ---------------------------------------------------------------------------
void ScreenPhoneDetail(App& a, int id) {
    PhoneDev d = a.store.phone(id);
    if (d.id == 0) {
        ui::Empty("设备不存在");
        return;
    }

    ui::Gap(2);
    ui::Text(devTitle(d), ui::colText, 3);
    ui::Gap(4);
    ui::StatusTag(d.status, 0);
    ui::Gap(10);

    double cost = d.inPrice + d.prepCost;
    ui::BeginCardAuto("ph_detail");
    ui::KeyValueRow("系统编码", d.code);
    ui::KeyValueRow("串号", d.imei.empty() ? "未填写" : d.imei);
    ui::KeyValueRow("成色", d.condition);
    ui::KeyValueRow("颜色", d.color.empty() ? "未填写" : d.color);
    ui::KeyValueRow("入库价", "¥" + ui::Money(d.inPrice));
    ui::KeyValueRow("整备成本", "¥" + ui::Money(d.prepCost));
    ui::KeyValueRow("设备成本", "¥" + ui::Money(cost));
    ui::KeyValueRow("供应商", a.partnerName(d.inSupplier));
    ui::KeyValueRow("入库时间", d.inTime);
    if (d.status == 1) {
        ui::KeyValueRow("销售价", "¥" + ui::Money(d.outPrice), ui::colSuccess);
        ui::KeyValueRow("毛利", "¥" + ui::Money(d.outPrice - cost), ui::colSuccess);
        ui::KeyValueRow("客户", a.partnerName(d.outCustomer));
        ui::KeyValueRow("收款方式", d.outPay);
        ui::KeyValueRow("出库时间", d.outTime);
    }
    if (!d.remark.empty()) ui::KeyValueRow("备注", d.remark);
    ui::EndCardAuto();

    ui::Gap(4);
    if (d.status == 0) {
        if (ui::PrimaryButton("出库开单")) {
            a.fSellPhoneId = d.id;
            a.fSellPrice = ui::Num(d.inPrice + d.prepCost);
            a.fSellPay = "微信";
            a.fSellCustomer = 0;
            a.fSellRemark.clear();
            a.go(ScPhoneOut);
        }
    }
    if (ui::SoftButton("编辑设备资料")) {
        a.fPhone = d;
        a.fPhoneId = d.id;
        a.fPhoneInPrice = ui::Num(d.inPrice);
        a.fPhonePrep = ui::Num(d.prepCost);
        a.go(ScPhoneIn);
    }
    if (d.status == 1) {
        if (ui::SoftButton("售后退货（回到库存）")) {
            if (ui::Confirm("ret_phone", "退货确认", "将冲回销售收入并将设备退回在库，确定？")) {
                a.store.returnPhone(d.id);
                ui::Toast("已退货");
            }
        }
    }
    ui::Gap(4);
    if (ui::DangerButton("删除该设备记录")) {
        if (ui::Confirm("del_phone", "删除确认", "删除后不可恢复，确定删除该设备？")) {
            a.store.removePhone(d.id);
            ui::Toast("已删除");
            a.back();
        }
    }
    ui::Gap(24);
}

// ---------------------------------------------------------------------------
// 手机报表
// ---------------------------------------------------------------------------
void ScreenPhoneReport(App& a) {
    a.applyRange();
    int seg = ui::Segmented("phone_rpt_seg", {"今日", "本周", "本月"}, a.reportRange > 2 ? 2 : a.reportRange);
    if (seg >= 0 && seg != a.reportRange) {
        a.reportRange = seg;
        a.applyRange();
    }

    SalesSummary sum = a.store.salesSummary(a.rangeFrom(), a.rangeTo());
    ui::BeginCardAuto("ph_rpt_sum");
    ui::KeyValueRow("区间销售额", "¥" + ui::Money(sum.phoneSales));
    ui::KeyValueRow("区间毛利", "¥" + ui::Money(sum.phoneProfit), ui::colSuccess);
    ui::KeyValueRow("出库台数", std::to_string(sum.phoneCount) + " 台");
    if (sum.phoneCount > 0) {
        ui::KeyValueRow("单台均价", "¥" + ui::Money(sum.phoneSales / sum.phoneCount));
    }
    ui::EndCardAuto();

    ui::Gap(4);
    ui::Text("每日销售额", ui::colText, 2);
    ui::Gap(6);
    {
        auto data = a.store.salesByDay(a.rangeFrom(), a.rangeTo());
        std::vector<float> vals;
        std::vector<std::string> labels;
        size_t start = data.size() > 10 ? data.size() - 10 : 0;
        for (size_t i = start; i < data.size(); i++) {
            vals.push_back((float)data[i].second);
            labels.push_back(data[i].first.size() >= 10 ? data[i].first.substr(8, 2) : data[i].first);
        }
        if (vals.empty()) ui::Empty("区间内暂无销售数据");
        else ui::BarChart(vals, labels, 140);
    }

    ui::Gap(10);
    ui::Text("出库明细", ui::colText, 2);
    ui::Gap(8);
    auto sold = a.store.phones(1, "", 500);
    int n = 0;
    for (auto& d : sold) {
        std::string day = Store::dateOf(d.outTime);
        if (day < a.rangeFrom() || day > a.rangeTo()) continue;
        double profit = d.outPrice - d.inPrice - d.prepCost;
        std::string sub = day + " · " + a.partnerName(d.outCustomer) + " · " + d.outPay;
        if (ui::ListRow(devTitle(d), sub, "+¥" + ui::Money(profit), ui::colSuccess)) a.go(ScPhoneDetail, d.id);
        n++;
    }
    if (n == 0) ui::Empty("区间内暂无出库记录");
    ui::Gap(20);
}
