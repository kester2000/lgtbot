#include "game_framework/unittest_base.h"
#include "match.h"

namespace lgtbot::game::GAME_MODULE_NAME {
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
GAME_TEST(1, insufficient_players) { ASSERT_FALSE(StartGame()); }
GAME_TEST(7, too_many_players) { ASSERT_FALSE(StartGame()); }
GAME_TEST(6, each_player_picture_commands) {
    START_GAME();
    for (int i=0; i<6; ++i) ASSERT_PRI_MSG(OK,i,"工厂");
    ASSERT_TIMEOUT(CHECKOUT); ASSERT_PUB_MSG(OK,0,"牌池");
    ASSERT_TIMEOUT(CHECKOUT);
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
