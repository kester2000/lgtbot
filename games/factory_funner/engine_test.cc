// Standalone regression/parity driver; no GTest or bot runtime needed.
#include "match.h"
#include "view.h"
#include <iostream>

using namespace factory_funner;
static int checks = 0;
static void Check(bool ok, const char* name) { ++checks; if (!ok) throw std::runtime_error(name); }
template<class F> void Throws(F action, const char* name) {
    bool threw = false; try { action(); } catch (const std::exception&) { threw = true; } Check(threw,name);
}
static Factory Simple() {
    Factory f; f.current = 41;
    Edit(f,Words("机 C4")); Edit(f,Words("绿 B3")); Edit(f,Words("黑 C5"));
    Edit(f,Words("旋转 C5 逆时针")); Edit(f,Words("旋转 C5 逆时针")); return f;
}
static int AddMachine(Factory& f, int id, Cell c, int rot) {
    f.current = id; Piece p; p.kind = Kind::Machine; p.cell = c; p.rot = rot;
    int result = f.Place(p); ++f.round; return result;
}
static void Add(Factory& f, Kind kind, Cell c, int value = 0) {
    Piece p; p.kind = kind; p.cell = c;
    if (kind == Kind::Supply) p.color = value;
    else if (kind == Kind::Pipe) p.edges = value;
    else p.rot = value;
    f.Place(p);
}
static void Tests() {
    {
        Draft draft; draft.factory.current = 41;
        draft.Apply(Words("机 C4")); draft.Apply(Words("自动"));
        Check(draft.factory.pieces.size() == 3,"only needed green and black tanks");
        Check(draft.factory.Cost() == 2,"only needed tanks charged");
        Check(draft.factory.Validate().Valid(),"automatic direct connections valid");
        Check(std::ranges::any_of(draft.factory.pieces,[](const auto& p) { return p.kind == Kind::Supply && p.color == 4 && p.cell == ParseCell("B3"); }),"matching supply near input");
        auto filled = draft.factory.pieces;
        draft.Apply(Words("自动")); Check(draft.factory.pieces == filled,"automatic idempotent");
        draft.Apply(Words("旋转 C4 逆时针")); Check(draft.factory.Get(1).rot == 5,"coordinate reverse rotation");
        draft.Apply(Words("旋转 1")); Check(draft.factory.Get(1).rot == 0,"ID rotation");
        draft.Apply(Words("拆除 C4")); Check(!draft.factory.HasCurrent(),"coordinate removal");
        draft.Apply(Words("撤销")); Check(draft.factory.HasCurrent(),"undo removal");
        Draft line;
        Draft overwrite; overwrite.factory.current=41;
        overwrite.Apply(Words("绿 C3")); overwrite.Apply(Words("白 C3"));
        Check(overwrite.factory.pieces.size()==1 && overwrite.factory.pieces[0].kind==Kind::White,"placement replaces conflicting tank");
        overwrite.Apply(Words("撤销")); Check(overwrite.factory.pieces[0].kind==Kind::Supply,"undo restores overwritten tank");
        overwrite.Apply(Words("绿 D3")); Check(overwrite.factory.pieces.size()==1 && overwrite.factory.pieces[0].cell==ParseCell("D3"),"repeated supply placement moves it");
        overwrite.Apply(Words("白 C3")); overwrite.Apply(Words("移动 D3 C3"));
        Check(overwrite.factory.pieces.size()==1 && overwrite.factory.pieces[0].kind==Kind::Supply,"move replaces destination occupant");
        overwrite.Apply(Words("机 C3")); overwrite.Apply(Words("机 B3"));
        Check(overwrite.factory.pieces.size()==1 && overwrite.factory.pieces[0].cell==ParseCell("B3"),"repeated machine placement moves it");
        overwrite.Apply(Words("旋转 B3 3")); Check(overwrite.factory.pieces[0].rot==3,"rotation count repeats single step");
        overwrite.Apply(Words("旋转 B3 逆时针 2")); Check(overwrite.factory.pieces[0].rot==1,"inverse rotation count");
        for(const std::string name:{"画线","连线","画管"}) {
            Draft alias; alias.Apply(Words("黑 B3")); alias.Apply(Words(name+" A3B3C3"));
            Check(alias.factory.pieces.size()==1 && alias.factory.pieces[0].kind==Kind::Pipe,"path aliases overwrite interior only");
        }
        Draft fixed; fixed.factory=Simple(); fixed.factory.baseline=fixed.factory.pieces; ++fixed.factory.round;
        auto preserved=fixed.factory.pieces;
        Throws([&] {fixed.Apply(Words("白 C4"));},"fixed machine cannot be overwritten");
        Check(fixed.factory.pieces==preserved,"failed overwrite atomic");
        line.Apply(Words("画线 A3→B3→C3")); Check(line.factory.pieces.size() == 1,"continuous arrow path leaves both endpoints empty");
        line.Apply(Words("撤销")); Check(line.factory.pieces.empty(),"path undo atomic");
        line.Apply(Words("连续画线 A3B3C3"));
        auto before = line.factory.pieces;
        Throws([&] { line.Apply(Words("画线 C3E3")); },"nonadjacent path rejected");
        Check(line.factory.pieces == before,"failed path unchanged");
        line.Apply(Words("旋转 B3 逆时针")); Check(line.factory.Get(1).edges == ((1 << 5) | (1 << 2)),"pipe coordinate rotation");
        line.Apply(Words("拆除 B3")); Check(line.factory.pieces.empty(),"pipe coordinate removal");
        Check(std::ranges::none_of(line.factory.pieces,[](const auto& p) { return p.cell==ParseCell("C3"); }),"empty terminal remains empty");
        Factory terminal;
        Add(terminal,Kind::White,ParseCell("C3"),3);
        terminal.LayPath({ParseCell("A3"),ParseCell("B3"),ParseCell("C3")});
        Check(terminal.pieces.size()==2 && terminal.Get(1).kind==Kind::White,"existing terminal tank preserved and connected");
        Draft full;
        for (int r=0;r<7;++r) for(int q=0;q<7;++q) if(Playable(0,{q,r}) && Cell{q,r} != ParseCell("A1")) Add(full.factory,Kind::Pipe,{q,r},9);
        auto blocked = full.factory.pieces;
        full.Apply(Words("自动"));
        Check(full.factory.pieces == blocked,"no tanks for pipes without machines");
        Draft output; output.factory.current=6;
        output.Apply(Words("机 c3")); output.Apply(Words("管 d3 侧三通"));
        output.Apply(Words("自动"));
        Check(output.factory.Validate().Valid(),"automatic supply and branched pipe caps valid");
        Check(std::ranges::count(output.factory.pieces,Kind::White,&Piece::kind)==2,"white tanks cap both pipe ends");
        Check(std::ranges::count(output.factory.pieces,Kind::Supply,&Piece::kind)==1,"only pink needed");
        output.Apply(Words("撤销")); Check(output.factory.pieces.size()==2,"undo whole automatic action");
        Draft input; input.factory.current=41;
        input.Apply(Words("机 C4")); input.Apply(Words("管 B3 直管")); input.Apply(Words("旋转 B3")); input.Apply(Words("自动"));
        Check(input.factory.Validate().Valid(),"supply follows input pipe network");
    }
    for (int skin = 0; skin < 6; ++skin) {
        int n = 0; for (int r = 0; r < 7; ++r) for (int q = 0; q < 7; ++q) {
            n += Playable(skin,{q,r});
            for (int e = 0; e < 6; ++e) Check(Neighbor(Neighbor({q,r},e),Mod(e+3)) == Cell{q,r},"reciprocal neighbors");
        }
        Check(n == 41,"board has 41 cells");
    }
    Check(kMachines.size() == 48,"48 machines");
    auto f = Simple(); Check(f.Validate().Valid(),"simple valid"); Check(f.Cost() == 2,"two tanks cost 2");
    f.Settle(false); Check(f.money == 15,"income 7 less 2"); Check(f.Cost() == 0,"settled cost zero"); ++f.round;
    Throws([&] { f.Move(1,{3,2}); },"old machine move blocked");
    Throws([&] { f.Rotate(1,1); },"old machine rotate blocked");
    Throws([&] { f.Remove(1); },"old machine remove blocked");
    f.Move(2,{0,2}); Check(f.Cost() == 1,"moving old tank costs 1");
    f.Move(2,{1,2}); Check(f.Cost() == 0,"moving back is free");
    f.Remove(2); Add(f,Kind::Supply,{1,2},4); Check(f.Cost() == 0,"new ID equivalent layout is free");
    f.Settle(true); Check(f.money == 15,"skip free without multiplayer modifier");
    f = Simple(); f.Get(2).color = 8; Check(!f.Validate().Valid(),"wrong color rejected");
    f = Simple(); f.Get(3).kind = Kind::White; Check(!f.Validate().Valid(),"black into white rejected");
    f = Simple(); f.Remove(3); Check(!f.Validate().Valid(),"disconnected rejected");
    f = Simple(); Throws([&] { Add(f,Kind::Supply,{0,0},4); },"duplicate supply blocked");
    Factory stock; for (int i=0;i<3;++i) Add(stock,Kind::White,{i,0});
    Throws([&] { Add(stock,Kind::White,{3,0}); },"fourth white blocked");
    Draft d; d.factory.current = 41;
    d.Apply(Words("连 A3 B3 C3")); Check(d.factory.pieces.size() == 1,"path leaves both endpoints empty");
    auto before = d.factory.pieces;
    Throws([&] { d.Apply(Words("连 A1 B1 D4")); },"invalid path fails"); Check(d.factory.pieces == before,"failed path atomic");
    d.Apply(Words("撤销")); Check(d.factory.pieces.empty(),"path undone together");
    d.Apply(Words("重做")); Check(d.factory.pieces == before,"path redone together");
    d.Apply(Words("重置")); Check(d.factory.pieces.empty(),"reset baseline"); d.Apply(Words("撤销")); Check(d.factory.pieces == before,"reset undo");
    Draft batch; batch.factory.current = 41;
    batch.Apply(Words("方案 机 C4；绿 B3; 黑 C5; 旋转 C5 逆时针; 旋转 C5 逆时针"));
    Check(batch.factory.Validate().Valid() && batch.factory.Cost()==2,"one message completes factory");
    batch.Apply(Words("撤销")); Check(batch.factory.pieces.empty(),"batch undone together");
    Throws([&] { batch.Apply(Words("方案 机 C4; 绿 B3; 黑 D4")); },"bad step rejects batch");
    Check(batch.factory.pieces.empty(),"failed batch leaves no partial machine");
    Throws([&] { batch.Apply(Words("方案 机 C4; 提交")); },"batch cannot submit");
    Factory cross; Add(cross,Kind::Pipe,{2,2},9); Add(cross,Kind::Pipe,{2,2},18); Add(cross,Kind::Pipe,{2,2},36);
    Check(cross.pieces.size() == 3,"three independent crossing pipes");
    const auto cross_picture=FactoryHtml(cross);
    Draft multiple; multiple.factory=cross;
    multiple.Apply(Words("拆 1 #2 3 3")); Check(multiple.factory.pieces.empty(),"multiple IDs removed including duplicates");
    multiple.Apply(Words("撤销")); Check(multiple.factory.pieces==cross.pieces,"multiple removal undone together");
    Throws([&] {multiple.Apply(Words("删除 1 99 2"));},"unknown ID rejects whole removal");
    Check(multiple.factory.pieces==cross.pieces,"failed multiple removal unchanged");
    Factory colored=cross;
    Add(colored,Kind::Supply,Neighbor(ParseCell("C3"),3),1);
    Add(colored,Kind::Supply,Neighbor(ParseCell("C3"),4),2);
    Add(colored,Kind::Supply,Neighbor(ParseCell("C3"),2),4);
    auto liquid=PipeLiquidColors(colored);
    Check(liquid.at(1)==1 && liquid.at(2)==2 && liquid.at(3)==4,"crossing pipes keep independent liquid colors");
    colored.LayPath({ParseCell("C3"),ParseCell("D3"),ParseCell("E3")});
    Check(PipeLiquidColors(colored).at(colored.pieces.back().id)==1,"liquid color propagates through connected pipes");
    const auto colored_picture=FactoryHtml(colored);
    for(int color:{1,2,4}) Check(colored_picture.find(std::string("fill=\"")+LiquidColor(color)+"\"")!=std::string::npos,"label follows determined liquid color");
    Factory mixed=cross;
    Add(mixed,Kind::Supply,ParseCell("B3"),1); Add(mixed,Kind::Supply,ParseCell("D3"),8);
    Check(PipeLiquidColors(mixed).at(1)==0,"incompatible liquid network stays neutral");
    Check(PipeLiquidColors(cross).at(1)==0,"unconnected pipe stays neutral");
    Check(cross_picture.find("component-labels")>cross_picture.rfind("<line "),"crossing labels drawn after all pipes");
    for(const std::string color:{"#85939d","#bec6cb","#65727e"})
        Check(cross_picture.find("fill=\""+color+"\"")!=std::string::npos,"each crossing label uses matching pipe color");
    for(const std::string alias:{"拆","拆除","移除","删","删除"}) {
        Draft removal; removal.factory=cross;
        removal.Apply(Words(alias+" #2"));
        Check(removal.factory.pieces.size()==2 && removal.factory.Get(1).edges==9 && removal.factory.Get(3).edges==36,"removal alias selects one crossing by ID");
        removal.Apply(Words("撤销")); Check(removal.factory.pieces==cross.pieces,"undo restores specified crossing");
        removal.Apply(Words(alias+" 3")); Check(removal.factory.pieces.size()==2,"bare ID removal");
    }
    Throws([&] { Add(cross,Kind::Pipe,{2,2},5); },"cross shared edges rejected");
    auto locked=cross.pieces;
    Throws([&] {cross.Rotate(1,1);},"no alternative orientation");
    Check(cross.pieces==locked,"failed rotation unchanged");
    Draft adaptive;
    adaptive.Apply(Words("管 C3 直管")); adaptive.Apply(Words("管 C3 直管")); adaptive.Apply(Words("管 C3 直管"));
    Check(adaptive.factory.Get(2).edges==18 && adaptive.factory.Get(3).edges==36,"automatic stacked placement");
    Factory turns; Add(turns,Kind::Pipe,{2,2},11); Add(turns,Kind::Pipe,{2,2},36);
    turns.Rotate(1,1); Check(turns.Get(1).edges==25 && turns.Get(1).rot==3,"rotation skips two blocked angles");
    Factory flips; Add(flips,Kind::Pipe,{2,2},11); Add(flips,Kind::Pipe,{2,2},36);
    flips.Flip(1); Check(flips.Get(1).edges==19 && flips.Get(1).rot==1,"flip rotates past collision");
    Factory impossible; Add(impossible,Kind::Pipe,{2,2},11); Add(impossible,Kind::Pipe,{2,2},52);
    auto unchanged=impossible.pieces;
    Throws([&] {impossible.Flip(1);},"impossible flip rejected");
    Check(impossible.pieces==unchanged,"failed flip unchanged");
    Throws([&] {Edit(impossible,Words("管 A1 03"));},"numeric edge syntax removed");
    Factory branch; Add(branch,Kind::Pipe,{2,2},9); branch.LayPath({{2,2},{3,2}});
    Check(branch.pieces.size() == 1 && branch.pieces[0].edges == 9,"endpoint pipe unchanged");
    Factory chain;
    AddMachine(chain,3,{3,2},0); Add(chain,Kind::Supply,{4,3},4);
    AddMachine(chain,40,{3,1},2); Add(chain,Kind::White,{3,0},2);
    Check(chain.Validate().Valid(),"machine chain valid"); Check(chain.Validate().bonus == 3,"per input dot bonus");
    auto merge = chain; merge.Get(4).kind = Kind::Supply; merge.Get(4).color = 2;
    Check(merge.Validate().Text().find("供应罐与机器输出不能合流") != std::string::npos,"source cannot merge with machine output");
    Factory split;
    Add(split,Kind::Pipe,{2,1},27);
    AddMachine(split,3,{1,1},2); AddMachine(split,40,{2,2},5); AddMachine(split,28,{1,0},0); AddMachine(split,17,{3,1},2);
    Add(split,Kind::Supply,{0,1},4); Add(split,Kind::White,{2,3},5); Add(split,Kind::White,{0,0},0);
    Add(split,Kind::Supply,{3,2},1); Add(split,Kind::Black,{3,0},2);
    Check(split.Validate().Valid() && split.Validate().bonus == 9,"three way split: 3 points at 3 each");
    split.Get(3).machine = 42;
    Check(split.Validate().Text().find("流量不足") != std::string::npos,"aggregate demand exceeds output");
    Factory cycle;
    AddMachine(cycle,32,{2,2},1); AddMachine(cycle,3,{2,3},4); AddMachine(cycle,31,{4,4},3); AddMachine(cycle,23,{4,2},2); cycle.round = 4;
    Add(cycle,Kind::Supply,{3,2},1);
    for (auto [q,r,edges] : std::vector<std::tuple<int,int,int>>{{3,1,5},{4,1,9},{5,1,12},{1,4,33},{2,4,9},{3,4,41},{4,3,36},{4,3,18},{5,3,18},{5,4,24}}) Add(cycle,Kind::Pipe,{q,r},edges);
    Check(cycle.Validate().issues.size() == 1,"cycle fixture has only one error");
    Check(cycle.Validate().issues[0].find("循环") != std::string::npos,"four machine production cycle rejected");
    auto cycle_before = cycle.pieces; Throws([&] { cycle.Settle(false); },"cycle cannot settle"); Check(cycle_before == cycle.pieces,"failed settlement preserves draft");
    cycle.Remove(6); cycle.Remove(7); Add(cycle,Kind::Supply,{3,1},2); Add(cycle,Kind::White,{4,1},0);
    Check(cycle.Validate().Valid(),"broken feedback with crossing is valid");
    Check(cycle.Validate().bonus > 0,"chain bonus retained");

    Match solo(1,"solo"), same_solo(1,"solo"); std::set<int> solo_cards;
    Check(solo.Solo() && solo.players[0].deck==same_solo.players[0].deck,"solo seeded deck reproducible");
    for (int round=1; round<=8; ++round) {
        const auto& p=solo.players[0];
        Check(solo.phase==Phase::Build && solo.market.size()==1 && solo.market[0].owner==0,"solo directly enters construction");
        Check(p.draft.factory.current==p.deck[round-1] && solo_cards.insert(p.draft.factory.current).second,"solo receives eight distinct cards in order");
        Check(!p.selection.first && !p.selection.last && p.selection.free_discard,"solo has no selection fees or rewards");
        solo.players[0].draft.Apply(Words("白 A3"));
        solo.Submit(0,true); solo.Commit();
        Check(solo.Score(0)==10 && solo.players[0].draft.factory.pieces.empty(),"solo skips roll back construction for free");
        if (round<8) { ++solo.round; solo.OpenRound(); }
    }
    Match solo_build(1,"solo-build"); solo_build.players[0].draft.factory=Simple();
    solo_build.Submit(0,false); solo_build.Commit();
    Check(solo_build.Score(0)==15,"solo build charges parts without selection adjustments");
    ++solo_build.round; solo_build.OpenRound();
    Check(solo_build.players[0].draft.factory.pieces.size()==3,"solo retains previous factory");
    solo_build.Submit(0,true); solo_build.Commit();
    Check(solo_build.Score(0)==15,"solo later skip preserves previous income");
    Match remaining(2,"remaining"); remaining.Leave(1); ++remaining.round; remaining.OpenRound();
    Check(!remaining.Solo() && remaining.phase==Phase::Ready,"multiplayer does not become solo when others leave");

    Match m(6,"test"); std::set<int> cards;
    for (auto& p : m.players) for (int card : p.deck) Check(cards.insert(card).second,"48 unique dealt cards");
    for (int round = 1; round <= 8; ++round) {
        m.phase = Phase::Select;
        for (int i=0;i<6;++i) m.Pass(i);
        for (auto& p : m.players) Check(p.selection.free_discard && !p.selection.first && !p.selection.last,"random allocation unmarked");
        m.phase = Phase::Build;
        for (int i=0;i<6;++i) m.Submit(i,true);
        m.Commit(); if (round < 8) { ++m.round; m.OpenRound(); }
    }
    for(int i=0;i<6;++i) Check(m.Score(i)==10,"eight rounds random skips keep 10");
    Match three(3,"three"); three.round=2; three.OpenRound(); three.phase=Phase::Select;
    three.Claim(0,1); Check(three.players[0].selection.first,"first marker after round1");
    Throws([&]{three.Claim(1,1);},"claim exclusive"); Throws([&]{three.Claim(0,2);},"cannot switch card");
    three.Claim(1,2); Check(three.players[2].selection.last,"last player automatically claims remaining card");
    three.phase=Phase::Build; three.Submit(0,true); three.Submit(1,true); three.Submit(2,true);
    Check(three.players[0].draft.factory.money==10,"simultaneous accounting");
    Throws([&]{three.Submit(0,true);},"duplicate submit rejected"); three.Commit();
    Check(three.Score(0)==7 && three.Score(1)==8 && three.Score(2)==10,"skip fees 3/2/0");
    Match two(2,"two"); two.round=2; two.OpenRound(); two.phase=Phase::Select;
    two.Claim(0,1); Check(!two.players[0].selection.first && two.players[1].selection.last,"two player markers");
    Match first(2,"first"); first.phase=Phase::Select; first.Claim(0,1);
    Check(!first.players[1].selection.last && first.players[1].selection.free_discard,"first round exemption without marker");
    Match timeout(2,"timeout"); timeout.round=2; timeout.OpenRound(); timeout.phase=Phase::Select; timeout.Resolve(true);
    Check(!timeout.players[1].selection.last && timeout.players[1].selection.free_discard,"timeout single card unmarked");
    timeout.phase=Phase::Build; timeout.players[0].draft.factory=Simple(); timeout.BuildTimeout();
    Check(timeout.players[0].decision && !timeout.players[0].submitted && timeout.players[1].submitted,"valid draft gets confirmation invalid skips");
    timeout.phase=Phase::Decision; timeout.Submit(0,false); timeout.Commit(); Check(timeout.Score(0)==15,"keep valid frozen draft");
    Match expired(2,"expired"); expired.phase=Phase::Select; expired.Resolve(true); expired.phase=Phase::Build;
    expired.players[0].draft.factory=Simple(); expired.BuildTimeout(); expired.phase=Phase::Decision; expired.DecisionTimeout(); expired.Commit();
    Check(expired.Score(0)==10 && expired.players[0].draft.factory.pieces.empty(),"confirmation timeout abandons valid draft");
    Match committed_leave(2,"committed-leave"); committed_leave.phase=Phase::Select; committed_leave.Resolve(true); committed_leave.phase=Phase::Build;
    committed_leave.players[0].draft.factory=Simple(); committed_leave.Submit(0,false); committed_leave.Leave(0);
    Check(committed_leave.Score(0)==15 && committed_leave.players[0].draft.factory.pieces.size()==3,"leaving after submission preserves it");
    Match leave(3,"leave"); leave.phase=Phase::Select; leave.Claim(0,1); leave.Leave(0); leave.Pass(1); leave.Pass(2);
    Check(leave.players[0].draft.factory.money==8,"claimed leaving player abandons");
    leave.phase=Phase::Build; leave.Submit(1,true); leave.Submit(2,true); leave.Commit(); ++leave.round; leave.OpenRound();
    Check(leave.market.size()==2,"leaver no longer reveals cards");
    Check(Selection{true,false,false}.Adjustment(true)==-3,"first discard");
    Check(Selection{false,true,true}.Adjustment(false)==1,"last built reward");
    Match estimates(3,"estimate"); estimates.round=2; estimates.OpenRound(); estimates.phase=Phase::Build;
    estimates.players[0].draft.factory=Simple(); estimates.players[0].selection={true,false,false};
    auto figures=estimates.Estimate(0);
    Check(figures.cash==10 && figures.income==4 && figures.total==14 && !figures.unfinished,"estimate includes costs and first fee");
    Check(figures.current_chain==0 && figures.machine_revenue==7 && figures.parts_cost==2 && figures.new_chain==0 && figures.adjustment==-1,"estimate exposes separate components");
    estimates.players[1].draft.factory=chain;
    Check(estimates.Estimate(1).current_chain==0 && estimates.Estimate(1).new_chain==3,"draft chain is separate from settled score");
    estimates.players[1].draft.factory.baseline=chain.pieces; estimates.players[1].draft.factory.current=0;
    Check(estimates.Estimate(1).current_chain==3 && estimates.Estimate(1).new_chain==0,"settled chain stays in current score");
    estimates.players[0].selection={false,true,true};
    Check(estimates.Estimate(0).income==6 && estimates.Estimate(0).total==16,"estimate includes last reward");
    estimates.Submit(0,true);
    Check(estimates.Estimate(0).cash==10 && estimates.Estimate(0).income==0 && estimates.Estimate(0).total==10,"submitted skip estimate uses locked result");
    estimates.Commit(); Check(estimates.Estimate(0).income==0,"committed result does not double count income");
    Check(FactoryHtml(Simple()).find("<svg") != std::string::npos,"board SVG exists");
    const auto picture = FactoryHtml(Simple(),"games/factory_funner/resource","玩家 <A&B>");
    Check(picture.find("当前分数")!=std::string::npos && picture.find("本轮预估收益")!=std::string::npos && picture.find("当前情况")!=std::string::npos,"financial headings above board");
    Check(picture.find("青色工厂")==std::string::npos,"board title omits color name");
    Check(picture.find("r=\"13\"")==std::string::npos && picture.find(">入</text>")==std::string::npos && picture.find(">出</text>")==std::string::npos,"port overlays do not obscure original artwork");
    Check(picture.find("玩家 &lt;A&amp;B&gt;") != std::string::npos,"escape player caption");
    Check(picture.find("Toke_Factory_Funner_Machine_Tiles_41_a0s0.png") != std::string::npos,"machine artwork");
    Check(picture.find("Reservoirs_Supply-green_a0s0.png") != std::string::npos,"supply artwork");
    Check(picture.find("Reservoir_b_a0s0.png") != std::string::npos,"output tank artwork");
    Check(picture.find("xlink:href=\"file:///") != std::string::npos,"local SVG 1.1 image URLs");
    for (int skin = 0; skin < 6; ++skin) {
        auto factory = Simple(); factory.skin = skin;
        Check(FactoryHtml(factory).find("Boar_Factory_Funner_pBoard_A_") != std::string::npos,"board artwork for each player skin");
    }
}

// JSON-free line protocol for comparing placement/validation with the JS source.
// Input: skin machine q r rotation. Output: valid cost bonus (or -1 on placement error).
static void Parity() {
    int skin,id,q,r,rot;
    while (std::cin >> skin >> id >> q >> r >> rot) {
        Factory f; f.skin=skin; f.current=id;
        try {
            Piece p; p.kind=Kind::Machine; p.cell={q,r}; p.rot=rot; f.Place(p);
            for (auto port : Ports(f.pieces.front())) {
                Piece tank; tank.cell=Neighbor({q,r},port.edge); tank.rot=Mod(port.edge+3);
                if (port.kind==PortKind::Input) { tank.kind=Kind::Supply; tank.color=port.colors & -port.colors; }
                else tank.kind=port.colors==16 ? Kind::Black : Kind::White;
                f.Place(tank);
            }
            auto v=f.Validate(); std::cout << v.Valid() << ' ' << f.Cost() << ' ' << v.bonus << '\n';
        } catch(const std::exception&) { std::cout << "-1\n"; }
    }
}
int main(int argc,char** argv) {
    try {
        if(argc>1 && std::string(argv[1])=="--parity") { Parity(); return 0; }
        if(argc>1 && std::string(argv[1])=="--html") {
            auto factory = Simple();
            if (argc > 3) factory.skin = Number(argv[3],0,5);
            std::cout << "<!doctype html><meta charset=\"utf-8\">" << FactoryHtml(factory,argc > 2 ? argv[2] : "games/factory_funner/resource","示例玩家"); return 0;
        }
        Tests(); std::cout << "Passed " << checks << " checks\n";
    } catch(const std::exception& e) { std::cerr << "FAILED: " << e.what() << '\n'; return 1; }
}
