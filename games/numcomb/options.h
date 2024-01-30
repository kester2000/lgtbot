EXTEND_OPTION("每回合最长时间x秒", 局时, (ArithChecker<uint32_t>(10, 3600, "局时（秒）")), 120)
EXTEND_OPTION("随机种子", 种子, (AnyArg("种子", "我是随便输入的一个字符串")), "")
EXTEND_OPTION("回合数，即放置数字的数量", 回合数, (ArithChecker<uint32_t>(10, 20, "回合数")), 20)
EXTEND_OPTION("对于洗好的56个砖块，若前X个均不是癞子，则跳过这X个砖块（用于提升游戏中癞子出现的概率）", 跳过非癞子, (ArithChecker<uint32_t>(0, 36, "数量")), 20)
EXTEND_OPTION("最多癞子数量", 癞子, (ArithChecker<uint32_t>(0, 2, "癞子数量")), 2)
EXTEND_OPTION("设置皮肤", 皮肤,
            AlterChecker<int>(
                {{"随机", 0}, {"默认", 1}, {"绿", 2}, {"粉", 3}, {"金", 4}}),
            0)
