// 维修业务：开单、接单、维修、交付、列表
#include "ui/app.h"

#include "imgui.h"

#include <algorithm>

static std::string repairTitle(const Repair& r) {
    std::string s = r.model.empty() ? "未填机型" : r.model;
    if (!r.customerName.empty()) s += " · " + r.customerName;
    return s;
}

static std::vector<std::string> staffOptions(App& a, std::vector<int>& ids) {
    ids.clear();
    std::vector<std::string> names;
    for (auto& e : a.store.employees(true)) {
        ids.push_back(e.id);
        names.push_back(e.name + "（" + e.role + "）");
    }
    if (names.empty()) {
        ids.push_back(1);
        names.push_back("老板");
    }
    return names;
}

void ScreenRepairHome(App& a) {
    auto all = a.store.repairs(100, "", 1000);
    int waiting = 0, doing = 0, toDeliver = 0;
    for (auto& r : all) {
        if (r.status == 0) waiting++;
        else if (r.status == 1) doing++;
        else if (r.status == 2) toDeliver++;
    }

    ui::Gap(4);
    ui::BeginCardAuto("rp_head");
    ui::KeyValueRow("待接单", std::to_string(waiting) + " 单", waiting ? ui::colWarn : ui::colText);
    ui::KeyValueRow("维修中", std::to_string(doing) + " 单", doing ? ui::colPrimary : ui::colText);
    ui::KeyValueRow("待交付", std::to_string(toDeliver) + " 单", toDeliver ? ui::colSuccess : ui::colText);
    ui::EndCardAuto();

    ui::Gap(6);
    if (ui::PrimaryButton("维修开单", 46)) a.openRepairNew();
    if (ui::SoftButton("维修单列表", 46)) a.go(ScRepairList);
    if (ui::SoftButton("维修报表", 46)) a.go(ScReportRepair);

    ui::Gap(10);
    ui::Text("处理中的维修单", ui::colText, 2);
    ui::Gap(8);
    auto list = a.store.repairs(100, "", 8);
    if (list.empty()) {
        ui::Empty("暂无维修单，点击「维修开单」开始");
    } else {
        for (auto& r : list) {
            std::string sub = r.orderNo + " · " + a.employeeName(r.technician);
            if (ui::ListRow(repairTitle(r), sub, "¥" + ui::Money(r.totalFee), ui::colSub, "", 0, 64))
                a.go(ScRepairDetail, r.id);
        }
    }
    ui::Gap(20);
}

void ScreenRepairNew(App& a) {
    ui::Gap(2);
    ui::FieldLabel("客户");
    {
        std::vector<int> ids;
        std::vector<std::string> names;
        ids.push_back(0);
        names.push_back("散客（手动填写）");
        for (auto& p : a.store.partners(1)) {
            ids.push_back(p.id);
            names.push_back(p.name);
        }
        int cur = 0;
        for (int i = 0; i < (int)ids.size(); i++) if (ids[i] == a.fRepair.partner) cur = i;
        int sel = ui::Dropdown("rp_cust", names[cur], "选择客户", names);
        if (sel >= 0) {
            a.fRepair.partner = ids[sel];
            if (sel > 0) {
                Partner p = a.store.partner(ids[sel]);
                a.fRepair.customerName = p.name;
                a.fRepair.customerPhone = p.phone;
            }
        }
    }
    ui::FieldLabel("客户姓名");
    ui::TextField("rp_name", a.fRepair.customerName, "请输入姓名");
    ui::FieldLabel("联系电话");
    ui::TextField("rp_phone", a.fRepair.customerPhone, "请输入手机号", true);
    ui::FieldLabel("维修机型");
    ui::TextField("rp_model", a.fRepair.model, "例如 iPhone 13 Pro");
    ui::FieldLabel("故障描述");
    ui::TextArea("rp_fault", a.fRepair.fault, "请描述故障现象");
    ui::FieldLabel("维修师");
    {
        std::vector<int> ids;
        std::vector<std::string> names = staffOptions(a, ids);
        int cur = 0;
        for (int i = 0; i < (int)ids.size(); i++) if (ids[i] == a.fRepair.technician) cur = i;
        int sel = ui::Dropdown("rp_tech", names[cur], "选择维修师", names);
        if (sel >= 0) a.fRepair.technician = ids[sel];
    }
    ui::FieldLabel("预估总价（元）");
    ui::TextField("rp_total", a.fRepairTotal, "0.00", true);
    ui::FieldLabel("工费（元，可留空自动计算）");
    ui::TextField("rp_labor", a.fRepairLabor, "留空则按提成比例自动计算", true);
    ui::FieldLabel("配件成本（元）");
    ui::TextField("rp_parts", a.fRepairParts, "0.00", true);
    ui::FieldLabel("备注");
    ui::TextField("rp_remark", a.fRepair.remark, "选填");

    ui::Gap(14);
    if (ui::PrimaryButton("确认开单")) {
        a.fRepair.totalFee = parseD(a.fRepairTotal, 0);
        a.fRepair.laborFee = parseD(a.fRepairLabor, 0);
        a.fRepair.partsCost = parseD(a.fRepairParts, 0);
        if (a.fRepair.customerName.empty() && a.fRepair.customerPhone.empty()) {
            ui::Toast("请填写客户信息");
        } else if (a.fRepair.model.empty()) {
            ui::Toast("请填写维修机型");
        } else if (a.fRepair.fault.empty()) {
            ui::Toast("请填写故障描述");
        } else {
            if (a.fRepair.totalFee <= 0) a.fRepair.totalFee = a.fRepair.laborFee + a.fRepair.partsCost;
            a.fRepair.status = a.fRepair.technician > 0 ? 1 : 0;
            if (a.fRepair.status == 1) a.fRepair.startedAt = Store::now();
            int id = a.store.createRepair(a.fRepair);
            ui::Toast("开单成功，单号 " + a.fRepair.orderNo);
            a.back();
            a.go(ScRepairDetail, id);
        }
    }
    ui::Gap(24);
}

void ScreenRepairDetail(App& a, int id) {
    Repair r = a.store.repair(id);
    if (r.id == 0) {
        ui::Empty("维修单不存在");
        return;
    }

    ui::Gap(2);
    ui::Text(r.orderNo, ui::colText, 3);
    ui::Gap(4);
    ui::StatusTag(r.status, 1);
    ui::Gap(10);

    ui::BeginCardAuto("rp_detail");
    ui::KeyValueRow("客户", r.customerName.empty() ? "散客" : r.customerName);
    if (!r.customerPhone.empty()) ui::KeyValueRow("电话", r.customerPhone);
    ui::KeyValueRow("机型", r.model);
    ui::KeyValueRow("故障", r.fault);
    ui::KeyValueRow("维修师", a.employeeName(r.technician));
    ui::KeyValueRow("工费", "¥" + ui::Money(r.laborFee));
    ui::KeyValueRow("配件成本", "¥" + ui::Money(r.partsCost));
    ui::KeyValueRow("应收总额", "¥" + ui::Money(r.totalFee), ui::colText);
    if (r.status == 3) {
        ui::KeyValueRow("实收", "¥" + ui::Money(r.paid), ui::colSuccess);
        ui::KeyValueRow("收款方式", r.payMethod);
        ui::KeyValueRow("交付时间", r.deliveredAt);
    }
    const char* results[] = {"", "修复成功", "修复失败", "放弃维修", "无法维修"};
    if (r.result > 0) ui::KeyValueRow("维修结果", results[r.result > 4 ? 4 : r.result]);
    if (!r.remark.empty()) ui::KeyValueRow("备注", r.remark);
    ui::KeyValueRow("开单时间", r.createdAt);
    ui::EndCardAuto();

    ui::Gap(4);
    if (r.status == 0) {
        ui::Gap(4);
        if (ui::PrimaryButton("接单并开始维修")) {
            a.store.acceptRepair(r.id, r.technician > 0 ? r.technician : 1);
            ui::Toast("已接单");
        }
    }
    if (r.status == 1) {
        ui::Gap(4);
        ui::Text("维修结果", ui::colText, 2);
        ui::Gap(6);
        int seg = ui::Segmented("rp_result", {"修复成功", "修复失败", "放弃维修", "无法维修"},
                                r.result > 0 ? r.result - 1 : 0);
        if (seg >= 0) {
            a.store.finishRepair(r.id, seg + 1, r.remark);
            ui::Toast("已提交维修结果");
        }
    }
    if (r.status == 2) {
        ui::Gap(4);
        ui::FieldLabel("实收金额（元）");
        ui::TextField("rp_paid", a.fRepairPaid, "0.00", true);
        ui::FieldLabel("收款方式");
        {
            std::vector<std::string> pays = {"现金", "微信", "支付宝", "银行卡", "其他"};
            int sel = ui::Dropdown("rp_pay", r.payMethod, "选择收款方式", pays);
            if (sel >= 0) {
                a.fRepair.payMethod = pays[sel];
                r.payMethod = pays[sel];
            }
        }
        if (ui::PrimaryButton("确认交付")) {
            double paid = parseD(a.fRepairPaid, r.totalFee);
            a.store.deliverRepair(r.id, paid, r.payMethod.empty() ? "现金" : r.payMethod);
            ui::Toast("已交付，收款 ¥" + ui::Money(paid));
        }
    }
    if (r.status < 3) {
        ui::Gap(4);
        if (ui::SoftButton("取消该维修单")) {
            if (ui::Confirm("rp_cancel", "取消确认", "确定取消该维修单？")) {
                a.store.cancelRepair(r.id);
                ui::Toast("已取消");
            }
        }
    }
    ui::Gap(24);
}

void ScreenRepairList(App& a) {
    ui::Gap(2);
    ui::SearchBar("repair_search", a.sRepair, "搜索单号/客户/机型");
    int seg = ui::Segmented("repair_seg", {"全部", "处理中", "待交付", "已交付"},
                            a.repairFilter > 3 ? 0 : a.repairFilter);
    if (seg >= 0) a.repairFilter = seg;

    int status = -1;
    if (a.repairFilter == 1) status = 100;
    else if (a.repairFilter == 2) status = 2;
    else if (a.repairFilter == 3) status = 3;

    auto list = a.store.repairs(status, a.sRepair, 500);
    if (list.empty()) {
        ui::Empty("暂无维修单");
    } else {
        for (auto& r : list) {
            std::string sub = r.orderNo + " · " + r.createdAt;
            ImGui::PushID(r.id);
            if (ui::ListRow(repairTitle(r), sub, "¥" + ui::Money(r.totalFee),
                            r.status == 3 ? ui::colSuccess : ui::colSub, "", 0, 64))
                a.go(ScRepairDetail, r.id);
            ImGui::PopID();
        }
    }
    ui::Gap(10);
    if (ui::PrimaryButton("+ 维修开单")) a.openRepairNew();
    ui::Gap(24);
}
