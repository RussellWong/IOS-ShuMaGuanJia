#include "core/store.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>

// ===========================================================================
// 工具函数
// ===========================================================================
static std::string formatTime(const std::tm& tm) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
                  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                  tm.tm_hour, tm.tm_min, tm.tm_sec);
    return buf;
}

static std::tm parseDate(const std::string& date) {
    std::tm tm{};
    int y = 1970, m = 1, d = 1;
    std::sscanf(date.c_str(), "%d-%d-%d", &y, &m, &d);
    tm.tm_year = y - 1900;
    tm.tm_mon = m - 1;
    tm.tm_mday = d;
    tm.tm_hour = 12;
    tm.tm_isdst = -1;
    std::mktime(&tm);
    return tm;
}

std::string Store::now() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    return formatTime(tm);
}

std::string Store::today() {
    std::string s = now();
    return s.substr(0, 10);
}

std::string Store::dateOf(const std::string& dt) {
    return dt.size() >= 10 ? dt.substr(0, 10) : dt;
}

std::string Store::addDays(const std::string& date, int days) {
    std::tm tm = parseDate(date);
    tm.tm_mday += days;
    std::mktime(&tm);
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
    return buf;
}

// ===========================================================================
// 初始化
// ===========================================================================
bool Store::init(const std::string& dbPath) {
    if (!db_.open(dbPath)) return false;
    createSchema();
    seedBase();
    return true;
}

void Store::createSchema() {
    db_.exec(R"SQL(
CREATE TABLE IF NOT EXISTS shop(
  id INTEGER PRIMARY KEY CHECK(id=1),
  name TEXT DEFAULT '', owner TEXT DEFAULT '', phone TEXT DEFAULT '',
  address TEXT DEFAULT '', created_at TEXT DEFAULT '');
CREATE TABLE IF NOT EXISTS employees(
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  name TEXT NOT NULL, phone TEXT DEFAULT '', role TEXT DEFAULT '员工',
  active INTEGER DEFAULT 1, commission REAL DEFAULT 0, created_at TEXT DEFAULT '');
CREATE TABLE IF NOT EXISTS partners(
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  type INTEGER DEFAULT 1, name TEXT NOT NULL, phone TEXT DEFAULT '',
  address TEXT DEFAULT '', remark TEXT DEFAULT '',
  receivable REAL DEFAULT 0, payable REAL DEFAULT 0, created_at TEXT DEFAULT '');
CREATE TABLE IF NOT EXISTS phones(
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  code TEXT UNIQUE, imei TEXT DEFAULT '', brand TEXT DEFAULT '', model TEXT DEFAULT '',
  storage TEXT DEFAULT '', color TEXT DEFAULT '', condition TEXT DEFAULT '',
  in_price REAL DEFAULT 0, prep_cost REAL DEFAULT 0, in_supplier INTEGER DEFAULT 0,
  in_time TEXT DEFAULT '', in_operator INTEGER DEFAULT 0,
  out_price REAL DEFAULT 0, out_customer INTEGER DEFAULT 0, out_time TEXT DEFAULT '',
  out_operator INTEGER DEFAULT 0, out_pay TEXT DEFAULT '', status INTEGER DEFAULT 0,
  remark TEXT DEFAULT '');
CREATE TABLE IF NOT EXISTS repairs(
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  order_no TEXT UNIQUE, partner INTEGER DEFAULT 0, customer_name TEXT DEFAULT '',
  customer_phone TEXT DEFAULT '', model TEXT DEFAULT '', fault TEXT DEFAULT '',
  status INTEGER DEFAULT 0, technician INTEGER DEFAULT 0,
  labor_fee REAL DEFAULT 0, parts_cost REAL DEFAULT 0, total_fee REAL DEFAULT 0,
  paid REAL DEFAULT 0, pay_method TEXT DEFAULT '',
  created_at TEXT DEFAULT '', started_at TEXT DEFAULT '', finished_at TEXT DEFAULT '',
  delivered_at TEXT DEFAULT '', result INTEGER DEFAULT 0, remark TEXT DEFAULT '');
CREATE TABLE IF NOT EXISTS parts(
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  sku TEXT DEFAULT '', name TEXT NOT NULL, model TEXT DEFAULT '', unit TEXT DEFAULT '个',
  cost REAL DEFAULT 0, price REAL DEFAULT 0, stock INTEGER DEFAULT 0,
  warn INTEGER DEFAULT 0, sn INTEGER DEFAULT 0, remark TEXT DEFAULT '');
CREATE TABLE IF NOT EXISTS part_flows(
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  part_id INTEGER, type INTEGER DEFAULT 1, qty INTEGER DEFAULT 0,
  price REAL DEFAULT 0, amount REAL DEFAULT 0, partner_id INTEGER DEFAULT 0,
  ref TEXT DEFAULT '', operator INTEGER DEFAULT 0, created_at TEXT DEFAULT '', remark TEXT DEFAULT '');
CREATE TABLE IF NOT EXISTS card_tpls(
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  name TEXT NOT NULL, times INTEGER DEFAULT 0, price REAL DEFAULT 0,
  valid_days INTEGER DEFAULT 0, remark TEXT DEFAULT '');
CREATE TABLE IF NOT EXISTS cards(
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  card_no TEXT UNIQUE, tpl_id INTEGER DEFAULT 0, partner_id INTEGER DEFAULT 0,
  times INTEGER DEFAULT 0, remain INTEGER DEFAULT 0, price REAL DEFAULT 0,
  created_at TEXT DEFAULT '', expire_at TEXT DEFAULT '', status INTEGER DEFAULT 1);
CREATE TABLE IF NOT EXISTS card_records(
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  card_id INTEGER, type INTEGER DEFAULT 1, times INTEGER DEFAULT 0,
  remark TEXT DEFAULT '', created_at TEXT DEFAULT '');
CREATE TABLE IF NOT EXISTS finances(
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  type INTEGER DEFAULT 1, category TEXT DEFAULT '', amount REAL DEFAULT 0,
  account TEXT DEFAULT '', ref_type TEXT DEFAULT '', ref_id INTEGER DEFAULT 0,
  remark TEXT DEFAULT '', created_at TEXT DEFAULT '', operator INTEGER DEFAULT 0);
CREATE TABLE IF NOT EXISTS accounts(
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  name TEXT UNIQUE, balance REAL DEFAULT 0);
CREATE TABLE IF NOT EXISTS models(
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  brand TEXT, model TEXT, storage TEXT, UNIQUE(brand,model,storage));
CREATE INDEX IF NOT EXISTS idx_phones_status ON phones(status);
CREATE INDEX IF NOT EXISTS idx_phones_out_time ON phones(out_time);
CREATE INDEX IF NOT EXISTS idx_repairs_status ON repairs(status);
CREATE INDEX IF NOT EXISTS idx_finances_time ON finances(created_at);
CREATE INDEX IF NOT EXISTS idx_part_flows_time ON part_flows(created_at);
)SQL");

    // 门店记录占位
    if (db_.queryInt("SELECT COUNT(*) FROM shop") == 0) {
        db_.exec("INSERT INTO shop(id,name,created_at) VALUES(1,'','" + now() + "');");
    }
}

void Store::seedBase() {
    // 默认结算账户
    if (db_.queryInt("SELECT COUNT(*) FROM accounts") == 0) {
        db_.exec("INSERT INTO accounts(name,balance) VALUES('现金',0),('微信',0),('支付宝',0),('银行卡',0);");
    }
    // 默认管理员
    if (db_.queryInt("SELECT COUNT(*) FROM employees") == 0) {
        Stmt s(db_, "INSERT INTO employees(name,phone,role,active,commission,created_at) VALUES('老板','','管理员',1,30,?)");
        s.bind(1, now());
        s.run();
    }
    // 常见机型资料库
    if (db_.queryInt("SELECT COUNT(*) FROM models") == 0) {
        struct Seed { const char* brand; const char* model; };
        static const Seed seeds[] = {
            {"Apple", "iPhone 17 Pro Max"}, {"Apple", "iPhone 17 Pro"}, {"Apple", "iPhone 17"},
            {"Apple", "iPhone 16 Pro Max"}, {"Apple", "iPhone 16 Pro"}, {"Apple", "iPhone 16"},
            {"Apple", "iPhone 15 Pro Max"}, {"Apple", "iPhone 15 Pro"}, {"Apple", "iPhone 15"},
            {"Apple", "iPhone 14 Pro Max"}, {"Apple", "iPhone 14 Pro"}, {"Apple", "iPhone 14"},
            {"Apple", "iPhone 13 Pro Max"}, {"Apple", "iPhone 13"}, {"Apple", "iPhone 12"},
            {"Apple", "iPad Air 5"}, {"Apple", "iPad Pro 11"}, {"Apple", "MacBook Air M3"},
            {"Apple", "Apple Watch S9"}, {"Apple", "AirPods Pro 2"},
            {"华为", "Mate 70 Pro"}, {"华为", "Mate 70"}, {"华为", "Mate 60 Pro"},
            {"华为", "Pura 70 Pro"}, {"华为", "nova 13"}, {"华为", "MatePad Pro"},
            {"小米", "小米15 Pro"}, {"小米", "小米15"}, {"小米", "小米14"},
            {"小米", "Redmi K80"}, {"小米", "Redmi Note 14 Pro"},
            {"OPPO", "Find X8 Pro"}, {"OPPO", "Find X8"}, {"OPPO", "Reno13 Pro"},
            {"vivo", "X200 Pro"}, {"vivo", "X200"}, {"vivo", "S20 Pro"},
            {"荣耀", "Magic7 Pro"}, {"荣耀", "Magic7"}, {"荣耀", "荣耀300 Pro"},
            {"三星", "Galaxy S24 Ultra"}, {"三星", "Galaxy S24"},
            {"一加", "一加13"}, {"realme", "GT7 Pro"}, {"努比亚", "Z70 Ultra"},
        };
        static const char* storages[] = {"64G", "128G", "256G", "512G", "1T"};
        db_.begin();
        for (const auto& s : seeds) {
            for (const char* st : storages) {
                Stmt q(db_, "INSERT OR IGNORE INTO models(brand,model,storage) VALUES(?,?,?)");
                q.bind(1, std::string(s.brand));
                q.bind(2, std::string(s.model));
                q.bind(3, std::string(st));
                q.run();
            }
        }
        db_.commit();
    }
}

bool Store::initialized() {
    return !db_.queryText("SELECT name FROM shop WHERE id=1").empty();
}

// ===========================================================================
// 门店
// ===========================================================================
Shop ShopFromRow(Stmt& s) {
    Shop v;
    v.id = s.colInt(0);
    v.name = s.colText(1);
    v.owner = s.colText(2);
    v.phone = s.colText(3);
    v.address = s.colText(4);
    v.createdAt = s.colText(5);
    return v;
}

Shop Store::shop() {
    Stmt s(db_, "SELECT id,name,owner,phone,address,created_at FROM shop WHERE id=1");
    if (s.step()) return ShopFromRow(s);
    return Shop{};
}

void Store::saveShop(const Shop& v) {
    Stmt s(db_, "UPDATE shop SET name=?,owner=?,phone=?,address=? WHERE id=1");
    s.bind(1, v.name);
    s.bind(2, v.owner);
    s.bind(3, v.phone);
    s.bind(4, v.address);
    s.run();
}

// ===========================================================================
// 员工
// ===========================================================================
std::vector<Employee> Store::employees(bool activeOnly) {
    std::vector<Employee> out;
    std::string sql = "SELECT id,name,phone,role,active,commission,created_at FROM employees";
    if (activeOnly) sql += " WHERE active=1";
    sql += " ORDER BY id";
    Stmt s(db_, sql);
    while (s.step()) {
        Employee e;
        e.id = s.colInt(0);
        e.name = s.colText(1);
        e.phone = s.colText(2);
        e.role = s.colText(3);
        e.active = s.colInt(4);
        e.commission = s.colDouble(5);
        e.createdAt = s.colText(6);
        out.push_back(e);
    }
    return out;
}

Employee Store::employee(int id) {
    Stmt s(db_, "SELECT id,name,phone,role,active,commission,created_at FROM employees WHERE id=?");
    s.bind(1, id);
    if (s.step()) {
        Employee e;
        e.id = s.colInt(0);
        e.name = s.colText(1);
        e.phone = s.colText(2);
        e.role = s.colText(3);
        e.active = s.colInt(4);
        e.commission = s.colDouble(5);
        e.createdAt = s.colText(6);
        return e;
    }
    return Employee{};
}

int Store::saveEmployee(Employee& e) {
    if (e.id > 0) {
        Stmt s(db_, "UPDATE employees SET name=?,phone=?,role=?,active=?,commission=? WHERE id=?");
        s.bind(1, e.name);
        s.bind(2, e.phone);
        s.bind(3, e.role);
        s.bind(4, e.active);
        s.bind(5, e.commission);
        s.bind(6, e.id);
        s.run();
        return e.id;
    }
    Stmt s(db_, "INSERT INTO employees(name,phone,role,active,commission,created_at) VALUES(?,?,?,?,?,?)");
    s.bind(1, e.name);
    s.bind(2, e.phone);
    s.bind(3, e.role);
    s.bind(4, e.active);
    s.bind(5, e.commission);
    s.bind(6, now());
    s.run();
    e.id = (int)sqlite3_last_insert_rowid(db_.raw());
    return e.id;
}

void Store::removeEmployee(int id) {
    Stmt s(db_, "DELETE FROM employees WHERE id=? AND role<>'管理员'");
    s.bind(1, id);
    s.run();
}

// ===========================================================================
// 往来单位
// ===========================================================================
static Partner PartnerFromRow(Stmt& s) {
    Partner p;
    p.id = s.colInt(0);
    p.type = s.colInt(1);
    p.name = s.colText(2);
    p.phone = s.colText(3);
    p.address = s.colText(4);
    p.remark = s.colText(5);
    p.receivable = s.colDouble(6);
    p.payable = s.colDouble(7);
    p.createdAt = s.colText(8);
    return p;
}

std::vector<Partner> Store::partners(int type, const std::string& key) {
    std::vector<Partner> out;
    std::string sql = "SELECT id,type,name,phone,address,remark,receivable,payable,created_at FROM partners WHERE 1=1";
    if (type == 1) sql += " AND (type=1 OR type=3)";
    else if (type == 2) sql += " AND (type=2 OR type=3)";
    if (!key.empty()) sql += " AND (name LIKE '%" + key + "%' OR phone LIKE '%" + key + "%')";
    sql += " ORDER BY id DESC";
    Stmt s(db_, sql);
    while (s.step()) out.push_back(PartnerFromRow(s));
    return out;
}

Partner Store::partner(int id) {
    Stmt s(db_, "SELECT id,type,name,phone,address,remark,receivable,payable,created_at FROM partners WHERE id=?");
    s.bind(1, id);
    if (s.step()) return PartnerFromRow(s);
    return Partner{};
}

int Store::savePartner(Partner& p) {
    if (p.id > 0) {
        Stmt s(db_, "UPDATE partners SET type=?,name=?,phone=?,address=?,remark=? WHERE id=?");
        s.bind(1, p.type);
        s.bind(2, p.name);
        s.bind(3, p.phone);
        s.bind(4, p.address);
        s.bind(5, p.remark);
        s.bind(6, p.id);
        s.run();
        return p.id;
    }
    Stmt s(db_, "INSERT INTO partners(type,name,phone,address,remark,receivable,payable,created_at) VALUES(?,?,?,?,?,0,0,?)");
    s.bind(1, p.type);
    s.bind(2, p.name);
    s.bind(3, p.phone);
    s.bind(4, p.address);
    s.bind(5, p.remark);
    s.bind(6, now());
    s.run();
    p.id = (int)sqlite3_last_insert_rowid(db_.raw());
    return p.id;
}

void Store::removePartner(int id) {
    Stmt s(db_, "DELETE FROM partners WHERE id=?");
    s.bind(1, id);
    s.run();
}

std::vector<Partner> Store::receivables() {
    std::vector<Partner> out;
    Stmt s(db_, "SELECT id,type,name,phone,address,remark,receivable,payable,created_at "
                "FROM partners WHERE receivable>0.001 OR payable>0.001 ORDER BY receivable DESC, payable DESC");
    while (s.step()) out.push_back(PartnerFromRow(s));
    return out;
}

// ===========================================================================
// 手机业务
// ===========================================================================
static PhoneDev PhoneFromRow(Stmt& s) {
    PhoneDev d;
    d.id = s.colInt(0);
    d.code = s.colText(1);
    d.imei = s.colText(2);
    d.brand = s.colText(3);
    d.model = s.colText(4);
    d.storage = s.colText(5);
    d.color = s.colText(6);
    d.condition = s.colText(7);
    d.inPrice = s.colDouble(8);
    d.prepCost = s.colDouble(9);
    d.inSupplier = s.colInt(10);
    d.inTime = s.colText(11);
    d.inOperator = s.colInt(12);
    d.outPrice = s.colDouble(13);
    d.outCustomer = s.colInt(14);
    d.outTime = s.colText(15);
    d.outOperator = s.colInt(16);
    d.outPay = s.colText(17);
    d.status = s.colInt(18);
    d.remark = s.colText(19);
    return d;
}

static const char* kPhoneCols =
    "id,code,imei,brand,model,storage,color,condition,in_price,prep_cost,in_supplier,in_time,"
    "in_operator,out_price,out_customer,out_time,out_operator,out_pay,status,remark";

std::vector<PhoneDev> Store::phones(int status, const std::string& key, int limit) {
    std::vector<PhoneDev> out;
    std::string sql = std::string("SELECT ") + kPhoneCols + " FROM phones WHERE 1=1";
    if (status >= 0) sql += " AND status=" + std::to_string(status);
    if (!key.empty()) {
        sql += " AND (model LIKE '%" + key + "%' OR brand LIKE '%" + key + "%' OR imei LIKE '%" + key +
               "%' OR code LIKE '%" + key + "%')";
    }
    sql += " ORDER BY id DESC LIMIT " + std::to_string(limit);
    Stmt s(db_, sql);
    while (s.step()) out.push_back(PhoneFromRow(s));
    return out;
}

PhoneDev Store::phone(int id) {
    Stmt s(db_, std::string("SELECT ") + kPhoneCols + " FROM phones WHERE id=?");
    s.bind(1, id);
    if (s.step()) return PhoneFromRow(s);
    return PhoneDev{};
}

std::string Store::nextDeviceCode() {
    // 以时间戳生成 12 位数字编码，如重复则自增
    std::time_t t = std::time(nullptr);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%02d%02d%02d%02d%02d%02d",
                  tm.tm_year % 100, tm.tm_mon + 1, tm.tm_mday,
                  tm.tm_hour, tm.tm_min, tm.tm_sec);
    std::string code(buf);
    while (db_.queryInt("SELECT COUNT(*) FROM phones WHERE code='" + code + "'") > 0) {
        long long v = std::stoll(code) + 1;
        std::snprintf(buf, sizeof(buf), "%012lld", v);
        code = buf;
    }
    return code;
}

int Store::addPhoneIn(PhoneDev& d) {
    if (d.code.empty()) d.code = nextDeviceCode();
    if (d.inTime.empty()) d.inTime = now();
    Stmt s(db_, "INSERT INTO phones(code,imei,brand,model,storage,color,condition,in_price,prep_cost,"
                "in_supplier,in_time,in_operator,out_price,out_customer,out_time,out_operator,out_pay,status,remark) "
                "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,0,0,'',0,'',0,?)");
    s.bind(1, d.code);
    s.bind(2, d.imei);
    s.bind(3, d.brand);
    s.bind(4, d.model);
    s.bind(5, d.storage);
    s.bind(6, d.color);
    s.bind(7, d.condition);
    s.bind(8, d.inPrice);
    s.bind(9, d.prepCost);
    s.bind(10, d.inSupplier);
    s.bind(11, d.inTime);
    s.bind(12, d.inOperator);
    s.bind(13, d.remark);
    s.run();
    d.id = (int)sqlite3_last_insert_rowid(db_.raw());
    // 采购支出流水
    if (d.inPrice > 0) {
        addFinance(2, "手机采购", d.inPrice, "现金", "手机", d.id,
                   "入库 " + d.brand + " " + d.model + " " + d.storage);
    }
    return d.id;
}

void Store::updatePhone(PhoneDev& d) {
    Stmt s(db_, "UPDATE phones SET imei=?,brand=?,model=?,storage=?,color=?,condition=?,"
                "in_price=?,prep_cost=?,in_supplier=?,remark=? WHERE id=?");
    s.bind(1, d.imei);
    s.bind(2, d.brand);
    s.bind(3, d.model);
    s.bind(4, d.storage);
    s.bind(5, d.color);
    s.bind(6, d.condition);
    s.bind(7, d.inPrice);
    s.bind(8, d.prepCost);
    s.bind(9, d.inSupplier);
    s.bind(10, d.remark);
    s.bind(11, d.id);
    s.run();
}

void Store::sellPhone(int id, double price, int customerId, const std::string& pay, int operatorId) {
    PhoneDev d = phone(id);
    if (d.id == 0 || d.status != 0) return;
    Stmt s(db_, "UPDATE phones SET out_price=?,out_customer=?,out_time=?,out_operator=?,out_pay=?,status=1 WHERE id=?");
    s.bind(1, price);
    s.bind(2, customerId);
    s.bind(3, now());
    s.bind(4, operatorId);
    s.bind(5, pay);
    s.bind(6, id);
    s.run();
    if (price > 0) {
        addFinance(1, "手机销售", price, pay, "手机", id,
                   "销售 " + d.brand + " " + d.model + " " + d.storage);
    }
}

void Store::returnPhone(int id) {
    PhoneDev d = phone(id);
    if (d.id == 0 || d.status != 1) return;
    Stmt s(db_, "UPDATE phones SET status=2, out_price=0, out_customer=0, out_time='', out_operator=0, out_pay='' WHERE id=?");
    s.bind(1, id);
    s.run();
    // 冲回销售收入
    if (d.outPrice > 0) {
        addFinance(2, "售后退款", d.outPrice, d.outPay.empty() ? "现金" : d.outPay, "手机", id,
                   "退货 " + d.code);
    }
}

void Store::removePhone(int id) {
    Stmt s(db_, "DELETE FROM phones WHERE id=?");
    s.bind(1, id);
    s.run();
}

// ===========================================================================
// 维修业务
// ===========================================================================
static Repair RepairFromRow(Stmt& s) {
    Repair r;
    r.id = s.colInt(0);
    r.orderNo = s.colText(1);
    r.partner = s.colInt(2);
    r.customerName = s.colText(3);
    r.customerPhone = s.colText(4);
    r.model = s.colText(5);
    r.fault = s.colText(6);
    r.status = s.colInt(7);
    r.technician = s.colInt(8);
    r.laborFee = s.colDouble(9);
    r.partsCost = s.colDouble(10);
    r.totalFee = s.colDouble(11);
    r.paid = s.colDouble(12);
    r.payMethod = s.colText(13);
    r.createdAt = s.colText(14);
    r.startedAt = s.colText(15);
    r.finishedAt = s.colText(16);
    r.deliveredAt = s.colText(17);
    r.result = s.colInt(18);
    r.remark = s.colText(19);
    return r;
}

static const char* kRepairCols =
    "id,order_no,partner,customer_name,customer_phone,model,fault,status,technician,"
    "labor_fee,parts_cost,total_fee,paid,pay_method,created_at,started_at,finished_at,"
    "delivered_at,result,remark";

std::vector<Repair> Store::repairs(int status, const std::string& key, int limit) {
    std::vector<Repair> out;
    std::string sql = std::string("SELECT ") + kRepairCols + " FROM repairs WHERE 1=1";
    if (status >= 0) {
        if (status == 100) sql += " AND status IN (0,1,2)";
        else sql += " AND status=" + std::to_string(status);
    }
    if (!key.empty()) {
        sql += " AND (customer_name LIKE '%" + key + "%' OR customer_phone LIKE '%" + key +
               "%' OR model LIKE '%" + key + "%' OR order_no LIKE '%" + key + "%')";
    }
    sql += " ORDER BY id DESC LIMIT " + std::to_string(limit);
    Stmt s(db_, sql);
    while (s.step()) out.push_back(RepairFromRow(s));
    return out;
}

Repair Store::repair(int id) {
    Stmt s(db_, std::string("SELECT ") + kRepairCols + " FROM repairs WHERE id=?");
    s.bind(1, id);
    if (s.step()) return RepairFromRow(s);
    return Repair{};
}

std::string Store::nextRepairNo() {
    std::string date = today();
    std::string compact;
    for (char c : date) if (c != '-') compact.push_back(c);
    std::string prefix = "WX" + compact;
    int n = db_.queryInt("SELECT COUNT(*) FROM repairs WHERE order_no LIKE '" + prefix + "%'");
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%03d", n + 1);
    return prefix + buf;
}

int Store::createRepair(Repair& r) {
    if (r.orderNo.empty()) r.orderNo = nextRepairNo();
    if (r.createdAt.empty()) r.createdAt = now();
    if (r.totalFee <= 0) r.totalFee = r.laborFee + r.partsCost;
    Stmt s(db_, "INSERT INTO repairs(order_no,partner,customer_name,customer_phone,model,fault,status,technician,"
                "labor_fee,parts_cost,total_fee,paid,pay_method,created_at,started_at,finished_at,delivered_at,result,remark) "
                "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?, '', '', '',0,?)");
    s.bind(1, r.orderNo);
    s.bind(2, r.partner);
    s.bind(3, r.customerName);
    s.bind(4, r.customerPhone);
    s.bind(5, r.model);
    s.bind(6, r.fault);
    s.bind(7, r.status);
    s.bind(8, r.technician);
    s.bind(9, r.laborFee);
    s.bind(10, r.partsCost);
    s.bind(11, r.totalFee);
    s.bind(12, r.paid);
    s.bind(13, r.payMethod);
    s.bind(14, r.createdAt);
    s.bind(15, r.remark);
    s.run();
    r.id = (int)sqlite3_last_insert_rowid(db_.raw());
    return r.id;
}

void Store::acceptRepair(int id, int technicianId) {
    Stmt s(db_, "UPDATE repairs SET technician=?, status=1, started_at=? WHERE id=? AND status=0");
    s.bind(1, technicianId);
    s.bind(2, now());
    s.bind(3, id);
    s.run();
}

void Store::startRepair(int id) {
    Stmt s(db_, "UPDATE repairs SET status=1, started_at=? WHERE id=? AND status IN (0,1)");
    s.bind(1, now());
    s.bind(2, id);
    s.run();
}

void Store::finishRepair(int id, int result, const std::string& remark) {
    Repair r = repair(id);
    if (r.id == 0) return;
    double labor = r.laborFee;
    // 工费自动计算：工费=总价 * 工费比例（取维修师提成比例）
    if (labor <= 0 && result == 1) {
        Employee tech = employee(r.technician);
        double rate = tech.commission > 0 ? tech.commission : 30.0;
        labor = r.totalFee * rate / 100.0;
    }
    Stmt s(db_, "UPDATE repairs SET status=2, result=?, labor_fee=?, finished_at=?, remark=? WHERE id=?");
    s.bind(1, result);
    s.bind(2, labor);
    s.bind(3, now());
    s.bind(4, remark);
    s.bind(5, id);
    s.run();
}

void Store::deliverRepair(int id, double paid, const std::string& payMethod) {
    Repair r = repair(id);
    if (r.id == 0 || r.status == 3) return;
    Stmt s(db_, "UPDATE repairs SET status=3, paid=?, pay_method=?, delivered_at=? WHERE id=?");
    s.bind(1, paid);
    s.bind(2, payMethod);
    s.bind(3, now());
    s.bind(4, id);
    s.run();
    if (paid > 0) {
        addFinance(1, "维修收入", paid, payMethod, "维修", id, "维修单 " + r.orderNo);
    }
    // 未结清金额计入客户应收
    double diff = r.totalFee - paid;
    if (diff > 0.009 && r.partner > 0) {
        Stmt u(db_, "UPDATE partners SET receivable=receivable+? WHERE id=?");
        u.bind(1, diff);
        u.bind(2, r.partner);
        u.run();
    }
}

void Store::cancelRepair(int id) {
    Stmt s(db_, "UPDATE repairs SET status=4 WHERE id=? AND status<3");
    s.bind(1, id);
    s.run();
}

// ===========================================================================
// 配件业务
// ===========================================================================
static Part PartFromRow(Stmt& s) {
    Part p;
    p.id = s.colInt(0);
    p.sku = s.colText(1);
    p.name = s.colText(2);
    p.model = s.colText(3);
    p.unit = s.colText(4);
    p.cost = s.colDouble(5);
    p.price = s.colDouble(6);
    p.stock = s.colInt(7);
    p.warn = s.colInt(8);
    p.sn = s.colInt(9);
    p.remark = s.colText(10);
    return p;
}

std::vector<Part> Store::parts(const std::string& key) {
    std::vector<Part> out;
    std::string sql = "SELECT id,sku,name,model,unit,cost,price,stock,warn,sn,remark FROM parts WHERE 1=1";
    if (!key.empty()) sql += " AND (name LIKE '%" + key + "%' OR sku LIKE '%" + key + "%' OR model LIKE '%" + key + "%')";
    sql += " ORDER BY id DESC";
    Stmt s(db_, sql);
    while (s.step()) out.push_back(PartFromRow(s));
    return out;
}

Part Store::part(int id) {
    Stmt s(db_, "SELECT id,sku,name,model,unit,cost,price,stock,warn,sn,remark FROM parts WHERE id=?");
    s.bind(1, id);
    if (s.step()) return PartFromRow(s);
    return Part{};
}

int Store::savePart(Part& p) {
    if (p.id > 0) {
        Stmt s(db_, "UPDATE parts SET sku=?,name=?,model=?,unit=?,cost=?,price=?,stock=?,warn=?,sn=?,remark=? WHERE id=?");
        s.bind(1, p.sku);
        s.bind(2, p.name);
        s.bind(3, p.model);
        s.bind(4, p.unit);
        s.bind(5, p.cost);
        s.bind(6, p.price);
        s.bind(7, p.stock);
        s.bind(8, p.warn);
        s.bind(9, p.sn);
        s.bind(10, p.remark);
        s.bind(11, p.id);
        s.run();
        return p.id;
    }
    Stmt s(db_, "INSERT INTO parts(sku,name,model,unit,cost,price,stock,warn,sn,remark) VALUES(?,?,?,?,?,?,?,?,?,?)");
    s.bind(1, p.sku);
    s.bind(2, p.name);
    s.bind(3, p.model);
    s.bind(4, p.unit);
    s.bind(5, p.cost);
    s.bind(6, p.price);
    s.bind(7, p.stock);
    s.bind(8, p.warn);
    s.bind(9, p.sn);
    s.bind(10, p.remark);
    s.run();
    p.id = (int)sqlite3_last_insert_rowid(db_.raw());
    return p.id;
}

void Store::removePart(int id) {
    Stmt s(db_, "DELETE FROM parts WHERE id=?");
    s.bind(1, id);
    s.run();
}

void Store::partStockChange(int partId, int type, int qty, double price,
                            int partnerId, const std::string& remark) {
    Part p = part(partId);
    if (p.id == 0 || qty <= 0) return;
    int delta = (type == 1) ? qty : (type == 2 ? -qty : 0);
    double amount = price * qty;
    db_.begin();
    if (type == 3) {
        // 盘点调整：直接把库存改为 qty，差额记录流水
        delta = qty - p.stock;
        amount = p.cost * delta;
        Stmt u(db_, "UPDATE parts SET stock=? WHERE id=?");
        u.bind(1, qty);
        u.bind(2, partId);
        u.run();
    } else {
        Stmt u(db_, "UPDATE parts SET stock=stock+? WHERE id=?");
        u.bind(1, delta);
        u.bind(2, partId);
        u.run();
    }
    Stmt s(db_, "INSERT INTO part_flows(part_id,type,qty,price,amount,partner_id,ref,operator,created_at,remark) "
                "VALUES(?,?,?,?,?,?, '',0,?,?)");
    s.bind(1, partId);
    s.bind(2, type);
    s.bind(3, type == 3 ? delta : qty);
    s.bind(4, price);
    s.bind(5, amount);
    s.bind(6, partnerId);
    s.bind(7, now());
    s.bind(8, remark);
    s.run();
    db_.commit();

    if (type == 1 && amount > 0) {
        addFinance(2, "配件采购", amount, "现金", "配件", partId, p.name + " 入库");
    } else if (type == 2 && amount > 0) {
        addFinance(1, "配件销售", amount, "现金", "配件", partId, p.name + " 出库");
    }
}

std::vector<PartFlow> Store::partFlows(int partId, int limit) {
    std::vector<PartFlow> out;
    std::string sql =
        "SELECT f.id,f.part_id,f.type,f.qty,f.price,f.amount,f.partner_id,f.ref,f.operator,f.created_at,f.remark,"
        "IFNULL(p.name,''),IFNULL(pt.name,'') FROM part_flows f "
        "LEFT JOIN parts p ON p.id=f.part_id LEFT JOIN partners pt ON pt.id=f.partner_id WHERE 1=1";
    if (partId > 0) sql += " AND f.part_id=" + std::to_string(partId);
    sql += " ORDER BY f.id DESC LIMIT " + std::to_string(limit);
    Stmt s(db_, sql);
    while (s.step()) {
        PartFlow f;
        f.id = s.colInt(0);
        f.partId = s.colInt(1);
        f.type = s.colInt(2);
        f.qty = s.colInt(3);
        f.price = s.colDouble(4);
        f.amount = s.colDouble(5);
        f.partnerId = s.colInt(6);
        f.ref = s.colText(7);
        f.operatorId = s.colInt(8);
        f.createdAt = s.colText(9);
        f.remark = s.colText(10);
        f.partName = s.colText(11);
        f.partnerName = s.colText(12);
        out.push_back(f);
    }
    return out;
}

// ===========================================================================
// 次卡业务
// ===========================================================================
static CardTpl TplFromRow(Stmt& s) {
    CardTpl t;
    t.id = s.colInt(0);
    t.name = s.colText(1);
    t.times = s.colInt(2);
    t.price = s.colDouble(3);
    t.validDays = s.colInt(4);
    t.remark = s.colText(5);
    return t;
}

std::vector<CardTpl> Store::cardTpls() {
    std::vector<CardTpl> out;
    Stmt s(db_, "SELECT id,name,times,price,valid_days,remark FROM card_tpls ORDER BY id DESC");
    while (s.step()) out.push_back(TplFromRow(s));
    return out;
}

CardTpl Store::cardTpl(int id) {
    Stmt s(db_, "SELECT id,name,times,price,valid_days,remark FROM card_tpls WHERE id=?");
    s.bind(1, id);
    if (s.step()) return TplFromRow(s);
    return CardTpl{};
}

int Store::saveCardTpl(CardTpl& t) {
    if (t.id > 0) {
        Stmt s(db_, "UPDATE card_tpls SET name=?,times=?,price=?,valid_days=?,remark=? WHERE id=?");
        s.bind(1, t.name);
        s.bind(2, t.times);
        s.bind(3, t.price);
        s.bind(4, t.validDays);
        s.bind(5, t.remark);
        s.bind(6, t.id);
        s.run();
        return t.id;
    }
    Stmt s(db_, "INSERT INTO card_tpls(name,times,price,valid_days,remark) VALUES(?,?,?,?,?)");
    s.bind(1, t.name);
    s.bind(2, t.times);
    s.bind(3, t.price);
    s.bind(4, t.validDays);
    s.bind(5, t.remark);
    s.run();
    t.id = (int)sqlite3_last_insert_rowid(db_.raw());
    return t.id;
}

void Store::removeCardTpl(int id) {
    Stmt s(db_, "DELETE FROM card_tpls WHERE id=?");
    s.bind(1, id);
    s.run();
}

static Card CardFromRow(Stmt& s) {
    Card c;
    c.id = s.colInt(0);
    c.cardNo = s.colText(1);
    c.tplId = s.colInt(2);
    c.partnerId = s.colInt(3);
    c.times = s.colInt(4);
    c.remain = s.colInt(5);
    c.price = s.colDouble(6);
    c.createdAt = s.colText(7);
    c.expireAt = s.colText(8);
    c.status = s.colInt(9);
    c.tplName = s.colText(10);
    c.partnerName = s.colText(11);
    return c;
}

std::vector<Card> Store::cards(const std::string& key) {
    std::vector<Card> out;
    std::string sql =
        "SELECT c.id,c.card_no,c.tpl_id,c.partner_id,c.times,c.remain,c.price,c.created_at,c.expire_at,c.status,"
        "IFNULL(t.name,''),IFNULL(p.name,'') FROM cards c "
        "LEFT JOIN card_tpls t ON t.id=c.tpl_id LEFT JOIN partners p ON p.id=c.partner_id WHERE 1=1";
    if (!key.empty()) sql += " AND (c.card_no LIKE '%" + key + "%' OR p.name LIKE '%" + key + "%' OR t.name LIKE '%" + key + "%')";
    sql += " ORDER BY c.id DESC";
    Stmt s(db_, sql);
    while (s.step()) out.push_back(CardFromRow(s));
    return out;
}

Card Store::card(int id) {
    std::string sql =
        "SELECT c.id,c.card_no,c.tpl_id,c.partner_id,c.times,c.remain,c.price,c.created_at,c.expire_at,c.status,"
        "IFNULL(t.name,''),IFNULL(p.name,'') FROM cards c "
        "LEFT JOIN card_tpls t ON t.id=c.tpl_id LEFT JOIN partners p ON p.id=c.partner_id WHERE c.id=?";
    Stmt s(db_, sql);
    s.bind(1, id);
    if (s.step()) return CardFromRow(s);
    return Card{};
}

int Store::openCard(int tplId, int partnerId) {
    CardTpl t = cardTpl(tplId);
    if (t.id == 0) return 0;
    std::string no = "CK" + Store::today().substr(0, 4) + Store::today().substr(5, 2) + Store::today().substr(8, 2);
    int seq = db_.queryInt("SELECT COUNT(*) FROM cards WHERE card_no LIKE '" + no + "%'") + 1;
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%04d", seq);
    no += buf;
    std::string expire;
    if (t.validDays > 0) expire = addDays(today(), t.validDays);
    Stmt s(db_, "INSERT INTO cards(card_no,tpl_id,partner_id,times,remain,price,created_at,expire_at,status) "
                "VALUES(?,?,?,?,?,?,?,?,1)");
    s.bind(1, no);
    s.bind(2, tplId);
    s.bind(3, partnerId);
    s.bind(4, t.times);
    s.bind(5, t.times);
    s.bind(6, t.price);
    s.bind(7, now());
    s.bind(8, expire);
    s.run();
    int id = (int)sqlite3_last_insert_rowid(db_.raw());

    Stmt r(db_, "INSERT INTO card_records(card_id,type,times,remark,created_at) VALUES(?,1,?,'开卡',?)");
    r.bind(1, id);
    r.bind(2, t.times);
    r.bind(3, now());
    r.run();

    if (t.price > 0) addFinance(1, "次卡收入", t.price, "现金", "次卡", id, "开卡 " + t.name);
    return id;
}

void Store::useCard(int cardId, int times, const std::string& remark) {
    Card c = card(cardId);
    if (c.id == 0 || times <= 0) return;
    int remain = std::max(0, c.remain - times);
    int status = remain == 0 ? 2 : 1;
    Stmt s(db_, "UPDATE cards SET remain=?,status=? WHERE id=?");
    s.bind(1, remain);
    s.bind(2, status);
    s.bind(3, cardId);
    s.run();
    Stmt r(db_, "INSERT INTO card_records(card_id,type,times,remark,created_at) VALUES(?,2,?,?,?)");
    r.bind(1, cardId);
    r.bind(2, times);
    r.bind(3, remark);
    r.bind(4, now());
    r.run();
}

std::vector<CardRecord> Store::cardRecords(int cardId) {
    std::vector<CardRecord> out;
    std::string sql = "SELECT id,card_id,type,times,remark,created_at FROM card_records WHERE 1=1";
    if (cardId > 0) sql += " AND card_id=" + std::to_string(cardId);
    sql += " ORDER BY id DESC";
    Stmt s(db_, sql);
    while (s.step()) {
        CardRecord r;
        r.id = s.colInt(0);
        r.cardId = s.colInt(1);
        r.type = s.colInt(2);
        r.times = s.colInt(3);
        r.remark = s.colText(4);
        r.createdAt = s.colText(5);
        out.push_back(r);
    }
    return out;
}

// ===========================================================================
// 财务
// ===========================================================================
std::vector<Account> Store::accounts() {
    std::vector<Account> out;
    Stmt s(db_, "SELECT id,name,balance FROM accounts ORDER BY id");
    while (s.step()) {
        Account a;
        a.id = s.colInt(0);
        a.name = s.colText(1);
        a.balance = s.colDouble(2);
        out.push_back(a);
    }
    return out;
}

int Store::saveAccount(Account& a) {
    if (a.id > 0) {
        Stmt s(db_, "UPDATE accounts SET name=? WHERE id=?");
        s.bind(1, a.name);
        s.bind(2, a.id);
        s.run();
        return a.id;
    }
    Stmt s(db_, "INSERT OR IGNORE INTO accounts(name,balance) VALUES(?,?)");
    s.bind(1, a.name);
    s.bind(2, a.balance);
    s.run();
    return (int)sqlite3_last_insert_rowid(db_.raw());
}

void Store::removeAccount(int id) {
    Stmt s(db_, "DELETE FROM accounts WHERE id=?");
    s.bind(1, id);
    s.run();
}

void Store::addFinance(int type, const std::string& category, double amount,
                       const std::string& account, const std::string& refType, int refId,
                       const std::string& remark) {
    if (amount <= 0) return;
    std::string acc = account.empty() ? "现金" : account;
    Stmt s(db_, "INSERT INTO finances(type,category,amount,account,ref_type,ref_id,remark,created_at,operator) "
                "VALUES(?,?,?,?,?,?,?,?,0)");
    s.bind(1, type);
    s.bind(2, category);
    s.bind(3, amount);
    s.bind(4, acc);
    s.bind(5, refType);
    s.bind(6, refId);
    s.bind(7, remark);
    s.bind(8, now());
    s.run();

    Stmt u(db_, "UPDATE accounts SET balance=balance+? WHERE name=?");
    u.bind(1, type == 1 ? amount : -amount);
    u.bind(2, acc);
    u.run();
    // 若账户不存在则自动创建，保证余额统计不丢失
    if (sqlite3_changes(db_.raw()) == 0) {
        Stmt ins(db_, "INSERT OR IGNORE INTO accounts(name,balance) VALUES(?,?)");
        ins.bind(1, acc);
        ins.bind(2, type == 1 ? amount : -amount);
        ins.run();
    }
}

std::vector<Finance> Store::finances(const std::string& from, const std::string& to, int limit) {
    std::vector<Finance> out;
    std::string sql = "SELECT id,type,category,amount,account,ref_type,ref_id,remark,created_at,operator "
                      "FROM finances WHERE 1=1";
    if (!from.empty()) sql += " AND date(created_at)>='" + from + "'";
    if (!to.empty()) sql += " AND date(created_at)<='" + to + "'";
    sql += " ORDER BY id DESC LIMIT " + std::to_string(limit);
    Stmt s(db_, sql);
    while (s.step()) {
        Finance f;
        f.id = s.colInt(0);
        f.type = s.colInt(1);
        f.category = s.colText(2);
        f.amount = s.colDouble(3);
        f.account = s.colText(4);
        f.refType = s.colText(5);
        f.refId = s.colInt(6);
        f.remark = s.colText(7);
        f.createdAt = s.colText(8);
        f.operatorId = s.colInt(9);
        out.push_back(f);
    }
    return out;
}

void Store::settle(int partnerId, double amount, const std::string& account, const std::string& remark) {
    if (amount <= 0) return;
    Partner p = partner(partnerId);
    if (p.id == 0) return;
    if (p.receivable > 0) {
        double use = std::min(amount, p.receivable);
        Stmt s(db_, "UPDATE partners SET receivable=MAX(0,receivable-?) WHERE id=?");
        s.bind(1, use);
        s.bind(2, partnerId);
        s.run();
        addFinance(1, "欠款回收", use, account, "往来单位", partnerId, remark);
        amount -= use;
    }
    if (amount > 0.001 && p.payable > 0) {
        double use = std::min(amount, p.payable);
        Stmt s(db_, "UPDATE partners SET payable=MAX(0,payable-?) WHERE id=?");
        s.bind(1, use);
        s.bind(2, partnerId);
        s.run();
        addFinance(2, "欠款支付", use, account, "往来单位", partnerId, remark);
    }
}

// ===========================================================================
// 机型资料库
// ===========================================================================
std::vector<PhoneModel> Store::models(const std::string& key) {
    std::vector<PhoneModel> out;
    std::string sql = "SELECT brand,model,storage FROM models WHERE 1=1";
    if (!key.empty()) sql += " AND (brand LIKE '%" + key + "%' OR model LIKE '%" + key + "%')";
    sql += " ORDER BY brand,model,storage";
    Stmt s(db_, sql);
    while (s.step()) {
        PhoneModel m;
        m.brand = s.colText(0);
        m.model = s.colText(1);
        m.storage = s.colText(2);
        out.push_back(m);
    }
    return out;
}

std::vector<std::string> Store::brands() {
    std::vector<std::string> out;
    Stmt s(db_, "SELECT DISTINCT brand FROM models ORDER BY brand");
    while (s.step()) out.push_back(s.colText(0));
    return out;
}

// ===========================================================================
// 统计
// ===========================================================================
DashboardStat Store::dashboard() {
    DashboardStat d;
    std::string todayStr = today();
    double phoneSales = db_.queryDouble("SELECT IFNULL(SUM(out_price),0) FROM phones WHERE status=1 AND date(out_time)='" + todayStr + "'");
    double repairIncome = db_.queryDouble("SELECT IFNULL(SUM(paid),0) FROM repairs WHERE date(delivered_at)='" + todayStr + "'");
    double partsSales = db_.queryDouble("SELECT IFNULL(SUM(amount),0) FROM part_flows WHERE type=2 AND date(created_at)='" + todayStr + "'");
    double cardIncome = db_.queryDouble("SELECT IFNULL(SUM(price),0) FROM cards WHERE date(created_at)='" + todayStr + "'");
    d.todaySales = phoneSales + repairIncome + partsSales + cardIncome;

    d.todayProfit =
        db_.queryDouble("SELECT IFNULL(SUM(out_price-in_price-prep_cost),0) FROM phones WHERE status=1 AND date(out_time)='" + todayStr + "'") +
        db_.queryDouble("SELECT IFNULL(SUM(paid-parts_cost),0) FROM repairs WHERE date(delivered_at)='" + todayStr + "'") +
        db_.queryDouble("SELECT IFNULL(SUM((f.price-IFNULL(p.cost,0))*f.qty),0) FROM part_flows f "
                        "LEFT JOIN parts p ON p.id=f.part_id WHERE f.type=2 AND date(f.created_at)='" + todayStr + "'") +
        cardIncome;

    d.todayOrders = db_.queryInt("SELECT COUNT(*) FROM phones WHERE status=1 AND date(out_time)='" + todayStr + "'") +
                    db_.queryInt("SELECT COUNT(*) FROM repairs WHERE date(delivered_at)='" + todayStr + "'") +
                    db_.queryInt("SELECT COUNT(*) FROM part_flows WHERE type=2 AND date(created_at)='" + todayStr + "'") +
                    db_.queryInt("SELECT COUNT(*) FROM cards WHERE date(created_at)='" + todayStr + "'");

    d.stockPhones = db_.queryInt("SELECT COUNT(*) FROM phones WHERE status=0");
    d.stockValue = db_.queryDouble("SELECT IFNULL(SUM(in_price+prep_cost),0) FROM phones WHERE status=0");
    d.pendingRepairs = db_.queryInt("SELECT COUNT(*) FROM repairs WHERE status IN (0,1,2)");
    d.warnParts = db_.queryInt("SELECT COUNT(*) FROM parts WHERE warn>0 AND stock<=warn");
    d.totalPartners = db_.queryInt("SELECT COUNT(*) FROM partners");
    d.receivableCount = db_.queryInt("SELECT COUNT(*) FROM partners WHERE receivable>0.001");
    d.receivable = db_.queryDouble("SELECT IFNULL(SUM(receivable),0) FROM partners");
    return d;
}

SalesSummary Store::salesSummary(const std::string& from, const std::string& to) {
    SalesSummary s;
    std::string range = "date(out_time)>='" + from + "' AND date(out_time)<='" + to + "'";
    s.phoneSales = db_.queryDouble("SELECT IFNULL(SUM(out_price),0) FROM phones WHERE status=1 AND " + range);
    s.phoneProfit = db_.queryDouble("SELECT IFNULL(SUM(out_price-in_price-prep_cost),0) FROM phones WHERE status=1 AND " + range);
    s.phoneCount = db_.queryInt("SELECT COUNT(*) FROM phones WHERE status=1 AND " + range);

    std::string rrange = "date(delivered_at)>='" + from + "' AND date(delivered_at)<='" + to + "'";
    s.repairIncome = db_.queryDouble("SELECT IFNULL(SUM(paid),0) FROM repairs WHERE " + rrange);
    s.repairProfit = db_.queryDouble("SELECT IFNULL(SUM(paid-parts_cost),0) FROM repairs WHERE " + rrange);
    s.repairCount = db_.queryInt("SELECT COUNT(*) FROM repairs WHERE " + rrange);

    std::string prange = "date(f.created_at)>='" + from + "' AND date(f.created_at)<='" + to + "'";
    s.partsSales = db_.queryDouble("SELECT IFNULL(SUM(f.amount),0) FROM part_flows f WHERE f.type=2 AND " + prange);
    s.partsProfit = db_.queryDouble("SELECT IFNULL(SUM((f.price-IFNULL(p.cost,0))*f.qty),0) FROM part_flows f "
                                    "LEFT JOIN parts p ON p.id=f.part_id WHERE f.type=2 AND " + prange);
    s.partQty = db_.queryInt("SELECT IFNULL(SUM(f.qty),0) FROM part_flows f WHERE f.type=2 AND " + prange);

    std::string crange = "date(created_at)>='" + from + "' AND date(created_at)<='" + to + "'";
    s.cardIncome = db_.queryDouble("SELECT IFNULL(SUM(price),0) FROM cards WHERE " + crange);
    s.cardCount = db_.queryInt("SELECT COUNT(*) FROM cards WHERE " + crange);

    s.totalIncome = s.phoneSales + s.repairIncome + s.partsSales + s.cardIncome;
    s.totalProfit = s.phoneProfit + s.repairProfit + s.partsProfit + s.cardIncome;
    return s;
}

std::vector<std::pair<std::string, double>> Store::salesByDay(const std::string& from, const std::string& to) {
    std::vector<std::pair<std::string, double>> out;
    std::string cur = from;
    int guard = 0;
    while (cur <= to && guard++ < 400) {
        double v = db_.queryDouble("SELECT IFNULL(SUM(out_price),0) FROM phones WHERE status=1 AND date(out_time)='" + cur + "'") +
                   db_.queryDouble("SELECT IFNULL(SUM(paid),0) FROM repairs WHERE date(delivered_at)='" + cur + "'") +
                   db_.queryDouble("SELECT IFNULL(SUM(amount),0) FROM part_flows WHERE type=2 AND date(created_at)='" + cur + "'") +
                   db_.queryDouble("SELECT IFNULL(SUM(price),0) FROM cards WHERE date(created_at)='" + cur + "'");
        out.push_back({cur, v});
        cur = addDays(cur, 1);
    }
    return out;
}

std::vector<std::pair<std::string, double>> Store::profitByDay(const std::string& from, const std::string& to) {
    std::vector<std::pair<std::string, double>> out;
    std::string cur = from;
    int guard = 0;
    while (cur <= to && guard++ < 400) {
        double v = db_.queryDouble("SELECT IFNULL(SUM(out_price-in_price-prep_cost),0) FROM phones WHERE status=1 AND date(out_time)='" + cur + "'") +
                   db_.queryDouble("SELECT IFNULL(SUM(paid-parts_cost),0) FROM repairs WHERE date(delivered_at)='" + cur + "'") +
                   db_.queryDouble("SELECT IFNULL(SUM((f.price-IFNULL(p.cost,0))*f.qty),0) FROM part_flows f "
                                   "LEFT JOIN parts p ON p.id=f.part_id WHERE f.type=2 AND date(f.created_at)='" + cur + "'") +
                   db_.queryDouble("SELECT IFNULL(SUM(price),0) FROM cards WHERE date(created_at)='" + cur + "'");
        out.push_back({cur, v});
        cur = addDays(cur, 1);
    }
    return out;
}

std::vector<StaffPerf> Store::staffPerf(const std::string& from, const std::string& to) {
    std::vector<StaffPerf> out;
    auto list = employees(true);
    for (auto& e : list) {
        StaffPerf p;
        p.id = e.id;
        p.name = e.name;
        p.repairCount = db_.queryInt("SELECT COUNT(*) FROM repairs WHERE technician=" + std::to_string(e.id) +
                                     " AND status=3 AND date(delivered_at)>='" + from + "' AND date(delivered_at)<='" + to + "'");
        p.repairAmount = db_.queryDouble("SELECT IFNULL(SUM(total_fee),0) FROM repairs WHERE technician=" + std::to_string(e.id) +
                                         " AND status=3 AND date(delivered_at)>='" + from + "' AND date(delivered_at)<='" + to + "'");
        p.phoneCount = db_.queryInt("SELECT COUNT(*) FROM phones WHERE out_operator=" + std::to_string(e.id) +
                                    " AND status=1 AND date(out_time)>='" + from + "' AND date(out_time)<='" + to + "'");
        p.phoneSales = db_.queryDouble("SELECT IFNULL(SUM(out_price),0) FROM phones WHERE out_operator=" + std::to_string(e.id) +
                                       " AND status=1 AND date(out_time)>='" + from + "' AND date(out_time)<='" + to + "'");
        out.push_back(p);
    }
    return out;
}

std::vector<std::pair<std::string, std::pair<int, double>>> Store::partsRanking(const std::string& from, const std::string& to) {
    std::vector<std::pair<std::string, std::pair<int, double>>> out;
    Stmt s(db_, "SELECT IFNULL(p.name,'未知'), SUM(f.qty), SUM(f.amount) FROM part_flows f "
                "LEFT JOIN parts p ON p.id=f.part_id WHERE f.type=2 AND date(f.created_at)>='" + from +
                "' AND date(f.created_at)<='" + to + "' GROUP BY f.part_id ORDER BY SUM(f.amount) DESC LIMIT 20");
    while (s.step()) {
        out.push_back({s.colText(0), {s.colInt(1), s.colDouble(2)}});
    }
    return out;
}

// ===========================================================================
// 数据维护
// ===========================================================================
void Store::loadDemoData() {
    // 示例客户与供应商
    Partner c1; c1.type = 1; c1.name = "张先生"; c1.phone = "13800138000"; c1.address = "本地"; c1.remark = "老客户";
    savePartner(c1);
    Partner c2; c2.type = 1; c2.name = "李女士"; c2.phone = "13900139000";
    savePartner(c2);
    Partner s1; s1.type = 2; s1.name = "华强北数码批发"; s1.phone = "0755-88888888";
    savePartner(s1);

    // 员工
    Employee e1; e1.name = "王师傅"; e1.phone = "13700137000"; e1.role = "维修师"; e1.commission = 35;
    saveEmployee(e1);
    Employee e2; e2.name = "小陈"; e2.phone = "13600136000"; e2.role = "销售";
    saveEmployee(e2);

    // 配件
    Part p1; p1.name = "iPhone 16 屏幕总成"; p1.sku = "SC-IP16"; p1.model = "原装拆机"; p1.unit = "片";
    p1.cost = 680; p1.price = 1080; p1.stock = 8; p1.warn = 3; savePart(p1);
    Part p2; p2.name = "Type-C 数据线"; p2.sku = "DL-TC01"; p2.model = "1米"; p2.unit = "条";
    p2.cost = 5; p2.price = 29; p2.stock = 60; p2.warn = 20; savePart(p2);
    Part p3; p3.name = "钢化膜"; p3.sku = "FM-001"; p3.model = "通用"; p3.unit = "张";
    p3.cost = 2; p3.price = 15; p3.stock = 12; p3.warn = 30; savePart(p3);
    Part p4; p4.name = "华为 P50 电池"; p4.sku = "BT-P50"; p4.model = "原装"; p4.unit = "块";
    p4.cost = 85; p4.price = 180; p4.stock = 15; p4.warn = 5; savePart(p4);

    // 手机入库
    PhoneDev d1; d1.brand = "Apple"; d1.model = "iPhone 16 Pro"; d1.storage = "256G"; d1.color = "沙漠金";
    d1.condition = "99新"; d1.imei = "359123456789012"; d1.inPrice = 5600; d1.prepCost = 0; d1.inSupplier = s1.id;
    addPhoneIn(d1);
    PhoneDev d2; d2.brand = "Apple"; d2.model = "iPhone 15"; d2.storage = "128G"; d2.color = "蓝色";
    d2.condition = "95新"; d2.imei = "359123456789013"; d2.inPrice = 3000; d2.prepCost = 50; d2.inSupplier = s1.id;
    addPhoneIn(d2);
    PhoneDev d3; d3.brand = "华为"; d3.model = "Mate 60 Pro"; d3.storage = "512G"; d3.color = "雅川青";
    d3.condition = "全新"; d3.imei = "869123456789014"; d3.inPrice = 5200; d3.inSupplier = s1.id;
    addPhoneIn(d3);

    // 卖出一台
    sellPhone(d2.id, 3680, c1.id, "微信", 2);

    // 维修单
    Repair r1; r1.partner = c1.id; r1.customerName = "张先生"; r1.customerPhone = "13800138000";
    r1.model = "iPhone 16 Pro"; r1.fault = "屏幕碎裂，需更换屏幕总成";
    r1.technician = e1.id; r1.laborFee = 0; r1.partsCost = 680; r1.totalFee = 1080; r1.status = 1;
    r1.createdAt = now();
    createRepair(r1);

    Repair r2; r2.partner = c2.id; r2.customerName = "李女士"; r2.customerPhone = "13900139000";
    r2.model = "华为 P50"; r2.fault = "电池不耐用，待机时间短";
    r2.technician = e1.id; r2.partsCost = 85; r2.totalFee = 180; r2.status = 0;
    createRepair(r2);

    // 次卡
    CardTpl t1; t1.name = "贴膜10次卡"; t1.times = 10; t1.price = 99; t1.validDays = 365;
    saveCardTpl(t1);
    CardTpl t2; t2.name = "清洗保养5次卡"; t2.times = 5; t2.price = 150; t2.validDays = 180;
    saveCardTpl(t2);
    openCard(t1.id, c1.id);

    // 手工支出
    addFinance(2, "房租水电", 3500, "银行卡", "手动", 0, "本月房租");
}

void Store::clearAllData() {
    const char* tables[] = {"phones", "repairs", "part_flows", "parts", "cards", "card_records",
                            "card_tpls", "finances", "partners", "employees", "accounts", "models"};
    db_.begin();
    for (const char* t : tables) db_.exec(std::string("DELETE FROM ") + t + ";");
    db_.exec("DELETE FROM sqlite_sequence;");
    db_.commit();
    seedBase();
}

bool Store::backupTo(const std::string& path) {
    // VACUUM INTO 生成完整备份文件
    std::string sql = "VACUUM INTO '" + path + "';";
    return db_.exec(sql);
}
