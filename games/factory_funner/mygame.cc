#include "game_framework/stage.h"
#include "game_framework/util.h"
#include "match.h"
#include "view.h"

namespace lgtbot::game::GAME_MODULE_NAME {
namespace ff = ::factory_funner;
const GameProperties k_properties {
    .name_ = "工厂欢乐多",
    .developer_ = "宽容",
    .description_ = "抢选机器、连接六角管道，经营自己的工厂（Factory Funner，1–6人，支持单机）",
    .shuffled_player_id_ = true,
};
uint64_t MaxPlayerNum(const CustomOptions&) { return 6; }
uint32_t Multiple(const CustomOptions&) { return 0; }
const MutableGenericOptions k_default_generic_options;
const std::vector<RuleCommand> k_rule_commands = {};
const std::vector<InitOptionsCommand> k_init_options_commands = {
    InitOptionsCommand("独自经营工厂，八轮挑战，每轮施工限时一小时",
        [](CustomOptions&, MutableGenericOptions& options) {
            options.bench_computers_to_player_num_ = 1;
            return NewGameMode::SINGLE_USER;
        }, VoidChecker("单机")),
};
bool AdaptOptions(MsgSenderBase& reply, CustomOptions&, const GenericOptions& options, MutableGenericOptions&) {
    if (options.PlayerNum() < 1 || options.PlayerNum() > 6) { reply() << "工厂欢乐多需要 1 至 6 人"; return false; }
    return true;
}

class MainStage;
class RoundStage;
template <typename... Sub> using SubGameStage = StageFsm<MainStage, Sub...>;
class MainStage : public StageFsm<void, RoundStage> {
  public:
    MainStage(StageUtility&& utility) : StageFsm(std::move(utility)), match(Global().PlayerNum(), GAME_OPTION(种子)) {}
    ff::Match match;
    void FirstStageFsm(SubStageFsmSetter setter) override;
    void NextStageFsm(RoundStage&, CheckoutReason, SubStageFsmSetter setter) override;
    int64_t PlayerScore(PlayerID pid) const override { return match.Score(pid); }
    std::string Caption(PlayerID pid) const {
        const auto& p=match.players[pid]; std::string name=Global().PlayerName(pid);
        if(p.left) name+=" [已退出]";
        else { if(p.selection.first) name+=" · 首抢 -1 分"; if(p.selection.last) name+=" · 末位建成 +1 分"; }
        return name;
    }
    Markdown OverviewPicture(bool settled_only=false) const {
        std::vector<std::pair<std::string,ff::Factory>> factories;
        std::vector<ff::BoardEstimate> estimates;
        for (size_t i = 0; i < match.players.size(); ++i) {
            const auto& p = match.players[i];
            std::string name = Caption(i);
            auto factory=p.draft.factory; auto estimate=match.Estimate(i);
            if(settled_only) {
                factory.pieces=factory.baseline;
                estimate.current_chain=factory.Validate().bonus;
                estimate.income=estimate.machine_revenue=estimate.parts_cost=estimate.new_chain=estimate.adjustment=0;
                estimate.total=estimate.cash+estimate.current_chain;
                estimate.status=p.left?"已退出":p.submitted?"已提交":p.decision?"等待超时确认":"施工中，草稿不公开";
            }
            factories.emplace_back(std::move(name),std::move(factory));
            estimates.push_back(std::move(estimate));
        }
        return Markdown(ff::FactoriesHtml(factories,Global().ResourceDir(),estimates),match.Solo() ? 660 : 1280);
    }
    std::string Scores() const {
        std::ostringstream s;
        s << "第 " << match.round << " / 8 轮（总分=已结算资金+连锁奖励）\n";
        for (size_t i = 0; i < match.players.size(); ++i) {
            const auto& p = match.players[i];
            s << Global().PlayerName(i) << "：$" << p.draft.factory.money << "，总分 " << match.Score(i)
                << (p.left ? " [已退出]" : p.submitted ? " [已提交]" : "") << "\n";
        }
        return s.str();
    }
};

class RoundStage : public SubGameStage<> {
  public:
    RoundStage(MainStage& main, ff::Phase phase)
        : StageFsm(main, "第" + std::to_string(main.match.round) + "轮 · " + PhaseName(phase),
            MakeStageCommand(*this, ff::kHelp.c_str(), &RoundStage::Command,
                RepeatableChecker<AnyArg>("指令参数"))), phase_(phase) { main.match.phase = phase; }

    static std::string PhaseName(ff::Phase phase) {
        switch (phase) {
            case ff::Phase::Ready: return "准备";
            case ff::Phase::Select: return "抢选机器";
            case ff::Phase::Build: return "施工";
            case ff::Phase::Decision: return "超时确认";
            case ff::Phase::Over: return "结束";
        }
        return "";
    }
    void OnStageBegin() override {
        auto& m = Main().match;
        if (phase_ == ff::Phase::Ready) {
            Global().StartTimer(10);
            auto message=Global().Boardcast();
            message << "10 秒后自动翻牌，进入抢选阶段。发送 操作 查看指令。";
            if (m.round == 1) message << Main().OverviewPicture();
        } else if (phase_ == ff::Phase::Select) {
            Global().StartTimer(GAME_OPTION(选机时限));
            Global().Boardcast() << "【当前阶段】第" << m.round << "轮 · 抢选机器\n" << Market()
                << "\n抢选时间 " << GAME_OPTION(选机时限) << " 秒。发送 抢 序号 或 不抢；按机器人收到指令的先后判定。最后一位自动领取剩余机器，超时随机分配。\n"
                << Markdown(MarketHtml(),1000);
        } else if (phase_ == ff::Phase::Build) {
            auto message=Global().Boardcast();
            if (m.Solo()) {
                Global().StartTimer(3600);
                message << "【单机模式】第" << m.round << "/8 轮，已领取机器 M" << m.players[0].draft.factory.current
                    << "。每轮施工限时一小时；提交 后立即结算，跳过 免费弃建并进入下一轮。超时有效草稿冻结并有20秒确认保留，否则免费跳过。发送 操作 查看指令。";
            } else {
                Global().StartTimer(GAME_OPTION(施工时限));
                message << "选机完成，开始施工。可私聊发送施工指令；发送 工厂 查看图，文字 查看端口，检查 校验，提交 或 跳过 锁定。";
            }
            message << Main().OverviewPicture();
        } else {
            Global().StartTimer(20);
            Global().Boardcast() << "施工时间到，未提交的有效草稿已冻结。相关玩家有 20 秒发送 保留 或 跳过；不回应则跳过。无效草稿已自动跳过。";
            for (size_t i = 0; i < m.players.size(); ++i) if (m.players[i].decision) Global().Tell(i) << "您的草稿有效，已冻结。请在 20 秒内发送 保留 或 跳过。";
        }
        SyncReady();
    }
    CheckoutErrCode OnStageTimeout() override {
        auto& m = Main().match;
        if (phase_ == ff::Phase::Select) m.Resolve(true);
        if (phase_ == ff::Phase::Build) m.BuildTimeout();
        if (phase_ == ff::Phase::Decision) m.DecisionTimeout();
        return StageErrCode::CHECKOUT;
    }
    CheckoutErrCode OnStageOver() override {
        auto& m = Main().match;
        if (phase_ == ff::Phase::Select) m.Resolve(true);
        if (phase_ == ff::Phase::Build || phase_ == ff::Phase::Decision) {
            for (int i = 0; i < int(m.players.size()); ++i) if (!m.players[i].left && !m.players[i].submitted) m.Submit(i,true);
        }
        return StageErrCode::CHECKOUT;
    }
    CheckoutErrCode OnPlayerLeave(PlayerID pid) override {
        Main().match.Leave(pid); SyncReady();
        Global().Boardcast() << Global().PlayerName(pid) << "退出，其往轮工厂和已结算分数保留。";
        return StageErrCode::CONTINUE;
    }
    AtomReqErrCode OnComputerAct(PlayerID pid, MsgSenderBase& reply) override {
        // Bench players progress safely; they do not attempt factory construction.
        if (phase_ == ff::Phase::Ready) return StageErrCode::OK;
        std::vector<std::string> cmd = {phase_ == ff::Phase::Select ? "不抢" : "跳过"};
        return Command(pid,false,reply,cmd);
    }
  private:
    ff::Phase phase_;
    Markdown FactoryPicture(PlayerID pid) const {
        return Markdown(ff::FactoryHtml(Main().match.players[pid].draft.factory,Global().ResourceDir(),Main().Caption(pid),false,Main().match.Estimate(pid)),1000);
    }
    std::string MarketHtml() const {
        std::ostringstream s;
        s << "<h3>第 " << Main().match.round << " 轮 · "
          << (Main().match.Solo() ? "本轮机器" : "牌池（发送 抢 序号）") << "</h3><table cellpadding=\"12\"><tr>";
        size_t i = 0;
        for (const auto& c : Main().match.market) {
            if (i == 3) s << "</tr><tr>";
            s << "<td style=\"vertical-align:top;width:280px\"><b>【" << ++i << "】" << (c.owner < 0 ? "可抢" : ff::EscapeHtml(Global().PlayerName(c.owner)))
              << "</b><br><img width=\"180\" src=\"" << ff::AssetUri(Global().ResourceDir(),ff::MachineImageName(c.machine)) << "\"/><pre>" << ff::MachineText(c.machine) << "</pre></td>";
        }
        s << "</tr></table>";
        return s.str();
    }
    void SyncReady() {
        if (phase_ == ff::Phase::Ready) return;
        auto& m = Main().match;
        for (size_t i = 0; i < m.players.size(); ++i) {
            auto& p = m.players[i];
            bool done = p.left || (phase_ == ff::Phase::Ready ? p.ready : phase_ == ff::Phase::Select ? p.draft.factory.current != 0 : p.submitted);
            if (done) Global().SetReady(i);
        }
    }
    std::string Market() const {
        const auto& m = Main().match;
        if (phase_ == ff::Phase::Ready) return "准备阶段不公开暗牌，倒计时 10 秒后自动翻牌。";
        std::ostringstream s;
        for (size_t i = 0; i < m.market.size(); ++i) {
            const auto& c = m.market[i];
            s << "【" << i + 1 << "】" << ff::MachineText(c.machine) << "\n"
                << (c.owner < 0 ? "可抢" : "已选：" + Global().PlayerName(c.owner)) << "\n";
        }
        return s.str();
    }
    static bool SubmitCommand(const std::string& cmd) {
        return cmd == "提交" || cmd == "commit" || cmd == "cm";
    }
    AtomReqErrCode Command(PlayerID pid, bool, MsgSenderBase& reply, const std::vector<std::string>& a) {
        try {
            ff::Require(!a.empty(), "发送 操作 查看指令");
            auto& m = Main().match; auto& p = m.players[pid];
            const auto& cmd = a[0];
            auto count = [&](size_t n) { ff::Require(a.size() == n, "参数数量不正确，请发送 操作"); };
            if (cmd == "操作") { count(1); reply() << ff::kHelp; }
            else if (cmd == "赛况") {
                count(1); auto message=reply();
                message << "【当前阶段】第" << m.round << "轮 · " << PhaseName(phase_) << "\n" << Main().Scores();
                if(phase_==ff::Phase::Select) message << Markdown(MarketHtml(),1000);
                else message << Main().OverviewPicture(!m.Solo() && (phase_==ff::Phase::Build || phase_==ff::Phase::Decision));
            }
            else if (cmd == "牌池") { count(1); auto message=reply(); message << Market(); if (phase_ != ff::Phase::Ready) message << Markdown(MarketHtml(),1000); }
            else if (cmd == "工厂") { count(1); reply() << FactoryPicture(pid); }
            else if (cmd == "文字") { count(1); reply() << ff::FactoryText(p.draft.factory); }
            else if (cmd == "检查") { count(1); reply() << p.draft.factory.Validate().Text() << "\n待付 $" << p.draft.factory.Cost() << (p.draft.factory.HasCurrent() ? "" : "\n尚未安装本轮机器，不能提交"); }
            else if (cmd == "机器") {
                count(2); const auto id=ff::Number(a[1],1,48);
                reply() << Markdown("<img width=\"260\" src=\"" + ff::AssetUri(Global().ResourceDir(),ff::MachineImageName(id)) + "\"/><pre>" + ff::MachineText(id) + "</pre>",600);
            }
            else {
                ff::Require(!p.left, "您已退出本场游戏");
                if (cmd == "抢") {
                    std::vector<int> previously_selected;
                    for(const auto& player:m.players) previously_selected.push_back(player.draft.factory.current);
                    count(2); m.Claim(pid,ff::Number(a[1],1,6));
                    auto message=Global().Boardcast();
                    message << Global().PlayerName(pid) << "选中 M" << p.draft.factory.current;
                    for(size_t i=0;i<m.players.size();++i) if(i!=pid && !previously_selected[i] && m.players[i].draft.factory.current)
                        message << "\n" << Global().PlayerName(i) << "自动领取最后一台机器 M" << m.players[i].draft.factory.current;
                } else if (cmd == "不抢") { count(1); m.Pass(pid); reply() << "已选择不主动抢选"; }
                else if (SubmitCommand(cmd) || cmd == "保留" || cmd == "跳过") {
                    count(1); ff::Require(cmd != "保留" || phase_ == ff::Phase::Decision, "保留仅用于超时确认");
                    m.Submit(pid,cmd == "跳过"); reply() << (m.Solo() ? "已确认，立即结算本轮" : "已锁定，等待其他玩家完成后统一结算");
                } else {
                    ff::Require(phase_ == ff::Phase::Build && !p.submitted, "仅施工阶段且尚未提交时可以修改");
                    const auto before = p.draft.factory.pieces.size();
                    bool submit = false;
                    if (cmd == "撤销" || cmd == "重做") p.draft.Apply(a);
                    else {
                        auto commands = ff::ConstructionCommands(cmd == "方案" ? std::vector<std::string>(a.begin() + 1, a.end()) : a);
                        submit = cmd != "方案" && commands.back().size() == 1 && SubmitCommand(commands.back()[0]);
                        if (submit) commands.pop_back();
                        auto saved = p;
                        try {
                            p.draft.ApplyCommands(commands);
                            if (submit) m.Submit(pid,false);
                        } catch (...) { p = std::move(saved); throw; }
                    }
                    auto message=reply();
                    if (submit) {
                        message << (m.Solo() ? "施工并提交成功，立即结算本轮" : "施工并提交成功，已锁定，等待其他玩家完成后统一结算");
                    } else if ((cmd == "自动" || cmd == "auto") && a.size() == 1) {
                        message << "自动补入 " << p.draft.factory.pieces.size() - before << " 个罐，待付 $" << p.draft.factory.Cost()
                            << "。可 撤销。\n" << p.draft.factory.Validate().Text();
                    } else {
                        message << "施工稿已更新，待付 $" << p.draft.factory.Cost() << "。发送 工厂 / 文字 / 检查 查看；可 撤销。";
                    }
                    message << FactoryPicture(pid);
                }
                SyncReady();
            }
            return StageErrCode::OK;
        } catch (const std::exception& e) { reply() << "[错误] " << e.what(); return StageErrCode::FAILED; }
    }
};

void MainStage::FirstStageFsm(SubStageFsmSetter setter) { setter.Emplace<RoundStage>(*this,match.phase); }
void MainStage::NextStageFsm(RoundStage&, CheckoutReason, SubStageFsmSetter setter) {
    if (match.All([](const auto&) { return false; })) { match.Commit(); match.phase = ff::Phase::Over; Global().Boardcast() << "所有玩家均已退出，游戏结束。\n" << Scores(); return; }
    if (match.phase == ff::Phase::Ready) setter.Emplace<RoundStage>(*this,ff::Phase::Select);
    else if (match.phase == ff::Phase::Select) setter.Emplace<RoundStage>(*this,ff::Phase::Build);
    else if (match.phase == ff::Phase::Build && !match.All([](auto& p) { return p.submitted; })) setter.Emplace<RoundStage>(*this,ff::Phase::Decision);
    else {
        match.Commit(); Global().Boardcast() << "本轮统一结算：\n" << Scores() << OverviewPicture();
        if (match.round == 8) {
            match.phase = ff::Phase::Over;
            if (match.Solo()) Global().Boardcast() << "单机挑战完成！最终得分为资金加连锁奖励。\n" << Scores();
            else Global().Boardcast() << "八轮结束，按资金加连锁奖励排名，同分并列。";
        }
        else { ++match.round; match.OpenRound(); setter.Emplace<RoundStage>(*this,match.phase); }
    }
}
auto* MakeMainStage(MainStageFactory factory) { return factory.Create<MainStage>(); }
}
