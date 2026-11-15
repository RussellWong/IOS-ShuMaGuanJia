#include "core/db.h"

#include <cstdio>

Db::~Db() { close(); }

bool Db::open(const std::string& path) {
    close();
    int rc = sqlite3_open(path.c_str(), &db_);
    if (rc != SQLITE_OK) {
        err_ = db_ ? sqlite3_errmsg(db_) : "无法打开数据库";
        if (db_) {
            sqlite3_close(db_);
            db_ = nullptr;
        }
        return false;
    }
    // WAL 提升写入性能，同时保证断电安全
    sqlite3_exec(db_, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
    sqlite3_exec(db_, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr);
    sqlite3_exec(db_, "PRAGMA foreign_keys=ON;", nullptr, nullptr, nullptr);
    return true;
}

void Db::close() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool Db::exec(const std::string& sql) {
    if (!db_) return false;
    char* msg = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &msg);
    if (rc != SQLITE_OK) {
        err_ = msg ? msg : "SQL 执行失败";
        if (msg) sqlite3_free(msg);
        return false;
    }
    return true;
}

std::string Db::lastError() const {
    if (!err_.empty()) return err_;
    return db_ ? sqlite3_errmsg(db_) : "数据库未打开";
}

int Db::queryInt(const std::string& sql, int def) {
    Stmt s(*this, sql);
    if (s.step()) return s.colInt(0);
    return def;
}

double Db::queryDouble(const std::string& sql, double def) {
    Stmt s(*this, sql);
    if (s.step()) return s.colDouble(0);
    return def;
}

std::string Db::queryText(const std::string& sql, const std::string& def) {
    Stmt s(*this, sql);
    if (s.step() && !s.colIsNull(0)) return s.colText(0);
    return def;
}

bool Db::begin() { return exec("BEGIN;"); }
bool Db::commit() { return exec("COMMIT;"); }

Stmt::Stmt(Db& db, const std::string& sql) {
    if (db.isOpen()) {
        sqlite3_prepare_v2(db.raw(), sql.c_str(), -1, &stmt_, nullptr);
    }
}

Stmt::~Stmt() {
    if (stmt_) sqlite3_finalize(stmt_);
}

void Stmt::bind(int idx, int value) {
    if (stmt_) sqlite3_bind_int(stmt_, idx, value);
}

void Stmt::bind(int idx, double value) {
    if (stmt_) sqlite3_bind_double(stmt_, idx, value);
}

void Stmt::bind(int idx, const std::string& value) {
    if (stmt_) sqlite3_bind_text(stmt_, idx, value.c_str(), -1, SQLITE_TRANSIENT);
}

void Stmt::bindNull(int idx) {
    if (stmt_) sqlite3_bind_null(stmt_, idx);
}

bool Stmt::step() {
    if (!stmt_) return false;
    int rc = sqlite3_step(stmt_);
    return rc == SQLITE_ROW;
}

bool Stmt::run() {
    if (!stmt_) return false;
    int rc = sqlite3_step(stmt_);
    return rc == SQLITE_DONE || rc == SQLITE_ROW;
}

int Stmt::colInt(int idx) const {
    return stmt_ ? sqlite3_column_int(stmt_, idx) : 0;
}

double Stmt::colDouble(int idx) const {
    return stmt_ ? sqlite3_column_double(stmt_, idx) : 0.0;
}

std::string Stmt::colText(int idx) const {
    if (!stmt_) return "";
    const unsigned char* p = sqlite3_column_text(stmt_, idx);
    return p ? reinterpret_cast<const char*>(p) : "";
}

bool Stmt::colIsNull(int idx) const {
    return !stmt_ || sqlite3_column_type(stmt_, idx) == SQLITE_NULL;
}
