// Copyright (c) 2024-present, Chang Liu <github.com/slontia>. All rights reserved.
//
// This source code is licensed under LGPLv2 (found in the LICENSE file).

#pragma once

#include <optional>
#include <span>
#include <ranges>

#include "game_util/mahjong_util.h"
#include "utility/defer.h"
#include "Mahjong/Rule.h"
#include "calsht_dw.hpp"

using namespace std::string_literals;

namespace lgtbot {

namespace game_util {

namespace mahjong {

template <typename Enum>
constexpr auto EnumValue(const Enum N) { return static_cast<std::underlying_type_t<Enum>>(N); }

static constexpr const std::array<BaseTile, 13> k_one_nine_tiles{_1m, _9m, _1s, _9s, _1p, _9p, east, south, west, north, 白, 发, 中};
static constexpr const uint32_t k_max_dora_num = 4;
static constexpr const uint32_t k_max_player = 4;

struct RiverTile
{
    uint32_t kiri_round_{0};
    Tile tile_;
    bool is_tsumo_kiri_{false};
    bool richii_{false};
};

struct KiriTile
{
    Tile tile_;
    enum class Type : uint8_t { NORMAL, ADD_KAN, DARK_KAN, NORTH, RIVER_BOTTOM } type_;
    bool break_ippatsu_{false};
};

struct PlayerKiriInfo
{
    uint32_t player_id_{UINT32_MAX};
    std::vector<KiriTile> kiri_tiles_;
};

struct KiriInfo
{
    std::vector<PlayerKiriInfo> other_players_;
    TileSet other_player_kiri_tiles_; // these tiles can used to nari
};

struct FuruTile
{
    Tile tile_;
    enum class Type : uint8_t { EMPTY, NORMAL, NARI, ADD_KAN, DARK } type_{Type::EMPTY};
};

struct Furu
{
    uint32_t nari_round_{0};
    std::array<FuruTile, 4> tiles_;
    bool is_tsumo_nari_{false};
};

enum class ActionState
{
    ROUND_BEGIN, // nari, get tile
    AFTER_CHI_PON, // kiri
    AFTER_GET_TILE, // kiri, kan, richii (if has no furu)
    AFTER_KAN, // kiri, kan, richii (if has no furu)
    AFTER_KAN_CAN_NARI, // kiri, kan
    AFTER_KIRI, // nari, over
    ROUND_OVER,
    NOTIFIED_RON,
};

struct FuResult {
    uint32_t player_id_;
    CounterResult counter_;
    Tile tile_;
};

struct SyncMahjongOption
{
#ifdef TEST_BOT
    std::variant<TilesOption, std::string> tiles_option_;
#else
    TilesOption tiles_option_;
#endif
    std::string image_path_;
    int32_t benchang_{0};
    int32_t richii_points_{0};
    std::vector<PlayerDesc> player_descs_;
};

template <size_t PlayerNum>
    requires (PlayerNum == 3 || PlayerNum == 4)
static constexpr std::array<BaseTile, k_tile_type_num> k_dora_sign_to_dora = []()
    {
        std::array<BaseTile, k_tile_type_num> result;
        auto fill_result = [&](const BaseTile start, const BaseTile end)
            {
                result[end] = start;
                for (int basetile = start; basetile < end; ++basetile) {
                    result[basetile] = static_cast<BaseTile>(basetile + 1);
                }
            };
        if constexpr (PlayerNum == 3) {
            result[_1m] = _9m;
            result[_9m] = _1m;
        } else {
            fill_result(_1m, _9m);
        }
        fill_result(_1s, _9s);
        fill_result(_1p, _9p);
        fill_result(east, north);
        fill_result(白, 中);
        return result;
    }();

static bool IsDora(const uint32_t player_num, const std::span<const std::pair<Tile, Tile>> doras, const BaseTile basetile)
{
    const auto is_dora = [player_num, basetile](const auto dora_signs)
        {
            return (player_num == 3 ? k_dora_sign_to_dora<3> : k_dora_sign_to_dora<4>)[dora_signs.first.tile] == basetile;
        };
    return std::ranges::any_of(doras, is_dora);
}

class DorasManager
{
  public:
    DorasManager(const std::array<std::pair<Tile, Tile>, 4>& doras) : doras_(doras) {}

    void TryOpenNewDora(const int32_t round)
    {
        if (round > last_open_round_ && dora_num_ < k_max_dora_num) {
            last_open_round_ = round;
            ++dora_num_;
        }
    }

    bool HasOpenedThisRound(const int32_t round) const { return last_open_round_ == round; }

    std::span<const std::pair<Tile, Tile>> Doras() const
    {
        return std::span<const std::pair<Tile, Tile>>(doras_.begin(), doras_.begin() + dora_num_);
    }

  private:
    std::array<std::pair<Tile, Tile>, 4> doras_;
    int32_t last_open_round_{0};
    uint32_t dora_num_{1};
};

enum class AutoOption { AUTO_FU, AUTO_KIRI, AUTO_GET_TILE };

constexpr int k_max_yaku = static_cast<int>(Yaku::双倍役满);

class SyncMahjongGamePlayer
{
    friend class SyncMajong;

  public:
    SyncMahjongGamePlayer(const std::string& image_path, const std::vector<PlayerDesc>& player_descs,
            const uint32_t player_id, std::vector<Tile> yama, TileSet hand,
            const std::array<std::pair<Tile, Tile>, 4>& doras)
        : image_path_(image_path)
        , player_descs_(player_descs)
        , player_id_(player_id)
        , wind_(player_descs_[player_id].wind_)
        , yama_(yama)
        , hand_(std::move(hand))
        , cur_round_my_kiri_info_{.player_id_ = player_id}
        , doras_manager_(doras)
        , public_html_(Html_(HtmlMode::PUBLIC))
    {
    }

    std::string PublicDoraHtml() const { return DoraHtml(image_path_, doras_manager_.Doras(), false); }

    const std::string& PublicHtml() const { return public_html_; }

    std::string PrivateHtml() const { return Html_(HtmlMode::PRIVATE); }

    void PerformAi()
    {
        while (state_ != ActionState::ROUND_OVER) {
            switch (state_) {
                case ActionState::ROUND_BEGIN:
                    GetTileInternal_();
                    state_ = ActionState::AFTER_GET_TILE;
                case ActionState::AFTER_GET_TILE:
                case ActionState::AFTER_KAN:
                case ActionState::AFTER_KAN_CAN_NARI:
                    if (Tsumo()) {
                        break;
                    }
                    if (IsRiichi_()) {
                        KiriInternal_(true /*is_tsumo*/, false /*richii*/, *tsumo_);
                        tsumo_ = std::nullopt;
                        state_ = ActionState::ROUND_OVER;
                        break;
                    }
                    KiriAI_();
                    break;
                case ActionState::AFTER_CHI_PON:
                    KiriAI_();
                    break;
                case ActionState::NOTIFIED_RON:
                    Ron(); // may be failed
                    break;
            }
        }
    }

    void PerformDefault()
    {
        switch (state_) {
            case ActionState::ROUND_BEGIN:
                GetTileInternal_();
            case ActionState::AFTER_GET_TILE:
            case ActionState::AFTER_KAN:
            case ActionState::AFTER_KAN_CAN_NARI:
                KiriInternal_(true /*is_tsumo*/, false /*richii*/, *tsumo_);
                tsumo_ = std::nullopt;
                break;
            case ActionState::AFTER_CHI_PON:
                KiriInternal_(false /*is_tsumo*/, false /*richii*/, *hand_.begin());
                hand_.erase(hand_.begin());
                break;
        }
        state_ = ActionState::ROUND_OVER;
    }

    bool GetTile()
    {
        if (state_ != ActionState::ROUND_BEGIN) {
            errstr_ = "当前状态不允许摸牌";
            return false;
        }
        GetTileInternal_();
        state_ = ActionState::AFTER_GET_TILE;
        TryAutoKiri_();
        return true;
    }

    bool Nagashi()
    {
        if (state_ != ActionState::AFTER_GET_TILE) {
            errstr_ = "当前状态不允许宣告九种九牌流局";
            return false;
        }
        if (round_ != 1) {
            errstr_ = "仅第一巡允许宣告九种九牌流局";
            return false;
        }
        if (!CanNagashi_()) {
            errstr_ = "手牌中幺九牌小于九种，无法宣告九种九牌流局";
            return false;
        }
        state_ = ActionState::ROUND_OVER;
        return true;
    }

    bool Kiri(const std::string_view tile, const bool richii)
    {
        if (state_ != ActionState::AFTER_CHI_PON && state_ != ActionState::AFTER_GET_TILE && state_ != ActionState::AFTER_KAN && state_ != ActionState::AFTER_KAN_CAN_NARI) {
            errstr_ = "当前状态不允许切牌";
            return false;
        }
        if (richii && IsRiichi_()) {
            errstr_ = "您已经立直";
            return false;
        }
        if (!tile.empty() && IsRiichi_()) {
            errstr_ = "立直状态下只能选择摸切";
            return false;
        }
        if (richii && yama_idx_ >= yama_.size()) {
            errstr_ = "只有在牌山有牌的情况下才可以立直";
            return false;
        }
        if (richii && !std::ranges::all_of(furus_,
                    [](const Furu& furu) { return IsDarkKan_(furu.tiles_) || IsKita_(furu.tiles_); })) {
            errstr_ = "在有副露的情况下不允许立直";
            return false;
        }
        if (tile.empty() ? !KiriTsumo_(richii) : !KiriHand_(tile, richii)) {
            return false;
        }
        state_ =
            (state_ == ActionState::AFTER_CHI_PON || state_ == ActionState::AFTER_KAN_CAN_NARI) &&
                (CanChi_() || CanPon_() || CanRon_()) ?
            ActionState::AFTER_KIRI : ActionState::ROUND_OVER;
        if (GetAutoOption(AutoOption::AUTO_FU)) {
            Ron(); // may be failed
        }
        return true;
    }

    bool Over()
    {
        if (state_ != ActionState::AFTER_KIRI && state_ != ActionState::NOTIFIED_RON) {
            errstr_ = "当前状态不允许结束行动";
            return false;
        }
        if (IsRiichi_()) {
            is_richii_furutin_ = true;
        }
        state_ = ActionState::ROUND_OVER;
        return true;
    }

    bool Chi(const std::string_view hand_tiles, const std::string_view others_kiri_tile)
    {
        if (state_ != ActionState::ROUND_BEGIN && state_ != ActionState::AFTER_KIRI) {
            errstr_ = "当前状态不允许吃牌";
            return false;
        }
        const auto tiles = GetTilesFrom(hand_, hand_tiles, errstr_);
        const auto kiri_tile = GetTilesFrom(cur_round_kiri_info_.other_player_kiri_tiles_, others_kiri_tile, errstr_, GetTileMode::PREFER_RED_DORA);
        const auto rollback =
            [&] {
                hand_.insert(tiles.begin(), tiles.end());
                cur_round_kiri_info_.other_player_kiri_tiles_.insert(kiri_tile.begin(), kiri_tile.end());
            };
        if (tiles.empty()) {
            errstr_ = "您的手牌中不存在「"s + hand_tiles.data() + "」";
            rollback();
            return false;
        }
        if (kiri_tile.empty()) {
            errstr_ = "前巡舍牌中不存在「"s + others_kiri_tile.data() + "」";
            rollback();
            return false;
        }
        if (tiles.size() != 2) {
            errstr_ = "您需要从手牌中指定 2 张牌，目前指定了「"s + hand_tiles.data() + "」共 " + std::to_string(tiles.size()) + " 张牌";
            rollback();
            return false;
        }
        if (kiri_tile.size() != 1) {
            errstr_ = "您需要从前巡舍牌中指定 1 张牌，目前指定了「"s + others_kiri_tile.data() + "」共 " + std::to_string(kiri_tile.size()) + " 张牌";
            rollback();
            return false;
        }
        if (!is_顺子({tiles.begin()->tile, std::next(tiles.begin())->tile, kiri_tile.begin()->tile})) {
            errstr_ = "「"s + hand_tiles.data() + others_kiri_tile.data() + "」无法形成顺子";
            rollback();
            return false;
        }
        if (!CheckFromChiPlayer_(*kiri_tile.begin())) {
            errstr_ = "您只能吃特定玩家的牌，这些玩家已经没有牌可供吃牌";
            rollback();
            return false;
        }
        furus_.emplace_back();
        auto& furu = furus_.back();
        furu.nari_round_ = round_;
        furu.tiles_[0] = FuruTile{*tiles.begin(), FuruTile::Type::NORMAL};
        furu.tiles_[1] = FuruTile{*std::next(tiles.begin()), FuruTile::Type::NORMAL};
        furu.tiles_[2] = FuruTile{*kiri_tile.begin(), FuruTile::Type::NARI};
        std::sort(furu.tiles_.begin(), furu.tiles_.begin() + 3,
                [](const auto& _1, const auto& _2) { return _1.tile_.tile < _2.tile_.tile; });
        state_ = ActionState::AFTER_CHI_PON;
        return true;
    }

    bool Pon(const std::string_view hand_tiles)
    {
        if (state_ != ActionState::ROUND_BEGIN && state_ != ActionState::AFTER_KIRI) {
            errstr_ = "当前状态不允许碰牌";
            return false;
        }
        // If player only specifies one tile, we should repeat the tile string to specify two tiles.
        const auto tiles = hand_tiles.size() > 3 ?
            GetTilesFrom(hand_, hand_tiles, errstr_) :
            GetTilesFrom(hand_, std::string(hand_tiles) + std::string(hand_tiles), errstr_);
        if (tiles.empty()) {
            errstr_ = "您的手牌中不存在足够的「"s + hand_tiles.data() + "」";
            return false;
        }
        const auto rollback = [&] { hand_.insert(tiles.begin(), tiles.end()); };
        if (tiles.size() != 2) {
            errstr_ = "您需要从手牌中指定 2 张牌，目前指定了「"s + hand_tiles.data() + "」共 " + std::to_string(tiles.size()) + " 张牌";
            rollback();
            return false;
        }
        const std::string& tile_str = basetile_to_string_simple(tiles.begin()->tile);
        const auto kiri_tile = GetTilesFrom(cur_round_kiri_info_.other_player_kiri_tiles_, tile_str, errstr_, GetTileMode::PREFER_RED_DORA);
        if (kiri_tile.empty()) {
            errstr_ = "前巡舍牌中不存在「"s + tile_str + "」";
            rollback();
            return false;
        }
        assert(kiri_tile.size() == 1);
        assert(is_刻子({tiles.begin()->tile, std::next(tiles.begin())->tile, kiri_tile.begin()->tile}));
        furus_.emplace_back();
        auto& furu = furus_.back();
        furu.nari_round_ = round_;
        furu.tiles_[0] = FuruTile{*tiles.begin(), FuruTile::Type::NORMAL};
        furu.tiles_[1] = FuruTile{*std::next(tiles.begin()), FuruTile::Type::NORMAL};
        furu.tiles_[2] = FuruTile{*kiri_tile.begin(), FuruTile::Type::NARI};
        state_ = ActionState::AFTER_CHI_PON;
        return true;
    }

    bool Kan(const std::string_view tile_sv)
    {
        if (tile_sv.size() <= 1) {
            errstr_ = "非法的牌型「"s + tile_sv.data() + "」";
            return false;
        }
        if (tile_sv.size() > 3) {
            errstr_ = "您需要且仅需要指定一张牌";
            return false;
        }
        if (yama_idx_ >= yama_.size()) {
            // no more tiles
            errstr_ = "您当前无可摸牌，无法杠牌";
            return false;
        }
        bool succ = false;
        if (state_ == ActionState::ROUND_BEGIN || state_ == ActionState::AFTER_KIRI) {
            succ = DirectKan_(tile_sv);
        } else if (state_ == ActionState::AFTER_GET_TILE || state_ == ActionState::AFTER_KAN || state_ == ActionState::AFTER_KAN_CAN_NARI) {
            succ = DarkKanOrAddKan_(tile_sv);
        } else {
            errstr_ = "当前状态不允许杠牌";
            return false;
        }
        if (succ) {
            doras_manager_.TryOpenNewDora(round_);
            GetTileInternal_();
            state_ = (state_ == ActionState::ROUND_BEGIN || state_ == ActionState::AFTER_KIRI || state_ == ActionState::AFTER_KAN_CAN_NARI)
                ? ActionState::AFTER_KAN_CAN_NARI : ActionState::AFTER_KAN;
            TryAutoKiri_();
        }
        return succ;
    }

    void KitaInternal_(const Tile& tile, const bool is_tsumo_nari)
    {
        furus_.emplace_back();
        auto& furu = furus_.back();
        furu.nari_round_ = round_;
        furu.tiles_[0] = FuruTile{tile, FuruTile::Type::NORMAL};
        furu.is_tsumo_nari_ = is_tsumo_nari;
        cur_round_my_kiri_info_.kiri_tiles_.emplace_back(
                KiriTile{
                    .tile_ = tile,
                    .type_ = KiriTile::Type::NORTH,
                    .break_ippatsu_ = HasNariThisRound_(),
                });
        GetTileInternal_();
        state_ = state_ == ActionState::AFTER_KAN_CAN_NARI ? ActionState::AFTER_KAN_CAN_NARI : ActionState::AFTER_KAN;
        TryAutoKiri_();
    }

    [[nodiscard]] bool NorthHand_()
    {
        assert(tsumo_.has_value());
        const auto tiles = GetTilesFrom(hand_, "4z", errstr_, GetTileMode::PREFER_TOUMEI);
        if (!tiles.empty()) {
            assert(tiles.size() == 1);
            hand_.emplace(*tsumo_);
            tsumo_ = std::nullopt;
            KitaInternal_(*tiles.begin(), false);
            return true;
        }
        if (tsumo_->tile == BaseTile::north) {
            KitaTsumo_();
            return true;
        }
        errstr_ = "您的手牌中不存在北风";
        return false;
    }

    void KitaTsumo_()
    {
        assert(tsumo_.has_value() && tsumo_->tile == BaseTile::north);
        const Tile tile = *tsumo_;
        tsumo_ = std::nullopt;
        KitaInternal_(tile, true);
    }

    bool Kita(const bool use_tsumo = false)
    {
        if (player_descs_.size() != 3) {
            errstr_ = "只有三麻可以拔北";
            return false;
        }
        if (yama_idx_ >= yama_.size()) {
            // no more tiles
            errstr_ = "您当前无可摸牌，无法拔北";
            return false;
        }
        if (state_ != ActionState::AFTER_GET_TILE && state_ != ActionState::AFTER_KAN && state_ != ActionState::AFTER_KAN_CAN_NARI) {
            errstr_ = "当前状态不允许拔北";
            return false;
        }
        assert(tsumo_.has_value());
        if (use_tsumo) {
            if (tsumo_->tile != BaseTile::north) {
                errstr_ = "当前自摸牌不是北风";
                return false;
            }
            KitaTsumo_();
            return true;
        }
        return NorthHand_();
    }

    bool Tsumo()
    {
        if (state_ != ActionState::AFTER_GET_TILE && state_ != ActionState::AFTER_KAN && state_ != ActionState::AFTER_KAN_CAN_NARI) {
            errstr_ = "当前状态不允许自摸";
            return false;
        }
        assert(tsumo_.has_value());
        std::optional<CounterResult> counter = GetCounter_(*tsumo_,
                std::bitset<EnumValue(GetCounterFlag::MAX_)>{}
                    .set(EnumValue(GetCounterFlag::TSUMO))
                    .set(EnumValue(GetCounterFlag::IS_LAST_TILE), yama_idx_ >= yama_.size())
                , &errstr_);
        if (counter.has_value()) {
            counter->calculate_score(true, true);
        }
        if (counter.has_value()) {
            fu_results_.emplace_back(k_none_player_id_, std::move(*counter), *tsumo_);
        }
        if (fu_results_.empty()) {
            // `errstr_` is filled by `GetCounter_`
            return false;
        }
        state_ = ActionState::ROUND_OVER;
        return true;
    }

    bool Ron()
    {
        if (IsFurutin_()) {
            errstr_ = "当前处于振听状态";
            return false;
        }
        if (state_ == ActionState::NOTIFIED_RON) {
            fu_results_ = GetFuResultsForRon_();
        } else if (state_ == ActionState::AFTER_KIRI) {
            fu_results_ = GetFuResultsForNariRon_();
        } else {
            errstr_ = "当前状态不允许荣和";
            return false;
        }
        if (fu_results_.empty()) {
            errstr_ = "其他玩家舍牌无法使手牌构成合法和牌型";
            return false;
        }
        state_ = ActionState::ROUND_OVER;
        return true;
    }

    void SetAutoOption(const AutoOption option, const bool value) { auto_options_[static_cast<int>(option)] = value; }

    bool GetAutoOption(const AutoOption option) const { return auto_options_[static_cast<int>(option)]; }

    ActionState State() const { return state_; }

    int64_t PointVariation() const { return point_variation_; }

    uint32_t PlayerID() const { return player_id_; }

    const std::string& ErrorString() const { return errstr_; }

    std::bitset<k_max_yaku> Yakus() const
    {
        std::bitset<k_max_yaku> yakus;
        for (const FuResult& fu_result : fu_results_) {
            for (const Yaku yaku : fu_result.counter_.yakus) {
                yakus.set(static_cast<int>(yaku));
            }
            yakus[static_cast<int>(Yaku::役满)] = yakus[static_cast<int>(Yaku::役满)] || fu_result.counter_.fan >= 13;
        }
        return yakus;
    }

#ifdef TEST_BOT
    Tile LastKiriTile() const { return river_.back().tile_; }

    void StartNormalStage(const int32_t round)
    {
        round_ = round;
        return StartNormalStage_();
    }
#endif

  private:
    enum class FuType { TSUMO, ROB_KAN, RON };

    enum class HtmlMode { OPEN, PRIVATE, PUBLIC };
    std::string Html_(const HtmlMode html_mode) const
    {
        std::string s;
        s += PlayerNameHtml(player_descs_[player_id_], point_variation_) + "\n\n";
        s += "<center>\n\n**剩余牌山：" HTML_COLOR_FONT_HEADER(blue);
        s += std::to_string(yama_.size() - yama_idx_);
        if (player_descs_.size() == 4) {
            s += HTML_FONT_TAIL HTML_ESCAPE_SPACE HTML_ESCAPE_SPACE HTML_ESCAPE_SPACE HTML_ESCAPE_SPACE;
            s += "吃牌来源：" HTML_COLOR_FONT_HEADER(blue);
            for (uint32_t player_id = 0; player_id < player_descs_.size(); ++player_id) {
                if (from_chi_players_[player_id]) {
                    s += wind2str(player_descs_[player_id].wind_);
                    s += " ";
                }
            }
        }
        s += HTML_FONT_TAIL "**\n\n</center>\n\n";
        enum class Mode { FU, NAGASHI_MANGAN, NORMAL };
        const Mode mode = fu_results_.empty() ? Mode::NORMAL :
                          fu_results_[0].counter_.yakus[0] == Yaku::流局满贯 ? Mode::NAGASHI_MANGAN : Mode::FU;
#define BIG_TEXT(color, content) "<center> <font size=\"6\">\n\n " HTML_COLOR_FONT_HEADER(color) content HTML_FONT_TAIL "\n\n</font> </center>\n\n"
        if (mode == Mode::FU) {
            s += BIG_TEXT(blue, "**和&nbsp;&nbsp;了**");
            s += DoraHtml(image_path_, doras_manager_.Doras(), IsRiichi_());
            s += "\n\n";
        } else if (mode == Mode::NAGASHI_MANGAN) {
            s += BIG_TEXT(blue, " **流&nbsp;&nbsp;局&nbsp;&nbsp;满&nbsp;&nbsp;贯** ");
        } else if (IsRiichi_() && river_.back().kiri_round_ == richii_round_) {
            s += BIG_TEXT(green, " **立&nbsp;&nbsp;直** ");
        } else if (big_text_) {
            s += big_text_;
        }
        if (mode == Mode::FU) {
            s += HandHtml_(TileStyle::FORWARD);
            s += "\n\n";
            s += RonInfoHtml_(image_path_, player_descs_, fu_results_);
            s += "\n\n<br />\n\n";
        } else if (html_mode == HtmlMode::OPEN) {
            s += HandHtml_(TileStyle::FORWARD) + "\n\n<br />\n\n";
        } else if (html_mode == HtmlMode::PRIVATE) {
            s += HandHtml_(TileStyle::HAND) + "\n\n<br />\n\n";
            if (IsFurutin_()) {
                s += "<center>\n\n" HTML_COLOR_FONT_HEADER(red) " **振听中，无法荣和** " HTML_FONT_TAIL "\n\n</center>";
            }
        } else {
            s += HandHtmlBack_() + "\n\n<br />\n\n";
        }
        if (IsRiichi_()) {
            s += "<div align=\"center\"><img src=\"file:///" + image_path_ + "/riichi.png\"/></div>\n\n<br />\n\n";
        }
        s += RiverHtml_(image_path_, river_);
        return s;
    }

    static std::string RiverHtml_(const std::string& image_path, const std::vector<RiverTile>& river)
    {
        if (river.empty()) {
            return "";
        }
        html::Table table(2, 0);
        table.SetTableStyle(" align=\"center\" cellpadding=\"0\" cellspacing=\"0\" ");
        table.SetRowStyle(" align=\"center\" ");
        for (uint32_t i = 0; i < river.size(); ++i) {
            table.AppendColumn();
            if (river[i].is_tsumo_kiri_) {
                table.GetLastColumn(0).SetContent(HTML_COLOR_FONT_HEADER(red) + std::to_string(river[i].kiri_round_) + HTML_FONT_TAIL);
            } else {
                table.GetLastColumn(0).SetContent(std::to_string(river[i].kiri_round_));
            }
            table.GetLastColumn(1).SetContent(Image(image_path, river[i].tile_, river[i].richii_ ? TileStyle::LEFT : TileStyle::FORWARD));
            if ((i + 1) % 6 == 0) {
                table.AppendColumn();
                table.GetLastColumn(1).SetContent("&nbsp;&nbsp;");
            }
        }
        return table.ToString();
    }

    static std::string FuruTilesHtml_(const std::string& image_path, const std::array<FuruTile, 4>& furu_tiles)
    {
        html::Table table(2, 4);
        table.SetTableStyle(" align=\"center\" valign=\"bottom\" cellpadding=\"0\" cellspacing=\"0\" width=\"1\"");
        table.SetRowStyle(" valign=\"bottom\" ");
        for (uint32_t i = 0; i < furu_tiles.size(); ++i) {
            const FuruTile& furu_tile = furu_tiles[i];
            switch (furu_tile.type_) {
                case FuruTile::Type::EMPTY:
                    break;
                case FuruTile::Type::NORMAL:
                    table.MergeDown(0, i, 2);
                    table.Get(0, i).SetContent(Image(image_path, furu_tile.tile_, TileStyle::FORWARD));
                    break;
                case FuruTile::Type::DARK:
                    table.MergeDown(0, i, 2);
                    if (furu_tile.tile_.toumei) {
                        table.Get(0, i).SetContent(Image(image_path, furu_tile.tile_, TileStyle::FORWARD));
                    } else {
                        table.Get(0, i).SetContent(BackImage(image_path, TileStyle::FORWARD));
                    }
                    break;
                case FuruTile::Type::NARI:
                    {
                        Tile tile = furu_tile.tile_;
                        tile.toumei = false;
                        table.Get(1, i).SetContent("<img src=\"file:///" + image_path + "/2_n" + tile.to_simple_string() + ".png\"/>");
                    }
                    break;
                case FuruTile::Type::ADD_KAN:
                    table.Get(0, i - 1).SetContent(Image(image_path, furu_tile.tile_, TileStyle::LEFT));
                    break;
                default:
                    assert(false);
            }
        }
        return table.ToString();
    }

    static std::string HandHtmlWithName_(const uint32_t player_num, const std::string& image_path, const TileSet& hand,
            const TileStyle style, const DorasManager& dora_manager, const std::optional<Tile>& tsumo = std::nullopt)
    {
        html::Table table(2, 0);
        table.SetTableStyle(" align=\"center\" cellpadding=\"0\" cellspacing=\"0\" ");
        const auto tile_str = [player_num, &dora_manager](const Tile& tile)
            {
                return IsDora(player_num, dora_manager.Doras(), tile.tile) || tile.red_dora
                    ? HTML_COLOR_FONT_HEADER(blue) + tile.to_simple_string() + HTML_FONT_TAIL
                    : tile.to_simple_string();
            };
        for (const auto& tile : hand) {
            table.AppendColumn();
            table.GetLastColumn(0).SetContent(tile_str(tile));
            table.GetLastColumn(1).SetContent(Image(image_path, tile, style));
        }
        if (tsumo.has_value()) {
            table.AppendColumn();
            table.GetLastColumn(0).SetContent(HTML_ESCAPE_SPACE HTML_ESCAPE_SPACE HTML_ESCAPE_SPACE HTML_ESCAPE_SPACE);
            table.AppendColumn();
            table.GetLastColumn(0).SetContent(tile_str(*tsumo));
            table.GetLastColumn(1).SetContent(Image(image_path, *tsumo, style));
        }
        return table.ToString();
    }

    static std::string RonInfoHtml_(const std::string& image_path, const std::vector<PlayerDesc>& player_descs, const std::vector<FuResult>& fu_results)
    {
        std::string s;
        for (const auto& result : fu_results) {
            std::vector<std::string> texts;
            const bool as_tsumo = result.player_id_ == k_none_player_id_;
            if (!as_tsumo) {
                texts.emplace_back(LoserHtml(player_descs[result.player_id_]));
            }
            // `result.counter_.score1` is the score that one player should pay.
            // `result.counter_.score1 * fu_results.size()` is the score that one player should play if only ron one player (6a).
            // We divided the score by 3 so that we can get 2a, which is same as the score that each player should pay when tsumo.
            const auto score = as_tsumo ? result.counter_.score1 : result.counter_.score1 * fu_results.size() / 3;
            std::string score_info = std::string(
                    score == 16000 * 6  ? "六倍役满" :
                    score == 16000 * 5  ? "五倍役满" :
                    score == 16000 * 4  ? "四倍役满" :
                    score == 16000 * 3  ? "三倍役满" :
                    score == 16000 * 2  ? "两倍役满" :
                    score == 16000      ? "役满" :
                    score == 12000      ? "三倍满" :
                    score == 8000      ? "倍满" :
                    score == 6000      ? "跳满" :
                    score == 4000      ? "满贯" : "") +
                " " + (result.counter_.fan > 0 ? std::to_string(result.counter_.fu) + " 符 " + std::to_string(result.counter_.fan) + " 番" : "") +
                " " + std::to_string(result.counter_.score1) + (as_tsumo ? " 点每家" : " 点");
            s += YakusHtml(image_path, result.tile_, score_info, result.counter_.yakus, texts,
                    std::vector<Yaku>{Yaku::场风_东, Yaku::场风_北, Yaku::场风_南, Yaku::场风_西});
            s += "\n\n";
        }
        return s;
    }

    std::string AppendFuruHtmls_(std::string hand_html) const {
        html::Table table(2, 1 + (!furus_.empty()) + furus_.size());
        table.SetTableStyle(" align=\"center\" cellpadding=\"0\" cellspacing=\"0\" ");
        table.SetRowStyle(" align=\"center\" valign=\"bottom\" ");
        table.MergeDown(0, 0, 2);
        table.Get(0, 0).SetContent(std::move(hand_html));
        if (furus_.empty()) {
            return table.ToString();
        }
        table.MergeDown(0, 1, 2);
        table.Get(0, 1).SetContent(HTML_ESCAPE_SPACE HTML_ESCAPE_SPACE HTML_ESCAPE_SPACE HTML_ESCAPE_SPACE);
        for (int64_t i = furus_.size() - 1; i >= 0; --i) {
            if (furus_[i].is_tsumo_nari_) {
                table.Get(0, 2 + furus_.size() - 1 - i).SetContent(
                        HTML_COLOR_FONT_HEADER(red) + std::to_string(furus_[i].nari_round_) + HTML_FONT_TAIL);
            } else {
                table.Get(0, 2 + furus_.size() - 1 - i).SetContent(std::to_string(furus_[i].nari_round_));
            }
            table.Get(1, 2 + furus_.size() - 1 - i).SetContent(FuruTilesHtml_(image_path_, furus_[i].tiles_));
        }
        return table.ToString();
    }

    std::string HandHtml_(const TileStyle tile_style) const {
        std::string hand_html = tile_style == TileStyle::HAND
            ? HandHtmlWithName_(player_descs_.size(), image_path_, hand_, tile_style, doras_manager_, tsumo_)
            : HandHtml(image_path_, hand_, tile_style, tsumo_);
        return AppendFuruHtmls_(std::move(hand_html));
    }

    std::string HandHtmlBack_() const {
        return AppendFuruHtmls_(HandHtmlBack(image_path_, hand_));
    }

    bool IsRiichi_() const { return !richii_listen_tiles_.empty() && richii_round_ != round_; }

    void TryAutoKiri_()
    {
        assert(tsumo_.has_value());
        if ((IsRiichi_() || GetAutoOption(AutoOption::AUTO_KIRI)) && !CanNariTsumoTile_() && !CanTsumo_()) {
            KiriInternal_(true /*is_tsumo*/, false /*richii*/, *tsumo_);
            tsumo_ = std::nullopt;
            state_ = ActionState::ROUND_OVER;
        }
    }

    void StartNormalStage_()
    {
        assert(state_ == ActionState::ROUND_OVER);
        cur_round_my_kiri_info_.kiri_tiles_.clear();
        if (!IsRiichi_() && !GetAutoOption(AutoOption::AUTO_GET_TILE) && (CanChi_() || CanPon_())) {
            state_ = ActionState::ROUND_BEGIN;
            return;
        }
        GetTileInternal_();
        state_ = ActionState::AFTER_GET_TILE;
        if (GetAutoOption(AutoOption::AUTO_FU) && Tsumo()) {
            return;
        }
        TryAutoKiri_();
    }

    void SetKiriInfos_(const std::vector<PlayerKiriInfo>& kiri_infos)
    {
        cur_round_kiri_info_.other_players_.clear();
        cur_round_kiri_info_.other_player_kiri_tiles_.clear();
        for (uint32_t player_id = 0; player_id < kiri_infos.size(); ++player_id) {
            if (player_id == player_id_) {
                continue;
            }
            cur_round_kiri_info_.other_players_.emplace_back(kiri_infos[player_id]);
            for (const auto& kiri_tile : kiri_infos[player_id].kiri_tiles_) {
                if (kiri_tile.type_ == KiriTile::Type::NORMAL || kiri_tile.type_ == KiriTile::Type::RIVER_BOTTOM) {
                    cur_round_kiri_info_.other_player_kiri_tiles_.emplace(kiri_tile.tile_);
                }
            }
        }
    }

    void StartRonStage_(const std::vector<PlayerKiriInfo>& kiri_infos)
    {
        SetKiriInfos_(kiri_infos);
        if (!CanRon_()) {
            return;
        }
        state_ = ActionState::NOTIFIED_RON;
        if (!GetAutoOption(AutoOption::AUTO_FU)) {
            return;
        }
        Ron();
    }

    void GetTileInternal_()
    {
        assert(yama_idx_ < yama_.size());
        tsumo_ = yama_[yama_idx_++];
    }

    bool Richii_()
    {
        std::vector<BaseTile> listen_tiles = GetListenTiles_();
        if (listen_tiles.empty()) {
            errstr_ = "切这张牌无法构成听牌牌型";
            return false;
        }
        assert(richii_listen_tiles_.empty());
        richii_listen_tiles_ = std::move(listen_tiles);
        is_richii_furutin_ = IsSelfFurutin_(richii_listen_tiles_, river_);
        is_double_richii_ = river_.empty() && furus_.empty();
        richii_round_ = round_;
        return true;
    }

    bool KiriInternal_(const bool is_tsumo, const bool richii, const Tile& tile)
    {
        if (richii && !Richii_()) {
            return false;
        }
        river_.emplace_back(round_, tile, is_tsumo, richii);
        cur_round_my_kiri_info_.kiri_tiles_.emplace_back(
                KiriTile{
                    .tile_ = tile,
                    .type_ = yama_idx_ >= yama_.size() ? KiriTile::Type::RIVER_BOTTOM : KiriTile::Type::NORMAL,
                    .break_ippatsu_ = HasNariThisRound_(),
                });
        return true;
    }

    bool HasNariThisRound_() const
    {
        return std::ranges::any_of(furus_, [round = round_](const Furu& furu) { return furu.nari_round_ == round; });
    }

    bool IsFurutin_() const
    {
        return (richii_listen_tiles_.empty() && IsSelfFurutin_(GetListenTiles_(), river_)) || is_richii_furutin_;
    }

    static bool IsSelfFurutin_(const std::vector<BaseTile>& listen_tiles, const std::vector<RiverTile>& river)
    {
        return std::ranges::any_of(river,
                [&listen_tiles](const RiverTile& river_tile) { return std::ranges::find(listen_tiles, river_tile.tile_.tile) != listen_tiles.end(); });
    }

    bool KiriTsumo_(const bool richii)
    {
        if (!tsumo_.has_value()) {
            errstr_ = "您当前没有自摸牌，无法摸切";
            return false;
        }
        assert(tsumo_.has_value());
        const auto tsumo = *tsumo_;
        tsumo_ = std::nullopt;
        if (!KiriInternal_(true, richii, *tsumo_)) {
            tsumo_ = tsumo;
            return false;
        }
        return true;
    }

    bool MatchTsumo_(const std::string_view tile_sv)
    {
        assert(tile_sv.size() == 2 || tile_sv.size() == 3);
        if (!tsumo_.has_value()) {
            return false;
        }
        const std::string tsumo_str = tsumo_->to_simple_string();
        const auto match_not_toumei_tile = [](const std::string_view tsumo_sv, const std::string_view tile_sv)
            {
                assert(tsumo_sv.size() == 2 && tile_sv.size() == 2);
                return tsumo_sv == tile_sv || (tsumo_sv[0] == '0' && tile_sv[0] == '5' && tsumo_sv[1] == tile_sv[1]);
            };
        const auto to_not_toumei_tile = [](const std::string_view toumei_tile_sv) -> std::string_view
            {
                assert(toumei_tile_sv.size() == 3);
                return toumei_tile_sv.substr(1);
            };
        return (tsumo_str.size() == 2 && tile_sv.size() == 2 && match_not_toumei_tile(tsumo_str, tile_sv)) ||
            (tsumo_str.size() == 3 && tile_sv.size() == 2 && match_not_toumei_tile(to_not_toumei_tile(tsumo_str), tile_sv)) ||
            (tsumo_str.size() == 3 && tile_sv.size() == 3 && match_not_toumei_tile(to_not_toumei_tile(tsumo_str), to_not_toumei_tile(tile_sv)));
    }

    bool KiriHand_(const std::string_view tile_sv, const bool richii)
    {
        if (tile_sv.size() <= 1) {
            errstr_ = "非法的牌型「"s + tile_sv.data() + "」";
            return false;
        }
        if (tile_sv.size() > 3) {
            errstr_ = "您需要且仅需要指定一张牌";
            return false;
        }
        auto tiles = GetTilesFrom(hand_, tile_sv, errstr_, tsumo_.has_value() ? GetTileMode::EXACT : GetTileMode::FUZZY);
        if (tiles.empty()) {
            if (!tsumo_.has_value()) {
                errstr_ = "您的手牌中不存在「"s + tile_sv.data() + "」";
                return false;
            }
            if (tsumo_->to_simple_string() == tile_sv) {
                return KiriTsumo_(richii);
            }
            tiles = GetTilesFrom(hand_, tile_sv, errstr_, GetTileMode::FUZZY);
            if (tiles.empty()) {
                if (!MatchTsumo_(tile_sv)) {
                    errstr_ = "您的手牌中不存在「"s + tile_sv.data() + "」";
                    return false;
                }
                return KiriTsumo_(richii);
            }
        }
        assert(tiles.size() == 1);
        if (!KiriInternal_(false, richii, *tiles.begin())) {
            hand_.insert(tiles.begin(), tiles.end()); // rollback
            return false;
        }
        if (tsumo_.has_value()) {
            hand_.emplace(*tsumo_);
            tsumo_ = std::nullopt;
        }
        return true;
    }

    bool DarkKanOrAddKan_(const std::string_view tile_sv) {
        assert(tsumo_.has_value());
        assert(tile_sv.size() <= 3);
        const bool kan_tsumo = MatchTsumo_(tile_sv);
        if (IsRiichi_() && !kan_tsumo) {
            errstr_ = "立直状态下只能选择暗杠或加杠自摸牌";
            return false;
        }
        const auto handle_tsumo = [&]() {
            if (!kan_tsumo) {
                hand_.emplace(*tsumo_);
            }
            tsumo_ = std::nullopt;
        };
        const std::string tile_str(tile_sv);
        const auto hand_tiles = kan_tsumo ? GetTilesFrom(hand_, tile_str + tile_str + tile_str, errstr_)
                                          : GetTilesFrom(hand_, tile_str + tile_str + tile_str + tile_str, errstr_);
        if (!hand_tiles.empty()) {
            // dark kan
            const Tile kan_tile = kan_tsumo ? *tsumo_ : *std::next(hand_tiles.begin(), 3);
            assert(is_杠({hand_tiles.begin()->tile, std::next(hand_tiles.begin())->tile,
                        std::next(hand_tiles.begin(), 2)->tile, kan_tile.tile}));
            if (!richii_listen_tiles_.empty()) {
                assert(tsumo_.has_value()); // richii must kan tsumo
                if (GetListenTiles_(hand_, std::nullopt) != richii_listen_tiles_) {
                    // kan after richii cannot change tinpai
                    hand_.insert(hand_tiles.begin(), hand_tiles.end());
                    errstr_ = "立直状态下暗杠或加杠不允许改变听牌种类";
                    return false;
                }
            }
            furus_.emplace_back();
            auto& furu = furus_.back();
            furu.nari_round_ = round_;
            furu.is_tsumo_nari_ = kan_tsumo;
            furu.tiles_[0] = FuruTile{*hand_tiles.begin(), FuruTile::Type::DARK};
            furu.tiles_[1] = FuruTile{*std::next(hand_tiles.begin()), FuruTile::Type::NORMAL};
            furu.tiles_[2] = FuruTile{*std::next(hand_tiles.begin(), 2), FuruTile::Type::NORMAL};
            furu.tiles_[3] = FuruTile{kan_tile, FuruTile::Type::DARK};
            cur_round_my_kiri_info_.kiri_tiles_.emplace_back(
                    KiriTile{
                        .tile_ = kan_tile,
                        .type_ = KiriTile::Type::DARK_KAN,
                        .break_ippatsu_ = HasNariThisRound_(),
                    });
            handle_tsumo();
            return true;
        }
        const auto one_hand_tile = GetTilesFrom(hand_, tile_str, errstr_);
        if (!kan_tsumo && one_hand_tile.empty()) {
            errstr_ = "您的手牌中不存在「"s + tile_sv.data() + "」";
            return false;
        }
        const Tile& kan_tile = kan_tsumo ? *tsumo_ : *one_hand_tile.begin();
        bool found = false;
        for (auto& furu : furus_) {
            if (IsPon_(furu.tiles_, kan_tile.tile)) {
                // add kan
                assert(furu.tiles_[3].type_ == FuruTile::Type::EMPTY);
                furu.nari_round_ = round_;
                furu.tiles_[3].type_ = FuruTile::Type::ADD_KAN;
                furu.tiles_[3].tile_ = kan_tile;
                cur_round_my_kiri_info_.kiri_tiles_.emplace_back(
                        KiriTile{
                            .tile_ = kan_tile,
                            .type_ = KiriTile::Type::ADD_KAN,
                            .break_ippatsu_ = HasNariThisRound_(),
                        });
                handle_tsumo();
                found = true;
                break;
            }
        }
        if (!found) {
            hand_.insert(one_hand_tile.begin(), one_hand_tile.end());
            errstr_ = "您的副露中不存在「"s + tile_sv.data() + "」的刻子";
        }
        return found;
    }

    bool CanNagashi_() const
    {
        std::array<bool, k_tile_type_num> basetile_bitset{false};
        for (const Tile& tile : hand_) {
            basetile_bitset[tile.tile] = true;
        }
        if (tsumo_.has_value()) {
            basetile_bitset[tsumo_->tile] = true;
        }
        return std::ranges::count_if(k_one_nine_tiles, [&](const BaseTile basetile) { return basetile_bitset[basetile]; }) >= 9;
    }

    bool CanChi_() const
    {
        const auto contains_tile = [&](const int basetile)
            {
                return std::ranges::find_if(hand_, [basetile](const Tile& tile) { return tile.tile == basetile; }) != hand_.end();
            };
        const auto can_make_flush = [contains_tile](const int basetile)
            {
                return basetile == _1m || basetile == _1s || basetile == _1p ?
                            contains_tile(basetile + 1) && contains_tile(basetile + 2) :
                       basetile == _9m || basetile == _9s || basetile == _9p ?
                            contains_tile(basetile - 1) && contains_tile(basetile - 2) :
                       basetile == _2m || basetile == _2s || basetile == _2p ?
                            (contains_tile(basetile + 1) && contains_tile(basetile + 2)) ||
                            (contains_tile(basetile + 1) && contains_tile(basetile - 1)) :
                       basetile == _8m || basetile == _8s || basetile == _8p ?
                            (contains_tile(basetile - 1) && contains_tile(basetile - 2)) ||
                            (contains_tile(basetile + 1) && contains_tile(basetile - 1)) :
                       basetile < east ?
                            (contains_tile(basetile + 1) && contains_tile(basetile + 2)) ||
                            (contains_tile(basetile - 1) && contains_tile(basetile - 2)) ||
                            (contains_tile(basetile + 1) && contains_tile(basetile - 1)) : false;
            };
        const auto can_chi = [&](const KiriTile& kiri_tile)
            {
                const auto& remaining_kiri_tiles = cur_round_kiri_info_.other_player_kiri_tiles_;
                return (kiri_tile.type_ == KiriTile::Type::NORMAL || kiri_tile.type_ == KiriTile::Type::RIVER_BOTTOM) &&
                    can_make_flush(kiri_tile.tile_.tile) &&
                    std::ranges::find_if(remaining_kiri_tiles,
                            [basetile = kiri_tile.tile_.tile](const Tile& tile) { return tile.tile == basetile; }) != remaining_kiri_tiles.end();
            };
        return std::ranges::any_of(cur_round_kiri_info_.other_players_, [&](const PlayerKiriInfo& player_kiri_info)
                {
                    return from_chi_players_[player_kiri_info.player_id_] && std::ranges::any_of(player_kiri_info.kiri_tiles_, can_chi);
                });
    }

    bool CanPon_() const
    {
        return std::ranges::any_of(cur_round_kiri_info_.other_player_kiri_tiles_, [this](const Tile& kiri_tile)
                {
                    return std::ranges::count_if(hand_, [&kiri_tile](const Tile& tile) { return tile.tile == kiri_tile.tile; }) >= 2;
                });
    }

    bool CanNariTsumoTile_() const
    {
        assert(tsumo_.has_value());
        if (player_descs_.size() == 3 && tsumo_->tile == north) {
            return true;
        }
        if (IsRiichi_()) {
            auto hand = hand_;
            // dark kan cannot change listen tiles when riichi
            return std::erase_if(hand, [this](const Tile& tile) { return tile.tile == tsumo_->tile; }) == 3 &&
                GetListenTiles_(hand, std::nullopt) == richii_listen_tiles_;
        }
        return std::ranges::count_if(hand_, [&](const Tile& tile) { return tile.tile == tsumo_->tile; }) >= 3 ||
            std::ranges::any_of(furus_, [&](const Furu& furu) { return IsPon_(furu.tiles_, tsumo_->tile); });
    }

    // If `CanRon_()` returns true, `Ron()` must return true.
    bool CanRon_() const
    {
        if (IsFurutin_()) {
            return false;
        }
        for (const auto& player_info : cur_round_kiri_info_.other_players_) {
            for (const auto& kiri_tile : player_info.kiri_tiles_) {
                if (RonCounter_(kiri_tile).has_value()) {
                    return true;
                }
            }
        }
        return false;
    }

    bool CanTsumo_() const
    {
        assert(tsumo_.has_value());
        return GetCounter_(*tsumo_,
                std::bitset<EnumValue(GetCounterFlag::MAX_)>{}
                    .set(EnumValue(GetCounterFlag::TSUMO))
                    .set(EnumValue(GetCounterFlag::IS_LAST_TILE), yama_idx_ >= yama_.size())
                ).has_value();
    }

    //                  IS_LAST_TILE    BREAK_IPPATSU   ROB_KAN TSUMO
    //         TSUMO          O               X            X      -
    //       ROB_KAN          X               O            -      -
    // BREAK_IPPATSU          O               -            -      -
    //  IS_LAST_TILE          -               -            -      -
    enum class GetCounterFlag : uint8_t { TSUMO, ROB_KAN, BREAK_IPPATSU, IS_LAST_TILE, MAX_ };

    static void ExcludeTableWindFromCounter_(CounterResult& counter)
    {
        counter.fan -= std::erase_if(counter.yakus, [](const Yaku yaku)
                    {
                        return yaku == Yaku::场风_东 || yaku == Yaku::场风_北 || yaku == Yaku::场风_南 || yaku == Yaku::场风_西;
                    });
    }

    void IncludeNorthDoraToCounter_(CounterResult& counter) const
    {
        const auto insert_yakus = [&counter](const Yaku yaku, const size_t count)
        {
            counter.fan += count;
            for (size_t i = 0; i < count; ++i) {
                counter.yakus.emplace_back(yaku);
            }
        };
        static const auto is_dora = [](const Tile& tile) { return tile.tile == BaseTile::west; }; // west directs to north
        const auto north_count = std::ranges::count_if(furus_, [](const Furu& furu) { return IsKita_(furu.tiles_); });
        insert_yakus(Yaku::北宝牌, north_count);
        insert_yakus(Yaku::宝牌,
                north_count * std::ranges::count_if(doras_manager_.Doras() | std::views::elements<0>, is_dora));
        if (IsRiichi_()) {
            insert_yakus(Yaku::里宝牌,
                    north_count * std::ranges::count_if(doras_manager_.Doras() | std::views::elements<1>, is_dora));
        }
    }

    static void RemoveUnnecessaryYakusFromCounterForYakuman_(CounterResult& counter)
    {
        if (std::ranges::any_of(counter.yakus,
                    [](const Yaku yaku) { return yaku > Yaku::满贯 && yaku < Yaku::双倍役满; })) {
            std::erase_if(counter.yakus, [](const Yaku yaku) { return yaku < Yaku::满贯; });
        }
    }

    std::optional<CounterResult> GetCounter_(const Tile& tile, const std::bitset<EnumValue(GetCounterFlag::MAX_)> flag, std::string* const errstr = nullptr) const
    {
        Table table;
        InitTable_(table);
        if (flag.test(EnumValue(GetCounterFlag::BREAK_IPPATSU))) {
            table.players[player_id_].一发 = false;
        }
        auto basetiles = convert_tiles_to_base_tiles(table.players[player_id_].hand);
        basetiles.emplace_back(tile.tile);
        if (!is和牌(basetiles)) {
            if (errstr) {
                *errstr = "不满足和牌牌型";
            }
            return std::nullopt;
        }
        if (flag.test(EnumValue(GetCounterFlag::TSUMO))) {
            table.players[player_id_].hand.emplace_back(const_cast<Tile*>(&tile));
        }
        if (flag.test(EnumValue(GetCounterFlag::IS_LAST_TILE))) {
            table.牌山.resize(14);
        }
        // There is no difference bewteen 枪杠 and 枪暗杠 in `yaku_counter`, so we set 枪暗杠 false.
        // We will remove 场风 yakus from counter later, so it doesn't matter what we pass as 场风.
        auto counter = yaku_counter(&table, player_id_, flag.test(EnumValue(GetCounterFlag::TSUMO)) ? nullptr : const_cast<Tile*>(&tile),
                flag.test(EnumValue(GetCounterFlag::ROB_KAN)) /*枪杠*/, false /*枪暗杠*/, wind_ /*自风*/, wind_ /*场风*/);
        if (counter.yakus.empty() || counter.yakus[0] == Yaku::None) {
            // Is is impossile that there are only table wind yakus because we set table wind as self wind. There is
            // must a self wind yaku attached to table wind.
            if (errstr) {
                *errstr = "无役";
            }
            return std::nullopt;
        }
        ExcludeTableWindFromCounter_(counter);
        IncludeNorthDoraToCounter_(counter);
        RemoveUnnecessaryYakusFromCounterForYakuman_(counter);
        return counter;
    }

    std::optional<CounterResult> RonCounter_(const KiriTile& kiri_tile) const
    {
        // `other_player_kiri_tiles_` does not contain add kan or dark kan tiles
        if (kiri_tile.type_ != KiriTile::Type::ADD_KAN &&
                kiri_tile.type_ != KiriTile::Type::DARK_KAN &&
                kiri_tile.type_ != KiriTile::Type::NORTH &&
                cur_round_kiri_info_.other_player_kiri_tiles_.find(kiri_tile.tile_) == cur_round_kiri_info_.other_player_kiri_tiles_.end()) {
            // this tile has been used, skip
            return std::nullopt;
        }
        const std::optional<CounterResult> counter = GetCounter_(kiri_tile.tile_,
                std::bitset<EnumValue(GetCounterFlag::MAX_)>{}
                    .set(EnumValue(GetCounterFlag::ROB_KAN),
                            kiri_tile.type_ == KiriTile::Type::ADD_KAN || kiri_tile.type_ == KiriTile::Type::DARK_KAN)
                    .set(EnumValue(GetCounterFlag::BREAK_IPPATSU), kiri_tile.break_ippatsu_)
                    .set(EnumValue(GetCounterFlag::IS_LAST_TILE), kiri_tile.type_ == KiriTile::Type::RIVER_BOTTOM));
        if (!counter.has_value()) {
            return std::nullopt;
        }
        if (kiri_tile.type_ == KiriTile::Type::DARK_KAN &&
                std::ranges::none_of(counter->yakus, [](const Yaku yaku) { return yaku == Yaku::国士无双; })) {
            return std::nullopt;
        }
        return counter;
    }

    std::vector<FuResult> GetFuResultsForRon_() const
    {
        std::vector<FuResult> fu_results;
        for (const auto& player_info : cur_round_kiri_info_.other_players_) {
            std::optional<FuResult> player_result;
            for (const auto& kiri_tile : player_info.kiri_tiles_) {
                std::optional<CounterResult> counter = RonCounter_(kiri_tile);
                if (!counter.has_value()) {
                    // cannot ron this tile, skip
                    continue;
                }
                counter->calculate_score(true, false);
                if (!player_result.has_value() || counter->score1 > player_result->counter_.score1) {
                    player_result = FuResult{player_info.player_id_, *counter, kiri_tile.tile_};
                }
            }
            if (player_result.has_value()) {
                fu_results.emplace_back(std::move(*player_result));
            }
        }
        if (fu_results.size() > 1) {
            // more than 1 player lose, we should update the scores
            assert(fu_results.size() <= 3);
            for (auto& player_info : fu_results) {
                if (player_info.counter_.score1 >= 12000) {
                    player_info.counter_.score1 /= fu_results.size();
                } else {
                    player_info.counter_.score1 =
                        static_cast<int32_t>(player_info.counter_.fu * pow(2, player_info.counter_.fan + 2) * 6 / fu_results.size() + 90) / 100 * 100; // upper alias to 100
                }
            }
        }
        return fu_results;
    }

    std::vector<FuResult> GetFuResultsForNariRon_() const
    {
        assert(state_ == ActionState::AFTER_KIRI);
        std::vector<FuResult> fu_results;
        for (const auto& player_info : cur_round_kiri_info_.other_players_) {
            for (const auto& kiri_tile : player_info.kiri_tiles_) {
                std::optional<CounterResult> counter = RonCounter_(kiri_tile);
                if (!counter.has_value()) {
                    // cannot ron this tile, skip
                    continue;
                }
                counter->calculate_score(true, true);
                if (fu_results.empty() || counter->score1 > fu_results.begin()->counter_.score1) {
                    fu_results.emplace_back(k_none_player_id_, *counter, kiri_tile.tile_);
                }
            }
        }
        return fu_results;
    }

    static bool IsKita_(const std::array<FuruTile, 4>& furu_tiles) { return furu_tiles[1].type_ == FuruTile::Type::EMPTY; }

    static bool IsChi_(const std::array<FuruTile, 4>& furu_tiles) { return furu_tiles[0].tile_.tile != furu_tiles[1].tile_.tile; }

    static bool IsPon_(const std::array<FuruTile, 4>& furu_tiles, const BaseTile basetile)
    {
        return furu_tiles[0].tile_.tile == basetile && furu_tiles[1].tile_.tile == basetile && furu_tiles[3].type_ == FuruTile::Type::EMPTY;
    }

    static bool IsDarkKan_(const std::array<FuruTile, 4>& furu_tiles) { return furu_tiles[3].type_ == FuruTile::Type::DARK; }

    static Tile NariTile_(const std::array<FuruTile, 4>& furu_tiles)
    {
        const auto it = std::ranges::find_if(furu_tiles, [](const FuruTile& furu_tile) { return furu_tile.type_ == FuruTile::Type::NARI; });
        assert(it != furu_tiles.end());
        return it->tile_;
    }

    void InitTable_(Table& table) const
    {
        table.dora_spec = doras_manager_.Doras().size();
        for (auto& [dora, inner_dora] : doras_manager_.Doras()) {
            table.宝牌指示牌.emplace_back(const_cast<Tile*>(&dora));
            table.里宝牌指示牌.emplace_back(const_cast<Tile*>(&inner_dora));
        }
        table.庄家 = player_id_;
        table.last_action = state_ == ActionState::AFTER_KAN || state_ == ActionState::AFTER_KAN_CAN_NARI ? Action::杠 : Action::pass;
        table.is_three_players = player_descs_.size() == 3;
        table.players[player_id_].first_round = river_.empty() && furus_.empty();
        table.players[player_id_].riichi = IsRiichi_();
        table.players[player_id_].double_riichi = table.players[player_id_].riichi && is_double_richii_;
        table.players[player_id_].亲家 = true;
        table.players[player_id_].一发 = table.players[player_id_].riichi && richii_round_ + 1 == round_ && (furus_.empty() || furus_.back().nari_round_ != round_);
        table.players[player_id_].门清 = true;
        for (auto& tile : hand_) {
            table.players[player_id_].hand.emplace_back(const_cast<Tile*>(&tile));
        }
        for (const auto& furu : furus_) {
            if (IsKita_(furu.tiles_)) {
                continue;
            }
            Fulu fulu;
            for (int i = 0; i < furu.tiles_.size(); i++) {
                const FuruTile& furu_tile = furu.tiles_[i];
                if (furu_tile.type_ != FuruTile::Type::EMPTY) {
                    fulu.tiles.emplace_back(const_cast<Tile*>(&furu_tile.tile_));
                }
                if (furu_tile.type_ == FuruTile::Type::NARI) {
                    fulu.take = i;
                }
            }
            if (furu.tiles_[3].type_ == FuruTile::Type::EMPTY) {
                fulu.type = IsChi_(furu.tiles_) ? Fulu::Type::Chi : Fulu::Type::Pon;
                table.players[player_id_].门清 = false;
            } else {
                fulu.type = Fulu::Type::大明杠;
                for (const FuruTile& furu_tile : furu.tiles_) {
                    if (furu_tile.type_ == FuruTile::Type::DARK) {
                        fulu.type = Fulu::Type::暗杠;
                        break;
                    }
                    table.players[player_id_].门清 = false;
                    if (furu_tile.type_ == FuruTile::Type::ADD_KAN) {
                        fulu.type = Fulu::Type::加杠;
                        break;
                    }
                }
            }
            table.players[player_id_].副露s.emplace_back(std::move(fulu));
        }
    }

    bool DirectKan_(const std::string_view tile_sv) {
        assert(tile_sv.size() <= 3);
        const auto kiri_tile = GetTilesFrom(cur_round_kiri_info_.other_player_kiri_tiles_, tile_sv, errstr_);
        if (kiri_tile.empty()) {
            errstr_ = "前巡舍牌中不存在「"s + tile_sv.data() + "」";
            return false;
        }
        const std::string tile_str_simple = basetile_to_string_simple(kiri_tile.begin()->tile);
        const bool kan_tsumo = MatchTsumo_(tile_sv);
        const auto tiles = kan_tsumo ? GetTilesFrom(hand_, tile_str_simple + tile_str_simple, errstr_)
                                     : GetTilesFrom(hand_, tile_str_simple + tile_str_simple + tile_str_simple, errstr_);
        if (tiles.empty()) {
            cur_round_kiri_info_.other_player_kiri_tiles_.insert(kiri_tile.begin(), kiri_tile.end());
            errstr_ = "您的手牌中不存在足够的「"s + tile_sv.data() + "」";
            return false;
        }
        // kan
        assert(kiri_tile.size() == 1);
        assert(is_杠({tiles.begin()->tile, std::next(tiles.begin())->tile, std::next(tiles.begin(), 2)->tile, kiri_tile.begin()->tile}));
        furus_.emplace_back();
        auto& furu = furus_.back();
        furu.nari_round_ = round_;
        furu.tiles_[0] = FuruTile{*tiles.begin(), FuruTile::Type::NORMAL};
        furu.tiles_[1] = FuruTile{*std::next(tiles.begin()), FuruTile::Type::NORMAL};
        furu.tiles_[2] = FuruTile{kan_tsumo ? *tsumo_ : *std::next(tiles.begin(), 2), FuruTile::Type::NORMAL};
        furu.tiles_[3] = FuruTile{*kiri_tile.begin(), FuruTile::Type::NARI};
        return true;
    }

    static std::vector<BaseTile> GetListenTiles_(const TileSet& hand, const std::optional<Tile>& tsumo)
    {
        std::vector<BaseTile> ret;
        std::vector<BaseTile> hand_basetiles;
        for (const Tile& tile : hand) {
            hand_basetiles.emplace_back(tile.tile);
        }
        if (tsumo.has_value()) {
            hand_basetiles.emplace_back(tsumo->tile);
        }
        std::ranges::sort(hand_basetiles);
        for (uint8_t basetile = 0; basetile < 9 * 3 + 7; ++basetile) {
            if (4 == std::count(hand_basetiles.begin(), hand_basetiles.end(), basetile)) {
                continue; // all same tiles are in hand
            }
            hand_basetiles.emplace_back(static_cast<BaseTile>(basetile));
            if (is和牌(hand_basetiles)) {
                ret.emplace_back(static_cast<BaseTile>(basetile));
            }
            hand_basetiles.pop_back();
        }
        return ret;
    }

    std::vector<BaseTile> GetListenTiles_() const { return GetListenTiles_(hand_, tsumo_); }

    int32_t CurrentRoundChiTileCount_(const Tile& kiri_tile) const
    {
        return std::ranges::count_if(furus_, [&](const Furu& furu)
                {
                    return furu.nari_round_ == round_ && IsChi_(furu.tiles_) && NariTile_(furu.tiles_) == kiri_tile;
                });
    }

    static bool IsKiriedTileMatch_(const KiriTile& kiri_tile_info, const Tile& tile)
    {
        return kiri_tile_info.tile_.tile == tile.tile &&
            (kiri_tile_info.type_ == KiriTile::Type::NORMAL || kiri_tile_info.type_ == KiriTile::Type::RIVER_BOTTOM);
    }

    int32_t CurrentRoundCanFromChiPlayersKiriedTileCount_(const Tile& kiri_tile) const
    {
        uint32_t can_from_chi_kiri_num = 0;
        for (const auto& player_kiri_info : cur_round_kiri_info_.other_players_) {
            if (from_chi_players_[player_kiri_info.player_id_]) {
                can_from_chi_kiri_num += std::ranges::count_if(player_kiri_info.kiri_tiles_,
                        std::bind(&SyncMahjongGamePlayer::IsKiriedTileMatch_, std::placeholders::_1, kiri_tile));
            }
        }
        return can_from_chi_kiri_num;
    }

    void UpdateFromChiPlayers_(const Tile& kiri_tile)
    {
        for (const auto& player_kiri_info : cur_round_kiri_info_.other_players_) {
            if (from_chi_players_[player_kiri_info.player_id_] &&
                    std::ranges::none_of(player_kiri_info.kiri_tiles_,
                        std::bind(&SyncMahjongGamePlayer::IsKiriedTileMatch_, std::placeholders::_1, kiri_tile))) {
                from_chi_players_.reset(player_kiri_info.player_id_);
            }
        }
    }

    bool CheckFromChiPlayer_(const Tile& kiri_tile)
    {
        if (CurrentRoundChiTileCount_(kiri_tile) >= CurrentRoundCanFromChiPlayersKiriedTileCount_(kiri_tile)) {
            return false;
        }
        UpdateFromChiPlayers_(kiri_tile);
        return true;
    }

    void KiriAI_()
    {
        const auto basetile_to_index = [](const int32_t basetile)
            {
                return basetile >= _1m && basetile <= _9m ? 9 * 0 + basetile - _1m :
                       basetile >= _1s && basetile <= _9s ? 9 * 1 + basetile - _1s :
                       basetile >= _1p && basetile <= _9p ? 9 * 2 + basetile - _1p : 9 * 3 + basetile - east;
            };
        static CalshtDW calsht;
        static std::once_flag once_flag;
        std::call_once(once_flag, []() { return calsht.initialize(); });
        std::vector<int> calsht_hand(k_tile_type_num, 0);
        for (const Tile& tile : hand_) {
            ++calsht_hand[basetile_to_index(tile.tile)];
        }
        if (tsumo_.has_value()) {
            ++calsht_hand[basetile_to_index(tsumo_->tile)];
        }
        const std::bitset<k_tile_type_num> disc_bitset(std::get<2>(calsht(calsht_hand, 4 - furus_.size(), 7)));
        BaseTile kiri_basetile = hand_.begin()->tile;
        int32_t min_sht = INT32_MAX;
        int32_t max_wait_tiles_num = 0;
        for (int32_t basetile = 0; basetile < k_tile_type_num; ++basetile) {
            if (disc_bitset[basetile_to_index(basetile)]) {
                --calsht_hand[basetile_to_index(basetile)];
                const auto [kiri_sht, _1, _2, wait_tiles] = calsht(calsht_hand, 4 - furus_.size(), 7);
                const auto wait_tiles_num = std::bitset<k_tile_type_num>(wait_tiles).count();
                if (kiri_sht < min_sht) {
                    kiri_basetile = static_cast<BaseTile>(basetile);
                    min_sht = kiri_sht;
                    max_wait_tiles_num = wait_tiles_num;
                } else if (kiri_sht == min_sht && wait_tiles_num > max_wait_tiles_num) {
                    kiri_basetile = static_cast<BaseTile>(basetile);
                    max_wait_tiles_num = wait_tiles_num;
                }
                ++calsht_hand[basetile_to_index(basetile)];
            }
        }
        if (kiri_basetile == BaseTile::north && Kita()) {
            return;
        }
        const bool ret = Kiri(basetile_to_string_simple(kiri_basetile), true) ||
            Kiri(basetile_to_string_simple(kiri_basetile), false);
        if (!ret) {
            std::cerr << "KiriAI_ failed: " << errstr_ << std::endl;
            assert(false);
        }
    }

    static constexpr const uint32_t k_none_player_id_ = UINT32_MAX;

    const std::string image_path_;
    const std::vector<PlayerDesc> player_descs_;

    const uint32_t player_id_{UINT32_MAX};
    const Wind wind_{Wind::East};
    const std::vector<Tile> yama_;
    int32_t point_variation_{0};

    size_t yama_idx_{0};
    TileSet hand_;
    std::optional<Tile> tsumo_;
    std::vector<RiverTile> river_;
    std::vector<Furu> furus_; // do not use array because there may be nuku pei
    std::bitset<k_max_player> from_chi_players_{
        player_descs_.size() == 4 ? ((1U << k_max_player) - 1) & ~(1U << player_id_) : 0};
    ActionState state_{ActionState::ROUND_OVER};

    int32_t round_{0};
    PlayerKiriInfo cur_round_my_kiri_info_;
    KiriInfo cur_round_kiri_info_;

    bool is_double_richii_{false};
    bool is_richii_furutin_{false};
    int32_t richii_round_{0};
    std::vector<BaseTile> richii_listen_tiles_;

    std::vector<FuResult> fu_results_;

    DorasManager doras_manager_;

    std::string errstr_;
    const char* big_text_{nullptr};
    std::string public_html_;

    bool auto_options_[3] = {false, false, false};
};

class SyncMajong
{
  public:
    SyncMajong(const SyncMahjongOption& option) : benchang_(option.benchang_), richii_points_(option.richii_points_)
    {
        const auto get_tiles = [&]()
            {
                std::array<Tile, k_tile_type_num * 4> tiles{BaseTile::_1m};
#ifdef TEST_BOT
                if (const TilesOption* const tiles_option = std::get_if<TilesOption>(&option.tiles_option_)) {
                    ShuffleTiles(*tiles_option, tiles);
                } else {
                    GenerateTilesFromDecodedString(std::get<std::string>(option.tiles_option_), tiles);
                }
#else
                ShuffleTiles(option.tiles_option_, tiles);
#endif
                return tiles;
            };
        AssignTiles_(option, get_tiles());
        ++round_;
        for (auto& player : players_) {
            ++player.round_;
        }
        StartNormalStage_();
    }

    enum class RoundOverResult { CHUTO_NAGASHI_三家和了, CHUTO_NAGASHI_九种九牌, CHUTO_NAGASHI_四风连打, CHUTO_NAGASHI_四家立直, NYANPAI_NAGASHI, RON_ROUND, NORMAL_ROUND, FU };
    RoundOverResult RoundOver() {
        for (auto& player : players_) {
            player.PerformDefault();
            player.public_html_.clear();
        }
        UpdateDora_();
        {
            const Defer defer([&]
                    {
                        for (auto& player : players_) {
                            if (player.public_html_.empty()) {
                                player.public_html_ = player.Html_(SyncMahjongGamePlayer::HtmlMode::PUBLIC);
                            }
                        }
                    });
            if (Is_三家和了(players_)) {
                return RoundOverResult::CHUTO_NAGASHI_三家和了;
            }
            if (HandleFuResults_()) {
                return RoundOverResult::FU;
            }
            ron_stage_ = !ron_stage_;
            if (ron_stage_) {
                if (StartRonStage_()) {
                    return RoundOverResult::RON_ROUND;
                }
                // `fu_results_` can be filled due to `AUTO_FU`
                if (Is_三家和了(players_)) {
                    return RoundOverResult::CHUTO_NAGASHI_三家和了;
                }
                if (HandleFuResults_()) {
                    return RoundOverResult::FU;
                }
                ron_stage_ = false;
            }
            if (round_ == 1 && UpdatePublicHtmlFor_九种九牌(players_)) {
                return RoundOverResult::CHUTO_NAGASHI_九种九牌;
            }
            for (auto& player : players_) {
                if (player.richii_round_ == round_) {
                    player.point_variation_ -= 1000;
                    richii_points_ += 1000;
                }
            }
            if (round_ == 1 && Is_四风连打(players_)) {
                return RoundOverResult::CHUTO_NAGASHI_四风连打;
            }
            if (IsNyanpaiNagashi_()) {
                TryFillNagashiManganResult_();
                if (Is_三家和了(players_)) {
                    return RoundOverResult::CHUTO_NAGASHI_三家和了;
                }
                if (HandleFuResults_()) {
                    return RoundOverResult::FU;
                }
                HandleTinpaiForNyanpaiNagashi_(players_);
                return RoundOverResult::NYANPAI_NAGASHI;
            }
            ++round_;
            for (auto& player : players_) {
                ++player.round_;
            }
            if (Is_四家立直(players_)) {
                return RoundOverResult::CHUTO_NAGASHI_四家立直;
            }
        }
        StartNormalStage_();
        return RoundOverResult::NORMAL_ROUND;
    }

    auto& Players() { return players_; }
    const auto& Players() const { return players_; }

    int32_t Round() const { return round_; }

    int32_t RichiiPoints() const { return richii_points_; }

  private:
    void StartNormalStage_()
    {
        for (auto& player : players_) {
            player.StartNormalStage_();
        }
    }

    [[nodiscard]] bool StartRonStage_()
    {
        std::vector<game_util::mahjong::PlayerKiriInfo> kiri_infos;
        kiri_infos.reserve(players_.size());
        for (const auto& player : players_) {
            kiri_infos.emplace_back(player.cur_round_my_kiri_info_);
        }
        for (auto& player : players_) {
            player.StartRonStage_(kiri_infos);
        }
        return std::ranges::any_of(players_, [](const auto& player) { return player.State() == ActionState::NOTIFIED_RON; });
    }

    void HandleOneFuResult_(const lgtbot::game_util::mahjong::FuResult& fu_result, SyncMahjongGamePlayer& player)
    {
        if (fu_result.player_id_ == SyncMahjongGamePlayer::k_none_player_id_) {
            // tsumo
            const int score_each_player = fu_result.counter_.score1 + benchang_ * 100;
            std::ranges::for_each(players_, [score_each_player](auto& player) { player.point_variation_ -= score_each_player; });
            player.point_variation_ += score_each_player * players_.size();
        } else {
            // ron
            const int score = fu_result.counter_.score1 + benchang_ * (players_.size() - 1) * 100 / player.fu_results_.size();
            players_[fu_result.player_id_].point_variation_ -= score;
            player.point_variation_ += score;
        }
    }

    [[nodiscard]] bool HandleFuResults_()
    {
        const uint32_t fu_players_num = std::ranges::count_if(players_, [](const auto& player) { return !player.fu_results_.empty(); });
        if (fu_players_num == 0) {
            return false;
        }
        for (auto& player : players_) {
            for (const auto& fu_result : player.fu_results_) {
                HandleOneFuResult_(fu_result, player);
            }
            if (!player.fu_results_.empty()) {
                player.point_variation_ += richii_points_ / fu_players_num;
            }
        }
        richii_points_ = 0;
        return true;
    }

    void UpdateDora_()
    {
        if (std::ranges::any_of(players_, [round = round_](const auto& player) { return player.doras_manager_.HasOpenedThisRound(round); })) {
            std::ranges::for_each(players_, [round = round_](auto& player) { player.doras_manager_.TryOpenNewDora(round); });
        }
    }

    void AssignTiles_(const SyncMahjongOption& option, std::array<Tile, k_tile_type_num * 4>&& tiles)
    {
        if (option.player_descs_.size() == 3) {
            std::ranges::remove_if(tiles, [](const Tile& tile) { return tile.tile >= _2m && tile.tile <= _8m; });
        } else {
            assert(option.player_descs_.size() == 4);
        }
        size_t tile_idx = 0;

        std::array<std::pair<Tile, Tile>, 4> doras;
        for (auto& [dora, inner_dora] : doras) {
            dora = tiles[tile_idx++];
            inner_dora = tiles[tile_idx++];
        }

        players_.reserve(option.player_descs_.size());
        for (uint32_t player_id = 0; player_id < option.player_descs_.size(); ++player_id) {
            const auto yama_tiles_num = option.player_descs_.size() == 3 ? 20 : 19;
            std::vector<Tile> yama_tiles;
            yama_tiles.reserve(yama_tiles_num);
            TileSet hand;
            for (uint32_t i = 0; i < yama_tiles_num; ++i) {
                yama_tiles.emplace_back(tiles[tile_idx++]);
            }
            for (uint32_t i = 0; i < k_hand_tile_num; ++i) {
                hand.emplace(tiles[tile_idx++]);
            }
            players_.emplace_back(option.image_path_, option.player_descs_, player_id, std::move(yama_tiles), std::move(hand), doras);
        }

        if (option.player_descs_.size() == 4) {
            assert(tile_idx == tiles.size());
        }
    }

    static bool Is_三家和了(const std::vector<SyncMahjongGamePlayer>& players)
    {
        return std::ranges::count_if(players, [](const SyncMahjongGamePlayer& player) { return !player.fu_results_.empty(); }) >= 3;
    }

    static bool Is_四家立直(const std::vector<SyncMahjongGamePlayer>& players)
    {
        return std::ranges::count_if(players, [](const SyncMahjongGamePlayer& player) -> bool { return !player.richii_listen_tiles_.empty(); }) == 4;
    }

    // should only check for the first round
    static bool UpdatePublicHtmlFor_九种九牌(std::vector<SyncMahjongGamePlayer>& players)
    {
        bool found = false;
        for (auto& player : players) {
            if (player.river_.empty() && player.fu_results_.empty()) {
                player.big_text_ = BIG_TEXT(green, "**九&nbsp;&nbsp;种&nbsp;&nbsp;九&nbsp;&nbsp;牌**");
                player.public_html_ = player.Html_(SyncMahjongGamePlayer::HtmlMode::OPEN);
                found = true;
            }
        }
        return found;
    }

    // should only check for the first round
    static bool Is_四风连打(const std::vector<SyncMahjongGamePlayer>& players)
    {
        if (players.size() != k_max_player) {
            return false;
        }
        const BaseTile first_player_kiri_basetile = players.begin()->cur_round_my_kiri_info_.kiri_tiles_.begin()->tile_.tile;
        const auto kiri_basetile_equal = [&](const SyncMahjongGamePlayer& player) -> bool
            {
                const auto& kiri_tiles = player.cur_round_my_kiri_info_.kiri_tiles_;
                return kiri_tiles.size() == 1 && kiri_tiles.begin()->tile_.tile == first_player_kiri_basetile;
            };
        static constexpr const std::array<BaseTile, 4> k_wind_tiles{east, south, west, north};
        return std::ranges::find(k_wind_tiles, first_player_kiri_basetile) != k_wind_tiles.end() &&
            std::all_of(std::next(players.begin()), players.end(), kiri_basetile_equal);
    }

    void TryFillNagashiManganResult_()
    {
        for (auto& player : players_) {
            if (std::ranges::all_of(player.river_, [](const RiverTile& river_tile)
                        {
                            return std::find(k_one_nine_tiles.begin(), k_one_nine_tiles.end(), river_tile.tile_.tile) != k_one_nine_tiles.end();
                        })) {
                CounterResult counter;
                counter.yakus.emplace_back(Yaku::流局满贯);
                counter.score1 = 4000;
                player.fu_results_.emplace_back(SyncMahjongGamePlayer::k_none_player_id_, std::move(counter), Tile{});
            }
        }
    }

    static void HandleTinpaiForNyanpaiNagashi_(std::vector<SyncMahjongGamePlayer>& players)
    {
        const int32_t nyanpai_tinpai_points = 1000 * (players.size() - 1);
        uint32_t tinpai_player_num = 0;
        for (auto& player : players) {
            if (player.GetListenTiles_().empty()) {
                player.big_text_ = BIG_TEXT(red, "**未&nbsp;&nbsp;听&nbsp;&nbsp;牌**");
            } else {
                ++tinpai_player_num;
                player.big_text_ = BIG_TEXT(green, "**听&nbsp;&nbsp;牌**");
            }
        }
        const Defer defer([&]
                {
                    for (auto& player : players) {
                        if (!player.GetListenTiles_().empty()) {
                            player.public_html_ = player.Html_(SyncMahjongGamePlayer::HtmlMode::OPEN);
                        }
                    }
                });
        if (tinpai_player_num == 0 || tinpai_player_num == players.size()) {
            return;
        }
        for (auto& player : players) {
            if (player.GetListenTiles_().empty()) {
                player.point_variation_ -= nyanpai_tinpai_points / (players.size() - tinpai_player_num);
            } else {
                player.point_variation_ += nyanpai_tinpai_points / tinpai_player_num;
            }
        }
    }

    bool IsNyanpaiNagashi_()
    {
        return std::ranges::any_of(players_,
                [](const SyncMahjongGamePlayer& player) { return player.yama_idx_ >= player.yama_.size(); });
    }

    int32_t benchang_{0};
    int32_t round_{0};
    bool ron_stage_{false};
    std::array<Tile, 4> pei_lingshang_; // only for 3 players
    std::vector<SyncMahjongGamePlayer> players_;
    int32_t richii_points_{0};

};

#undef BIG_TEXT

} // namespace mahjong

} // namespace game_util

} // namespace lgtbot
