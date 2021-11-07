#ifdef WITH_SQLITE

#include "db_manager.h"

#include <sstream>
#include <type_traits>

#include "log.h"
#include "match.h"

#include "sqlite_modern_cpp.h"

static void HandleError(const sqlite::sqlite_exception& e)
{
    ErrorLog() << "DB error " << e.get_code() << ": " << e.what() << ", during " << e.get_sql();
}

static void HandleError(const std::exception& e)
{
    ErrorLog() << "DB error " << e.what();
}

template <typename Fn>
static bool ExecuteTransaction(const std::string& db_name, const Fn& fn)
{
    try {
        sqlite::database db(db_name);
        db << "BEGIN;";
        if (fn(db)) {
            db << "COMMIT;";
            return true;
        } else {
            db << "ROLLBACK";
            return false;
        }
    } catch (const sqlite::sqlite_exception& e) {
        HandleError(e);
    } catch (const std::exception& e) {
        HandleError(e);
    }
    return false;
}


#define FAIL_THROW(sql_func, ...)                                                    \
    do {                                                                             \
        SQLRETURN ret = sql_func(##__VA_ARGS__);                                     \
        if (!SQL_SUCCEEDED(ret)) {                                                   \
            throw(std::stringstream() << #sql_func << " failed ret: " << ret).str(); \
        }                                                                            \
    } while (0)

SQLiteDBManager::SQLiteDBManager(const std::string& db_name) : db_name_(db_name)
{
}

SQLiteDBManager::~SQLiteDBManager() {}

bool SQLiteDBManager::RecordMatch(const std::string& game_name, const std::optional<GroupID> gid, const UserID host_uid,
                    const uint64_t multiple, const std::vector<ScoreInfo>& score_infos)
{
    return ExecuteTransaction(db_name_, [&](sqlite::database& db)
        {
            db << "INSERT INTO match (game_name, group_id, host_user_id, user_count, multiple) VALUES (?,?,?,?,?);"
               << game_name
               << gid
               << host_uid.Get()
               << score_infos.size()
               << multiple;
            const uint64_t match_id  = db.last_insert_rowid();
            for (const ScoreInfo& score_info : score_infos) {
                db << "INSERT INTO user (user_id) SELECT ? WHERE NOT EXISTS "
                      "(SELECT user_id FROM user WHERE user_id = ?);"
                   << score_info.uid_.Get() << score_info.uid_.Get();
                db << "INSERT INTO user_with_match (match_id, user_id, game_score, zero_sum_score, top_score) VALUES (?,?,?,?,?);"
                   << match_id
                   << score_info.uid_.Get()
                   << score_info.game_score_
                   << score_info.zero_sum_score_
                   << score_info.top_score_;
            }
            return true;
        });
}

UserProfile SQLiteDBManager::GetUserProfile(const UserID uid)
{
    UserProfile profit;
    ExecuteTransaction(db_name_, [&](sqlite::database& db)
        {
            db << "SELECT COUNT(*), SUM(zero_sum_score), SUM(top_score) FROM user_with_match WHERE user_id = ?;"
               << uid.Get()
               >> [&](const uint64_t match_count, const int64_t total_zero_sum_score, const int64_t total_top_score)
                      {
                          profit.uid_ = uid;
                          profit.match_count_ = match_count;
                          profit.total_zero_sum_score_ = total_zero_sum_score;
                          profit.total_top_score_ = total_top_score;
                      };
            db << "SELECT match.game_name, match.user_count, user_with_match.game_score, user_with_match.zero_sum_score, "
                        "user_with_match.top_score "
                  "FROM user_with_match, match "
                  "WHERE user_with_match.user_id = ? AND user_with_match.match_id = match.match_id "
                  "ORDER BY match.match_id DESC LIMIT 10"
               << uid.Get()
               >> [&](const std::string& game_name, const uint64_t user_count, const int64_t game_score,
                          const int64_t zero_sum_score, const int64_t top_score)
                      {
                          profit.recent_matches_.emplace_back();
                          profit.recent_matches_.back().game_name_ = game_name;
                          profit.recent_matches_.back().user_count_ = user_count;
                          profit.recent_matches_.back().game_score_ = game_score;
                          profit.recent_matches_.back().zero_sum_score_ = zero_sum_score;
                          profit.recent_matches_.back().top_score_ = top_score;
                      };
            return true;
        });
    return profit;
}

std::unique_ptr<DBManagerBase> SQLiteDBManager::UseDB(const std::string& db_name)
{
    try {
        sqlite::database db(db_name);
        db << "CREATE TABLE IF NOT EXISTS match("
                "match_id INTEGER PRIMARY KEY AUTOINCREMENT, "
                "game_name VARCHAR(100) NOT NULL, "
                "finish_time DATETIME DEFAULT CURRENT_TIMESTAMP, "
                "group_id BIGINT UNSIGNED, "
                "host_user_id BIGINT UNSIGNED NOT NULL, "
                "user_count BIGINT UNSIGNED NOT NULL, "
                "multiple INT UNSIGNED NOT NULL);";
        db << "CREATE TABLE IF NOT EXISTS user_with_match("
                "user_id BIGINT UNSIGNED NOT NULL, "
                "match_id BIGINT UNSIGNED NOT NULL, "
                "game_score BIGINT NOT NULL, "
                "zero_sum_score BIGINT NOT NULL, "
                "top_score BIGINT NOT NULL, "
                "PRIMARY KEY (user_id, match_id));";
        db << "CREATE INDEX IF NOT EXISTS user_id_index ON user_with_match(user_id);";
        db << "CREATE TABLE IF NOT EXISTS user("
                "user_id BIGINT UNSIGNED PRIMARY KEY, "
                "register_time DATETIME DEFAULT CURRENT_TIMESTAMP, "
                "passwd VARCHAR(100));";
        db << "CREATE TABLE IF NOT EXISTS achievement("
                "achi_id BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY, "
                "achi_name VARCHAR(100) NOT NULL, "
                "description VARCHAR(1000));";
        db << "CREATE TABLE IF NOT EXISTS user_with_achievement("
                "user_id BIGINT UNSIGNED NOT NULL, "
                "achi_id BIGINT UNSIGNED NOT NULL);";
        return std::unique_ptr<DBManagerBase>(new SQLiteDBManager(db_name));
    } catch (const sqlite::sqlite_exception& e) {
        HandleError(e);
    } catch (const std::exception& e) {
        HandleError(e);
    }
    return nullptr;
}

#endif
