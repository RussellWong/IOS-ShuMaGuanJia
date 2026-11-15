// 财务中心：日常收付款、流水、应收应付、账户
#include "ui/app.h"

#include "imgui.h"

static std::vector<std::string> accountNames(App& a) {
    std::vector<std::string> names;
    for (auto& acc : a.store.accounts()) names.push_back(acc.name);
    if (names.empty()) names.push_back("现金");
    return names;
}

void ScreenFinanceHome(App& a) {
    a.applyRange();
    SalesSummary sum = a.store.salesSummary(a.rangeFrom(), a.rangeTo());
    auto accs = a.store.accounts();
    double totalBalance = 0;
    for (auto& acc : accs) totalBalance += acc.balance;

    ui::Gap(4);
    ui::BeginCardAuto("fin_head");
    ui::KeyValueRow("账户总余额", "¥" + ui::Money(totalBalance), ui::colPrimary);
    ui::KeyValueRow("区间收入", "¥" + ui::Money(sum.totalIncome), ui::colSuccess);
    ui::KeyValueRow("区间利润", "¥" + ui::Money(sum.totalProfit));
    ui::KeyValueRow("结算账户", std::to_string(accs.size()) + " 个");
    ui::EndCardAuto();

    ui::Gap(6);
    if (ui::PrimaryButton("日常收付款", 46)) {
        a.fFinType = 1;
        a.fFinCategory.clear();
        a.fFinAmount.clear();
        a.fFinAccount = "现金";
        a.fFinRemark.clear();
        a.go(ScFinanceAdd);
    }
    if (ui::SoftButton("财务流水", 46)) a.go(ScFinanceFlows);
    if (ui::SoftButton("应收应付", 46)) a.go(ScReceivables);
    if (ui::SoftButton("结算账户", 46)) a.go(ScAccounts);

    ui::Gap(10);
    ui::Text("账户余额", ui::colText, 2);
    ui::Gap(8);
    for (auto& acc : accs) {
        ui::ListRow(acc.name, "结算账户", "¥" + ui::Money(acc.balance),
                    acc.balance < 0 ? ui::colDanger : ui::colText);
    }

    ui::Gap(10);
    ui::Text("最近流水", ui::colText, 2);
    ui::Gap(8);
    auto flows = a.store.finances("", "", 8);
    if (flows.empty()) {
        ui::Empty("暂无流水");
    } else {
        for (auto& f : flows) {
            std::string sub = f.category + " · " + f.account + " · " + f.createdAt.substr(5, 11);
            std::string amt = (f.type == 1 ? "+" : "-") + ui::Money(f.amount);
            ImGui::PushID(f.id);
            ui::ListRow(f.remark.empty() ? f.category : f.remark, sub, amt,
                        f.type == 1 ? ui::colSuccess : ui::colDanger);
            ImGui::PopID();
        }
    }
    ui::Gap(20);
}

void ScreenFinanceAdd(App& a) {
    ui::Gap(2);
    int seg = ui::Segmented("fin_type", {"收入", "支出"}, a.fFinType - 1);
    if (seg >= 0) {
        a.fFinType = seg + 1;
        a.fFinCategory.clear();
    }

    std::vector<std::string> cats = a.fFinType == 1
        ? std::vector<std::string>{"手机销售", "维修收入", "配件销售", "次卡收入", "欠款回收", "其他收入"}
        : std::vector<std::string>{"手机采购", "配件采购", "欠款支付", "房租", "工资", "水电", "其他支出"};

    ui::FieldLabel("收支类别");
    {
        int sel = ui::Dropdown("fin_cat", a.fFinCategory, "选择类别", cats);
        if (sel >= 0) a.fFinCategory = cats[sel];
    }
    ui::FieldLabel("金额（元）");
    ui::TextField("fin_amount", a.fFinAmount, "0.00", true);
    ui::FieldLabel("结算账户");
    {
        std::vector<std::string> names = accountNames(a);
        int cur = 0;
        for (int i = 0; i < (int)names.size(); i++) if (names[i] == a.fFinAccount) cur = i;
        int sel = ui::Dropdown("fin_acc", names[cur], "选择账户", names);
        if (sel >= 0) a.fFinAccount = names[sel];
    }
    ui::FieldLabel("备注");
    ui::TextField("fin_remark", a.fFinRemark, "选填，例如 支付房租");

    ui::Gap(14);
    if (ui::PrimaryButton("确认保存")) {
        double amount = parseD(a.fFinAmount, 0);
        if (a.fFinCategory.empty()) ui::Toast("请选择收支类别");
        else if (amount <= 0) ui::Toast("请输入金额");
        else {
            a.store.addFinance(a.fFinType, a.fFinCategory, amount, a.fFinAccount,
                               "手动", 0, a.fFinRemark);
            ui::Toast("已保存");
            a.back();
        }
    }
    ui::Gap(24);
}

void ScreenFinanceFlows(App& a) {
    ui::Gap(2);
    int seg = ui::Segmented("flow_seg", {"今日", "本周", "本月", "全部"}, a.reportRange > 3 ? 2 : a.reportRange);
    if (seg >= 0) a.reportRange = seg;

    std::string from, to;
    if (a.reportRange < 3) {
        a.applyRange();
        from = a.rangeFrom();
        to = a.rangeTo();
    }
    ui::SearchBar("flow_search", a.sFlow, "搜索类别/备注/账户");

    auto flows = a.store.finances(from, to, 500);
    double income = 0, expense = 0;
    for (auto& f : flows) {
        if (f.type == 1) income += f.amount;
        else expense += f.amount;
    }
    ui::BeginCardAuto("flow_sum");
    ui::KeyValueRow("收入合计", "¥" + ui::Money(income), ui::colSuccess);
    ui::KeyValueRow("支出合计", "¥" + ui::Money(expense), ui::colDanger);
    ui::KeyValueRow("净额", "¥" + ui::Money(income - expense), ui::colPrimary);
    ui::EndCardAuto();

    int n = 0;
    for (auto& f : flows) {
        if (!a.sFlow.empty() && f.category.find(a.sFlow) == std::string::npos &&
            f.remark.find(a.sFlow) == std::string::npos &&
            f.account.find(a.sFlow) == std::string::npos) continue;
        std::string sub = f.category + " · " + f.account + " · " + f.createdAt;
        std::string amt = (f.type == 1 ? "+" : "-") + ui::Money(f.amount);
        ImGui::PushID(f.id);
        ui::ListRow(f.remark.empty() ? f.category : f.remark, sub, amt,
                    f.type == 1 ? ui::colSuccess : ui::colDanger);
        ImGui::PopID();
        n++;
    }
    if (n == 0) ui::Empty("该区间暂无流水");
    ui::Gap(10);
    if (ui::PrimaryButton("+ 记一笔")) a.go(ScFinanceAdd);
    ui::Gap(24);
}

void ScreenReceivables(App& a) {
    auto list = a.store.receivables();
    double recv = 0, pay = 0;
    for (auto& p : list) {
        recv += p.receivable;
        pay += p.payable;
    }

    ui::Gap(2);
    ui::BeginCardAuto("recv_sum");
    ui::KeyValueRow("客户应收合计", "¥" + ui::Money(recv), ui::colDanger);
    ui::KeyValueRow("供应商应付合计", "¥" + ui::Money(pay), ui::colWarn);
    ui::KeyValueRow("欠款单位", std::to_string(list.size()) + " 家");
    ui::EndCardAuto();

    if (list.empty()) {
        ui::Empty("暂无应收应付");
        ui::Gap(10);
        if (ui::PrimaryButton("+ 记一笔收付款")) a.go(ScFinanceAdd);
        ui::Gap(24);
        return;
    }

    for (auto& p : list) {
        if (a.fSettlePartner == p.id) {
            ui::Gap(4);
            ui::BeginCardAuto("settle_card");
            ui::Text("欠款核销 · " + p.name, ui::colText, 2);
            ui::Gap(8);
            ui::KeyValueRow("应收", "¥" + ui::Money(p.receivable), ui::colDanger);
            ui::KeyValueRow("应付", "¥" + ui::Money(p.payable), ui::colWarn);
            ui::FieldLabel("核销金额（元）");
            ui::TextField("stl_amount", a.fSettleAmount, "0.00", true);
            ui::FieldLabel("结算账户");
            {
                std::vector<std::string> names = accountNames(a);
                int cur = 0;
                for (int i = 0; i < (int)names.size(); i++) if (names[i] == a.fSettleAccount) cur = i;
                int sel = ui::Dropdown("stl_acc", names[cur], "选择账户", names);
                if (sel >= 0) a.fSettleAccount = names[sel];
            }
            ui::FieldLabel("备注");
            ui::TextField("stl_remark", a.fSettleRemark, "选填");
            ui::Gap(10);
            if (ui::PrimaryButton("确认核销", 44)) {
                double amt = parseD(a.fSettleAmount, 0);
                if (amt <= 0) ui::Toast("请输入核销金额");
                else {
                    a.store.settle(p.id, amt, a.fSettleAccount, a.fSettleRemark);
                    ui::Toast("核销成功");
                    a.fSettlePartner = 0;
                }
            }
            if (ui::SoftButton("取消", 40)) a.fSettlePartner = 0;
            ui::EndCardAuto();
            ui::Gap(6);
            continue;
        }

        std::string right = p.receivable > 0.009 ? ("应收 ¥" + ui::Money(p.receivable))
                                                 : ("应付 ¥" + ui::Money(p.payable));
        ImU32 rc = p.receivable > 0.009 ? ui::colDanger : ui::colWarn;
        ImGui::PushID(p.id);
        if (ui::ListRow(p.name, p.phone.empty() ? "未填写电话" : p.phone, right, rc, "核销", ui::colPrimary)) {
            a.fSettlePartner = p.id;
            a.fSettleAmount = ui::Num(p.receivable > 0 ? p.receivable : p.payable);
            a.fSettleAccount = "现金";
            a.fSettleRemark.clear();
        }
        ImGui::PopID();
    }
    ui::Gap(10);
    if (ui::PrimaryButton("+ 记一笔收付款")) a.go(ScFinanceAdd);
    ui::Gap(24);
}
