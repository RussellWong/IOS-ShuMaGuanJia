#include "ui/app.h"
#include "platform/platform.h"

#include "imgui.h"

#include <cstdio>

// ===========================================================================
// 首次使用：门店初始化 / 登录
// ===========================================================================
void ScreenLogin(App& a) {
    if (a.fShop.name.empty()) {
        a.fShop = a.store.shop();
        a.fShop.createdAt = Store::now();
        if (a.fShop.owner.empty()) a.fShop.owner = "老板";
    }

    ui::Gap(52);
    // 品牌标识
    ImVec2 p = ImGui::GetCursorScreenPos();
    float cx = p.x + (ui::CanvasW() * ui::Scale() - ui::S(32)) * 0.5f;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(ImVec2(cx - ui::S(34), p.y), ImVec2(cx + ui::S(34), p.y + ui::S(68)),
                      ui::colPrimary, ui::S(20));
    dl->AddText(ui::fonts.huge, ui::fonts.huge->FontSize, ImVec2(cx - ui::S(16), p.y + ui::S(16)),
                ui::colWhite, "数");
    ImGui::Dummy(ImVec2(0, ui::S(68)));
    ui::Gap(18);
    ui::TextCenter("数码管家", ui::colText, 4);
    ui::Gap(6);
    ui::TextCenter("手机行业进销存管理", ui::colSub, 0);
    ui::Gap(34);

    ui::Hint("门店名称");
    ui::TextField("login_shop", a.fShop.name, "例如：飞阳手机通讯");
    ui::Hint("负责人");
    ui::TextField("login_owner", a.fShop.owner, "请输入负责人姓名");
    ui::Hint("联系电话（选填）");
    ui::TextField("login_phone", a.fShop.phone, "请输入手机号", true);
    ui::Gap(18);
    if (ui::PrimaryButton("开始使用")) {
        if (a.fShop.name.empty()) {
            ui::Toast("请填写门店名称");
        } else {
            a.store.saveShop(a.fShop);
            Employee e = a.store.employee(1);
            if (e.id > 0 && (e.name.empty() || e.name == "老板")) {
                e.name = a.fShop.owner.empty() ? "老板" : a.fShop.owner;
                a.store.saveEmployee(e);
            }
            a.me = 1;
            a.applyRange();
            ui::Toast("欢迎使用数码管家");
        }
    }
    ui::Gap(14);
    ui::TextCenter("所有数据保存在本机，无需联网", ui::colSub, 0);
    ui::Gap(20);
}

// ===========================================================================
// 往来单位详情
// ===========================================================================
void ScreenPartnerDetail(App& a, int id) {
    Partner p = a.store.partner(id);
    if (p.id == 0) {
        ui::Empty("单位不存在");
        return;
    }
    ui::Gap(4);
    ui::Text(p.name, ui::colText, 3);
    ui::Gap(2);
    ui::Text(p.type == 2 ? "供应商" : (p.type == 3 ? "客户兼供应商" : "客户"), ui::colSub, 0);
    ui::Gap(12);

    ui::BeginCard("p_info", p.remark.empty() ? 128 : 150);
    ui::KeyValueRow("联系电话", p.phone.empty() ? "未填写" : p.phone);
    ui::KeyValueRow("地址", p.address.empty() ? "未填写" : p.address);
    ui::KeyValueRow("应收金额", "¥" + ui::Money(p.receivable), p.receivable > 0.009 ? ui::colDanger : ui::colText);
    ui::KeyValueRow("应付金额", "¥" + ui::Money(p.payable), p.payable > 0.009 ? ui::colWarn : ui::colText);
    if (!p.remark.empty()) ui::KeyValueRow("备注", p.remark);
    ui::EndCard();

    ui::Gap(4);
    if (ui::PrimaryButton("编辑资料")) a.openPartnerForm(p.id);
    if ((p.receivable > 0.009 || p.payable > 0.009)) {
        if (ui::SoftButton("欠款核销")) {
            a.fSettlePartner = p.id;
            a.fSettleAmount = ui::Num(p.receivable > 0 ? p.receivable : p.payable);
            a.fSettleAccount = "现金";
            a.fSettleRemark.clear();
            a.go(ScReceivables, p.id);
        }
    }

    // 交易记录
    ui::Gap(10);
    ui::Text("交易记录", ui::colText, 2);
    ui::Gap(8);
    auto flows = a.store.finances("", "", 300);
    int count = 0;
    for (auto& f : flows) {
        if (f.refType == "往来单位" && f.refId == p.id) {
            std::string amt = (f.type == 1 ? "+" : "-") + ui::Money(f.amount);
            ui::ListRow(f.category, f.createdAt, amt, f.type == 1 ? ui::colSuccess : ui::colDanger);
            count++;
        }
    }
    if (count == 0) ui::Empty("暂无交易记录");
    ui::Gap(20);
}

void ScreenPartnerForm(App& a) {
    ui::Gap(4);
    ui::FieldLabel("单位类型");
    int seg = ui::Segmented("pf_type", {"客户", "供应商", "两者"}, a.fPartner.type - 1);
    if (seg >= 0) a.fPartner.type = seg + 1;

    ui::FieldLabel("名称");
    ui::TextField("pf_name", a.fPartner.name, "请输入客户/供应商名称");
    ui::FieldLabel("联系电话");
    ui::TextField("pf_phone", a.fPartner.phone, "请输入手机号", true);
    ui::FieldLabel("地址");
    ui::TextField("pf_addr", a.fPartner.address, "选填");
    ui::FieldLabel("备注");
    ui::TextField("pf_remark", a.fPartner.remark, "选填");
    ui::Gap(16);
    if (ui::PrimaryButton(a.fPartnerId > 0 ? "保存修改" : "保存并返回")) {
        if (a.fPartner.name.empty()) {
            ui::Toast("请填写名称");
        } else {
            a.store.savePartner(a.fPartner);
            ui::Toast("已保存");
            a.back();
        }
    }
    if (a.fPartnerId > 0) {
        ui::Gap(8);
        if (ui::DangerButton("删除该往来单位")) {
            if (ui::Confirm("del_partner", "删除确认", "删除后不可恢复，确定删除？")) {
                a.store.removePartner(a.fPartnerId);
                ui::Toast("已删除");
                a.stack.pop_back();
                a.back();
            }
        }
    }
    ui::Gap(30);
}

// ===========================================================================
// 门店信息
// ===========================================================================
void ScreenShopInfo(App& a) {
    if (a.fShop.name.empty()) a.fShop = a.store.shop();
    ui::Gap(4);
    ui::FieldLabel("门店名称");
    ui::TextField("shop_name", a.fShop.name, "请输入门店名称");
    ui::FieldLabel("负责人");
    ui::TextField("shop_owner", a.fShop.owner, "请输入负责人");
    ui::FieldLabel("联系电话");
    ui::TextField("shop_phone", a.fShop.phone, "请输入联系电话", true);
    ui::FieldLabel("门店地址");
    ui::TextField("shop_addr", a.fShop.address, "请输入地址");
    ui::Gap(16);
    if (ui::PrimaryButton("保存")) {
        if (a.fShop.name.empty()) ui::Toast("门店名称不能为空");
        else {
            a.store.saveShop(a.fShop);
            ui::Toast("门店信息已保存");
            a.back();
        }
    }
    ui::Gap(30);
}

// ===========================================================================
// 员工
// ===========================================================================
void ScreenEmployees(App& a) {
    auto list = a.store.employees();
    ui::Gap(4);
    ui::Hint("员工可用于维修派单、销售业绩统计与工费提成");
    ui::Gap(10);
    for (auto& e : list) {
        std::string sub = e.role + (e.active ? " · 在职" : " · 离职");
        std::string right = e.commission > 0 ? ("提成 " + ui::Num(e.commission) + "%") : "";
        if (ui::ListRow(e.name, sub, right, e.active ? ui::colSub : ui::colDanger)) a.openEmployeeForm(e.id);
    }
    ui::Gap(8);
    if (ui::PrimaryButton("+ 新增员工")) a.openEmployeeForm(0);
    ui::Gap(24);
}

void ScreenEmployeeForm(App& a) {
    ui::Gap(4);
    ui::FieldLabel("姓名");
    ui::TextField("emp_name", a.fEmp.name, "请输入员工姓名");
    ui::FieldLabel("联系电话");
    ui::TextField("emp_phone", a.fEmp.phone, "请输入手机号", true);
    ui::FieldLabel("岗位角色");
    std::vector<std::string> roles = {"管理员", "店长", "销售", "维修师", "仓管"};
    int cur = 0;
    for (int i = 0; i < (int)roles.size(); i++) if (roles[i] == a.fEmp.role) cur = i;
    int sel = ui::Dropdown("emp_role", a.fEmp.role, "选择岗位", roles);
    if (sel >= 0) a.fEmp.role = roles[sel];

    ui::FieldLabel("维修工费提成比例（%）");
    ui::TextField("emp_comm", a.fEmpComm, "例如 30", true);
    ui::Gap(10);
    ui::KeyValueRow("在职状态", a.fEmpActive ? "在职" : "离职");
    ui::Switch("emp_active", a.fEmpActive);

    ui::Gap(12);
    if (ui::PrimaryButton(a.fEmpId > 0 ? "保存修改" : "保存")) {
        if (a.fEmp.name.empty()) ui::Toast("请填写姓名");
        else {
            a.fEmp.commission = parseD(a.fEmpComm, 0);
            a.fEmp.active = a.fEmpActive ? 1 : 0;
            a.store.saveEmployee(a.fEmp);
            ui::Toast("已保存");
            a.back();
        }
    }
    if (a.fEmpId > 0) {
        ui::Gap(8);
        if (ui::DangerButton("删除员工")) {
            a.store.removeEmployee(a.fEmpId);
            ui::Toast("已删除");
            a.back();
        }
    }
    ui::Gap(30);
    (void)cur;
}

// ===========================================================================
// 结算账户
// ===========================================================================
void ScreenAccounts(App& a) {
    auto list = a.store.accounts();
    ui::Gap(4);
    ui::Hint("每笔业务收款都会更新对应账户余额");
    ui::Gap(10);
    for (auto& acc : list) {
        ui::ListRow(acc.name, "结算账户", "¥" + ui::Money(acc.balance),
                    acc.balance < 0 ? ui::colDanger : ui::colText);
    }
    ui::Gap(10);
    ui::FieldLabel("新增账户");
    static std::string newName;
    ui::TextField("acc_new", newName, "例如：备用金");
    if (ui::SoftButton("添加账户")) {
        if (newName.empty()) ui::Toast("请输入账户名称");
        else {
            Account acc;
            acc.name = newName;
            a.store.saveAccount(acc);
            newName.clear();
            ui::Toast("已添加");
        }
    }
    ui::Gap(30);
}

// ===========================================================================
// 机型资料库
// ===========================================================================
void ScreenModels(App& a) {
    static std::string key;
    ui::SearchBar("model_search", key, "搜索品牌或机型");
    auto list = a.store.models(key);
    ui::Gap(4);
    ui::Hint("共 " + std::to_string(list.size()) + " 条机型数据，入库时可直接选择");
    ui::Gap(8);
    std::string lastBrand;
    for (auto& m : list) {
        if (m.brand != lastBrand) {
            lastBrand = m.brand;
            ui::Gap(6);
            ui::Text(m.brand, ui::colText, 2);
            ui::Gap(6);
        }
        ui::ListRow(m.model, m.storage, "");
    }
    ui::Gap(20);
}

// ===========================================================================
// 数据管理
// ===========================================================================
void ScreenData(App& a) {
    ui::Gap(4);
    ui::Hint("当前数据库：" + plat::dataDir());
    ui::Gap(12);
    ui::Text("数据备份", ui::colText, 2);
    ui::Gap(6);
    if (ui::SoftButton("备份到本机")) {
        std::string ts = Store::now();
        for (char& c : ts) if (c == '-' || c == ':' || c == ' ') c = '_';
        std::string path = plat::dataDir() + "/backup_" + ts + ".db";
        if (a.store.backupTo(path)) ui::Toast("备份成功：" + path);
        else ui::Toast("备份失败");
    }
    ui::Gap(12);
    ui::Text("演示数据", ui::colText, 2);
    ui::Gap(4);
    ui::Hint("载入示例客户、设备、维修单与次卡，便于快速体验");
    if (ui::SoftButton("载入演示数据")) {
        a.store.loadDemoData();
        ui::Toast("演示数据已载入");
    }
    ui::Gap(14);
    ui::Text("危险操作", ui::colText, 2);
    ui::Gap(6);
    if (ui::DangerButton("清空全部业务数据")) {
        if (ui::Confirm("clear_all", "清空数据", "将删除所有业务数据且不可恢复，确定继续？")) {
            a.store.clearAllData();
            a.stack.clear();
            a.tab = 0;
            ui::Toast("已清空");
        }
    }
    ui::Gap(30);
}

// ===========================================================================
// 关于
// ===========================================================================
void ScreenAbout(App& a) {
    ui::Gap(20);
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = ui::CanvasW() * ui::Scale() - ui::S(32);
    float cx = p.x + w * 0.5f;
    ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(cx - ui::S(36), p.y), ImVec2(cx + ui::S(36), p.y + ui::S(72)),
                                              ui::colPrimary, ui::S(20));
    ImGui::GetWindowDrawList()->AddText(ui::fonts.huge, ui::fonts.huge->FontSize,
                                        ImVec2(cx - ui::S(17), p.y + ui::S(17)), ui::colWhite, "数");
    ImGui::Dummy(ImVec2(0, ui::S(72)));
    ui::Gap(14);
    ui::TextCenter("数码管家", ui::colText, 3);
    ui::Gap(4);
    ui::TextCenter("v1.0.0", ui::colSub, 0);
    ui::Gap(20);
    ui::BeginCard("about_card", 214);
    ui::KeyValueRow("软件名称", "数码管家");
    ui::KeyValueRow("软件类型", "手机进销存管理");
    ui::KeyValueRow("数据存储", "本机离线");
    ui::KeyValueRow("支持模块", "手机/维修/配件/次卡");
    ui::KeyValueRow("管理能力", "财务/报表/往来/员工");
    ui::KeyValueRow("技术架构", "C++ / ImGui / SQLite");
    ui::EndCard();
    ui::Gap(8);
    ui::TextWrapped("数码管家是一款专为手机行业打造的全场景管理软件，覆盖新机、二手、维修、配件与次卡业务，"
                    "帮助店家高效管理库存、财务与客户关系。所有数据均保存在本机，无需联网即可使用。",
                    ui::colSub, 0);
    ui::Gap(30);
}
