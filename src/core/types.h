// 数据模型定义：数码管家（手机进销存管理）
// 所有结构体仅包含数据，不包含业务逻辑，便于界面层与数据层解耦。
#pragma once

#include <string>
#include <vector>

// 门店信息（单机版只有一个门店）
struct Shop {
    int id = 1;
    std::string name;      // 门店名称
    std::string owner;     // 老板/负责人
    std::string phone;     // 联系电话
    std::string address;   // 地址
    std::string createdAt; // 创建时间
};

// 员工
struct Employee {
    int id = 0;
    std::string name;
    std::string phone;
    std::string role;         // 角色：管理员/店长/销售/维修师/仓管
    int active = 1;           // 是否在职
    double commission = 0.0;  // 维修工费提成比例（0~100）
    std::string createdAt;
};

// 往来单位（客户 / 供应商）
struct Partner {
    int id = 0;
    int type = 1;            // 1 客户 2 供应商 3 客户兼供应商
    std::string name;
    std::string phone;
    std::string address;
    std::string remark;
    double receivable = 0;   // 应收（客户欠款）
    double payable = 0;      // 应付（欠供应商）
    std::string createdAt;
};

// 手机 / 数码设备（一机一码）
struct PhoneDev {
    int id = 0;
    std::string code;        // 系统内唯一编码
    std::string imei;        // IMEI / SN
    std::string brand;       // 品牌
    std::string model;       // 型号
    std::string storage;     // 容量
    std::string color;       // 颜色
    std::string condition;   // 成色（全新/99新/95新...）
    double inPrice = 0;      // 入库价
    double prepCost = 0;     // 整备成本
    int inSupplier = 0;      // 入库供应商
    std::string inTime;      // 入库时间
    int inOperator = 0;      // 入库操作员
    double outPrice = 0;     // 出库价
    int outCustomer = 0;     // 出库客户
    std::string outTime;     // 出库时间
    int outOperator = 0;     // 出库操作员
    std::string outPay;      // 收款方式
    int status = 0;          // 0 在库 1 已售 2 已退货
    std::string remark;
};

// 维修单
struct Repair {
    int id = 0;
    std::string orderNo;      // 单号
    int partner = 0;          // 客户 ID
    std::string customerName; // 客户姓名（快照）
    std::string customerPhone;
    std::string model;        // 机型
    std::string fault;        // 故障描述
    int status = 0;           // 0 待接单 1 维修中 2 待交付 3 已交付 4 已取消
    int technician = 0;       // 维修师
    double laborFee = 0;      // 工费
    double partsCost = 0;     // 配件成本
    double totalFee = 0;      // 应收总额
    double paid = 0;          // 已收
    std::string payMethod;    // 收款方式
    std::string createdAt;
    std::string startedAt;
    std::string finishedAt;
    std::string deliveredAt;
    int result = 0;           // 0 未完成 1 修复成功 2 修复失败 3 放弃维修 4 无法维修
    std::string remark;
};

// 配件商品
struct Part {
    int id = 0;
    std::string sku;      // 条码 / SKU
    std::string name;
    std::string model;    // 规格型号
    std::string unit;     // 单位
    double cost = 0;      // 成本价
    double price = 0;     // 销售价
    int stock = 0;        // 当前库存
    int warn = 0;         // 库存预警值
    int sn = 0;           // 是否启用 SN 管控
    std::string remark;
};

// 配件出入库流水
struct PartFlow {
    int id = 0;
    int partId = 0;
    int type = 1;        // 1 入库 2 出库 3 盘点调整
    int qty = 0;
    double price = 0;
    double amount = 0;
    int partnerId = 0;
    std::string ref;     // 关联单号
    int operatorId = 0;
    std::string createdAt;
    std::string remark;
    // 展示用冗余字段
    std::string partName;
    std::string partnerName;
};

// 次卡模板
struct CardTpl {
    int id = 0;
    std::string name;
    int times = 0;
    double price = 0;
    int validDays = 0;   // 有效期天数，0 表示永久
    std::string remark;
};

// 会员次卡
struct Card {
    int id = 0;
    std::string cardNo;
    int tplId = 0;
    int partnerId = 0;
    int times = 0;
    int remain = 0;
    double price = 0;
    std::string createdAt;
    std::string expireAt;
    int status = 1;      // 1 正常 2 已用完 3 已过期
    // 展示用冗余字段
    std::string tplName;
    std::string partnerName;
};

// 次卡使用记录
struct CardRecord {
    int id = 0;
    int cardId = 0;
    int type = 1;   // 1 开卡 2 核销 3 退卡
    int times = 0;
    std::string remark;
    std::string createdAt;
};

// 财务流水
struct Finance {
    int id = 0;
    int type = 1;   // 1 收入 2 支出
    std::string category;
    double amount = 0;
    std::string account;
    std::string refType;  // 关联业务：手机/维修/配件/次卡/手动
    int refId = 0;
    std::string remark;
    std::string createdAt;
    int operatorId = 0;
};

// 结算账户
struct Account {
    int id = 0;
    std::string name;
    double balance = 0;
};

// 机型资料库
struct PhoneModel {
    std::string brand;
    std::string model;
    std::string storage;
};

// 首页统计
struct DashboardStat {
    double todaySales = 0;
    double todayProfit = 0;
    int todayOrders = 0;
    int stockPhones = 0;
    double stockValue = 0;
    int pendingRepairs = 0;
    int warnParts = 0;
    int totalPartners = 0;
    int receivableCount = 0;
    double receivable = 0;
};
