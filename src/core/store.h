// 业务数据仓库：所有增删改查、单据流转、财务联动与统计聚合都在这里实现
#pragma once

#include "core/db.h"
#include "core/types.h"

#include <string>
#include <utility>
#include <vector>

// 经营汇总（用于报表）
struct SalesSummary {
    double phoneSales = 0;
    double repairIncome = 0;
    double partsSales = 0;
    double cardIncome = 0;
    double totalIncome = 0;

    double phoneProfit = 0;
    double repairProfit = 0;
    double partsProfit = 0;
    double totalProfit = 0;

    int phoneCount = 0;
    int repairCount = 0;
    int partQty = 0;
    int cardCount = 0;
};

// 员工业绩
struct StaffPerf {
    int id = 0;
    std::string name;
    int repairCount = 0;
    double repairAmount = 0;
    int phoneCount = 0;
    double phoneSales = 0;
};

class Store {
public:
    bool init(const std::string& dbPath);
    Db& db() { return db_; }

    // ---------------- 通用工具 ----------------
    static std::string now();                      // YYYY-MM-DD HH:MM:SS
    static std::string today();                    // YYYY-MM-DD
    static std::string dateOf(const std::string& dt); // 取日期部分
    static std::string addDays(const std::string& date, int days);

    // ---------------- 门店 ----------------
    Shop shop();
    void saveShop(const Shop& s);
    bool initialized();   // 是否已完成初始化（有门店名）

    // ---------------- 员工 ----------------
    std::vector<Employee> employees(bool activeOnly = false);
    Employee employee(int id);
    int saveEmployee(Employee& e);
    void removeEmployee(int id);

    // ---------------- 往来单位 ----------------
    std::vector<Partner> partners(int type = 0, const std::string& key = "");
    Partner partner(int id);
    int savePartner(Partner& p);
    void removePartner(int id);
    std::vector<Partner> receivables();   // 有应收或应付的单位

    // ---------------- 手机业务 ----------------
    std::vector<PhoneDev> phones(int status = -1, const std::string& key = "", int limit = 1000);
    PhoneDev phone(int id);
    int addPhoneIn(PhoneDev& d);                                   // 入库（自动编码 + 支出流水）
    void updatePhone(PhoneDev& d);                                 // 修改设备信息
    void sellPhone(int id, double price, int customerId, const std::string& pay, int operatorId);
    void returnPhone(int id);                                      // 售后退货，回库
    void removePhone(int id);
    std::string nextDeviceCode();

    // ---------------- 维修业务 ----------------
    std::vector<Repair> repairs(int status = -1, const std::string& key = "", int limit = 1000);
    Repair repair(int id);
    int createRepair(Repair& r);
    void acceptRepair(int id, int technicianId);
    void startRepair(int id);
    void finishRepair(int id, int result, const std::string& remark);
    void deliverRepair(int id, double paid, const std::string& payMethod);
    void cancelRepair(int id);
    std::string nextRepairNo();

    // ---------------- 配件业务 ----------------
    std::vector<Part> parts(const std::string& key = "");
    Part part(int id);
    int savePart(Part& p);
    void removePart(int id);
    void partStockChange(int partId, int type, int qty, double price,
                         int partnerId, const std::string& remark);
    std::vector<PartFlow> partFlows(int partId = 0, int limit = 300);

    // ---------------- 次卡业务 ----------------
    std::vector<CardTpl> cardTpls();
    CardTpl cardTpl(int id);
    int saveCardTpl(CardTpl& t);
    void removeCardTpl(int id);
    std::vector<Card> cards(const std::string& key = "");
    Card card(int id);
    int openCard(int tplId, int partnerId);
    void useCard(int cardId, int times, const std::string& remark);
    std::vector<CardRecord> cardRecords(int cardId = 0);

    // ---------------- 财务 ----------------
    std::vector<Account> accounts();
    int saveAccount(Account& a);
    void removeAccount(int id);
    void addFinance(int type, const std::string& category, double amount,
                    const std::string& account, const std::string& refType, int refId,
                    const std::string& remark);
    std::vector<Finance> finances(const std::string& from, const std::string& to, int limit = 500);
    void settle(int partnerId, double amount, const std::string& account, const std::string& remark);

    // ---------------- 机型资料库 ----------------
    std::vector<PhoneModel> models(const std::string& key = "");
    std::vector<std::string> brands();

    // ---------------- 统计 ----------------
    DashboardStat dashboard();
    SalesSummary salesSummary(const std::string& from, const std::string& to);
    std::vector<std::pair<std::string, double>> salesByDay(const std::string& from, const std::string& to);
    std::vector<std::pair<std::string, double>> profitByDay(const std::string& from, const std::string& to);
    std::vector<StaffPerf> staffPerf(const std::string& from, const std::string& to);
    std::vector<std::pair<std::string, std::pair<int, double>>> partsRanking(const std::string& from, const std::string& to);

    // ---------------- 数据维护 ----------------
    void loadDemoData();
    void clearAllData();
    bool backupTo(const std::string& path);

private:
    Db db_;
    void createSchema();
    void seedBase();
};
