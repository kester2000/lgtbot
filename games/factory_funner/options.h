EXTEND_OPTION("每轮抢选时限（秒）", 选机时限, (ArithChecker<uint32_t>(10, 600, "秒")), 60)
EXTEND_OPTION("每轮施工时限（秒）", 施工时限, (ArithChecker<uint32_t>(30, 3600, "秒")), 600)
EXTEND_OPTION("固定随机种子（设置后为测试局）", 种子, (AnyArg("种子")), std::string())
