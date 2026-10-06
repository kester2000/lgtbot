#include "game_framework/stage.h"
#include "game_framework/util.h"
#include "match.h"
#include "view.h"

namespace lgtbot::game::GAME_MODULE_NAME {
namespace ff = ::factory_funner;
const GameProperties k_properties {
    .name_ = "工厂欢乐多",
    .developer_ = "LGTBot / FactoryFunner 移植",
    .description_ = "抢选机器、连接六角管道，经营自己的工厂（Factory Funner，2–6人）",
    .shuffled_player_id_ = true,
};
uint64_t MaxPlayerNum(const CustomOptions&) { return 6; }
uint32_t Multiple(const CustomOptions& options) { return GET_OPTION_VALUE(options, 种子).empty() ? 2 : 0; }
const MutableGenericOptions k_default_generic_options;
const std::vector<RuleCommand> k_rule_commands = {};
const std::vector<InitOptionsCommand> k_init_options_commands = {};
bool AdaptOptions(MsgSenderBase& reply, CustomOptions&, const GenericOptions& options, MutableGenericOptions&) {
    if (options.PlayerNum() < 2 || options.PlayerNum() > 6) { reply() << "工厂欢乐多需要 2 至 6 人"; return false; }
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
    Markdown OverviewPicture() const {
        std::vector<std::pair<std::string,ff::Factory>> factories;
        std::vector<ff::BoardEstimate> estimates;
        for (size_t i = 0; i < match.players.size(); ++i) {
            const auto& p = match.players[i];
            std::string name = Caption(i);
            factories.emplace_back(std::move(name),p.draft.factory);
            estimates.push_back(match.Estimate(i));
        }
        return Markdown(ff::FactoriesHtml(factories,Global().ResourceDir(),estimates),1280);
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
            Global().Boardcast() << "10 秒后自动翻牌，进入抢选阶段。发送 操作 查看指令。";
            Global().Boardcast() << Main().OverviewPicture();
        } else if (phase_ == ff::Phase::Select) {
            Global().StartTimer(GAME_OPTION(选机时限));
            Global().Boardcast() << Market() << "\n发送 抢 序号 或 不抢；按机器人收到指令的先后判定。超时随机分配。";
            Global().Boardcast() << Markdown(MarketHtml(),1000);
        } else if (phase_ == ff::Phase::Build) {
            Global().StartTimer(GAME_OPTION(施工时限));
            Global().Boardcast() << "选机完成，开始施工。可私聊发送施工指令；发送 工厂 查看图，文字 查看端口，检查 校验，提交 或 跳过 锁定。";
            Global().Boardcast() << Main().OverviewPicture();
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
        s << "<h3>第 " << Main().match.round << " 轮 · 牌池（发送 抢 序号）</h3><table cellpadding=\"12\"><tr>";
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
    AtomReqErrCode Command(PlayerID pid, bool, MsgSenderBase& reply, const std::vector<std::string>& a) {
        try {
            ff::Require(!a.empty(), "发送 操作 查看指令");
            auto& m = Main().match; auto& p = m.players[pid];
            const auto& cmd = a[0];
            auto count = [&](size_t n) { ff::Require(a.size() == n, "参数数量不正确，请发送 操作"); };
            if (cmd == "操作") { count(1); reply() << ff::kHelp; }
            else if (cmd == "赛况") { count(1); reply() << Main().Scores(); }
            else if (cmd == "牌池") { count(1); reply() << Market(); if (phase_ != ff::Phase::Ready) reply() << Markdown(MarketHtml(),1000); }
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
                    Global().Boardcast() << Global().PlayerName(pid) << "选中 M" << p.draft.factory.current;
                    for(size_t i=0;i<m.players.size();++i) if(i!=pid && !previously_selected[i] && m.players[i].draft.factory.current)
                        Global().Boardcast() << Global().PlayerName(i) << "自动领取最后一台机器 M" << m.players[i].draft.factory.current;
                } else if (cmd == "不抢") { count(1); m.Pass(pid); reply() << "已选择不主动抢选"; }
                else if (cmd == "提交" || cmd == "保留" || cmd == "跳过") {
                    count(1); ff::Require(cmd != "保留" || phase_ == ff::Phase::Decision, "保留仅用于超时确认");
                    m.Submit(pid,cmd == "跳过"); reply() << "已锁定，等待其他玩家完成后统一结算";
                } else {
                    ff::Require(phase_ == ff::Phase::Build && !p.submitted, "仅施工阶段且尚未提交时可以修改");
                    const auto before = p.draft.factory.pieces.size();
                    p.draft.Apply(a);
                    if (cmd == "自动") {
                        reply() << "自动补入 " << p.draft.factory.pieces.size() - before << " 个罐，待付 $" << p.draft.factory.Cost()
                            << "。可 撤销。\n" << p.draft.factory.Validate().Text();
                    } else {
                        reply() << "施工稿已更新，待付 $" << p.draft.factory.Cost() << "。发送 工厂 / 文字 / 检查 查看；可 撤销。";
                    }
                    reply() << FactoryPicture(pid);
                }
                SyncReady();
            }
            return StageErrCode::OK;
        } catch (const std::exception& e) { reply() << "[错误] " << e.what(); return StageErrCode::FAILED; }
    }
};

void MainStage::FirstStageFsm(SubStageFsmSetter setter) { setter.Emplace<RoundStage>(*this,ff::Phase::Ready); }
void MainStage::NextStageFsm(RoundStage&, CheckoutReason, SubStageFsmSetter setter) {
    if (match.All([](const auto&) { return false; })) { match.Commit(); match.phase = ff::Phase::Over; Global().Boardcast() << "所有玩家均已退出，游戏结束。\n" << Scores(); return; }
    if (match.phase == ff::Phase::Ready) setter.Emplace<RoundStage>(*this,ff::Phase::Select);
    else if (match.phase == ff::Phase::Select) setter.Emplace<RoundStage>(*this,ff::Phase::Build);
    else if (match.phase == ff::Phase::Build && !match.All([](auto& p) { return p.submitted; })) setter.Emplace<RoundStage>(*this,ff::Phase::Decision);
    else {
        match.Commit(); Global().Boardcast() << "本轮统一结算：\n" << Scores();
        for (size_t i = 0; i < match.players.size(); ++i) {
            const auto picture = Markdown(ff::FactoryHtml(match.players[i].draft.factory,Global().ResourceDir(),Caption(i),false,match.Estimate(i)),1000);
            Global().Boardcast() << picture;
        }
        if (match.round == 8) { match.phase = ff::Phase::Over; Global().Boardcast() << "八轮结束，按资金加连锁奖励排名，同分并列。"; }
        else { ++match.round; match.OpenRound(); setter.Emplace<RoundStage>(*this,ff::Phase::Ready); }
    }
}
auto* MakeMainStage(MainStageFactory factory) { return factory.Create<MainStage>(); }
}
