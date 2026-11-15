// 次卡业务：模板、开卡、核销、会员卡
#include "ui/app.h"

#include "imgui.h"

void ScreenCardHome(App& a) {
    auto tpls = a.store.cardTpls();
    auto cards = a.store.cards();
    int active = 0, remainTotal = 0, timesTotal = 0;
    double income = 0;
    for (auto& c : cards) {
        if (c.status == 1) active++;
        remainTotal += c.remain;
        timesTotal += c.times;
        income += c.price;
    }

    ui::Gap(4);
    ui::BeginCardAuto("card_head");
    ui::KeyValueRow("次卡模板", std::to_string(tpls.size()) + " 个");
    ui::KeyValueRow("有效会员卡", std::to_string(active) + " 张");
    ui::KeyValueRow("剩余可用次数", std::to_string(remainTotal) + " 次", ui::colPrimary);
    ui::KeyValueRow("累计开卡次数", std::to_string(timesTotal) + " 次");
    ui::KeyValueRow("累计开卡金额", "¥" + ui::Money(income), ui::colSuccess);
    ui::EndCardAuto();

    ui::Gap(6);
    if (ui::PrimaryButton("次卡开卡", 46)) a.openCardOpen();
    if (ui::SoftButton("会员次卡列表", 46)) a.go(ScCardList);
    if (ui::SoftButton("+ 新增次卡模板", 46)) a.openCardTplForm(0);

    ui::Gap(10);
    ui::Text("次卡模板", ui::colText, 2);
    ui::Gap(8);
    if (tpls.empty()) {
        ui::Empty("还没有模板，先创建一个吧");
    } else {
        for (auto& t : tpls) {
            std::string sub = std::to_string(t.times) + " 次 · " +
                              (t.validDays > 0 ? std::to_string(t.validDays) + " 天有效" : "长期有效");
            ImGui::PushID(t.id);
            if (ui::ListRow(t.name, sub, "¥" + ui::Money(t.price), ui::colSub)) a.openCardTplForm(t.id);
            ImGui::PopID();
        }
    }
    ui::Gap(20);
}

void ScreenCardTplForm(App& a) {
    ui::Gap(2);
    ui::FieldLabel("模板名称");
    ui::TextField("tpl_name", a.fTpl.name, "例如 洗车10次卡");
    ui::FieldLabel("包含次数");
    ui::TextField("tpl_times", a.fTplTimes, "10", true, true);
    ui::FieldLabel("售价（元）");
    ui::TextField("tpl_price", a.fTplPrice, "0.00", true);
    ui::FieldLabel("有效期（天，0 表示长期有效）");
    ui::TextField("tpl_days", a.fTplDays, "365", true, true);
    ui::FieldLabel("备注");
    ui::TextField("tpl_remark", a.fTpl.remark, "选填");

    ui::Gap(14);
    if (ui::PrimaryButton(a.fTplId > 0 ? "保存修改" : "保存模板")) {
        if (a.fTpl.name.empty()) {
            ui::Toast("请填写模板名称");
        } else if (parseI(a.fTplTimes, 0) <= 0) {
            ui::Toast("次数必须大于 0");
        } else {
            a.fTpl.times = parseI(a.fTplTimes, 0);
            a.fTpl.price = parseD(a.fTplPrice, 0);
            a.fTpl.validDays = parseI(a.fTplDays, 0);
            a.store.saveCardTpl(a.fTpl);
            ui::Toast("已保存");
            a.back();
        }
    }
    if (a.fTplId > 0) {
        ui::Gap(4);
        if (ui::SoftButton("用该模板开卡")) {
            a.fCardTplId = a.fTpl.id;
            a.fCardPartnerId = 0;
            a.go(ScCardOpen);
        }
        ui::Gap(4);
        if (ui::DangerButton("删除该模板")) {
            if (ui::Confirm("del_tpl", "删除确认", "删除模板不影响已开的卡，确定删除？")) {
                a.store.removeCardTpl(a.fTplId);
                ui::Toast("已删除");
                a.stack.pop_back();
                a.back();
            }
        }
    }
    ui::Gap(24);
}

void ScreenCardOpen(App& a) {
    auto tpls = a.store.cardTpls();
    ui::Gap(2);
    ui::FieldLabel("选择次卡模板");
    {
        std::vector<int> ids;
        std::vector<std::string> names;
        for (auto& t : tpls) {
            ids.push_back(t.id);
            names.push_back(t.name + " · " + std::to_string(t.times) + " 次");
        }
        if (names.empty()) {
            ui::Hint("暂无模板，请先在「次卡首页」新增模板");
        } else {
            int cur = 0;
            for (int i = 0; i < (int)ids.size(); i++) if (ids[i] == a.fCardTplId) cur = i;
            int sel = ui::Dropdown("card_tpl", names[cur], "选择模板", names);
            if (sel >= 0) a.fCardTplId = ids[sel];
        }
    }
    ui::FieldLabel("选择客户");
    {
        std::vector<int> ids;
        std::vector<std::string> names;
        ids.push_back(0);
        names.push_back("散客");
        for (auto& p : a.store.partners(1)) {
            ids.push_back(p.id);
            names.push_back(p.name);
        }
        int cur = 0;
        for (int i = 0; i < (int)ids.size(); i++) if (ids[i] == a.fCardPartnerId) cur = i;
        int sel = ui::Dropdown("card_cust", names[cur], "选择客户", names);
        if (sel >= 0) a.fCardPartnerId = ids[sel];
    }

    CardTpl t = a.store.cardTpl(a.fCardTplId);
    if (t.id > 0) {
        ui::Gap(6);
        ui::BeginCardAuto("card_preview");
        ui::KeyValueRow("模板", t.name);
        ui::KeyValueRow("次数", std::to_string(t.times) + " 次");
        ui::KeyValueRow("售价", "¥" + ui::Money(t.price), ui::colSuccess);
        ui::KeyValueRow("有效期", t.validDays > 0 ? std::to_string(t.validDays) + " 天" : "长期有效");
        ui::EndCardAuto();
    }

    ui::Gap(12);
    if (ui::PrimaryButton("确认开卡")) {
        if (a.fCardTplId <= 0) {
            ui::Toast("请选择次卡模板");
        } else {
            int id = a.store.openCard(a.fCardTplId, a.fCardPartnerId);
            if (id > 0) {
                ui::Toast("开卡成功");
                a.back();
                a.go(ScCardDetail, id);
            } else {
                ui::Toast("开卡失败");
            }
        }
    }
    ui::Gap(24);
}

void ScreenCardDetail(App& a, int id) {
    Card c = a.store.card(id);
    if (c.id == 0) {
        ui::Empty("次卡不存在");
        return;
    }

    ui::Gap(2);
    ui::Text(c.tplName.empty() ? "会员次卡" : c.tplName, ui::colText, 3);
    ui::Gap(4);
    ui::StatusTag(c.status, 3);
    ui::Gap(10);

    ui::BeginCardAuto("card_info");
    ui::KeyValueRow("卡号", c.cardNo);
    ui::KeyValueRow("客户", c.partnerName.empty() ? "散客" : c.partnerName);
    ui::KeyValueRow("总次数", std::to_string(c.times) + " 次");
    ui::KeyValueRow("剩余次数", std::to_string(c.remain) + " 次", ui::colPrimary);
    ui::KeyValueRow("开卡金额", "¥" + ui::Money(c.price));
    ui::KeyValueRow("开卡时间", c.createdAt);
    ui::KeyValueRow("到期时间", c.expireAt.empty() ? "长期有效" : c.expireAt);
    ui::EndCardAuto();

    if (c.status != 2) {
        ui::Gap(4);
        ui::FieldLabel("本次核销次数");
        ui::TextField("card_use", a.fUseTimes, "1", true, true);
        ui::FieldLabel("备注");
        ui::TextField("card_use_remark", a.fUseRemark, "例如 洗车 / 贴膜");
        if (ui::PrimaryButton("确认核销")) {
            int times = parseI(a.fUseTimes, 1);
            if (times <= 0) ui::Toast("请输入核销次数");
            else if (times > c.remain) ui::Toast("剩余次数不足");
            else {
                a.store.useCard(c.id, times, a.fUseRemark);
                ui::Toast("核销成功，剩余 " + std::to_string(c.remain - times) + " 次");
            }
        }
    }

    ui::Gap(10);
    ui::Text("使用记录", ui::colText, 2);
    ui::Gap(8);
    auto recs = a.store.cardRecords(c.id);
    if (recs.empty()) {
        ui::Empty("暂无记录");
    } else {
        for (auto& r : recs) {
            std::string type = r.type == 1 ? "开卡" : (r.type == 2 ? "核销" : "退卡");
            std::string right = (r.type == 1 ? "+" : "-") + std::to_string(r.times);
            ImGui::PushID(r.id);
            ui::ListRow(type, r.createdAt + (r.remark.empty() ? "" : " · " + r.remark), right,
                        r.type == 1 ? ui::colPrimary : ui::colWarn);
            ImGui::PopID();
        }
    }
    ui::Gap(20);
}

void ScreenCardList(App& a) {
    ui::Gap(2);
    ui::SearchBar("card_search", a.sCard, "搜索卡号/客户/模板");
    int seg = ui::Segmented("card_seg", {"全部", "有效", "已用完"}, a.cardFilter);
    if (seg >= 0) a.cardFilter = seg;

    auto list = a.store.cards(a.sCard);
    int n = 0;
    for (auto& c : list) {
        if (a.cardFilter == 1 && c.status != 1) continue;
        if (a.cardFilter == 2 && c.status == 1) continue;
        std::string sub = c.cardNo + " · " + (c.partnerName.empty() ? "散客" : c.partnerName);
        std::string right = "剩 " + std::to_string(c.remain) + "/" + std::to_string(c.times);
        ImGui::PushID(c.id);
        if (ui::ListRow(c.tplName.empty() ? "会员次卡" : c.tplName, sub, right,
                        c.status == 1 ? ui::colPrimary : ui::colSub))
            a.go(ScCardDetail, c.id);
        ImGui::PopID();
        n++;
    }
    if (n == 0) ui::Empty("暂无次卡");
    ui::Gap(10);
    if (ui::PrimaryButton("+ 次卡开卡")) a.openCardOpen();
    ui::Gap(24);
}
