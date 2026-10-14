#include "game_framework/unittest_base.h"
#include "match.h"

namespace lgtbot::game::GAME_MODULE_NAME {
GAME_TEST(1, standalone_commit_alias) {
    ASSERT_PUB_MSG(OK,0,"种子 solo-build"); START_GAME();
    ASSERT_PRI_MSG(FAILED,0,"commit");
    ASSERT_PRI_MSG(OK,0,"c4 auto");
    ASSERT_PRI_MSG(FAILED,0,"commit auto");
    ASSERT_PRI_MSG(CHECKOUT,0,"commit");
    ASSERT_PRI_MSG(FAILED,0,"拆 c4");
}
GAME_TEST(1, standalone_cm_alias) {
    ASSERT_PUB_MSG(OK,0,"种子 solo-build"); START_GAME();
    ASSERT_PRI_MSG(FAILED,0,"cm");
    ASSERT_PRI_MSG(OK,0,"c4 auto");
    ASSERT_PRI_MSG(FAILED,0,"cm auto");
    ASSERT_PRI_MSG(CHECKOUT,0,"cm");
    ASSERT_PRI_MSG(FAILED,0,"拆 c4");
}
GAME_TEST(1, batch_commit_alias) {
    ASSERT_PUB_MSG(OK,0,"种子 solo-build"); START_GAME();
    ASSERT_PRI_MSG(FAILED,0,"c4 commit");
    ASSERT_PRI_MSG(FAILED,0,"撤销");
    ASSERT_PRI_MSG(FAILED,0,"c4 commit auto");
    ASSERT_PRI_MSG(FAILED,0,"方案 c4 auto commit");
    ASSERT_PRI_MSG(FAILED,0,"拆 c4");
    ASSERT_PRI_MSG(CHECKOUT,0,"c4 r6 auto commit");
    ASSERT_PRI_MSG(FAILED,0,"拆 c4");
}
GAME_TEST(1, batch_cm_alias) {
    ASSERT_PUB_MSG(OK,0,"种子 solo-build"); START_GAME();
    ASSERT_PRI_MSG(FAILED,0,"c4 cm");
    ASSERT_PRI_MSG(FAILED,0,"撤销");
    ASSERT_PRI_MSG(FAILED,0,"c4 cm auto");
    ASSERT_PRI_MSG(FAILED,0,"方案 c4 auto cm");
    ASSERT_PRI_MSG(FAILED,0,"拆 c4");
    ASSERT_PRI_MSG(CHECKOUT,0,"c4；r6；auto；cm");
    ASSERT_PRI_MSG(FAILED,0,"拆 c4");
}
GAME_TEST(2, shortcut_and_implicit_batch_commands) {
    START_GAME();
    ASSERT_PRI_MSG(FAILED,0,"b4 旋3");
    ASSERT_TIMEOUT(CHECKOUT); ASSERT_TIMEOUT(CHECKOUT);
    ASSERT_PRI_MSG(FAILED,0,"旋3");
    ASSERT_PRI_MSG(FAILED,0,"r3");
    ASSERT_PRI_MSG(OK,0,"b4"); ASSERT_PRI_MSG(OK,0,"旋3");
    ASSERT_PRI_MSG(OK,0,"旋 2"); ASSERT_PRI_MSG(OK,0,"旋");
    ASSERT_PRI_MSG(OK,1,"a1b1c1");
    ASSERT_PRI_MSG(OK,1,"撤销"); ASSERT_PRI_MSG(OK,1,"重做");
    ASSERT_PRI_MSG(OK,0,"c4 旋3 自动");
    ASSERT_PRI_MSG(OK,0,"r3"); ASSERT_PRI_MSG(OK,0,"auto");
    ASSERT_PRI_MSG(OK,0,"c4 r3 auto");
    ASSERT_PRI_MSG(OK,0,"撤销"); ASSERT_PRI_MSG(OK,0,"重做");
    ASSERT_PRI_MSG(OK,0,"黑 C5；旋转 C5 逆时针 2；拆 C5;旋2");
    ASSERT_PRI_MSG(FAILED,1,"c4 旋3 黑 D4");
    ASSERT_PRI_MSG(FAILED,1,"拆 C4");
    ASSERT_PRI_MSG(OK,1,"c4\n旋3\n自动");
    ASSERT_PRI_MSG(OK,0,"跳过");
    ASSERT_PRI_MSG(FAILED,0,"b4 旋3");
}
GAME_TEST(1, shortcut_batch_submit_and_rollback) {
    ASSERT_PUB_MSG(OK,0,"种子 solo-build"); START_GAME();
    ASSERT_PRI_MSG(FAILED,0,"c4 提交");
    ASSERT_PRI_MSG(FAILED,0,"拆 C4");
    ASSERT_PRI_MSG(FAILED,0,"撤销");
    ASSERT_PRI_MSG(FAILED,0,"c4 提交 自动");
    ASSERT_PRI_MSG(FAILED,0,"方案 c4 自动 提交");
    ASSERT_PRI_MSG(FAILED,0,"撤销");
    ASSERT_PRI_MSG(OK,0,"黑 A1"); ASSERT_PRI_MSG(OK,0,"撤销");
    ASSERT_PRI_MSG(FAILED,0,"c4 提交");
    ASSERT_PRI_MSG(OK,0,"重做"); ASSERT_PRI_MSG(OK,0,"撤销");
    namespace ff = ::factory_funner;
    ff::Match reference(1,"solo-build");
    reference.players[0].draft.Apply(ff::Words("c4 自动")); reference.Submit(0,false); reference.Commit();
    ASSERT_PRI_MSG(CHECKOUT,0,"c4；r6；auto；提交");
    ASSERT_SCORE(reference.Score(0));
    ASSERT_PRI_MSG(FAILED,0,"拆 C4"); ASSERT_PRI_MSG(FAILED,0,"旋3");
}
GAME_TEST(2, short_tank_commands) {
    START_GAME(); ASSERT_TIMEOUT(CHECKOUT); ASSERT_TIMEOUT(CHECKOUT);
    ASSERT_PRI_MSG(OK,0,"白 c1"); ASSERT_PRI_MSG(OK,0,"黑 c2"); ASSERT_PRI_MSG(OK,0,"旋转 c2 逆时针");
    ASSERT_PRI_MSG(OK,0,"粉 a1"); ASSERT_PRI_MSG(OK,0,"黄 b1");
    ASSERT_PRI_MSG(OK,0,"绿 d1"); ASSERT_PRI_MSG(OK,0,"蓝 e1");
    ASSERT_PRI_MSG(FAILED,0,"白"); ASSERT_PRI_MSG(FAILED,0,"黑 c3 6");
    ASSERT_PRI_MSG(FAILED,0,"罐 白 c3");
    ASSERT_PRI_MSG(FAILED,0,"方案 白 a3; 罐 黑 b3");
    ASSERT_PRI_MSG(OK,0,"方案 白 a3；黑 b3；旋转 b3；旋转 b3");
    ASSERT_PRI_MSG(OK,0,"撤销"); ASSERT_PRI_MSG(OK,0,"重做");
}
GAME_TEST(2, convenient_construction_commands) {
    START_GAME(); ASSERT_TIMEOUT(CHECKOUT); ASSERT_TIMEOUT(CHECKOUT);
    ASSERT_PRI_MSG(OK,0,"机 C4"); ASSERT_PRI_MSG(OK,0,"自动");
    ASSERT_PRI_MSG(OK,0,"旋转 C4 逆时针"); ASSERT_PRI_MSG(OK,0,"拆除 C4");
    ASSERT_PRI_MSG(OK,0,"撤销"); ASSERT_PRI_MSG(OK,0,"检查");
    ASSERT_PRI_MSG(OK,1,"连续画线 A3B3C3"); ASSERT_PRI_MSG(OK,1,"拆除 B3");
}
GAME_TEST(0, insufficient_players) { ASSERT_FALSE(StartGame()); }
GAME_TEST(1, solo_eight_rounds) {
    ASSERT_PUB_MSG(OK,0,"单机"); START_GAME();
    ASSERT_PUB_MSG(OK,0,"操作"); ASSERT_PUB_MSG(OK,0,"牌池");
    for (int round=1; round<=8; ++round) {
        ASSERT_PUB_MSG(FAILED,0,"抢 1"); ASSERT_PUB_MSG(FAILED,0,"不抢");
        ASSERT_PRI_MSG(FAILED,0,"提交"); ASSERT_PRI_MSG(FAILED,0,"保留");
        ASSERT_PRI_MSG(OK,0,"白 A3");
        ASSERT_PRI_MSG(OK,0,"撤销"); ASSERT_PRI_MSG(OK,0,"重做");
        ASSERT_PRI_MSG(CHECKOUT,0,"跳过");
        ASSERT_SCORE(10); ASSERT_FINISHED(round==8);
    }
}
GAME_TEST(1, solo_timeout_skips_eight_rounds) {
    ASSERT_PUB_MSG(OK,0,"单机"); START_GAME();
    for (int round=1; round<=8; ++round) {
        ASSERT_PRI_MSG(OK,0,"白 A3");
        ASSERT_TIMEOUT(CHECKOUT);
        ASSERT_SCORE(10); ASSERT_FINISHED(round==8);
        if (round<8) ASSERT_PRI_MSG(FAILED,0,"撤销");
    }
}
GAME_TEST(1, solo_timeout_freezes_valid_factory) {
    ASSERT_PUB_MSG(OK,0,"种子 solo-build"); START_GAME();
    namespace ff = ::factory_funner;
    ff::Match reference(1,"solo-build");
    reference.players[0].draft.Apply(ff::Words("c4 auto"));
    reference.Submit(0,false); reference.Commit();
    ASSERT_PRI_MSG(OK,0,"c4 auto");
    ASSERT_TIMEOUT(CHECKOUT);
    ASSERT_PRI_MSG(FAILED,0,"r3"); ASSERT_PRI_MSG(FAILED,0,"撤销");
    ASSERT_PRI_MSG(CHECKOUT,0,"保留");
    ASSERT_SCORE(reference.Score(0));
    ASSERT_PRI_MSG(FAILED,0,"拆 c4");
    for (int round=2; round<=8; ++round) ASSERT_TIMEOUT(CHECKOUT);
    ASSERT_FINISHED(true); ASSERT_SCORE(reference.Score(0));
}
GAME_TEST(1, solo_frozen_confirmation_timeout_skips) {
    ASSERT_PUB_MSG(OK,0,"种子 solo-build"); START_GAME();
    ASSERT_PRI_MSG(OK,0,"c4 auto");
    ASSERT_TIMEOUT(CHECKOUT); ASSERT_TIMEOUT(CHECKOUT);
    ASSERT_SCORE(10);
    ASSERT_PRI_MSG(FAILED,0,"拆 c4"); ASSERT_PRI_MSG(FAILED,0,"撤销");
    ASSERT_PRI_MSG(FAILED,0,"保留");
}
GAME_TEST(1, solo_build_and_keep_previous_factory) {
    ASSERT_PUB_MSG(OK,0,"种子 solo-build"); START_GAME();
    namespace ff = ::factory_funner;
    ff::Match reference(1,"solo-build");
    auto& draft=reference.players[0].draft;
    draft.Apply(ff::Words("机 C4")); draft.Apply(ff::Words("自动"));
    ASSERT_TRUE(draft.factory.Validate().Valid());
    const int expected=10+ff::MachineById(draft.factory.current).revenue-draft.factory.Cost();
    ASSERT_PRI_MSG(OK,0,"机 C4"); ASSERT_PRI_MSG(OK,0,"自动");
    ASSERT_PUB_MSG(OK,0,"赛况"); ASSERT_PRI_MSG(CHECKOUT,0,"提交");
    ASSERT_SCORE(expected);
    ASSERT_PRI_MSG(FAILED,0,"拆除 C4"); ASSERT_PRI_MSG(FAILED,0,"撤销");
    for (int round=2; round<=8; ++round) ASSERT_PRI_MSG(CHECKOUT,0,"跳过");
    ASSERT_FINISHED(true); ASSERT_SCORE(expected);
}
GAME_TEST(7, too_many_players) { ASSERT_FALSE(StartGame()); }
GAME_TEST(6, each_player_picture_commands) {
    START_GAME();
    ASSERT_PUB_MSG(OK,0,"赛况");
    for (int i=0; i<6; ++i) ASSERT_PRI_MSG(OK,i,"工厂");
    ASSERT_TIMEOUT(CHECKOUT); ASSERT_PUB_MSG(OK,0,"牌池"); ASSERT_PUB_MSG(OK,0,"赛况");
    ASSERT_TIMEOUT(CHECKOUT);
    ASSERT_PUB_MSG(OK,0,"赛况");
    for (int i=0; i<6; ++i) {
        ASSERT_PRI_MSG(OK,i,"工厂");
        ASSERT_PRI_MSG(OK,i,"机器 41");
        ASSERT_PRI_MSG(OK,i,"管 A3 直管");
    }
}
GAME_TEST(2, eight_rounds_pass_and_skip) {
    START_GAME();
    for(int round=1;round<=8;++round) {
        ASSERT_PUB_MSG(FAILED,0,"准备"); ASSERT_TIMEOUT(CHECKOUT);
        ASSERT_PUB_MSG(OK,0,"不抢"); ASSERT_PUB_MSG(CHECKOUT,1,"不抢");
        ASSERT_PRI_MSG(OK,0,"文字"); ASSERT_PRI_MSG(OK,0,"检查");
        ASSERT_PRI_MSG(OK,0,"跳过"); ASSERT_PRI_MSG(FAILED,0,"机 C4");
        ASSERT_PRI_MSG(FAILED,0,"跳过"); ASSERT_PRI_MSG(CHECKOUT,1,"跳过");
    }
    ASSERT_FINISHED(true); ASSERT_SCORE(10,10);
}
GAME_TEST(3, selection_exclusive_and_fees) {
    START_GAME(); ASSERT_TIMEOUT(CHECKOUT);
    ASSERT_PUB_MSG(OK,0,"抢 1"); ASSERT_PUB_MSG(FAILED,1,"抢 1"); ASSERT_PUB_MSG(FAILED,0,"抢 2");
    ASSERT_PUB_MSG(CHECKOUT,1,"抢 2");
    ASSERT_PRI_MSG(OK,0,"跳过"); ASSERT_PRI_MSG(OK,1,"跳过"); ASSERT_PRI_MSG(CHECKOUT,2,"跳过");
    ASSERT_SCORE(8,8,10);
    ASSERT_TIMEOUT(CHECKOUT);
    ASSERT_PUB_MSG(OK,0,"抢 1"); ASSERT_PUB_MSG(CHECKOUT,1,"抢 2");
    ASSERT_PRI_MSG(OK,0,"跳过"); ASSERT_PRI_MSG(OK,1,"跳过"); ASSERT_PRI_MSG(CHECKOUT,2,"跳过");
    ASSERT_SCORE(5,6,10);
}
GAME_TEST(2, full_timeout_game) {
    START_GAME();
    for(int i=0;i<8;++i) { ASSERT_TIMEOUT(CHECKOUT); ASSERT_TIMEOUT(CHECKOUT); ASSERT_TIMEOUT(CHECKOUT); }
    ASSERT_FINISHED(true); ASSERT_SCORE(10,10);
}
GAME_TEST(6, six_player_eight_rounds) {
    START_GAME();
    for(int i=0;i<8;++i) { ASSERT_TIMEOUT(CHECKOUT); ASSERT_TIMEOUT(CHECKOUT); ASSERT_TIMEOUT(CHECKOUT); }
    ASSERT_FINISHED(true); ASSERT_SCORE(10,10,10,10,10,10);
}
GAME_TEST(2, batch_failure_and_undo) {
    START_GAME(); ASSERT_TIMEOUT(CHECKOUT); ASSERT_TIMEOUT(CHECKOUT);
    ASSERT_PRI_MSG(FAILED,0,"方案 管 A3 直管; 管 D4 直管");
    ASSERT_PRI_MSG(FAILED,0,"撤销");
    ASSERT_PRI_MSG(OK,0,"方案 管 A3 直管；管 B3 直管");
    ASSERT_PRI_MSG(OK,0,"撤销"); ASSERT_PRI_MSG(FAILED,0,"拆 1");
    ASSERT_PRI_MSG(OK,0,"重做"); ASSERT_PRI_MSG(OK,0,"拆 1");
}
GAME_TEST(2, successful_build_and_freeze) {
    ASSERT_PUB_MSG(OK,0,"种子 freeze"); START_GAME(); ASSERT_TIMEOUT(CHECKOUT);
    // Derive the selected machine from the deterministic deck, not a magic seed.
    ::factory_funner::Match reference(2,"freeze");
    const int card=reference.market[0].machine;
    ASSERT_PUB_MSG(CHECKOUT,0,"抢 1");
    namespace ff = ::factory_funner;
    ff::Factory f; f.current=card;
    bool found=false;
    for(int r=0;r<7 && !found;++r) for(int q=0;q<7 && !found;++q) for(int rot=0;rot<6 && !found;++rot) {
        ff::Factory next=f;
        try {
            const auto machine_command="机 "+ff::Coord({q,r});
            ff::Edit(next,ff::Words(machine_command));
            std::vector<std::string> commands;
            auto apply=[&](const std::string& cmd) { ff::Edit(next,ff::Words(cmd)); commands.push_back(cmd); };
            for(int step=0;step<rot;++step) apply("旋转 "+ff::Coord({q,r}));
            for(auto port:ff::Ports(next.pieces[0])) {
                auto cell=ff::Neighbor({q,r},port.edge);
                std::string color=port.kind==ff::PortKind::Input ? ff::ColorName(port.colors & -port.colors) : port.colors==16 ? "黑" : "白";
                apply(color+" "+ff::Coord(cell));
                for(int step=0;step<ff::Mod(port.edge+3);++step) apply("旋转 "+ff::Coord(cell));
            }
            if(!next.Validate().Valid()) continue;
            ASSERT_PRI_MSG(OK,0,machine_command);
            for(auto& cmd:commands) ASSERT_PRI_MSG(OK,0,cmd);
            f=next; found=true;
        } catch(const std::exception&) {}
    }
    ASSERT_TRUE(found);
    ASSERT_TIMEOUT(CHECKOUT); // valid player enters frozen decision, other skips
    ASSERT_PRI_MSG(FAILED,0,"撤销"); ASSERT_PRI_MSG(FAILED,0,"拆 1");
    ASSERT_PRI_MSG(CHECKOUT,0,"保留");
    ASSERT_SCORE(10+ff::MachineById(card).revenue-f.Cost(),10);
}
GAME_TEST(3, leave_does_not_block) {
    START_GAME(); ASSERT_LEAVE(CONTINUE,0);
    ASSERT_PUB_MSG(FAILED,1,"准备"); ASSERT_TIMEOUT(CHECKOUT);
    ASSERT_PUB_MSG(OK,1,"不抢"); ASSERT_PUB_MSG(CHECKOUT,2,"不抢");
    ASSERT_PRI_MSG(OK,1,"跳过"); ASSERT_PRI_MSG(CHECKOUT,2,"跳过");
    for(int i=1;i<8;++i) { ASSERT_TIMEOUT(CHECKOUT); ASSERT_TIMEOUT(CHECKOUT); ASSERT_TIMEOUT(CHECKOUT); }
    ASSERT_FINISHED(true);
}
}
int main(int argc,char** argv) {
    testing::InitGoogleTest(&argc,argv); gflags::ParseCommandLineFlags(&argc,&argv,true); return RUN_ALL_TESTS();
}
