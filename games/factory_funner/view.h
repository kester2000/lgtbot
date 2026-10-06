#pragma once
#include "match.h"
#include <cmath>
#include <filesystem>

namespace factory_funner {
inline const std::string kHelp =
    "允许的操作（直接发送，不加#；帮助 文字 查看文字版）：\n\n"
    "准备阶段：倒计时10秒后自动翻牌，无需发送指令。每轮开始群发所有玩家工厂的拼接总览图。\n\n"
    "抢选阶段：抢 1（牌池序号）；不抢（不可反悔）。最后一位自动领取剩余机器。默认1分钟。\n\n"
    "施工阶段：机 C3；绿 B2；黑 D4；管 B3 直管。先放置，再旋转调整。默认10分钟。\n\n"
    "罐子：白 c1；黑 d4；绿 b2。粉/黄/绿/蓝/白/黑后直接加坐标，坐标不区分大小写。\n\n"
    "管型：直管、弯管、急弯、三通、侧三通、扇三通、四通、侧四通、扇四通、五通、六通。叠放冲突时自动寻找可放置朝向。\n\n"
    "自动：按机器和管网需求补供料罐、白罐、黑罐，并将开口对准缺口；不放无需求的罐，每个新罐$1，可撤销。\n\n"
    "自动后发送检查查看剩余问题；没有空位、已有同色罐需连线或达到白罐上限的缺口保留，已有组件保持原位。\n\n"
    "连续画线（头尾格不铺管，只铺中间格）：画线 A3 B3 C3；画线 A3B3C3；画线 A3→B3→C3；连线 A3B3C3；画管 A3B3C3；连 A3 B3 C3（相邻格自动弯管/分流/交叉）\n\n"
    "批量施工：方案 机 C4; 自动（一条消息整体执行，可一次撤销）\n\n"
    "旋转：旋转 C3（顺时针60度）；旋转 C3 3（连续三次）；旋转 C3 逆时针 2；转 编号。管道遇冲突时继续尝试后续角度；没有其他合法朝向则保持原位。\n\n"
    "翻转：翻转 C3 或 翻 编号（仅管道）；原地翻转冲突时自动旋转寻找合法朝向。\n\n"
    "拆除：拆、拆除、移除、删、删除均支持；如 删除 16、拆 #16、拆 C3 或 拆 8 9 10 11 12 13。可一次拆多个部件，整条操作一次撤销；任一部件不能拆时全部不生效。交叉管道需按编号指定部件，标签颜色与管道对应。移动 C3 D3 或 移 组件编号 坐标。放置和移动遇占用时自动拆除冲突组件；本轮机器、同色供应罐重复放置会移动已有组件。往轮机器固定。\n\n"
    "撤销；重做；重置（恢复本轮开始布局）。修改仅在施工阶段且尚未提交时允许。\n\n"
    "随时查询：工厂（自己的详情图）；文字（文字棋盘和组件）；检查；机器 41；牌池；赛况（抢选阶段补牌池图，其他阶段补工厂总览，施工草稿不公开）；操作\n\n"
    "施工完成：提交 或 跳过；提交后锁定，全部完成才统一记账\n\n"
    "超时确认阶段：保留（保留冻结的有效草稿）或 跳过\n\n"
    "坐标：A–G 为列，1–7 为行；偶数行向左错半格。\n\n"
    "白/黑罐放置后用旋转调整开口；供应罐六面可接。连续画线自动选择管型及朝向。";

inline std::string MachineText(int id, int rot = 0) {
    const auto& m = MachineById(id);
    std::string s = "M" + std::to_string(id) + " " + m.name + " 收入$" + std::to_string(m.revenue);
    for (auto p : m.ports) s += "\n  " + std::string(p.input ? "输入 · " : "输出 · ") + Direction(p.edge+rot) + " " + ColorName(p.colors) + "×" + std::to_string(p.amount);
    return s;
}
inline std::string FactoryText(const Factory& f) {
    std::ostringstream out;
    out << "第" << f.round << "轮 | 资金$" << f.money << " | 待付$" << f.Cost() << "\n";
    if (f.current) out << "本轮 " << MachineText(f.current) << "\n";
    out << "列 A B C D E F G（##墙，..空；偶数行向左错半格）\n";
    for (int r = 0; r < 7; ++r) {
        out << r + 1 << (r % 2 ? " " : "   ");
        for (int q = 0; q < (r % 2 ? 7 : 6); ++q) {
            const auto it = std::ranges::find(f.pieces, Cell{q,r}, &Piece::cell);
            out << (!Playable(f.skin,{q,r}) ? "##" : it == f.pieces.end() ? ".." : Label(*it)) << " ";
        }
        out << "\n";
    }
    for (const auto& p : f.pieces) {
        out << "#" << p.id << " " << Coord(p.cell) << " " << Label(p);
        if (p.kind == Kind::Machine) out << " " << (p.round < f.round ? "固定 " : "") << MachineText(p.machine,p.rot);
        else if (p.kind == Kind::Pipe) { out << " 接口："; for (auto port : Ports(p)) out << Direction(port.edge) << " "; }
        else if (p.kind != Kind::Supply) out << " 开口朝" << Direction(p.rot);
        out << "\n";
    }
    return out.str();
}
inline std::string EscapeHtml(const std::string& text) {
    std::string out;
    for (char c : text) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            default: out += c;
        }
    }
    return out;
}
inline std::string AssetUri(const std::string& dir, const std::string& filename) {
    const auto path = std::filesystem::absolute(std::filesystem::path(dir) / filename).generic_string();
    std::string out = path.starts_with('/') ? "file://" : "file:///";
    constexpr char digits[] = "0123456789ABCDEF";
    for (unsigned char c : path) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
            c == '/' || c == ':' || c == '-' || c == '_' || c == '.' || c == '~') out += c;
        else { out += '%'; out += digits[c >> 4]; out += digits[c & 15]; }
    }
    return out;
}
inline std::string MachineImageName(int id) {
    return "Toke_Factory_Funner_Machine_Tiles_" + std::string(id < 10 ? "0" : "") + std::to_string(id) + "_a0s0.png";
}
inline std::map<int,int> PipeLiquidColors(const Factory& factory) {
    struct Node { const Piece* piece; Port port; };
    std::vector<Node> nodes; std::vector<int> parent;
    std::map<std::pair<Cell,int>,int> ends;
    std::function<int(int)> root=[&](int i) { return parent[i]==i?i:parent[i]=root(parent[i]); };
    auto join=[&](int a,int b) { parent[root(a)]=root(b); };
    for(const auto& piece:factory.pieces) {
        const int first=nodes.size();
        for(const auto port:Ports(piece)) {
            const int i=nodes.size(); nodes.push_back({&piece,port}); parent.push_back(i);
            ends.emplace(std::make_pair(piece.cell,port.edge),i);
            if(piece.kind!=Kind::Machine && i!=first) join(first,i);
        }
    }
    for(int i=0;i<int(nodes.size());++i) {
        const auto& n=nodes[i]; auto other=ends.find({Neighbor(n.piece->cell,n.port.edge),Mod(n.port.edge+3)});
        if(other!=ends.end() && !(n.port.kind==PortKind::Source && nodes[other->second].port.kind==PortKind::Source)) join(i,other->second);
    }
    std::map<int,int> networks, result;
    for(int i=0;i<int(nodes.size());++i) {
        auto [it,inserted]=networks.try_emplace(root(i),31);
        if(nodes[i].port.colors) it->second &= nodes[i].port.colors;
    }
    for(int i=0;i<int(nodes.size());++i) if(nodes[i].piece->kind==Kind::Pipe) {
        const int colors=networks.at(root(i));
        result[nodes[i].piece->id]=std::popcount(unsigned(colors))==1?colors:0;
    }
    return result;
}
inline const char* LiquidColor(int color) {
    switch(color) {
        case 1:return "#cc397d"; case 2:return "#edc83d"; case 4:return "#23865b";
        case 8:return "#167da9"; case 16:return "#263942"; default:return "#85939d";
    }
}
inline std::string FactoryHtml(const Factory& f, const std::string& resources = "games/factory_funner/resource", const std::string& player = "", bool compact = false, std::optional<BoardEstimate> estimate = std::nullopt) {
    // Qt WebKit supports SVG 1.1 xlink:href, including local artwork.
    std::ostringstream s;
    std::ostringstream labels;
    const auto liquid_colors=PipeLiquidColors(f);
    s << "<div style=\"font-family:'Microsoft YaHei',sans-serif;background:#f4f1e6;padding:12px;width:" << (compact ? 600 : 965) << "px\">";
    s << "<h3>" << EscapeHtml(player) << " · 第" << f.round << "轮</h3>";
    auto figures=estimate.value_or(BoardEstimate{});
    if(!estimate) {
        auto baseline=f; baseline.pieces=f.baseline;
        figures.cash=f.money; figures.current_chain=baseline.Validate().bonus;
        figures.machine_revenue=f.current?MachineById(f.current).revenue:0;
        figures.parts_cost=f.Cost(); figures.new_chain=f.Validate().bonus-figures.current_chain;
        figures.income=figures.machine_revenue-figures.parts_cost;
        figures.total=figures.cash+figures.current_chain+figures.machine_revenue-figures.parts_cost+figures.new_chain;
        figures.status=f.Validate().Text();
        if(f.current && !f.HasCurrent()) figures.status="尚未安装本轮机器；"+figures.status;
    }
    s << "<p style=\"font-size:18px;font-weight:bold\">当前分数：" << figures.cash+figures.current_chain << "（当前现金 $" << figures.cash << " + 当前连锁得分 " << figures.current_chain << "）</p>";
    s << "<p style=\"font-size:15px\">本轮预估收益：" << figures.income+figures.new_chain << "（本轮预估机器收益 $" << figures.machine_revenue << " − 本轮零件支出 $" << figures.parts_cost << " + 新加连锁得分 " << figures.new_chain;
    if(figures.adjustment) s << " + 首末抢调整 " << figures.adjustment;
    s << "）</p>";
    s << "<p style=\"font-size:14px\">当前情况：" << EscapeHtml(figures.status) << "</p>";
    s << "<svg xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" width=\"" << (compact ? 600 : 965) << "\" height=\"" << (compact ? 548 : 880) << "\" viewBox=\"0 0 965 880\">";
    s << "<defs><clipPath id=\"machine-clip\"><polygon points=\"66.68,38.5 0,77 -66.68,38.5 -66.68,-38.5 0,-77 66.68,-38.5\"/></clipPath><clipPath id=\"tank-clip\"><circle r=\"64\"/></clipPath></defs>";
    const std::array<const char*,6> skins = {"Cian","Green","Red","Violet","White","Yellow"};
    s << "<image xlink:href=\"" << AssetUri(resources,"Boar_Factory_Funner_pBoard_A_" + std::string(skins[f.skin]) + "_a0s0.png") << "\" width=\"965\" height=\"880\"/>";
    for (int r = 0; r < 7; ++r) for (int q = 0; q < (r % 2 ? 7 : 6); ++q) {
        Cell cell{q,r}; double x = 144 + 135*q - (r%2 ? 67.5 : 0), y = 89 + 117*r;
        if (!Playable(f.skin,cell)) continue;
        std::vector<const Piece*> occupants;
        for (const auto& p : f.pieces) if (p.cell == cell) occupants.push_back(&p);
        const std::array<const char*,3> unknown_pipes = {"#85939d","#bec6cb","#65727e"};
        for (size_t i = 0; i < occupants.size(); ++i) {
            const auto& p = *occupants[i];
            const int liquid=p.kind==Kind::Pipe?liquid_colors.at(p.id):0;
            const char* pipe_color=liquid?LiquidColor(liquid):unknown_pipes[i%3];
            if (p.kind != Kind::Pipe) {
                s << "<g transform=\"translate(" << x << " " << y << ")\">";
                if (p.kind == Kind::Machine) {
                    s << "<g clip-path=\"url(#machine-clip)\"><image xlink:href=\"" << AssetUri(resources,MachineImageName(p.machine))
                      << "\" x=\"-78\" y=\"-67.5\" width=\"156\" height=\"135\" transform=\"rotate(" << 30+p.rot*60 << ")\"/></g>";
                } else {
                    const char* color = p.color == 1 ? "red" : p.color == 2 ? "yellow" : p.color == 4 ? "green" : "blue";
                    const std::string name = p.kind == Kind::Supply ? "Toke_Factory_Funner_Reservoirs_Supply-" + std::string(color) : p.kind == Kind::Black ? "Toke_Factory_Funner_Reservoir_b" : "Toke_Factory_Funner_Reservoirs";
                    s << "<g clip-path=\"url(#tank-clip)\"><image xlink:href=\"" << AssetUri(resources,name+"_a0s0.png") << "\" x=\"-53\" y=\"-64\" width=\"106\" height=\"116\" transform=\"rotate(" << 90+p.rot*60 << ")\"/></g>";
                }
                s << "</g>";
            }
            for (auto port : Ports(p)) {
                double a = port.edge * 3.141592653589793 / 3, dx = std::cos(a), dy = std::sin(a);
                if (p.kind == Kind::Pipe) {
                    // Thick outline makes independently crossing pipes visible.
                    s << "<line x1=\"" << x << "\" y1=\"" << y << "\" x2=\"" << x+67.5*dx << "\" y2=\"" << y+67.5*dy << "\" stroke=\"#243e3e\" stroke-width=\"18\"/>";
                    s << "<line x1=\"" << x << "\" y1=\"" << y << "\" x2=\"" << x+67.5*dx << "\" y2=\"" << y+67.5*dy << "\" stroke=\"" << pipe_color << "\" stroke-width=\"10\"/>";
                }
            }
            labels << "<rect x=\"" << x-38 << "\" y=\"" << y+12+int(i)*14 << "\" width=\"76\" height=\"14\" fill=\"" << (p.kind==Kind::Pipe?pipe_color:"#fff9e8") << "\" stroke=\"white\" stroke-width=\"1\"/>";
            labels << "<text x=\"" << x << "\" y=\"" << y+23+int(i)*14 << "\" text-anchor=\"middle\" font-size=\"11\" font-weight=\"bold\" fill=\"" << (p.kind==Kind::Pipe && liquid!=2 && (liquid || i%3!=1)?"white":"#182a3d") << "\">" << Label(p) << " #" << p.id << (p.kind == Kind::Machine && p.round < f.round ? " 固定" : "") << "</text>";
        }
    }
    // Draw coordinates last so rotated machines and tanks cannot hide them.
    for (int r = 0; r < 7; ++r) for (int q = 0; q < (r % 2 ? 7 : 6); ++q) {
        if (!Playable(f.skin,{q,r})) continue;
        const double x = 144 + 135*q - (r%2 ? 67.5 : 0), y = 89 + 117*r;
        s << "<rect x=\"" << x-17 << "\" y=\"" << y-31 << "\" width=\"34\" height=\"17\" rx=\"4\" fill=\"#fff9e8\"/>";
        s << "<text x=\"" << x << "\" y=\"" << y-18 << "\" text-anchor=\"middle\" font-size=\"13\" fill=\"#283b37\">" << Coord({q,r}) << "</text>";
    }
    // All component labels go above all board artwork, pipes and coordinates.
    s << "<g class=\"component-labels\">" << labels.str() << "</g>";
    if (compact) {
        s << "</svg>";
        if (f.current) s << "<p>本轮机器：<img src=\"" << AssetUri(resources,MachineImageName(f.current))
            << "\" width=\"100\" style=\"vertical-align:middle\"/></p><pre>" << MachineText(f.current) << "</pre>";
        s << "</div>"; return s.str();
    }
    s << "</svg><p>使用旋转、翻转调整朝向。交叉管道各自独立。</p>";
    s << "<p>连 A3 B3 C3 · 撤销 · 检查 · 提交 · 文字（查看完整端口）</p>";
    s << "<div style=\"font-size:16px\">";
    if (f.current && !f.HasCurrent()) s << "<p><img src=\"" << AssetUri(resources,MachineImageName(f.current)) << "\" width=\"140\" style=\"vertical-align:middle\"/>待安装（原始方向）：</p><pre>" << MachineText(f.current) << "</pre>";
    for (const auto& p : f.pieces) if (p.kind == Kind::Machine)
        s << "<pre>" << Coord(p.cell) << " #" << p.id << (p.round < f.round ? " 固定 " : " 本轮 ") << MachineText(p.machine,p.rot) << "</pre>";
    s << "</div></div>";
    return s.str();
}
inline std::string FactoriesHtml(const std::vector<std::pair<std::string,Factory>>& players, const std::string& resources, const std::vector<BoardEstimate>& estimates = {}) {
    std::ostringstream s;
    s << "<h2>本轮工厂总览</h2><table cellspacing=\"8\" cellpadding=\"0\">";
    for (size_t i = 0; i < players.size(); ++i) {
        if (i % 2 == 0) s << "<tr>";
        s << "<td style=\"vertical-align:top\">" << FactoryHtml(players[i].second,resources,players[i].first,true,i<estimates.size()?std::optional<BoardEstimate>(estimates[i]):std::nullopt) << "</td>";
        if (i % 2 == 1 || i + 1 == players.size()) s << "</tr>";
    }
    s << "</table>";
    return s.str();
}
}
