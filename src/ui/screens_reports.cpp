// 报表中心：销售、利润、库存、员工、维修、配件
#include "ui/app.h"

#include "imgui.h"

#include <algorithm>

static void rangeBar(App& a, const char* id) {
    int seg = ui::Segmented(id, {"今日", "本周", "本月"}, a.reportRange > 2 ? 2 : a.reportRange);
    if (seg >= 0 && seg != a.reportRange) {
        a.reportRange = seg;
        a.applyRange();
    }
}

static void dailyChart(App& a, bool profit) {
    auto data = profit ? a.store.profitByDay(a.rangeFrom(), a.rangeTo())
                       : a.store.salesByDay(a.rangeFrom(), a.rangeTo());
    std::vector<float> vals;
    std::vector<std::string> labels;
    size_t start = data.size() > 10 ? data.size() - 10 : 0;
    for (size_t i = start; i < data.size(); i++) {
        vals.push_back((float)data[i].second);
        labels.push_back(data[i].first.size() >= 10 ? data[i].first.substr(8, 2) : data[i].first);
    }
    if (vals.empty()) ui::Empty("区间内暂无数据");
    else ui::BarChart(vals, labels, 150, profit ? ui::colSuccess : 0);
}

void ScreenReportSales(App& a) {
    a.applyRange();
    rangeBar(a, "rpt_sales_seg");
    SalesSummary s = a.store.salesSummary(a.rangeFrom(), a.rangeTo());

    ui::BeginCardAuto("rpt_sales_sum");
    ui::KeyValueRow("销售总额", "¥" + ui::Money(s.totalIncome), ui::colPrimary);
    ui::KeyValueRow("手机销售", "¥" + ui::Money(s.phoneSales) + " · " + std::to_string(s.phoneCount) + " 台");
    ui::KeyValueRow("维修收入", "¥" + ui::Money(s.repairIncome) + " · " + std::to_string(s.repairCount) + " 单");
    ui::KeyValueRow("配件销售", "¥" + ui::Money(s.partsSales) + " · " + std::to_string(s.partQty) + " 件");
    ui::KeyValueRow("次卡收入", "¥" + ui::Money(s.cardIncome) + " · " + std::to_string(s.cardCount) + " 张");
    ui::EndCardAuto();

    ui::Gap(4);
    ui::Text("每日销售额", ui::colText, 2);
    ui::Gap(6);
    dailyChart(a, false);

    ui::Gap(10);
    ui::Text("收入构成", ui::colText, 2);
    ui::Gap(8);
    double total = s.totalIncome > 0 ? s.totalIncome : 1;
    ui::HBarRow("手机", (float)(s.phoneSales / total), "¥" + ui::Money(s.phoneSales), ui::colPrimary);
    ui::HBarRow("维修", (float)(s.repairIncome / total), "¥" + ui::Money(s.repairIncome), ui::colWarn);
    ui::HBarRow("配件", (float)(s.partsSales / total), "¥" + ui::Money(s.partsSales), ui::colSuccess);
    ui::HBarRow("次卡", (float)(s.cardIncome / total), "¥" + ui::Money(s.cardIncome), IM_COL32(139, 92, 246, 255));
    ui::Gap(20);
}

void ScreenReportProfit(App& a) {
    a.applyRange();
    rangeBar(a, "rpt_profit_seg");
    SalesSummary s = a.store.salesSummary(a.rangeFrom(), a.rangeTo());

    ui::BeginCardAuto("rpt_profit_sum");
    ui::KeyValueRow("利润合计", "¥" + ui::Money(s.totalProfit), ui::colSuccess);
    ui::KeyValueRow("手机毛利", "¥" + ui::Money(s.phoneProfit));
    ui::KeyValueRow("维修毛利", "¥" + ui::Money(s.repairProfit));
    ui::KeyValueRow("配件毛利", "¥" + ui::Money(s.partsProfit));
    if (s.totalIncome > 0) {
        double rate = s.totalProfit / s.totalIncome * 100.0;
        ui::KeyValueRow("毛利率", ui::Num(rate) + "%", ui::colPrimary);
    }
    ui::EndCardAuto();

    ui::Gap(4);
    ui::Text("每日利润", ui::colText, 2);
    ui::Gap(6);
    dailyChart(a, true);

    ui::Gap(10);
    ui::Text("利润构成", ui::colText, 2);
    ui::Gap(8);
    double total = s.totalProfit > 0 ? s.totalProfit : 1;
    ui::HBarRow("手机", (float)(s.phoneProfit / total), "¥" + ui::Money(s.phoneProfit), ui::colPrimary);
    ui::HBarRow("维修", (float)(s.repairProfit / total), "¥" + ui::Money(s.repairProfit), ui::colWarn);
    ui::HBarRow("配件", (float)(s.partsProfit / total), "¥" + ui::Money(s.partsProfit), ui::colSuccess);
    ui::Gap(20);
}

void ScreenReportStock(App& a) {
    auto phones = a.store.phones(0, "", 5000);
    double phoneCost = 0;
    std::vector<std::pair<std::string, int>> byBrand;
    for (auto& d : phones) {
        phoneCost += d.inPrice + d.prepCost;
        bool found = false;
        for (auto& b : byBrand) {
            if (b.first == d.brand) { b.second++; found = true; break; }
        }
        if (!found) byBrand.push_back({d.brand.empty() ? "其他" : d.brand, 1});
    }
    std::sort(byBrand.begin(), byBrand.end(),
              [](const std::pair<std::string, int>& x, const std::pair<std::string, int>& y) {
                  return x.second > y.second;
              });

    auto parts = a.store.parts();
    double partCost = 0;
    int partQty = 0;
    for (auto& p : parts) {
        partCost += p.cost * p.stock;
        partQty += p.stock;
    }

    ui::Gap(2);
    ui::BeginCardAuto("rpt_stock_sum");
    ui::KeyValueRow("在库设备", std::to_string(phones.size()) + " 台");
    ui::KeyValueRow("设备库存成本", "¥" + ui::Money(phoneCost), ui::colPrimary);
    ui::KeyValueRow("配件库存", std::to_string(partQty) + " 件");
    ui::KeyValueRow("配件库存成本", "¥" + ui::Money(partCost), ui::colSuccess);
    ui::KeyValueRow("库存总成本", "¥" + ui::Money(phoneCost + partCost), ui::colText);
    ui::EndCardAuto();

    ui::Gap(4);
    ui::Text("设备品牌分布", ui::colText, 2);
    ui::Gap(8);
    if (byBrand.empty()) {
        ui::Empty("暂无在库设备");
    } else {
        int maxN = byBrand.front().second > 0 ? byBrand.front().second : 1;
        for (auto& b : byBrand) {
            ui::HBarRow(b.first, (float)b.second / (float)maxN, std::to_string(b.second) + " 台");
        }
    }

    ui::Gap(12);
    ui::Text("配件库存 TOP", ui::colText, 2);
    ui::Gap(8);
    std::vector<Part> ps = parts;
    std::sort(ps.begin(), ps.end(), [](const Part& x, const Part& y) {
        return x.cost * x.stock > y.cost * y.stock;
    });
    int n = 0;
    for (auto& p : ps) {
        if (n >= 8) break;
        ui::ListRow(p.name, std::to_string(p.stock) + " " + p.unit + " · 成本 ¥" + ui::Money(p.cost),
                    "¥" + ui::Money(p.cost * p.stock), ui::colSub);
        n++;
    }
    if (n == 0) ui::Empty("暂无配件库存");
    ui::Gap(20);
}

void ScreenReportStaff(App& a) {
    a.applyRange();
    rangeBar(a, "rpt_staff_seg");
    auto list = a.store.staffPerf(a.rangeFrom(), a.rangeTo());
    if (list.empty()) ui::Empty("暂无员工业绩数据");

    ui::Gap(2);
    int rank = 1;
    for (auto& s : list) {
        std::string sub = "维修 " + std::to_string(s.repairCount) + " 单 / 手机 " +
                          std::to_string(s.phoneCount) + " 台";
        std::string right = "¥" + ui::Money(s.repairAmount + s.phoneSales);
        ImGui::PushID(s.id);
        ui::ListRow(std::to_string(rank) + ". " + s.name, sub, right, ui::colPrimary);
        ImGui::PopID();
        rank++;
    }
    ui::Gap(10);
    ui::Hint("业绩按维修工单金额与手机销售金额统计，区间与报表日期一致");
    ui::Gap(20);
}

void ScreenReportRepair(App& a) {
    a.applyRange();
    rangeBar(a, "rpt_repair_seg");
    SalesSummary s = a.store.salesSummary(a.rangeFrom(), a.rangeTo());

    auto list = a.store.repairs(-1, "", 1000);
    int waiting = 0, doing = 0, toDeliver = 0, delivered = 0, canceled = 0;
    double totalFee = 0;
    for (auto& r : list) {
        if (r.status == 0) waiting++;
        else if (r.status == 1) doing++;
        else if (r.status == 2) toDeliver++;
        else if (r.status == 3) delivered++;
        else canceled++;
        totalFee += r.totalFee;
    }

    ui::Gap(2);
    ui::BeginCardAuto("rpt_repair_sum");
    ui::KeyValueRow("区间维修收入", "¥" + ui::Money(s.repairIncome), ui::colSuccess);
    ui::KeyValueRow("区间维修单量", std::to_string(s.repairCount) + " 单");
    ui::KeyValueRow("累计工单金额", "¥" + ui::Money(totalFee));
    ui::EndCardAuto();

    ui::Gap(4);
    ui::Text("工单状态分布", ui::colText, 2);
    ui::Gap(8);
    ui::BeginCardAuto("rpt_repair_state");
    ui::KeyValueRow("待接单", std::to_string(waiting) + " 单", waiting ? ui::colWarn : ui::colText);
    ui::KeyValueRow("维修中", std::to_string(doing) + " 单", doing ? ui::colPrimary : ui::colText);
    ui::KeyValueRow("待交付", std::to_string(toDeliver) + " 单", toDeliver ? ui::colSuccess : ui::colText);
    ui::KeyValueRow("已交付", std::to_string(delivered) + " 单");
    ui::KeyValueRow("已取消", std::to_string(canceled) + " 单", ui::colSub);
    ui::EndCardAuto();

    ui::Gap(4);
    ui::Text("最近工单", ui::colText, 2);
    ui::Gap(8);
    int n = 0;
    for (auto& r : list) {
        if (n >= 10) break;
        if (ui::ListRow(r.orderNo, r.model + " · " + (r.customerName.empty() ? "散客" : r.customerName),
                        "¥" + ui::Money(r.totalFee), ui::colSub, "", 0, 60))
            a.go(ScRepairDetail, r.id);
        n++;
    }
    if (n == 0) ui::Empty("暂无维修单");
    ui::Gap(20);
}

void ScreenReportParts(App& a) {
    a.applyRange();
    rangeBar(a, "rpt_parts_seg");
    auto rank = a.store.partsRanking(a.rangeFrom(), a.rangeTo());

    ui::Gap(2);
    ui::BeginCardAuto("rpt_parts_sum");
    SalesSummary s = a.store.salesSummary(a.rangeFrom(), a.rangeTo());
    ui::KeyValueRow("配件销售额", "¥" + ui::Money(s.partsSales), ui::colPrimary);
    ui::KeyValueRow("配件毛利", "¥" + ui::Money(s.partsProfit), ui::colSuccess);
    ui::KeyValueRow("销售数量", std::to_string(s.partQty) + " 件");
    ui::EndCardAuto();

    ui::Gap(4);
    ui::Text("销量排行", ui::colText, 2);
    ui::Gap(8);
    if (rank.empty()) {
        ui::Empty("该区间暂无配件销售");
    } else {
        int maxQty = 0;
        for (auto& r : rank) maxQty = std::max(maxQty, r.second.first);
        if (maxQty <= 0) maxQty = 1;
        for (auto& r : rank) {
            ui::HBarRow(r.first, (float)r.second.first / (float)maxQty,
                        std::to_string(r.second.first) + " 件 / ¥" + ui::Money(r.second.second),
                        ui::colPrimary);
        }
    }
    ui::Gap(20);
}
