#pragma once
#include <stdint.h>

#include "bot_core/msg_sender.h"

#ifdef _WIN32
#define DLLEXPORT(type) __declspec(dllexport) type __cdecl
#else
#define DLLEXPORT(type) type
#endif

class MsgSenderForGame;

class GameOptionBase
{
  public:
    GameOptionBase(const uint64_t size) : size_(size) {}
    void SetPlayerNum(const uint64_t player_num) { player_num_ = player_num; }
    const uint64_t PlayerNum() const { return player_num_; }
    const uint64_t Size() const { return size_; }
    virtual const char* Info(const uint64_t index) const = 0;
    virtual const char* Status() const = 0;
    virtual bool SetOption(const char* const msg) = 0;

  private:
    const uint64_t size_;
    uint64_t player_num_;
};

class StageBase
{
  public:
    StageBase() : is_over_(false) {}
    enum class StageErrCode { OK, CHECKOUT, FAILED, NOT_FOUND };
    virtual ~StageBase() {}
    virtual void Init(void* const match) = 0;
    virtual StageErrCode HandleTimeout() = 0;
    virtual uint64_t CommandInfo(uint64_t i, MsgSenderBase::MsgSenderGuard& sender) const = 0;
    virtual StageErrCode HandleRequest(const char* const msg, const uint64_t player_id, const bool is_public,
                                       MsgSenderBase& reply) = 0;
    virtual StageErrCode HandleLeave(const PlayerID pid) = 0;
    virtual StageErrCode HandleComputerAct(const uint64_t begin_pid, const uint64_t end_pid) = 0;
    virtual void StageInfo(MsgSenderBase::MsgSenderGuard& sender) const = 0;
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

