#pragma once
#include "engine.h"
#include "utility/random.h"
#include <optional>

namespace factory_funner {
enum class Phase { Ready, Select, Build, Decision, Over };
struct Selection {
    bool first = false, last = false, free_discard = false;
    int Adjustment(bool skip) const { return (last && !skip ? 1 : 0) - (first ? 1 : 0) - (skip && !free_discard ? 2 : 0); }
};
struct Player {
    Draft draft;
    std::vector<int> deck;
    bool left = false, ready = false, passed = false, submitted = false, decision = false;
    Selection selection;
    std::optional<Factory> pending;
    int round_start_cash = 10;
    int locked_revenue=0, locked_cost=0, locked_adjustment=0;
    bool committed=false;
};
struct BoardEstimate {
    int cash=0, income=0, total=0;
    bool unfinished=false;
    int current_chain=0, machine_revenue=0, parts_cost=0, new_chain=0, adjustment=0;
    std::string status;
};
struct Card { int machine, owner = -1; };
class Match {
  public:
    std::vector<Player> players;
    std::vector<Card> market;
    Phase phase = Phase::Ready;
    int round = 1, claim_count = 0, round_players = 0;
    std::mt19937 rng;

    Match(int count, const std::string& seed) : players(count), rng(MakeRng(seed)) {
        Require(count >= 1 && count <= 6, "需要 1 至 6 人");
        std::vector<int> deck(48); std::iota(deck.begin(), deck.end(), 1); SeededShuffle(deck.begin(), deck.end(), rng);
        for (int i = 0; i < count; ++i) { players[i].deck.assign(deck.begin() + 8*i, deck.begin() + 8*i + 8); players[i].draft.factory.skin = 0; }
        OpenRound();
    }
    // The mode depends on the initial player count, not on how many remain.
    bool Solo() const { return players.size() == 1; }
    void OpenRound() {
        market.clear(); claim_count = 0; round_players = 0; phase = Phase::Ready;
        for (auto& p : players) if (!p.left) {
            ++round_players; market.push_back({p.deck[round - 1]});
            p.ready = p.passed = p.submitted = p.decision = false; p.selection = {}; p.pending.reset();
            p.round_start_cash = p.draft.factory.money;
            p.committed=false; p.locked_revenue=p.locked_cost=p.locked_adjustment=0;
            p.draft.factory.current = 0; p.draft.factory.round = round; p.draft.undo.clear(); p.draft.redo.clear();
        }
        if (!round_players) phase = Phase::Over;
        else if (Solo()) {
            Assign(0,0,true);
            phase = Phase::Build;
        }
    }
    bool All(const std::function<bool(const Player&)>& pred) const {
        return std::ranges::all_of(players, [&](auto& p) { return p.left || pred(p); });
    }
    int RemainingCards() const { return std::ranges::count_if(market, [](auto& c) { return c.owner < 0; }); }
    void Assign(int pid, int index, bool automatic) {
        auto& p = players.at(pid); auto& c = market.at(index);
        Require(!p.left && !p.draft.factory.current && c.owner < 0, "该机器已被抢走，或您已经选过机器");
        bool last = RemainingCards() == 1;
        p.selection = {!automatic && round > 1 && round_players > 2 && claim_count == 0,
            !automatic && round > 1 && last, automatic || last};
        c.owner = pid; p.draft.factory.current = c.machine;
        if (!automatic) ++claim_count;
    }
    void Claim(int pid, int index) {
        Require(phase == Phase::Select, "现在不是选机阶段");
        Require(!players.at(pid).passed, "本轮已选择不抢，不能反悔");
        Require(index >= 1 && index <= int(market.size()), "请输入牌池中的序号"); Assign(pid,index - 1,false); Resolve(false);
    }
    void Pass(int pid) {
        auto& p = players.at(pid);
        Require(phase == Phase::Select && !p.draft.factory.current && !p.left, "已经选机或不在选机阶段");
        p.passed = true; Resolve(false);
    }
    void Resolve(bool expired) {
        std::vector<int> waiting, cards;
        for (int i = 0; i < int(players.size()); ++i) if (!players[i].left && !players[i].draft.factory.current) waiting.push_back(i);
        if (waiting.empty()) return;
        for (int i = 0; i < int(market.size()); ++i) if (market[i].owner < 0) cards.push_back(i);
        if (!expired && waiting.size()==1 && cards.size()==1) { Assign(waiting[0],cards[0],false); return; }
        if (!expired && !std::ranges::all_of(waiting, [&](int i) { return players[i].passed; })) return;
        SeededShuffle(cards.begin(), cards.end(), rng);
        const bool automatic = expired || cards.size() > 1;
        for (size_t i = 0; i < waiting.size(); ++i) Assign(waiting[i], cards.at(i), automatic);
    }
    void Submit(int pid, bool skip) {
        auto& p = players.at(pid);
        Require(!p.left && !p.submitted, "您已提交或退出");
        Require(phase == Phase::Build || (phase == Phase::Decision && p.decision), "现在不能提交施工");
        Factory next = p.draft.factory;
        p.locked_revenue=skip?0:MachineById(next.current).revenue;
        p.locked_cost=skip?0:next.Cost();
        p.locked_adjustment=p.selection.Adjustment(skip);
        next.Settle(skip, p.selection.Adjustment(skip)); p.pending = std::move(next); p.submitted = true; p.decision = false;
    }
    void BuildTimeout() {
        for (int i = 0; i < int(players.size()); ++i) {
            auto& p = players[i]; if (p.left || p.submitted) continue;
            if (p.draft.factory.HasCurrent() && p.draft.factory.Validate().Valid()) p.decision = true;
            else Submit(i,true);
        }
    }
    void DecisionTimeout() {
        for (int i = 0; i < int(players.size()); ++i) if (!players[i].left && !players[i].submitted) Submit(i,true);
    }
    void Leave(int pid) {
        auto& p = players.at(pid); if (p.left) return;
        // An already confirmed submission remains committed. Otherwise abandon the
        // construction, charging the ordinary selection fee when a card was claimed.
        if (!p.pending && p.draft.factory.current) {
            auto next = p.draft.factory; next.Settle(true,p.selection.Adjustment(true)); p.pending = next;
        }
        if (p.pending) { p.draft.factory = *p.pending; p.committed=true; }
        p.left = true; p.draft.factory.pieces = p.draft.factory.baseline;
        if (phase == Phase::Select) Resolve(false);
    }
    void Commit() {
        for (auto& p : players) if (!p.left && p.pending) { p.draft.factory = *p.pending; p.committed=true; }
    }
    int Score(int pid) const {
        Factory confirmed = players.at(pid).draft.factory; confirmed.pieces = confirmed.baseline;
        return confirmed.money + confirmed.Validate().bonus;
    }
    BoardEstimate Estimate(int pid) const {
        const auto& p=players.at(pid); const auto& f=p.draft.factory;
        BoardEstimate result; result.cash=f.money;
        auto baseline=f; baseline.pieces=f.baseline;
        result.current_chain=baseline.Validate().bonus;
        if(p.pending) {
            result.income=p.committed?0:p.pending->money-p.round_start_cash;
            result.total=p.pending->money+p.pending->Validate().bonus;
            if(!p.committed) {
                result.machine_revenue=p.locked_revenue; result.parts_cost=p.locked_cost;
                result.adjustment=p.locked_adjustment;
                result.new_chain=p.pending->Validate().bonus-result.current_chain;
            }
            result.status=p.committed?"已结算":"已锁定，等待统一结算";
        } else {
            const bool selected=f.current && !p.left;
            result.machine_revenue=selected?MachineById(f.current).revenue:0;
            result.parts_cost=selected?f.Cost():0;
            result.adjustment=selected?p.selection.Adjustment(false):0;
            result.new_chain=selected?f.Validate().bonus-result.current_chain:0;
            result.income=result.machine_revenue-result.parts_cost+result.adjustment;
            result.total=f.money+result.current_chain+result.income+result.new_chain;
            result.unfinished=selected && (!f.HasCurrent() || !f.Validate().Valid());
            if(p.left) result.status="已退出";
            else if(!selected) result.status="等待本轮机器";
            else {
                if(!f.HasCurrent()) result.status="尚未安装本轮机器；";
                auto validation=f.Validate();
                if(validation.Valid()) result.status+=f.HasCurrent()?"连接完整，可提交":"";
                else for(const auto& issue:validation.issues) result.status+=issue+"；";
            }
        }
        return result;
    }
};
}
