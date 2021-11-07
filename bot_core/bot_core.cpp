#include "bot_core.h"

#include <fstream>
#include <filesystem>

#include "utility/msg_checker.h"
#include "game_framework/game_main.h"

#include "bot_core/db_manager.h"
#include "bot_core/log.h"
#include "bot_core/match.h"
#include "bot_core/message_handlers.h"
#include "bot_core/msg_sender.h"

#include "sqlite_modern_cpp.h"

extern void LoadGameModules();

const int32_t LGT_AC = -1;

BotCtx::BotCtx(const BotOption& option, std::unique_ptr<DBManagerBase> db_manager)
    : this_uid_(option.this_uid_)
    , game_path_(std::filesystem::absolute(option.game_path_).string())
    , match_manager_(*this)
    , db_manager_(std::move(db_manager))
{
    LoadGameModules_(option.game_path_);
    LoadAdmins_(option.admins_);
}

void BotCtx::LoadAdmins_(const uint64_t* admins)
{
    if (admins == nullptr) {
        return;
    }
    for (; *admins != 0; ++admins) {
        InfoLog() << "New administor: " << *admins;
        admins_.emplace(*admins);
    }
}

static ErrCode HandleRequest(BotCtx& bot, const std::optional<GroupID> gid, const UserID uid, const std::string& msg,
                             MsgSender& reply)
{
    if (std::string first_arg; !(std::stringstream(msg) >> first_arg) || first_arg.empty()) {
        reply() << "[错误] 我不理解，所以你是想表达什么？";
        return EC_REQUEST_EMPTY;
    } else {
        switch (first_arg[0]) {
        case '#':
            return HandleMetaRequest(bot, uid, gid, msg, reply);
        case '%':
            if (!bot.HasAdmin(uid)) {
                reply() << "[错误] 您未持有管理员权限";
                return EC_REQUEST_NOT_ADMIN;
            }
            return HandleAdminRequest(bot, uid, gid, msg, reply);
        default:
            std::shared_ptr<Match> match = bot.match_manager().GetMatch(uid);
            if (!match) {
                reply() << "[错误] 您未参与游戏\n"
                           "若您想执行元指令，请尝试在请求前加\"#\"，或通过\"#帮助\"查看所有支持的元指令";
                return EC_MATCH_USER_NOT_IN_MATCH;
            }
            if (match->gid() != gid && gid.has_value()) {
                reply() << "[错误] 您未在本群参与游戏\n";
                "若您想执行元指令，请尝试在请求前加\"#\"，或通过\"#帮助\"查看所有支持的元指令";
                return EC_MATCH_NOT_THIS_GROUP;
            }
            return match->Request(uid, gid, msg, reply);
        }
    }
}



void* /*__cdecl*/ BOT_API::Init(const BotOption* option)
{
    std::srand(std::chrono::steady_clock::now().time_since_epoch().count());
    if (option == nullptr) {
        return nullptr;
    }
    return new BotCtx(*option, SQLiteDBManager::UseDB(option->db_path_));
}

void /*__cdelcl*/ BOT_API::Release(void* const bot) { delete static_cast<BotCtx*>(bot); }

ErrCode /*__cdecl*/ BOT_API::HandlePrivateRequest(void* const bot_p, const uint64_t uid, const char* const msg)
{
    if (!bot_p) {
        return EC_NOT_INIT;
    }
    BotCtx& bot = *static_cast<BotCtx*>(bot_p);
    MsgSender sender(UserID{uid});
    return HandleRequest(bot, std::nullopt, uid, msg, sender);
}

class PublicReplyMsgSender : public MsgSender
{
  public:
    PublicReplyMsgSender(const GroupID& gid, const UserID& uid) : MsgSender(gid), uid_(uid) {}
    MsgSenderGuard operator()() override
    {
        MsgSenderGuard guard(*this);
        guard << At(uid_) << "\n";
        return guard;
    }
  private:
    const UserID uid_;
};

ErrCode /*__cdecl*/ BOT_API::HandlePublicRequest(void* const bot_p, const uint64_t gid, const uint64_t uid,
                                                 const char* const msg)
{
    if (!bot_p) {
        return EC_NOT_INIT;
    }
    BotCtx& bot = *static_cast<BotCtx*>(bot_p);
    PublicReplyMsgSender sender(GroupID{gid}, UserID{uid});
    return HandleRequest(bot, gid, uid, msg, sender);
}

