// Native port of FactoryFunner's engine and routing. See PORTING.md.
#pragma once
#include "machines.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cctype>
#include <functional>
#include <map>
#include <numeric>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <tuple>

namespace factory_funner {
inline void Require(bool ok, const std::string& message) { if (!ok) throw std::runtime_error(message); }
inline int Mod(int n) { return (n % 6 + 6) % 6; }
inline std::string Direction(int edge) {
    static const std::array<std::string,6> names={"右","右下","左下","左","左上","右上"};
    return names[Mod(edge)];
}
inline int RotatedEdges(int edges,int steps) {
    int result=0; for(int e=0;e<6;++e) if(edges & (1<<e)) result |= 1<<Mod(e+steps); return result;
}
inline int PipeShape(const std::string& name) {
    static const std::map<std::string,int> shapes={
        {"直管",9},{"弯管",5},{"急弯",3},{"三通",21},{"侧三通",11},{"扇三通",7},
        {"四通",27},{"侧四通",23},{"扇四通",15},{"五通",31},{"六通",63}};
    auto it=shapes.find(name); Require(it!=shapes.end(),"管型：直管、弯管、急弯、三通、侧三通、扇三通、四通、侧四通、扇四通、五通、六通；也可直接画线"); return it->second;
}
struct Cell {
    int q = 0, r = 0;
    auto operator<=>(const Cell&) const = default;
};
inline Cell Neighbor(Cell c, int e)
{
    const bool odd = c.r % 2 != 0;
    switch (Mod(e)) {
        case 0: return {c.q + 1, c.r};
        case 1: return {c.q + !odd, c.r + 1};
        case 2: return {c.q - odd, c.r + 1};
        case 3: return {c.q - 1, c.r};
        case 4: return {c.q - odd, c.r - 1};
        default: return {c.q + !odd, c.r - 1};
    }
}
inline int EdgeBetween(Cell a, Cell b) {
    for (int e = 0; e < 6; ++e) if (Neighbor(a, e) == b) return e;
    throw std::runtime_error("路径必须经过相邻六边形");
}
inline Cell ParseCell(std::string s) {
    Require(s.size() == 2, "坐标格式为 A1 至 G7（字母列、数字行）");
    s[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(s[0])));
    Require(s[0] >= 'A' && s[0] <= 'G' && s[1] >= '1' && s[1] <= '7', "坐标超出 A1 至 G7");
    return {s[0] - 'A', s[1] - '1'};
}
inline std::string Coord(Cell c) { return std::string(1, 'A' + c.q) + std::to_string(c.r + 1); }
inline const std::array<std::string, 6> kSkins = {"青", "绿", "红", "紫", "白", "黄"};
inline bool Playable(int skin, Cell c) {
    static const std::array<std::array<Cell, 3>, 6> walls = {{
        {{{3,6},{4,6},{5,6}}}, {{{5,0},{6,1},{6,5}}}, {{{6,1},{6,3},{6,5}}},
        {{{5,0},{6,1},{0,6}}}, {{{0,0},{0,1},{6,5}}}, {{{4,0},{5,0},{6,1}}}
    }};
    return skin >= 0 && skin < 6 && c.r >= 0 && c.r < 7 && c.q >= 0 && c.q < (c.r % 2 ? 7 : 6) &&
        c != Cell{3,3} && std::ranges::find(walls[skin], c) == walls[skin].end();
}
inline std::string ColorName(int colors) {
    std::string out;
    const std::array<std::string, 5> names = {"粉", "黄", "绿", "蓝", "黑"};
    for (int i = 0; i < 5; ++i) if (colors & (1 << i)) { if (!out.empty()) out += "/"; out += names[i]; }
    return out;
}
inline const Machine& MachineById(int id) {
    Require(id >= 1 && id <= 48, "未知机器编号"); return kMachines[id - 1];
}
enum class Kind { Machine, Pipe, Supply, White, Black };
struct Piece {
    int id = 0;
    Kind kind = Kind::Pipe;
    Cell cell;
    int rot = 0, edges = 0, color = 0, machine = 0, round = 0;
    bool operator==(const Piece&) const = default;
};
enum class PortKind { Input, Output, Pipe, Source, Sink };
struct Port { int edge, colors, amount; PortKind kind; };
inline std::vector<Port> Ports(const Piece& p) {
    std::vector<Port> out;
    if (p.kind == Kind::Machine) {
        for (auto x : MachineById(p.machine).ports)
            out.push_back({Mod(x.edge + p.rot), x.colors, x.amount, x.input ? PortKind::Input : PortKind::Output});
    } else if (p.kind == Kind::Pipe) {
        for (int e = 0; e < 6; ++e) if (p.edges & (1 << e)) out.push_back({e, 0, 0, PortKind::Pipe});
    } else if (p.kind == Kind::Supply) {
        for (int e = 0; e < 6; ++e) out.push_back({e, p.color, 0, PortKind::Source});
    } else out.push_back({p.rot, p.kind == Kind::Black ? 16 : 0, 0, PortKind::Sink});
    return out;
}
inline std::string Label(const Piece& p) {
    if (p.kind == Kind::Machine) return "M" + std::to_string(p.machine);
    if (p.kind == Kind::Supply) return ColorName(p.color) + "供";
    if (p.kind == Kind::White) return "白罐";
    if (p.kind == Kind::Black) return "黑罐";
    return "管";
}
struct Validation {
    std::vector<std::string> issues;
    int bonus = 0;
    bool Valid() const { return issues.empty(); }
    std::string Text() const {
        std::string s = Valid() ? "连接有效" : "连接未通过";
        for (const auto& x : issues) s += "\n• " + x;
        return s + "\n当前连锁奖励：" + std::to_string(bonus);
    }
};
struct Factory {
    int skin = 0, round = 1, money = 10, current = 0, next_id = 1;
    std::vector<Piece> pieces, baseline;

    int Cost() const {
        using Sig = std::tuple<Kind, Cell, int>;
        auto signature = [](const Piece& p) -> Sig {
            return {p.kind, p.cell, p.kind == Kind::Pipe ? p.edges : p.kind == Kind::Supply ? p.color : p.rot};
        };
        std::multiset<Sig> old;
        for (const auto& p : baseline) if (p.kind != Kind::Machine) old.insert(signature(p));
        int cost = 0;
        for (const auto& p : pieces) if (p.kind != Kind::Machine) {
            auto it = old.find(signature(p));
            if (it == old.end()) ++cost; else old.erase(it);
        }
        return cost;
    }
    void CheckPlacement(const Piece& p, int ignore = 0) const {
        Require(Playable(skin, p.cell), "这里是墙壁或棋盘外：" + Coord(p.cell));
        if (p.kind == Kind::Pipe) Require(p.edges > 0 && p.edges < 64 && std::popcount(unsigned(p.edges)) >= 2, "管道需要 2 至 6 个不同接口");
        if (p.kind == Kind::Supply) Require(p.color == 1 || p.color == 2 || p.color == 4 || p.color == 8, "供应罐只能为粉、黄、绿、蓝");
        int whites = 0;
        for (const auto& x : pieces) if (x.id != ignore) {
            if (x.cell == p.cell) {
                Require(p.kind == Kind::Pipe && x.kind == Kind::Pipe, "格子已被占用：" + Coord(p.cell));
                Require(!(p.edges & x.edges), "同格交叉管道不能共用接口");
            }
            if (p.kind == Kind::Machine) Require(x.kind != Kind::Machine || x.round != round, "本轮只能安装一台机器");
            if (p.kind == Kind::Supply) Require(x.kind != Kind::Supply || x.color != p.color, "每色只有一个供应罐，请移动已有罐");
            whites += x.kind == Kind::White;
        }
        Require(p.kind != Kind::White || whites < 3, "白罐最多三个");
    }
    int Place(Piece p, bool auto_rotate = false) {
        p.id = next_id; p.round = round; p.rot = Mod(p.rot);
        if (p.kind == Kind::Machine) { MachineById(current); p.machine = current; }
        if(auto_rotate && p.kind==Kind::Pipe) {
            const auto original=p; bool placed=false;
            for(int offset=0;offset<6;++offset) {
                p=original; p.rot=Mod(p.rot+offset); p.edges=RotatedEdges(p.edges,offset);
                try { CheckPlacement(p); placed=true; break; } catch(const std::runtime_error&) {}
            }
            Require(placed,"此格没有可放置该管型的朝向");
        } else CheckPlacement(p);
        pieces.push_back(p); return next_id++;
    }
    Piece& Get(int id) {
        auto it = std::ranges::find(pieces, id, &Piece::id);
        Require(it != pieces.end(), "组件编号不存在"); return *it;
    }
    void Editable(const Piece& p) const { Require(p.kind != Kind::Machine || p.round == round, "往轮机器已固定，不能移动、旋转或拆除"); }
    void Remove(int id) { Editable(Get(id)); std::erase_if(pieces, [id](auto& p) { return p.id == id; }); }
    void ClearCell(Cell cell, int ignore=0) {
        for(const auto& p:pieces) if(p.cell==cell && p.id!=ignore) Editable(p);
        std::erase_if(pieces,[&](const auto& p) { return p.cell==cell && p.id!=ignore; });
    }
    void Move(int id, Cell c) {
        Factory next=*this; auto p=next.Get(id); next.Editable(p); p.cell=c;
        try { next.CheckPlacement(p,id); }
        catch(const std::runtime_error&) { next.ClearCell(c,id); next.CheckPlacement(p,id); }
        next.Get(id)=p; *this=std::move(next);
    }
    void Put(Piece p) {
        Factory next=*this;
        for(const auto& old:pieces) if((p.kind==Kind::Machine && old.kind==Kind::Machine && old.round==round) ||
            (p.kind==Kind::Supply && old.kind==Kind::Supply && old.color==p.color)) {
            next.Move(old.id,p.cell); *this=std::move(next); return;
        }
        if(p.kind==Kind::Pipe) {
            try { next.Place(p,true); *this=std::move(next); return; } catch(const std::runtime_error&) {}
        }
        next.ClearCell(p.cell); next.Place(p,p.kind==Kind::Pipe); *this=std::move(next);
    }
    void Rotate(int id, int steps) {
        const auto original=Get(id); Editable(original);
        if(original.kind==Kind::Pipe && (steps==1 || steps==-1)) {
            for(int offset=1;offset<6;++offset) {
                auto candidate=original; candidate.rot=Mod(candidate.rot+steps*offset);
                candidate.edges=RotatedEdges(original.edges,steps*offset);
                if(candidate.edges==original.edges) continue;
                try { CheckPlacement(candidate,id); Get(id)=candidate; return; } catch(const std::runtime_error&) {}
            }
            throw std::runtime_error("没有其他合法旋转角度，管道保持原位");
        }
        auto p = Get(id); Editable(p); p.rot = Mod(p.rot + steps);
        if (p.kind == Kind::Pipe) {
            int edges = 0;
            for (int e = 0; e < 6; ++e) if (p.edges & (1 << e)) edges |= 1 << Mod(e + steps);
            p.edges = edges;
        }
        CheckPlacement(p, id); Get(id) = p;
    }
    void Flip(int id) {
        const auto original=Get(id); Require(original.kind==Kind::Pipe,"只有管道可以翻转");
        int reflected=0;
        for(int e=0;e<6;++e) if(original.edges & (1<<e)) reflected |= 1<<Mod(2*original.rot-e);
        for(int offset=0;offset<6;++offset) {
            auto candidate=original; candidate.rot=Mod(candidate.rot+offset); candidate.edges=RotatedEdges(reflected,offset);
            try { CheckPlacement(candidate,id); Get(id)=candidate; return; } catch(const std::runtime_error&) {}
        }
        throw std::runtime_error("翻转后没有合法朝向，管道保持原位");
    }
    bool HasCurrent() const { return std::ranges::any_of(pieces, [&](auto& p) { return p.kind == Kind::Machine && p.round == round; }); }
    Validation Validate() const {
        Validation result;
        auto issue = [&](const std::string& s) { if (std::ranges::find(result.issues, s) == result.issues.end()) result.issues.push_back(s); };
        struct Node { const Piece* piece; Port port; };
        std::vector<Node> nodes;
        std::vector<int> parent;
        std::map<std::pair<Cell,int>, int> ends;
        std::function<int(int)> root = [&](int i) { return parent[i] == i ? i : parent[i] = root(parent[i]); };
        auto join = [&](int a, int b) { parent[root(a)] = root(b); };
        for (const auto& p : pieces) {
            if (!Playable(skin, p.cell)) issue("组件超出棋盘：" + Coord(p.cell));
            const int first = nodes.size();
            for (auto port : Ports(p)) {
                int n = nodes.size(); nodes.push_back({&p, port}); parent.push_back(n);
                if (p.kind != Kind::Machine && n != first) join(first, n);
                if (!ends.emplace(std::make_pair(p.cell, port.edge), n).second) issue("同格管道共用接口：" + Coord(p.cell));
            }
        }
        for (int i = 0; i < int(nodes.size()); ++i) {
            const auto& n = nodes[i];
            auto other = ends.find({Neighbor(n.piece->cell, n.port.edge), Mod(n.port.edge + 3)});
            if (other == ends.end()) {
                if (n.port.kind != PortKind::Source) issue(Coord(n.piece->cell) + " #" + std::to_string(n.piece->id) + " " + Direction(n.port.edge) + "侧接口未连接");
            } else if (!(n.port.kind == PortKind::Source && nodes[other->second].port.kind == PortKind::Source)) join(i, other->second);
        }
        std::map<int,std::vector<int>> groups;
        for (int i = 0; i < int(nodes.size()); ++i) groups[root(i)].push_back(i);
        std::map<int,std::set<int>> deps;
        for (const auto& [_, group] : groups) {
            int allowed = 31, demand = 0, capacity = 0, sources = 0, sinks = 0;
            bool constrained = false, white = false;
            std::vector<int> inputs, outputs;
            std::string where = "管路（" + Coord(nodes[group.front()].piece->cell) + "）：";
            for (int i : group) {
                const auto& n = nodes[i];
                if (n.port.colors) { allowed &= n.port.colors; constrained = true; }
                if (n.port.kind == PortKind::Input) { inputs.push_back(i); demand += n.port.amount; }
                if (n.port.kind == PortKind::Output) { outputs.push_back(i); capacity += n.port.amount; }
                sources += n.port.kind == PortKind::Source; sinks += n.port.kind == PortKind::Sink;
                white |= n.piece->kind == Kind::White;
            }
            if (constrained && !allowed) issue(where + "混色或端口颜色不兼容");
            if (!sources && outputs.empty()) issue(where + "没有供应来源");
            if (inputs.empty() && !sinks) issue(where + "没有通往机器或回收罐");
            if (inputs.empty() && outputs.empty()) issue(where + "储罐和管道必须服务于机器");
            if (sources && !outputs.empty()) issue(where + "供应罐与机器输出不能合流");
            if (!sources && capacity < demand) issue(where + "流量不足，产出" + std::to_string(capacity) + "，需求" + std::to_string(demand));
            if (allowed == 16 && white) issue(where + "黑色产品必须接黑罐");
            for (int o : outputs) for (int i : inputs) deps[nodes[o].piece->id].insert(nodes[i].piece->id);
            if (!outputs.empty() && !sources && constrained && allowed && capacity >= demand) result.bonus += demand * 3;
        }
        std::map<int,int> marks;
        std::function<void(int)> visit = [&](int id) {
            if (marks[id] == 1) { issue("生产链存在循环，机器输出不能回流到自身（含经过其他机器）"); return; }
            if (marks[id] == 2) return;
            marks[id] = 1;
            for (int to : deps[id]) visit(to);
            marks[id] = 2;
        };
        for (const auto& [id, _] : deps) visit(id);
        return result;
    }
    void Settle(bool skip, int adjustment = 0) {
        if (skip) pieces = baseline;
        else { Require(HasCurrent(), "请先安装本轮机器，或发送 跳过"); auto v = Validate(); Require(v.Valid(), v.Text()); money += MachineById(current).revenue - Cost(); }
        money += adjustment; baseline = pieces;
    }
    void LayPath(const std::vector<Cell>& path) {
        Require(path.size() >= 2 && path.size() <= 45, "路径需要 2 至 45 格");
        Require(std::set<Cell>(path.begin(), path.end()).size() == path.size(), "路径不能重复经过同一格");
        Factory draft = *this;
        for (size_t i = 0; i < path.size(); ++i) {
            auto cell = path[i]; Require(Playable(skin, cell), "路径经过墙壁或棋盘外");
            int edges = 0;
            if (i) edges |= 1 << EdgeBetween(cell, path[i - 1]);
            if (i + 1 < path.size()) edges |= 1 << EdgeBetween(cell, path[i + 1]);
            if (i == 0 || i + 1 == path.size()) {
                bool occupied=false, connected=false;
                for (const auto& p : draft.pieces) if (p.cell==cell) {
                    occupied=true;
                    connected |= std::ranges::any_of(Ports(p),[&](auto port) { return edges & (1 << port.edge); });
                }
                Require(!occupied || connected,"路径没有对准头尾组件接口");
                continue;
            }
            std::vector<int> touched;
            bool fixed = false;
            for(const auto& p:draft.pieces) if(p.cell==cell && p.kind!=Kind::Pipe) draft.Editable(p);
            std::erase_if(draft.pieces,[&](const auto& p) { return p.cell==cell && p.kind!=Kind::Pipe; });
            for (const auto& p : draft.pieces) if (p.cell == cell) {
                if (p.kind != Kind::Pipe) {
                    Require(std::popcount(unsigned(edges)) == 1, "路径不能穿过机器或储罐内部");
                    Require(std::ranges::any_of(Ports(p), [&](auto port) { return edges & (1 << port.edge); }), "路径没有对准机器或储罐接口");
                    fixed = true;
                } else if (p.edges & edges) touched.push_back(p.id);
            }
            if (fixed) continue;
            Require(touched.size() <= 1, "不能合并同格的独立交叉管道");
            if (!touched.empty()) {
                auto p = draft.Get(touched[0]); p.edges |= edges; draft.CheckPlacement(p, p.id); draft.Get(p.id) = p;
            } else {
                if (std::popcount(unsigned(edges)) == 1) edges |= 1 << Mod(std::countr_zero(unsigned(edges)) + 3);
                Piece p; p.cell = cell; p.edges = edges; draft.Place(p);
            }
        }
        *this = std::move(draft);
    }
};

inline int Number(const std::string& s, int lo, int hi) {
    Require(!s.empty() && s.size() <= 8 && std::ranges::all_of(s, [](unsigned char c) { return std::isdigit(c); }), "请输入有效整数");
    int n = std::stoi(s); Require(n >= lo && n <= hi, "数字超出允许范围"); return n;
}
inline std::vector<std::string> Words(const std::string& line) {
    std::istringstream in(line); std::vector<std::string> out;
    for (std::string s; in >> s;) out.push_back(s);
    return out;
}
inline int Component(Factory& f, const std::string& target) {
    if (target.starts_with('#')) return Number(target.substr(1),1,1000000);
    if (!target.empty() && std::isdigit(static_cast<unsigned char>(target[0]))) return Number(target,1,1000000);
    const auto cell = ParseCell(target);
    int id = 0;
    for (const auto& p : f.pieces) if (p.cell == cell) {
        Require(id == 0,"此格有多条交叉管道，请使用文字列表中的组件编号"); id = p.id;
    }
    Require(id != 0,"该坐标没有组件"); return id;
}
inline void AutoTanks(Factory& f) {
    // Rebuild port networks after each addition: one tank can close several
    // neighboring inputs, and pipe colors come from the whole connected network.
    for (;;) {
        struct Node { Cell cell; Kind kind; int piece; Port port; };
        std::vector<Node> nodes;
        std::vector<int> parent;
        std::map<std::pair<Cell,int>,int> ends;
        std::function<int(int)> root = [&](int i) { return parent[i] == i ? i : parent[i] = root(parent[i]); };
        auto join = [&](int a,int b) { parent[root(a)] = root(b); };
        for (const auto& piece : f.pieces) {
            const int first = nodes.size();
            for (auto port : Ports(piece)) {
                int i = nodes.size(); nodes.push_back({piece.cell,piece.kind,piece.id,port}); parent.push_back(i);
                ends.emplace(std::make_pair(piece.cell,port.edge),i);
                if (piece.kind != Kind::Machine && i != first) join(first,i);
            }
        }
        std::vector<int> open;
        for (int i=0;i<int(nodes.size());++i) {
            const auto& n = nodes[i];
            auto other = ends.find({Neighbor(n.cell,n.port.edge),Mod(n.port.edge+3)});
            if (other != ends.end()) {
                if (!(n.port.kind == PortKind::Source && nodes[other->second].port.kind == PortKind::Source)) join(i,other->second);
            } else if (n.port.kind != PortKind::Source) open.push_back(i);
        }
        struct Network { int colors=31, inputs=0, outputs=0, sources=0; bool machine=false; };
        std::map<int,Network> networks;
        for (int i=0;i<int(nodes.size());++i) {
            const auto& n=nodes[i]; auto& net=networks[root(i)];
            if(n.port.colors) net.colors &= n.port.colors;
            net.inputs += n.port.kind == PortKind::Input;
            net.outputs += n.port.kind == PortKind::Output;
            net.sources += n.port.kind == PortKind::Source;
            net.machine |= n.kind == Kind::Machine;
        }
        bool added=false;
        // Supply missing inputs first, then cap remaining output/pipe ends.
        for(int pass=0;pass<2 && !added;++pass) for(int i : open) {
            const auto& n=nodes[i]; const auto& net=networks.at(root(i));
            if(!net.machine || !net.colors || (net.sources && net.outputs)) continue;
            const bool need_source = net.inputs && !net.outputs && !net.sources;
            if(need_source != (pass == 0)) continue;
            Piece tank; tank.cell=Neighbor(n.cell,n.port.edge); tank.rot=Mod(n.port.edge+3);
            if(!Playable(f.skin,tank.cell) || std::ranges::any_of(f.pieces,[&](const auto& p) {return p.cell==tank.cell;})) continue;
            if(need_source) {
                tank.kind=Kind::Supply;
                for(int color : {1,2,4,8}) if((net.colors & color) && !std::ranges::any_of(f.pieces,[&](const auto& p) {return p.kind==Kind::Supply && p.color==color;})) {tank.color=color; break;}
                if(!tank.color) continue;
            } else {
                tank.kind = (net.colors & 15) ? Kind::White : Kind::Black;
                if(tank.kind==Kind::White && std::ranges::count(f.pieces,Kind::White,&Piece::kind)>=3) continue;
            }
            // A supply has six ports. Don't accidentally join another network
            // with conflicting colors or a machine output at a neighboring edge.
            Factory candidate=f; candidate.Place(tank);
            auto issues_before=f.Validate().issues; auto issues_after=candidate.Validate().issues;
            const bool new_conflict=std::ranges::any_of(issues_after,[&](const auto& issue) {
                return (issue.find("混色")!=std::string::npos || issue.find("不能合流")!=std::string::npos) &&
                    std::ranges::find(issues_before,issue)==issues_before.end();
            });
            if(new_conflict) continue;
            f=std::move(candidate); added=true; break;
        }
        if(!added) return;
    }
}
inline std::vector<Cell> PathCells(const std::vector<std::string>& a) {
    std::string text;
    for (size_t i = 1; i < a.size(); ++i) text += a[i] + " ";
    for (const std::string separator : {"，","、","→"})
        for (size_t at = 0; (at = text.find(separator,at)) != std::string::npos;) text.replace(at,separator.size()," ");
    std::vector<Cell> cells;
    for (size_t i = 0; i < text.size();) {
        if (std::isspace(static_cast<unsigned char>(text[i])) || text[i] == ',' || text[i] == '-' || text[i] == '>') { ++i; continue; }
        Require(i + 1 < text.size(),"画线格式：画线 A3 B3 C3，或 画线 A3B3C3");
        cells.push_back(ParseCell(text.substr(i,2))); i += 2;
    }
    Require(cells.size() >= 2 && cells.size() <= 45,"连续画线需要 2 至 45 个相邻坐标"); return cells;
}
inline bool CoordinateSequence(const std::string& text) {
    if (text.empty() || text.size() % 2) return false;
    for (size_t i = 0; i < text.size(); i += 2)
        if (!((text[i] >= 'a' && text[i] <= 'g') || (text[i] >= 'A' && text[i] <= 'G')) ||
            text[i + 1] < '0' || text[i + 1] > '9') return false;
    return true;
}
inline bool PathCommand(const std::string& cmd) {
    return cmd == "连" || cmd == "画线" || cmd == "连续画线" || cmd == "连线" || cmd == "画管";
}
inline bool RemoveCommand(const std::string& cmd) {
    return cmd == "拆" || cmd == "拆除" || cmd == "移除" || cmd == "删" || cmd == "删除";
}
// Fixed-arity commands can follow each other with spaces. Variable-arity paths
// and removals consume their targets; a semicolon explicitly ends either one.
inline std::vector<std::vector<std::string>> ConstructionCommands(const std::vector<std::string>& args) {
    std::string text;
    for (const auto& arg : args) { text += arg; text += ' '; }
    Require(text.size() <= 8192, "一条施工消息最多 8192 字节");
    for (size_t at = 0; (at = text.find("；", at)) != std::string::npos;)
        text.replace(at, std::string("；").size(), ";");
    std::istringstream in(text);
    std::vector<std::vector<std::string>> commands;
    for (std::string line; std::getline(in, line, ';');) {
        auto words = Words(line);
        if (words.empty() && in.eof() && !commands.empty()) break;
        Require(!words.empty(), "施工步骤不能为空");
        for (size_t i = 0; i < words.size();) {
            const size_t start = i++;
            const auto& cmd = words[start];
            size_t required = 0;
            if (cmd == "机" || cmd == "粉" || cmd == "黄" || cmd == "绿" || cmd == "蓝" ||
                cmd == "白" || cmd == "黑" || cmd == "转" || cmd == "旋转" || cmd == "翻" || cmd == "翻转") required = 1;
            else if (cmd == "管" || cmd == "移" || cmd == "移动") required = 2;
            Require(words.size() - i >= required, "参数数量不正确，请发送 操作");
            i += required;
            if (cmd == "转" || cmd == "旋转" || cmd == "旋") {
                const size_t max_options = cmd == "旋" ? 1 : 2;
                for (size_t n = 0; n < max_options && i < words.size(); ++n) {
                    const auto& option = words[i];
                    if ((cmd != "旋" && (option == "顺时针" || option == "逆时针")) ||
                        std::ranges::all_of(option, [](unsigned char c) { return std::isdigit(c); })) ++i;
                    else break;
                }
            } else if (PathCommand(cmd) || RemoveCommand(cmd)) {
                while (i < words.size()) {
                    const auto& target = words[i];
                    if (CoordinateSequence(target) ||
                        (PathCommand(cmd) && (target.find_first_of(",->") != std::string::npos ||
                            target.find("→") != std::string::npos || target.find("，") != std::string::npos || target.find("、") != std::string::npos)) ||
                        (RemoveCommand(cmd) && (target.starts_with('#') || std::isdigit(static_cast<unsigned char>(target[0]))))) ++i;
                    else break;
                }
            }
            Require(commands.size() < 64, "一条施工消息最多 64 步");
            commands.emplace_back(words.begin() + start, words.begin() + i);
        }
    }
    Require(!commands.empty(), "请输入施工指令");
    return commands;
}
inline void Edit(Factory& f, std::vector<std::string> a) {
    Require(!a.empty(), "请输入施工指令");
    if (a[0] == "auto") a[0] = "自动";
    if (a[0].starts_with('r') && a[0].size() > 1) a[0] = "旋" + a[0].substr(1);
    if (CoordinateSequence(a[0])) a.insert(a.begin(), a[0].size() == 2 ? "机" : "连");
    auto count = [&](size_t lo, size_t hi) { Require(a.size() >= lo && a.size() <= hi, "参数数量不正确，请发送 操作"); };
    const bool tank = a[0] == "粉" || a[0] == "黄" || a[0] == "绿" || a[0] == "蓝" || a[0] == "白" || a[0] == "黑";
    if (a[0] == "机" || tank || a[0] == "管") {
        Piece p;
        if (a[0] == "机") { count(2,2); p.kind = Kind::Machine; p.cell = ParseCell(a[1]); }
        if (tank) {
            count(2,2); p.cell = ParseCell(a[1]); p.kind = Kind::Supply;
            if (a[0] == "白") p.kind = Kind::White;
            else if (a[0] == "黑") p.kind = Kind::Black;
            else { for (int i = 0; i < 4; ++i) if (a[0] == ColorName(1 << i)) p.color = 1 << i; }
        }
        if (a[0] == "管") {
            count(3,3); p.cell = ParseCell(a[1]);
            p.edges = PipeShape(a[2]);
        }
        f.Put(p);
    } else if (a[0] == "自动") {
        count(1,1); AutoTanks(f);
    } else if (PathCommand(a[0])) {
        f.LayPath(PathCells(a));
    } else if (a[0] == "移" || a[0] == "移动") { count(3,3); f.Move(Component(f,a[1]), ParseCell(a[2])); }
    else if (a[0] == "转" || a[0] == "旋转") {
        count(2,4); int steps=1, times=1;
        for(size_t i=2;i<a.size();++i) {
            if(a[i]=="逆时针") steps=-1;
            else if(a[i]=="顺时针") steps=1;
            else times=Number(a[i],1,60);
        }
        const int id=Component(f,a[1]);
        for(int i=0;i<times;++i) f.Rotate(id,steps);
    }
    else if (a[0].starts_with("旋")) {
        int times = 1;
        if (a[0] == "旋") { count(1,2); if (a.size() == 2) times = Number(a[1],1,60); }
        else { count(1,1); times = Number(a[0].substr(std::string("旋").size()),1,60); }
        const auto machine = std::ranges::find_if(f.pieces, [&](const auto& p) { return p.kind == Kind::Machine && p.round == f.round && p.machine == f.current; });
        Require(machine != f.pieces.end(), "请先放置本轮机器，再使用旋转简写");
        const int id = machine->id;
        for (int i = 0; i < times; ++i) f.Rotate(id,1);
    }
    else if (a[0] == "翻" || a[0] == "翻转") { count(2,2); f.Flip(Component(f,a[1])); }
    else if (RemoveCommand(a[0])) {
        count(2,65);
        std::set<int> ids;
        for(size_t i=1;i<a.size();++i) { const int id=Component(f,a[i]); f.Editable(f.Get(id)); ids.insert(id); }
        for(int id:ids) f.Remove(id);
    }
    else if (a[0] == "重置") { count(1,1); f.pieces = f.baseline; }
    else throw std::runtime_error("未知施工指令，请发送 操作");
}
struct Draft {
    Factory factory;
    std::vector<Factory> undo, redo;
    // Each message is one transaction. Failed commands never leave partial changes.
    void Apply(const std::vector<std::string>& a) {
        Require(!a.empty(), "请输入指令");
        if (a[0] == "撤销" || a[0] == "重做") {
            Require(a.size() == 1, "撤销和重做不带参数");
            auto& from = a[0] == "撤销" ? undo : redo;
            auto& to = a[0] == "撤销" ? redo : undo;
            Require(!from.empty(), "没有可撤销或重做的操作");
            to.push_back(factory); factory = from.back(); from.pop_back(); return;
        }
        const auto commands = ConstructionCommands(a[0] == "方案" ? std::vector<std::string>(a.begin() + 1, a.end()) : a);
        ApplyCommands(commands);
    }
    void ApplyCommands(const std::vector<std::vector<std::string>>& commands) {
        Require(!commands.empty(), "请输入施工指令");
        Factory next = factory;
        for (const auto& command : commands) Edit(next, command);
        if (undo.size() == 100) undo.erase(undo.begin());
        undo.push_back(factory); redo.clear(); factory = std::move(next);
    }
};
}
