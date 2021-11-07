#ifdef ENUM_BEGIN
#ifdef ENUM_MEMBER
#ifdef ENUM_END

ENUM_BEGIN(StageErrCode)
ENUM_MEMBER(StageErrCode, OK)
ENUM_MEMBER(StageErrCode, CHECKOUT)
ENUM_MEMBER(StageErrCode, READY)
ENUM_MEMBER(StageErrCode, FAILED)
ENUM_MEMBER(StageErrCode, CONTINUE)
ENUM_MEMBER(StageErrCode, NOT_FOUND)
ENUM_END(StageErrCode)

#endif
#endif
#endif

#ifndef GAME_MAIN_H_
#define GAME_MAIN_H_

#include <stdint.h>

#include "bot_core/msg_sender.h"

#ifdef _WIN32
#define DLLEXPORT(type) __declspec(dllexport) type __cdecl
#else
#define DLLEXPORT(type) type
#endif

#define ENUM_FILE "game_framework/game_main.h"
#include "utility/extend_enum.h"

class MsgSenderForGame;
class MatchBase;

class GameOptionBase
{
  public:
    GameOptionBase(const uint64_t size) : size_(size), player_num_(0), resource_dir_(nullptr) {}
    virtual ~GameOptionBase() {}
    void SetPlayerNum(const uint64_t player_num) { player_num_ = player_num; }
    const uint64_t PlayerNum() const { return player_num_; }
    const uint64_t Size() const { return size_; }
    virtual void SetResourceDir(const char* const resource_dir) = 0;
    virtual const char* ResourceDir() const = 0;
    virtual const char* Info(const uint64_t index) const = 0;
    virtual const char* ColoredInfo(const uint64_t index) const = 0;
    virtual const char* Status() const = 0;
    virtual bool SetOption(const char* const msg) = 0;

  private:
    const uint64_t size_;
    uint64_t player_num_;
    const char* resource_dir_;
};

class StageBase
{
  public:
    StageBase() : is_over_(false) {}
    virtual ~StageBase() {}
    virtual const char* StageInfoC() const = 0;
    virtual const char* CommandInfoC(const bool text_mode) const = 0;
    virtual void HandleStageBegin() = 0;
    virtual StageErrCode HandleTimeout() = 0;
    virtual StageErrCode HandleRequest(const char* const msg, const uint64_t player_id, const bool is_public,
                                       MsgSenderBase& reply) = 0;
    virtual StageErrCode HandleLeave(const PlayerID pid) = 0;
    virtual StageErrCode HandleComputerAct(const uint64_t pid) = 0;
    bool IsOver() const { return is_over_; }
    virtual void Over() { is_over_ = true; }
  private:
    bool is_over_;
};

class MainStageBase : virtual public StageBase
{
   public:
    virtual int64_t PlayerScore(const PlayerID pid) const = 0;
};

#endif
