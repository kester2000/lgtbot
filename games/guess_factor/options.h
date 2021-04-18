GAME_OPTION("每回合最长时间x秒", 局时, (std::make_unique<ArithChecker<uint32_t, 10, 3600>>("局时（秒）")), 300)
GAME_OPTION("可预测的最大数字（可预测的最小数字为1）", 最大数字,
            (std::make_unique<ArithChecker<uint32_t, 3, 9999>>("数字")), 10)
GAME_OPTION("开始淘汰时游戏必须已经进展到第x回合", 淘汰回合,
            (std::make_unique<ArithChecker<uint32_t, 1, 100>>("回合数")), 1)
GAME_OPTION("达到开始淘汰的回合数之后，每隔x回合淘汰一名玩家", 淘汰间隔,
            (std::make_unique<ArithChecker<uint32_t, 1, 5>>("回合数")), 1)
GAME_OPTION("开始淘汰时最高分玩家必须已经达到x分", 淘汰分数, (std::make_unique<ArithChecker<uint32_t, 1, 100>>("分数")),
            20)
GAME_OPTION("分数末尾玩家分数距离次末尾玩家分数达到x分时才会被淘汰", 淘汰分差,
            (std::make_unique<ArithChecker<uint32_t, 1, 100>>("分数")), 5)
GAME_OPTION("游戏最多进展到第x回合", 最大回合, (std::make_unique<ArithChecker<uint32_t, 1, 100>>("回合数")), 20)
