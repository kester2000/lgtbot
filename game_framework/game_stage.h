#pragma once

#include <cassert>
#include <chrono>
#include <optional>
#include <variant>
#include <concepts>

#include "bot_core/match_base.h"
#include "utility/msg_checker.h"

#include "game_framework/util.h"

using AtomReqErrCode = StageErrCode::SubSet<StageErrCode::OK,        // act successfully
                                              StageErrCode::FAILED,    // act failed
                                              StageErrCode::READY,     // act successfully and ready to checkout
                                              StageErrCode::CHECKOUT>; // to checkout

using CompReqErrCode = StageErrCode::SubSet<StageErrCode::OK,
                                              StageErrCode::FAILED>;

using TimeoutErrCode = StageErrCode::SubSet<StageErrCode::FAILED,
                                             StageErrCode::CHECKOUT>;

using LeaveErrCode = StageErrCode::SubSet<StageErrCode::OK,
                                             StageErrCode::CHECKOUT>;

class Masker
{
  private:
    enum class State { SET, UNSET, PINNED };

  public:
    Masker(const size_t size) : recorder_(size, State::UNSET), unset_count_(size) {}

    bool Set(const size_t index) { return Record_(index, State::SET); }

    void Unset(const size_t index) { Record_(index, State::UNSET); }

    bool Pin(const size_t index) { return Record_(index, State::PINNED); }

    void Clear()
    {
        for (auto& state : recorder_) {
            if (state == State::SET) {
                state = State::UNSET;
                ++unset_count_;
            }
        }
    }

    bool IsReady() const { return unset_count_ == 0; }

  private:
    bool Record_(const size_t index, const State state)
    {
        const auto old = recorder_[index];
        if (old != State::PINNED) {
            recorder_[index] = state;
            unset_count_ += (state == State::UNSET);
            unset_count_ -= (old == State::UNSET);
        }
        return IsReady();
    }

    std::vector<State> recorder_;
    size_t unset_count_;
};

template <typename RetType>
using GameCommand = Command<RetType(const uint64_t, const bool, MsgSenderBase&)>;

enum class CheckoutReason { BY_REQUEST, BY_TIMEOUT, BY_LEAVE, SKIP };

template <typename SubStage, typename RetType>
class SubStageCheckoutHelper
{
  public:
    virtual RetType NextSubStage(SubStage& sub_stage, const CheckoutReason reason) = 0;
};

template <bool IS_ATOM>
class StageBaseWrapper : virtual public StageBase
{
  public:
    template <typename String, typename ...Commands>
    StageBaseWrapper(const GameOptionBase& option, MatchBase& match, String&& name, Commands&& ...commands)
        : option_(option)
        , match_(match)
        , name_(std::forward<String>(name))
        , commands_{std::forward<Commands>(commands)...}
    {}

    virtual ~StageBaseWrapper() {}

    virtual StageErrCode HandleRequest(const char* const msg, const uint64_t player_id, const bool is_public,
                                       MsgSenderBase& reply) override final
    {
        MsgReader reader(msg);
        return HandleRequest(reader, player_id, is_public, reply);
    }
    virtual const char* StageInfoC() const override final
    {
        thread_local static std::string info_;
        info_ = StageInfo();
        return info_.c_str();
    }
    virtual const char* CommandInfoC(const bool text_mode) const override final
    {
        thread_local static std::string info_;
        info_ = CommandInfo(text_mode);
        return info_.c_str();
    }

    decltype(auto) BoardcastMsgSender() const { return match_.BoardcastMsgSender(); }

    decltype(auto) TellMsgSender(const PlayerID pid) const { return match_.TellMsgSender(pid); }

    decltype(auto) Boardcast() const { return BoardcastMsgSender()(); }

    decltype(auto) Tell(const PlayerID pid) const { return TellMsgSender(pid)(); }

    const std::string& name() const { return name_; }

    MatchBase& match() { return match_; }

    virtual void HandleStageBegin() override = 0;
    virtual StageErrCode HandleTimeout() = 0;
    virtual StageErrCode HandleRequest(MsgReader& reader, const uint64_t player_id, const bool is_public,
                                       MsgSenderBase& reply) = 0;
    virtual StageErrCode HandleLeave(const PlayerID pid) = 0;
    virtual StageErrCode HandleComputerAct(const uint64_t pid) = 0;
    virtual std::string StageInfo() const = 0;
    virtual std::string CommandInfo(const bool text_mode) const
    {
        if (commands_.empty()) {
            return "";
        }
        std::string outstr = "\n\n### 游戏命令-" + name();
        uint64_t i = 1;
        for (const auto& cmd : commands_) {
             outstr += "\n" + std::to_string(i++) + ". " + cmd.Info(true /* with_example */, !text_mode /* with_html_color */);
        }
        return outstr;
    }

  protected:
    template <typename Stage, typename RetType, typename... Args, typename... Checkers>
    GameCommand<RetType> MakeStageCommand(const char* const description, RetType (Stage::*cb)(Args...),
            Checkers&&... checkers)
    {
        return GameCommand<RetType>(description, std::bind_front(cb, static_cast<Stage*>(this)),
                std::forward<Checkers>(checkers)...);
    }

    const std::string name_;
    const GameOptionBase& option_;
    MatchBase& match_;

    std::vector<GameCommand<std::conditional_t<IS_ATOM, AtomReqErrCode, CompReqErrCode>>> commands_;
};

template <bool IS_ATOM, typename MainStage>
class SubStageBaseWrapper : public StageBaseWrapper<IS_ATOM>
{
  public:
    template <typename ...Commands>
    SubStageBaseWrapper(MainStage& main_stage, Commands&& ...commands)
        : StageBaseWrapper<IS_ATOM>(main_stage.option(), main_stage.match(), "（匿名子阶段）", std::forward<Commands>(commands)...)
        , main_stage_(main_stage)
    {}

    template <typename String, typename ...Commands>
    SubStageBaseWrapper(MainStage& main_stage, String&& name, Commands&& ...commands)
        : StageBaseWrapper<IS_ATOM>(main_stage.option(), main_stage.match(), std::forward<String>(name), std::forward<Commands>(commands)...)
        , main_stage_(main_stage)
    {}

    MainStage& main_stage() { return main_stage_; }
    const MainStage& main_stage() const { return main_stage_; }

    Masker& masker() { return main_stage_.masker(); }
    const Masker& masker() const { return main_stage_.masker(); }

  private:
    MainStage& main_stage_;
};

template <bool IS_ATOM>
class MainStageBaseWrapper : public MainStageBase, public StageBaseWrapper<IS_ATOM>
{
  public:
    template <typename ...Commands>
    MainStageBaseWrapper(const GameOptionBase& option, MatchBase& match, Commands&& ...commands)
        : StageBaseWrapper<IS_ATOM>(option, match, "（匿名主阶段）", std::forward<Commands>(commands)...)
        , masker_(option.PlayerNum())
    {}

    template <typename String, typename ...Commands>
    MainStageBaseWrapper(const GameOptionBase& option, MatchBase& match, String&& name, Commands&& ...commands)
        : StageBaseWrapper<IS_ATOM>(option, match, std::forward<String>(name), std::forward<Commands>(commands)...)
        , masker_(option.PlayerNum())
    {}

    virtual int64_t PlayerScore(const PlayerID pid) const = 0;

    Masker& masker() { return masker_; }
    const Masker& masker() const { return masker_; }

  private:
    Masker masker_;
};

template <typename GameOption, typename MainStage, typename... SubStages>
class GameStage;

template <typename GameOption, typename MainStage, typename... SubStages> requires (sizeof...(SubStages) > 0)
class GameStage<GameOption, MainStage, SubStages...>
    : public std::conditional_t<std::is_void_v<MainStage>, MainStageBaseWrapper<false>, SubStageBaseWrapper<false, MainStage>>
    , public SubStageCheckoutHelper<SubStages, std::variant<std::unique_ptr<SubStages>...>>...
{
  public:
    using Base = std::conditional_t<std::is_void_v<MainStage>, MainStageBaseWrapper<false>, SubStageBaseWrapper<false, MainStage>>;
    using VariantSubStage = std::variant<std::unique_ptr<SubStages>...>;
    using SubStageCheckoutHelper<SubStages, VariantSubStage>::NextSubStage...;

    template <typename ...Args>
    GameStage(Args&& ...args) : Base(std::forward<Args>(args)...) {}

    virtual ~GameStage() {}

    virtual VariantSubStage OnStageBegin() = 0;

    virtual void HandleStageBegin()
    {
        sub_stage_ = OnStageBegin();
        std::visit(
                [this](auto&& sub_stage)
                {
                    sub_stage->HandleStageBegin();
                    if (sub_stage->IsOver()) {
                        this->CheckoutSubStage(CheckoutReason::SKIP);
                    }
                }, sub_stage_);
    }

    virtual StageErrCode HandleRequest(MsgReader& reader, const uint64_t player_id, const bool is_public,
                                                  MsgSenderBase& reply) override
    {
        for (const auto& cmd : Base::commands_) {
            if (const auto rc = cmd.CallIfValid(reader, player_id, is_public, reply); rc.has_value()) {
                return *rc;
            }
        }
        return PassToSubStage_(
                [&](auto&& sub_stage) { return sub_stage->HandleRequest(reader, player_id, is_public, reply); },
                CheckoutReason::BY_REQUEST);
    }

    virtual StageErrCode HandleTimeout() override
    {
        return PassToSubStage_([](auto&& sub_stage) { return sub_stage->HandleTimeout(); }, CheckoutReason::BY_TIMEOUT);
    }

    virtual StageErrCode HandleLeave(const PlayerID pid) override
    {
        // We must call CompStage's OnPlayerLeave first so that it can deceide whether to finish game when substage is over.
        this->OnPlayerLeave(pid);
        return PassToSubStage_(
                [pid](auto&& sub_stage) { return sub_stage->HandleLeave(pid); },
                CheckoutReason::BY_LEAVE);
    }

    virtual StageErrCode HandleComputerAct(const uint64_t pid) override
    {
        // For run_game_xxx, the tell msg will be output, so do not use EmptyMsgSender here.
        const auto rc = OnComputerAct(pid, Base::TellMsgSender(pid));
        if (rc != StageErrCode::OK) {
            return rc;
        }
        return PassToSubStage_(
                [pid](auto&& sub_stage) { return sub_stage->HandleComputerAct(pid); },
                CheckoutReason::BY_REQUEST); // game logic not care abort computer
    }

    virtual std::string CommandInfo(const bool text_mode) const override
    {
        return std::visit([&](auto&& sub_stage) { return Base::CommandInfo(text_mode) + sub_stage->CommandInfo(text_mode); }, sub_stage_);
    }

    virtual std::string StageInfo() const override
    {
        return std::visit([this](auto&& sub_stage) { return Base::name_ + " - " + sub_stage->StageInfo(); }, sub_stage_);
    }

    void CheckoutSubStage(const CheckoutReason reason)
    {
        // ensure previous substage is released before next substage built
        sub_stage_ = std::visit(
                [this, reason](auto&& sub_stage)
                {
                    VariantSubStage new_sub_stage = NextSubStage(*sub_stage, reason);
                    sub_stage = nullptr;
                    return std::move(new_sub_stage);
                },
                sub_stage_);
        Base::masker().Clear();
        std::visit(
                [this](auto&& sub_stage)
                {
                    if (!sub_stage) {
                        Over();
                        // no more substages
                    } else {
                        sub_stage->HandleStageBegin();
                        if (sub_stage->IsOver()) {
                            this->CheckoutSubStage(CheckoutReason::SKIP);
                        }
                    }
                },
                sub_stage_);
    }

    const GameOption& option() const { return static_cast<const GameOption&>(Base::option_); }

  private:
    using StageBase::Over;

    // CompStage cannot checkout by itself so return type is void
    virtual void OnPlayerLeave(const PlayerID pid) {}
    virtual CompReqErrCode OnComputerAct(const PlayerID pid, MsgSenderBase& reply) { return StageErrCode::OK; }

    template <typename Task>
    StageErrCode PassToSubStage_(const Task& internal_task, const CheckoutReason checkout_reason)
    {
        const auto task = [this, &internal_task, checkout_reason](auto&& sub_stage) {
            // return rc to check in unittest
            const auto rc = internal_task(sub_stage);
            if (sub_stage->IsOver()) {
                this->CheckoutSubStage(checkout_reason);
            }
            return rc;
        };
        return std::visit(task, sub_stage_);
    }

    VariantSubStage sub_stage_;
};

template <typename GameOption, typename MainStage>
class GameStage<GameOption, MainStage>
    : public std::conditional_t<std::is_void_v<MainStage>, MainStageBaseWrapper<true>, SubStageBaseWrapper<true, MainStage>>
{
   public:
    using Base = std::conditional_t<std::is_void_v<MainStage>, MainStageBaseWrapper<true>, SubStageBaseWrapper<true, MainStage>>;

    template <typename ...Args>
    GameStage(Args&& ...args) : Base(std::forward<Args>(args)...) {}

    virtual ~GameStage() { Base::match_.StopTimer(); }
    virtual void OnStageBegin() {}
    virtual void HandleStageBegin()
    {
        OnStageBegin();
        Handle_(StageErrCode::OK);
    }

    virtual StageErrCode HandleTimeout() override final
    {
        return Handle_(OnTimeout());
    }

    virtual StageErrCode HandleLeave(const PlayerID pid) override final
    {
        Base::masker().Pin(pid);
        return  Handle_(pid, OnPlayerLeave(pid));
    }

    virtual StageErrCode HandleRequest(MsgReader& reader, const uint64_t pid, const bool is_public,
                                                  MsgSenderBase& reply) override final
    {
        for (const auto& cmd : Base::commands_) {
            if (const auto rc = cmd.CallIfValid(reader, pid, is_public, reply); rc.has_value()) {
                return Handle_(pid, *rc);
            }
        }
        return StageErrCode::NOT_FOUND;
    }

    virtual StageErrCode HandleComputerAct(const uint64_t pid) override final
    {
        // For run_game_xxx, the tell msg will be output, so do not use EmptyMsgSender here.
        return Handle_(pid, OnComputerAct(pid, Base::TellMsgSender(pid)));
    }

    virtual std::string StageInfo() const override
    {
        std::string outstr = Base::name_;
        if (finish_time_.has_value()) {
            outstr += "（剩余时间：";
            outstr += std::to_string(
                    std::chrono::duration_cast<std::chrono::seconds>(*finish_time_ - std::chrono::steady_clock::now()).count());
            outstr += "秒）";
        }
        return outstr;
    }

    const GameOption& option() const { return static_cast<const GameOption&>(Base::option_); }

  protected:
    virtual LeaveErrCode OnPlayerLeave(const PlayerID pid) { return StageErrCode::READY; }
    virtual TimeoutErrCode OnTimeout() { return StageErrCode::CHECKOUT; }
    virtual AtomReqErrCode OnComputerAct(const PlayerID pid, MsgSenderBase& reply) { return StageErrCode::READY; }
    // User can use ClearReady to unset masker. In this case, stage will not checkout.
    virtual void OnAllPlayerReady() {}

    void StartTimer(const uint64_t sec)
    {
        finish_time_ = std::chrono::steady_clock::now() + std::chrono::seconds(sec);
        Base::match_.StartTimer(sec);
    }

    void StopTimer()
    {
        StopTimer();
        finish_time_ = nullptr;
    }

    void ClearReady() { Base::masker().Clear(); }
    void ClearReady(const PlayerID pid) { Base::masker().Unset(pid); }
    void SetReady(const PlayerID pid) { Base::masker().Set(pid); }

   private:
    StageErrCode Handle_(StageErrCode rc)
    {
        if (Base::masker().IsReady()) {
            // We do not check IsReady only when rc is READY to handle all player force exit.
            OnAllPlayerReady();
            if (Base::masker().IsReady()) {
                rc = StageErrCode::CHECKOUT;
            }
        }
        if (rc == StageErrCode::CHECKOUT) {
            Over();
        }
        return rc;
    }

    StageErrCode Handle_(const PlayerID pid, StageErrCode rc)
    {
        if (rc == StageErrCode::READY) {
            Base::masker().Set(pid);
            rc = StageErrCode::OK;
        }
        return Handle_(rc);
    }

    virtual void Over() override final
    {
        StageBase::Over();
    }
    // if command return true, the stage will be over
    std::optional<std::chrono::time_point<std::chrono::steady_clock>> finish_time_;
};

class GameOption;
class MainStage;

template <typename... SubStages>
using SubGameStage = GameStage<GameOption, MainStage, SubStages...>;

template <typename... SubStages>
using MainGameStage = GameStage<GameOption, void, SubStages...>;

