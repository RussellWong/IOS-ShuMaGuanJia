// 配件业务：档案、库存、出入库、流水
#include "ui/app.h"

#include "imgui.h"

#include <algorithm>

static int warnCount(const std::vector<Part>& list) {
    int n = 0;
    for (auto& p : list) if (p.warn > 0 && p.stock <= p.warn) n++;
    return n;
}

void ScreenPartsHome(App& a) {
    auto list = a.store.parts();
    int totalQty = 0;
    double stockValue = 0, saleValue = 0;
    for (auto& p : list) {
        totalQty += p.stock;
        stockValue += p.cost * p.stock;
        saleValue += p.price * p.stock;
    }

    ui::Gap(4);
    ui::BeginCardAuto("pt_head");
    ui::KeyValueRow("配件种类", std::to_string(list.size()) + " 种");
    ui::KeyValueRow("库存数量", std::to_string(totalQty) + " 件");
    ui::KeyValueRow("库存成本", "¥" + ui::Money(stockValue));
    ui::KeyValueRow("库存售价", "¥" + ui::Money(saleValue), ui::colSuccess);
    ui::KeyValueRow("库存预警", std::to_string(warnCount(list)) + " 项",
                    warnCount(list) ? ui::colDanger : ui::colText);
    ui::EndCardAuto();

    ui::Gap(6);
    if (ui::PrimaryButton("配件入库", 46)) a.openPartStock(1);
    if (ui::SoftButton("配件出库", 46)) a.openPartStock(2);
    if (ui::SoftButton("配件库存", 46)) a.go(ScPartList);
    if (ui::SoftButton("库存流水", 46)) a.go(ScPartFlows);
    if (ui::SoftButton("+ 新增配件档案", 46)) a.openPartForm(0);

    ui::Gap(10);
    ui::Text("库存预警", ui::colText, 2);
    ui::Gap(8);
    int n = 0;
    for (auto& p : list) {
        if (!(p.warn > 0 && p.stock <= p.warn)) continue;
        ImGui::PushID(p.id);
        if (ui::ListRow(p.name, "安全库存 " + std::to_string(p.warn) + " " + p.unit,
                        "剩 " + std::to_string(p.stock), ui::colDanger)) {
            a.fPart = p;
            a.openPartForm(p.id);
        }
        ImGui::PopID();
        n++;
    }
    if (n == 0) ui::Empty("暂无预警配件");
    ui::Gap(20);
}

void ScreenPartList(App& a) {
    ui::Gap(2);
    ui::SearchBar("part_search", a.sPart, "搜索配件名称/条码/规格");
    auto list = a.store.parts(a.sPart);
    if (list.empty()) {
        ui::Empty("暂无配件，点击下方新增");
    } else {
        for (auto& p : list) {
            std::string sub = (p.sku.empty() ? "无条码" : p.sku) + (p.model.empty() ? "" : " · " + p.model);
            std::string right = std::to_string(p.stock) + " " + p.unit + " · ¥" + ui::Money(p.price);
            ImU32 rc = (p.warn > 0 && p.stock <= p.warn) ? ui::colDanger : ui::colSub;
            ImGui::PushID(p.id);
            if (ui::ListRow(p.name, sub, right, rc)) a.openPartForm(p.id);
            ImGui::PopID();
        }
    }
    ui::Gap(10);
    if (ui::PrimaryButton("+ 新增配件档案")) a.openPartForm(0);
    ui::Gap(24);
}

void ScreenPartForm(App& a) {
    ui::Gap(2);
    ui::FieldLabel("配件名称");
    ui::TextField("pt_name", a.fPart.name, "例如 电池 / 屏幕总成");
    ui::FieldLabel("条码 / SKU");
    ui::TextField("pt_sku", a.fPart.sku, "可扫码录入", true);
    ui::FieldLabel("规格型号");
    ui::TextField("pt_model", a.fPart.model, "例如 iPhone 13 电池");
    ui::FieldLabel("单位");
    {
        std::vector<std::string> units = {"个", "件", "套", "张", "片", "卷", "米"};
        int sel = ui::Dropdown("pt_unit", a.fPart.unit, "选择单位", units);
        if (sel >= 0) a.fPart.unit = units[sel];
    }
    ui::FieldLabel("成本价（元）");
    ui::TextField("pt_cost", a.fPartCost, "0.00", true);
    ui::FieldLabel("销售价（元）");
    ui::TextField("pt_price", a.fPartPrice, "0.00", true);
    if (a.fPartId == 0) {
        ui::FieldLabel("初始库存");
        ui::TextField("pt_stock", a.fPartStockQty, "0", true, true);
    }
    ui::FieldLabel("库存预警值");
    ui::TextField("pt_warn", a.fPartWarn, "低于该值提示预警", true, true);
    ui::FieldLabel("备注");
    ui::TextField("pt_remark", a.fPart.remark, "选填");

    ui::Gap(14);
    if (ui::PrimaryButton(a.fPartId > 0 ? "保存修改" : "保存档案")) {
        if (a.fPart.name.empty()) {
            ui::Toast("请填写配件名称");
        } else {
            a.fPart.cost = parseD(a.fPartCost, 0);
            a.fPart.price = parseD(a.fPartPrice, 0);
            a.fPart.warn = parseI(a.fPartWarn, 0);
            if (a.fPartId == 0) a.fPart.stock = parseI(a.fPartStockQty, 0);
            a.store.savePart(a.fPart);
            ui::Toast("已保存");
            a.back();
        }
    }
    if (a.fPartId > 0) {
        ui::Gap(4);
        if (ui::SoftButton("入库 / 出库 / 盘点")) {
            a.fStockPartId = a.fPart.id;
            a.fStockType = 1;
            a.fStockQty = "1";
            a.fStockPrice = ui::Num(a.fPart.cost);
            a.fStockPartner = 0;
            a.fStockRemark.clear();
            a.go(ScPartStockForm);
        }
        ui::Gap(4);
        if (ui::DangerButton("删除该配件")) {
            if (ui::Confirm("del_part", "删除确认", "删除后不可恢复，确定删除？")) {
                a.store.removePart(a.fPartId);
                ui::Toast("已删除");
                a.stack.pop_back();
                a.back();
            }
        }
    }
    ui::Gap(24);
}

void ScreenPartStockForm(App& a) {
    if (a.fStockPartId <= 0) {
        ui::Gap(2);
        ui::Hint("请选择要操作的配件");
        ui::Gap(8);
        ui::SearchBar("stk_search", a.sPart, "搜索配件");
        auto list = a.store.parts(a.sPart);
        if (list.empty()) {
            ui::Empty("暂无配件，请先新增档案");
        } else {
            for (auto& p : list) {
                ImGui::PushID(p.id);
                if (ui::ListRow(p.name, (p.sku.empty() ? "无条码" : p.sku) + (" · 库存 " + std::to_string(p.stock)),
                                "¥" + ui::Money(a.fStockType == 1 ? p.cost : p.price), ui::colSub)) {
                    a.fStockPartId = p.id;
                    a.fStockQty = "1";
                    a.fStockPrice = ui::Num(a.fStockType == 1 ? p.cost : p.price);
                }
                ImGui::PopID();
            }
        }
        ui::Gap(24);
        return;
    }

    Part p = a.store.part(a.fStockPartId);
    if (p.id == 0) {
        ui::Toast("配件不存在");
        a.fStockPartId = 0;
        return;
    }

    ui::Gap(2);
    ui::BeginCardAuto("stk_part");
    ui::KeyValueRow("配件", p.name);
    ui::KeyValueRow("当前库存", std::to_string(p.stock) + " " + p.unit);
    ui::KeyValueRow("成本 / 售价", "¥" + ui::Money(p.cost) + " / ¥" + ui::Money(p.price));
    ui::EndCardAuto();

    int seg = ui::Segmented("stk_type", {"入库", "出库", "盘点"}, a.fStockType - 1);
    if (seg >= 0) a.fStockType = seg + 1;

    ui::FieldLabel(a.fStockType == 3 ? "盘点后库存数量" : "数量");
    ui::TextField("stk_qty", a.fStockQty, "1", true, true);
    ui::FieldLabel(a.fStockType == 3 ? "成本价（用于计算差异金额）" : "单价（元）");
    ui::TextField("stk_price", a.fStockPrice, "0.00", true);
    if (a.fStockType != 3) {
        ui::FieldLabel(a.fStockType == 1 ? "供应商" : "客户");
        {
            std::vector<int> ids;
            std::vector<std::string> names;
            ids.push_back(0);
            names.push_back("不指定");
            for (auto& pt : a.store.partners(a.fStockType == 1 ? 2 : 1)) {
                ids.push_back(pt.id);
                names.push_back(pt.name);
            }
            int cur = 0;
            for (int i = 0; i < (int)ids.size(); i++) if (ids[i] == a.fStockPartner) cur = i;
            int sel = ui::Dropdown("stk_partner", names[cur], "选择往来单位", names);
            if (sel >= 0) a.fStockPartner = ids[sel];
        }
    }
    ui::FieldLabel("备注");
    ui::TextField("stk_remark", a.fStockRemark, "选填");

    ui::Gap(14);
    if (ui::PrimaryButton(a.fStockType == 1 ? "确认入库" : (a.fStockType == 2 ? "确认出库" : "确认盘点"))) {
        int qty = parseI(a.fStockQty, 0);
        double price = parseD(a.fStockPrice, 0);
        if (qty <= 0) {
            ui::Toast("请输入数量");
        } else {
            a.store.partStockChange(a.fStockPartId, a.fStockType, qty, price, a.fStockPartner, a.fStockRemark);
            ui::Toast(a.fStockType == 1 ? "入库成功" : (a.fStockType == 2 ? "出库成功" : "盘点完成"));
            a.back();
        }
    }
    ui::Gap(4);
    if (ui::SoftButton("重新选择配件")) a.fStockPartId = 0;
    ui::Gap(4);
    if (ui::SoftButton("查看库存流水")) a.go(ScPartFlows);
    ui::Gap(24);
}

void ScreenPartFlows(App& a) {
    ui::Gap(2);
    ui::Text("全部配件流水", ui::colText, 2);
    ui::Gap(8);
    auto list = a.store.partFlows(0, 300);
    if (list.empty()) {
        ui::Empty("暂无流水");
    } else {
        for (auto& f : list) {
            std::string type = f.type == 1 ? "入库" : (f.type == 2 ? "出库" : "盘点");
            std::string sub = f.createdAt + " · " + type;
            if (!f.partnerName.empty()) sub += " · " + f.partnerName;
            std::string right = (f.type == 2 ? "-" : "+") + std::to_string(f.qty);
            ImU32 rc = f.type == 1 ? ui::colPrimary : (f.type == 2 ? ui::colWarn : ui::colSub);
            ImGui::PushID(f.id);
            ui::ListRow(f.partName.empty() ? "配件" : f.partName, sub, right, rc);
            ImGui::PopID();
        }
    }
    ui::Gap(10);
    if (ui::PrimaryButton("+ 配件入库")) a.openPartStock(1);
    if (ui::SoftButton("+ 配件出库")) a.openPartStock(2);
    ui::Gap(24);
}
