#include "game_stage.h"
#include "msg_checker.h"
#include "dllmain.h"
#include "resource.h"
#include <memory>
#include <array>
#include <functional>
#include "resource_loader.h"

const std::string k_game_name = "LIE";
const uint64_t k_min_player = 2; /* should be larger than 1 */
const uint64_t k_max_player = 2; /* 0 means no max-player limits */
const char* Rule()
{
  static std::string rule = LoadText(IDR_TEXT1_RULE, TEXT("Text"));
  return rule.c_str();
}

class NumberStage : public AtomStage
{
 public:
  NumberStage(const uint64_t questioner)
    : AtomStage("设置数字阶段",
      {
        MakeStageCommand(this, "设置数字", &NumberStage::Number, std::make_unique<ArithChecker<int, 1, 6>>("数字")),
      }), questioner_(questioner), num_(0) {}

  int num() const { return num_; }

 private:
   bool Number(const uint64_t pid, const bool is_public, const std::function<void(const std::string&)> reply, const int num)
   {
     if (pid != questioner_)
     {
       reply("[错误] 本回合您为猜测者，无法设置数字");
       return false;
     }
     if (is_public)
     {
       reply("[错误] 请私信裁判选择数字，公开选择无效");
       return false;
     }
     num_ = num;
     reply("设置成功，请提问数字");
     return true;
   }

   const uint64_t questioner_;
   int num_;
};

class LieStage : public AtomStage
{
public:
  LieStage(const uint64_t questioner)
    : AtomStage("设置数字阶段",
      {
        MakeStageCommand(this, "提问数字", &LieStage::Lie, std::make_unique<ArithChecker<int, 1, 6>>("数字")),
      }), questioner_(questioner), lie_num_(0) {}

  int lie_num() const { return lie_num_; }

private:
  bool Lie(const uint64_t pid, const bool is_public, const std::function<void(const std::string&)> reply, const int lie_num)
  {
    if (pid != questioner_)
    {
      reply("[错误] 本回合您为猜测者，无法提问");
      return false;
    }
    lie_num_ = lie_num;
    Boardcast((std::stringstream() << "玩家" << At(pid) << "提问数字" << lie_num << "，请玩家" << At(1 - pid) << "相信或质疑").str());
    return true;
  }

  const uint64_t questioner_;
  int lie_num_;
};

class GuessStage : public AtomStage
{
public:
  GuessStage(const uint64_t guesser)
    : AtomStage("设置数字阶段",
      {
        MakeStageCommand(this, "猜测", &GuessStage::Guess, std::make_unique<BoolChecker>("质疑", "相信")),
      }), guesser_(guesser) {}

  bool doubt() const { return doubt_; }

private:
  bool Guess(const uint64_t pid, const bool is_public, const std::function<void(const std::string&)> reply, const bool doubt)
  {
    if (pid != guesser_)
    {
      reply("[错误] 本回合您为提问者，无法猜测");
      return false;
    }
    doubt_ = doubt;
    return true;
  }

  const uint64_t guesser_;
  bool doubt_;
};

class RoundStage : public CompStage<NumberStage, LieStage, GuessStage>
{
 public:
   RoundStage(const uint64_t round, const uint64_t questioner, std::array<std::array<int, 6>, 2>& player_nums)
     : CompStage("第" + std::to_string(round) + "回合", {}),
     questioner_(questioner), num_(0), lie_num_(0), player_nums_(player_nums), loser_(0) {}

   uint64_t loser() const { return loser_; }

   virtual VariantSubStage OnStageBegin() override
   {
     Boardcast(name_ + "开始，请玩家" + At(questioner_) + "私信裁判选择数字");
     return std::make_unique<NumberStage>(questioner_);
   }

   virtual VariantSubStage NextSubStage(NumberStage& sub_stage, const bool is_timeout) override
   {
     num_ = sub_stage.num();
     return std::make_unique<LieStage>(questioner_);
   }

   virtual VariantSubStage NextSubStage(LieStage& sub_stage, const bool is_timeout) override
   {
     lie_num_ = sub_stage.lie_num();
     return std::make_unique<GuessStage>(1 - questioner_);
   }

   virtual VariantSubStage NextSubStage(GuessStage& sub_stage, const bool is_timeout) override
   {
     const bool doubt = sub_stage.doubt();
     const bool suc = doubt ^ (num_ == lie_num_);
     loser_ = suc ? questioner_ : 1 - questioner_;
     ++player_nums_[loser_][num_ - 1];
     std::stringstream ss;
     ss << "实际数字为" << num_ << "，"
       << (doubt ? "怀疑" : "相信") << (suc ? "成功" : "失败") << "，"
       << "玩家" << At(loser_) << "获得数字" << num_ << std::endl
       << "数字获得情况：" << std::endl << At(0) << "：" << At(1);
     for (int num = 1; num <= 6; ++num)
     {
       ss << std::endl << player_nums_[0][num - 1] << " [" << num << "] " << player_nums_[1][num - 1];
     }
     Boardcast(ss.str());
     return {};
   }

private:
  const uint64_t questioner_;
  int num_;
  int lie_num_;
  std::array<std::array<int, 6>, 2>& player_nums_;
  uint64_t loser_;
};

class MainStage : public CompStage<RoundStage>
{
 public:
  MainStage() : CompStage("", {}), questioner_(0), round_(1), player_nums_{ {0} } {}

  virtual VariantSubStage OnStageBegin() override
  {
    return std::make_unique<RoundStage>(1, std::rand() % 2, player_nums_);
  }

  virtual VariantSubStage NextSubStage(RoundStage& sub_stage, const bool is_timeout) override
  {
    questioner_ = sub_stage.loser();
    if (JudgeOver()) { return {}; }
    return std::make_unique<RoundStage>(++round_, questioner_, player_nums_);
  }

  int64_t PlayerScore(const uint64_t pid) const
  {
    return pid == questioner_ ? -10 : 10;
  }

 private:
   bool JudgeOver()
   {
     bool has_all_num = true;
     for (const int count : player_nums_[questioner_])
     {
       if (count >= 3) { return true; }
       else if (count == 0) { has_all_num = false; }
     }
     return has_all_num;
   }

   uint64_t questioner_;
   uint64_t round_;
   std::array<std::array<int, 6>, 2> player_nums_;
};

std::pair<std::unique_ptr<Stage>, std::function<int64_t(uint64_t)>> MakeMainStage(const uint64_t player_num)
{
  assert(player_num == 2);
  std::unique_ptr<MainStage> main_stage = std::make_unique<MainStage>();
  const auto get_player_score = std::bind(&MainStage::PlayerScore, main_stage.get(), std::placeholders::_1);
  return { static_cast<std::unique_ptr<Stage>&&>(std::move(main_stage)), get_player_score };
}
