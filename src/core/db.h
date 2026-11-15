// SQLite 轻量封装：提供连接、执行与带参数查询能力
#pragma once

#include <string>
#include <sqlite3.h>

class Db {
public:
    Db() = default;
    ~Db();

    // 打开（或创建）数据库文件
    bool open(const std::string& path);
    void close();
    bool isOpen() const { return db_ != nullptr; }

    // 执行无返回值的 SQL
    bool exec(const std::string& sql);

    // 最近一次错误信息
    std::string lastError() const;

    sqlite3* raw() const { return db_; }

    // 查询单条整型结果（如 COUNT）
    int queryInt(const std::string& sql, int def = 0);
    // 查询单条浮点结果（如 SUM）
    double queryDouble(const std::string& sql, double def = 0.0);
    // 查询单条文本结果
    std::string queryText(const std::string& sql, const std::string& def = "");

    // 开启/提交事务
    bool begin();
    bool commit();

private:
    sqlite3* db_ = nullptr;
    std::string err_;
};

// 预处理语句封装，简化参数绑定
class Stmt {
public:
    Stmt(Db& db, const std::string& sql);
    ~Stmt();

    Stmt(const Stmt&) = delete;
    Stmt& operator=(const Stmt&) = delete;

    bool valid() const { return stmt_ != nullptr; }

    void bind(int idx, int value);
    void bind(int idx, double value);
    void bind(int idx, const std::string& value);
    void bindNull(int idx);

    // 执行一步：有行为 true，结束为 false
    bool step();
    // 执行写操作（不关心结果集）
    bool run();

    int colInt(int idx) const;
    double colDouble(int idx) const;
    std::string colText(int idx) const;
    bool colIsNull(int idx) const;

    sqlite3_stmt* raw() const { return stmt_; }

private:
    sqlite3_stmt* stmt_ = nullptr;
};
