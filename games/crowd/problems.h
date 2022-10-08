#include <array>
#include <functional>
#include <memory>
#include <set>

#include <map>
#include <vector>
#include <set>
#include <string>
#include <algorithm>

using namespace std;

class Player
{
public:
	Player()
	{
		lastSelect = 0;
		select = 0;
		lastScore = 0;
		score = 0;
	}
	int lastSelect;
	int select;
	double lastScore;
	double score;
	
	string getScore()
	{
		return to_string(score);
	}
};

class Question
{
public:
	
	Question()
	{
		vars.clear();
	} 
	
	int id;
	string author;
	vector<string> texts;
	vector<string> options;
	vector<string> expects;
	
	map<string,double> vars;
	
	
	double playerNum;
	double maxScore;
	double minScore;
	vector<double> optionCount; 
	double maxSelect;
	double minSelect;
	vector<double> tempScore;
	
	
	// init playerNum
	void init(vector<Player>& players)
	{
		playerNum = players.size();
	}
	
	// init texts and options. This function must be overloaded
	virtual void initTexts(){}
	virtual void initOptions(){}
	virtual void initExpects(){}
	
	
	//-----------------------------------------------
	string str(double x)
    {
        string ret = "";
        x = dec2(x);
        string sx = to_string(x);
        int n = sx.length();
        for(int i = 0; i < n; i++)
        {
            if(sx[i] == '.')
            {
                if(i + 2 < n && (sx[i + 1] != '0' || sx[i + 2] != '0'))
                {
                    ret += sx[i];
                    ret += sx[i + 1];
                    if(sx[i + 2] != '0')
                    	ret += sx[i + 2];
                }
                break;
            }
            ret += sx[i];
        }
        return ret;
    }

    double dec2(double x)
    {
        x = x * 100;
        if(x > 0) x += 0.5;
        else x -= 0.5;
        x = (int) x;
        x = x / 100;

        return x;
    }
	//-----------------------------------------------
	
	// init calc before call it.
	void initCalc(vector<Player>& players)
	{
		maxScore = -99999;
		minScore = 99999;
		maxSelect = -99999;
		minSelect = 99999;
		optionCount.clear();
		
		for(int i = 0; i < options.size(); i++) optionCount.push_back(0);
		for(int i = 0; i < options.size(); i++) tempScore.push_back(0);
		
		for(int i = 0; i < playerNum; i++)
		{
			maxScore = max(maxScore, players[i].score);
			minScore = min(minScore, players[i].score);
			optionCount[players[i].select] += 1;
		}
		
		for(int i = 0; i < optionCount.size(); i++)
		{
			maxSelect = max(maxSelect, optionCount[i]);
			minSelect = min(minSelect, optionCount[i]);
		}
	}
	
	// calc function. This function must be overloaded.
	virtual void calc(vector<Player>& players){}
	
	// quick calc.
	void quickScore(vector<Player>& players)
	{
		for(int i = 0; i < players.size(); i++)
		{
			players[i].score += tempScore[players[i].select];
		}
	}
	
	string Markdown()
	{
		string md = "";
	    md += "<font size=7>";
	    for(auto text:texts)
	    {
	        md += "　";
	        md += text;
	        md += "<br>";
	    }
	    md += "</font>";
	    
	    md += "<table style=\"text-align:center\"><tbody>";
	    md += "<tr><td><font size=6>　　</font></td>";
	    md += "<td><font size=6>　　　　　　　　　　　　　　　　　　　　　　　　</font></td>";
	    int count = 0;
	    for(auto option:options)
	    {
	        md += "<tr><td bgcolor=\"#FFE4E1\"><font size=6>";
	        md += (char)(count + 'A');
	        count++;
	        md += "</font></td>";
			md += "<td bgcolor=\"#F5F5F5\"><font size=6>";
	        md += option;
	        md += "</font></td>";
	    }
	
	    return md;
	}
};

class QE : public Question
{
public:
	QE()
	{
		id = -1;
		author = "NULL";
	}
	
	virtual void initTexts() override
	{
		texts.push_back("我是texts");
	}
	virtual void initOptions() override
	{
		vars["v1"] = 1.36;
		options.push_back("我是选项1，选我获得" + str(vars["v1"]) + "分");
		options.push_back("我是选项2，选我获得2.17分");
	}
	virtual void initExpects() override
	{
		expects.push_back("a");
	}
	virtual void calc(vector<Player>& players) override
	{
		tempScore[0] = vars["v1"];
		tempScore[1] = 2.17;
	}
};

class Q1 : public Question
{
public:
	Q1()
	{
		id = 1;
		author = "Mutsuki";
	}
	
	virtual void initTexts() override
	{
		texts.push_back("选择一项。");
	}
	virtual void initOptions() override
	{
		options.push_back("如果选这项的人数比 B 少，+3 分。");
		options.push_back("如果选这项的人数最多，+2 分。");
		options.push_back("+1分。如果选这项的人最多，这项改为 -1。");
	}
	virtual void initExpects() override
	{
		expects.push_back("aaabbbcc");
	}
	virtual void calc(vector<Player>& players) override
	{
		if(optionCount[0] < optionCount[1]) tempScore[0] = 3;
		if(optionCount[1] == maxSelect) tempScore[1] = 2;
		tempScore[2] = 1;
		if(optionCount[2] == maxSelect) tempScore[2] = -1;
	}
};

class Q2 : public Question
{
public:
	Q2()
	{
		id = 2;
		author = "ShenHuXiaoDe";
	}
	
	virtual void initTexts() override
	{
		texts.push_back("选择一项。");
	}
	virtual void initOptions() override
	{
		options.push_back("和平：+2分");
		options.push_back("战争：如果恰有两名玩家选择这个选项，则+6分。");
	}
	virtual void initExpects() override
	{
		expects.push_back("aaaaab");
	}
	virtual void calc(vector<Player>& players) override
	{
		tempScore[0] = 2;
		if(optionCount[1] == 2) tempScore[1] = 6;
	}
};

class Q3 : public Question
{
public:
	Q3()
	{
		id = 3;
		author = "Mutsuki";
	}
	
	virtual void initTexts() override
	{
		texts.push_back("选择一项。");
	}
	virtual void initOptions() override
	{
		vars["A"] = playerNum/4;
		options.push_back("中立：获得 " + str(vars["A"]) + " 分。");
		options.push_back("激进：获得 [ 选择 A 选项的玩家个数 / 2 ] 分。");
	}
	virtual void initExpects() override
	{
		expects.push_back("aaaabbbbb");
	}
	virtual void calc(vector<Player>& players) override
	{
		tempScore[0] = vars["A"];
		tempScore[1] = optionCount[0] / 2;
	}
};

class Q4 : public Question
{
public:
	Q4()
	{
		id = 4;
		author = "ShenHuXiaoDe";
	}
	
	virtual void initTexts() override
	{
		texts.push_back("选择一项。");
	}
	virtual void initOptions() override
	{
		vars["E"] = (int)playerNum / 3 + 1;
		options.push_back("谨慎：+1分。");
		options.push_back("团结：如果选择这项的人数最多，+2分。");
		options.push_back("智慧：如果选择这项的人数最少，+4分。");
		options.push_back("勇敢：如果只有一人选择这项，+5分。");
		options.push_back("公正：选择这项的人平分 " + str(vars["E"]) + " 分。");
	}
	virtual void initExpects() override
	{
		expects.push_back("aaaabbccddddeeeeeeeeeee");
	}
	virtual void calc(vector<Player>& players) override
	{
		tempScore[0] = 1;
		if(optionCount[1] == maxSelect) tempScore[1] = 2;
		if(optionCount[2] == minSelect) tempScore[2] = 4;
		if(optionCount[3] == 1) tempScore[3] = 5;
		tempScore[4] = vars["E"] / optionCount[4];
	}
};

class Q5 : public Question
{
public:
	Q5()
	{
		id = 5;
		author = "Mutsuki";
	}
	
	virtual void initTexts() override
	{
		texts.push_back("选择一项。");
	}
	virtual void initOptions() override
	{
		options.push_back("独享：如果只有 1 人选择此项，获得 4 分。");
		options.push_back("聚餐：如果有 2 或 3 人选择此项，获得 3 分。");
		options.push_back("盛宴：如果有 4 或 5 人选择此项，获得 2 分。");
		options.push_back("闭户：获得 1 分。");
	}
	virtual void initExpects() override
	{
		expects.push_back("aabbbbbcccccccccddddddd");
	}
	virtual void calc(vector<Player>& players) override
	{
		if(optionCount[0] == 1) tempScore[0] = 4;
		if(optionCount[1] == 2 || optionCount[1] == 3) tempScore[1] = 3;
		if(optionCount[2] == 4 || optionCount[2] == 5) tempScore[2] = 2;
		tempScore[3] = 1;
	}
};

class Q6 : public Question
{
public:
	Q6()
	{
		id = 6;
		author = "Mutsuki";
	}
	
	virtual void initTexts() override
	{
		texts.push_back("选择一项。");
		texts.push_back(" [ 只有选择人数最多的选项会生效 ]"); 
	}
	virtual void initOptions() override
	{
		options.push_back("破坏：-1，然后使选 B 的玩家 -3");
		options.push_back("合作：+2，然后使选 C 的玩家 +1");
		options.push_back("平衡：-0.5。");
	}
	virtual void initExpects() override
	{
		expects.push_back("aaaaabbbcccc");
	}
	virtual void calc(vector<Player>& players) override
	{
		if(optionCount[0] == maxSelect)
		{
			tempScore[0] -= 1;
			tempScore[1] -= 3;
		}
		if(optionCount[1] == maxSelect)
		{
			tempScore[1] += 2;
			tempScore[2] += 1;
		}
		if(optionCount[2] == maxSelect)
		{
			tempScore[2] -= 0.5;
		}
	}
};

class Q7 : public Question
{
public:
	Q7()
	{
		id = 7;
		author = "Mutsuki";
	}
	
	virtual void initTexts() override
	{
		texts.push_back("选择一项。先执行 A ，后执行 B 。");
	}
	virtual void initOptions() override
	{
		options.push_back("均衡：如果有 3 或更多名玩家选择本项，则得分最高的玩家 -3");
		options.push_back("观望：+1");
	}
	virtual void initExpects() override
	{
		expects.push_back("abbbb");
	}
	virtual void calc(vector<Player>& players) override
	{
		if(optionCount[0] >= 3)
		{
			for(int i = 0; i < playerNum; i++)
			{
				if(players[i].score == maxScore)
				{
					players[i].score -= 3;
				}
			}
		}
		tempScore[1] = 1;
	}
};

class Q8 : public Question
{
public:
	Q8()
	{
		id = 8;
		author = "Mutsuki";
	}
	
	virtual void initTexts() override
	{
		texts.push_back("选择一项。");
	}
	virtual void initOptions() override
	{
		vars["A"] = playerNum;
		vars["B1"] = (int)(playerNum / 4);
		vars["B2"] = (int)(playerNum * 1.5); 
		options.push_back("均势：选择本项的人平分" + str(vars["A"]) + "分。");
		options.push_back("幽灵：选择本项的人，按人数+" + str(vars["B1"]) + "计算，平分" + str(vars["B2"]) + "分。");
	}
	virtual void initExpects() override
	{
		expects.push_back("aaaabbbbb");
	}
	virtual void calc(vector<Player>& players) override
	{
		tempScore[0] = vars["A"] / optionCount[0];
		tempScore[1] = vars["B2"] / (optionCount[1] + vars["B1"]);
	}
};

class Q9 : public Question
{
public:
	Q9()
	{
		id = 9;
		author = "Mutsuki";
	}
	
	virtual void initTexts() override
	{
		texts.push_back("选择一项。");
	}
	virtual void initOptions() override
	{
		options.push_back("老师：如果好学生比坏学生多，+2");
		options.push_back("好学生：如果比坏学生多，+1");
		options.push_back("坏学生：如果比好学生多，老师 -2");
		options.push_back("校霸：如果坏学生比好学生多，+0.5");
	}
	virtual void initExpects() override
	{
		expects.push_back("aabbbccccdddd");
	}
	virtual void calc(vector<Player>& players) override
	{
		if(optionCount[1] > optionCount[2]) tempScore[0] += 2;
		if(optionCount[1] > optionCount[2]) tempScore[1] += 1;
		if(optionCount[1] < optionCount[2]) tempScore[0] -= 2;
		if(optionCount[1] < optionCount[2]) tempScore[3] += 0.5;
	}
};

class Q10 : public Question
{
public:
	Q10()
	{
		id = 10;
		author = "Mutsuki";
	}
	
	virtual void initTexts() override
	{
		texts.push_back("选择一项。");
	}
	virtual void initOptions() override
	{
		options.push_back("老实人：+2。");
		options.push_back("背叛者：-1。如果任何人选择此项，则选 A 的玩家改为 -2。");
		options.push_back("旁观者：-0.5。");
	}
	virtual void initExpects() override
	{
		expects.push_back("aabbccccc");
	}
	virtual void calc(vector<Player>& players) override
	{
		tempScore[0] += 2;
		tempScore[1] -= 1;
		tempScore[2] -= 0.5;
		if(optionCount[1] > 0) tempScore[0] = -2;
	}
};

class Q11 : public Question
{
public:
	Q11()
	{
		id = 11;
		author = "Mutsuki";
	}
	
	virtual void initTexts() override
	{
		texts.push_back("选择一项。");
	}
	virtual void initOptions() override
	{
		options.push_back("苦工：+1。");
		options.push_back("老板：如果存在苦工，+2。否则 -1 。");
		options.push_back("高官：如果存在老板，+2.5。");
		options.push_back("制度：如果存在高官，+3。");
	}
	virtual void initExpects() override
	{
		expects.push_back("aaabbbbcddddddddddddddddddddddd");
	}
	virtual void calc(vector<Player>& players) override
	{
		tempScore[0] += 1;
		if(optionCount[0] > 0) tempScore[1] = 2;
		else tempScore[1] = -1;
		
		if(optionCount[1] > 0) tempScore[2] += 2.5;
		if(optionCount[2] > 0) tempScore[3] += 3;
	}
};

class Q12 : public Question
{
public:
	Q12()
	{
		id = 12;
		author = "Mutsuki";
	}
	
	virtual void initTexts() override
	{
		texts.push_back("选择一项。");
	}
	virtual void initOptions() override
	{
		vars["D"] = (int)(playerNum / 3);
		options.push_back("智慧：人数比 B 少则 +3");
		options.push_back("体能：人数比 A 多则 +2");
		options.push_back("坚持：+1");
		options.push_back("好运：如果恰有 " + str(vars["D"]) + " 玩家选择这个选项，+" + str(vars["D"]));
	}
	virtual void initExpects() override
	{
		expects.push_back("abbccccccccccccddddd");
	}
	virtual void calc(vector<Player>& players) override
	{
		if(optionCount[0] < optionCount[1]) tempScore[0] = 3;
		if(optionCount[0] < optionCount[1]) tempScore[1] = 2;
		tempScore[2] = 1;
		if(optionCount[3] == vars["D"]) tempScore[3] = vars["D"];
	}
};

class Q13 : public Question
{
public:
	Q13()
	{
		id = 13;
		author = "Dva";
	}
	
	virtual void initTexts() override
	{
		texts.push_back("选择一项。");
	}
	virtual void initOptions() override
	{
		options.push_back("信任：+3 分");
		options.push_back("怀疑：+2 分");
		options.push_back("顾虑：-1 分");
		options.push_back("背叛：失去等同于选择该选项人数的分数。如果有超过 1 玩家选择本选项，则 A 改为 -4，B改为 -2 。");
	}
	virtual void initExpects() override
	{
		expects.push_back("aaaabccccccccccdddd");
	}
	virtual void calc(vector<Player>& players) override
	{
		tempScore[0] = 3;
		tempScore[1] = 2;
		tempScore[2] = -1;
		tempScore[3] = -optionCount[3];
		
		if(optionCount[3] > 1)
		{
			tempScore[0] = -4;
			tempScore[1] = -2;
		}
	}
};

class Q14 : public Question
{
public:
	Q14()
	{
		id = 14;
		author = "Dva";
	}
	
	virtual void initTexts() override
	{
		texts.push_back("选择一项。对于每个选项，如果所选人数小于等于其人数，则选择该选项的人获得对应分数。");
	}
	virtual void initOptions() override
	{
		vars["A"] = 1;
		vars["B"] = (int)(playerNum * 2 / 10) + 1;
		vars["C"] = (int)(playerNum * 3 / 10) + 1;
		vars["D"] = (int)(playerNum * 4 / 10);
		options.push_back(str(vars["A"]) + "人，3分");
		options.push_back(str(vars["B"]) + "人，2分");
		options.push_back(str(vars["C"]) + "人，1分");
		options.push_back(str(vars["D"]) + "人，2分");
	}
	virtual void initExpects() override
	{
		expects.push_back("abbcccdddd");
	}
	virtual void calc(vector<Player>& players) override
	{
		if(optionCount[0] <= vars["A"]) tempScore[0] = 3;
		if(optionCount[1] <= vars["B"]) tempScore[1] = 2;
		if(optionCount[2] <= vars["C"]) tempScore[2] = 1;
		if(optionCount[3] <= vars["D"]) tempScore[3] = 2;
	}
};

class Q15 : public Question
{
public:
	Q15()
	{
		id = 15;
		author = "Guest";
	}
	
	virtual void initTexts() override
	{
		texts.push_back("选择一项。");
	}
	virtual void initOptions() override
	{
		options.push_back("自负：如若选C的人最多，+2。如若选C的人最少，-1。");
		options.push_back("叛逆：如若选C的人最少，+3。如若选C的人最多，-1。");
		options.push_back("平庸：+1。");
	}
	virtual void initExpects() override
	{
		expects.push_back("aaaabbccccccccc");
	}
	virtual void calc(vector<Player>& players) override
	{
		tempScore[2] = 1;
		
		if(optionCount[2] == maxSelect)
		{
			tempScore[0] += 2;
			tempScore[1] -= 1;
		}
		if(optionCount[2] == minSelect)
		{
			tempScore[0] -= 1;
			tempScore[1] += 3;
		} 
	}
};

class Q16 : public Question
{
public:
	Q16()
	{
		id = 16;
		author = "403";
	}
	
	virtual void initTexts() override
	{
		texts.push_back("选择一项。");
	}
	virtual void initOptions() override
	{
		options.push_back("如果选择该项的人数比 B 多，-1 分。");
		options.push_back("如果选择该项的人数比 A 多，-1 分。");
		options.push_back("如若前两个选项都有人选择，+0。否则 -4 。");
	}
	virtual void initExpects() override
	{
		expects.push_back("aabbccccc");
	}
	virtual void calc(vector<Player>& players) override
	{
		if(optionCount[0] > optionCount[1]) tempScore[0] = -1;
		if(optionCount[1] > optionCount[0]) tempScore[1] = -1;
		
		if(!optionCount[0] || !optionCount[1]) tempScore[2] = -4;
	}
};

class Q17 : public Question
{
public:
	Q17()
	{
		id = 17;
		author = "Guest";
	}
	
	virtual void initTexts() override
	{
		texts.push_back("选择一项。");
	}
	virtual void initOptions() override
	{
		options.push_back("内卷：+3 ，如果选择人数大于另外两项之和，改为 -3");
		options.push_back("正常：+1");
		options.push_back("摸鱼：-1 ，如果选择人数小于另外两项之和，改为 +2");
	}
	virtual void initExpects() override
	{
		expects.push_back("aaaaabcccc");
	}
	virtual void calc(vector<Player>& players) override
	{
		if(optionCount[0] > optionCount[1] + optionCount[2]) tempScore[0] = -3;
		else tempScore[0] = 3;
		if(optionCount[2] < optionCount[1] + optionCount[0]) tempScore[2] = +2;
		else tempScore[2] = -1;
		
		tempScore[1] = 1;
	}
};

class Q18 : public Question
{
public:
	Q18()
	{
		id = 18;
		author = "Dva";
	}
	
	virtual void initTexts() override
	{
		vars["S1"] = (int)(playerNum / 3);
		vars["S2"] = (int)(playerNum / 2);
		texts.push_back("选择一个阵营加入，每个阵营内部平分 " + str(vars["S1"]) + " 分，然后战斗力最高的阵营平分 " + str(vars["S2"]) + " 分");
	}
	virtual void initOptions() override
	{
		options.push_back("团结：每有一个加入者，+3战力。");
		options.push_back("科技：战力固定为 " + str(playerNum) + "。");
		options.push_back("经济：战力为 -1 。该阵营有 2 个或更多加入者时，不平分 " + str(vars["S1"]) + " 分，而是 " + str(vars["S2"]) + " 分。");
	}
	virtual void initExpects() override
	{
		expects.push_back("abc");
	}
	virtual void calc(vector<Player>& players) override
	{
		tempScore[0] += vars["S1"] / optionCount[0];
		tempScore[1] += vars["S1"] / optionCount[1];
		tempScore[2] += vars["S1"] / optionCount[2];
		
		if(optionCount[0] * 3 >= playerNum) tempScore[0] += vars["S2"] / optionCount[0];
		if(optionCount[0] * 3 <= playerNum) tempScore[1] += vars["S2"] / optionCount[1];
		if(optionCount[2] >= 2) tempScore[2] = vars["S2"] / optionCount[2];
	}
};

class Q19 : public Question
{
public:
	Q19()
	{
		id = 19;
		author = "Guest";
	}
	
	virtual void initTexts() override
	{
		texts.push_back("面对谣言，你选择：");
	}
	virtual void initOptions() override
	{
		vars["A"] = (int)(playerNum / 5);
		vars["B"] = (int)(playerNum * 3 / 5);
		options.push_back("传谣：若选择人数<= " + str(vars["A"]) + "，+2。否则 -1。");
		options.push_back("沉默：+0。");
		options.push_back("思考：若选择人数>= " + str(vars["B"]) + "，+1。否则 -2。");
	}
	virtual void initExpects() override
	{
		expects.push_back("aaaabbbbbcc");
	}
	virtual void calc(vector<Player>& players) override
	{
		if(optionCount[0] <= vars["A"]) tempScore[0] = 2;
		else tempScore[0] = -1;
		
		tempScore[1] = 0;
		
		if(optionCount[2] >= vars["B"]) tempScore[0] = 1;
		else tempScore[2] = -2;
	}
};

class Q20 : public Question
{
public:
	Q20()
	{
		id = 20;
		author = "403";
	}
	
	virtual void initTexts() override
	{
		texts.push_back("选择一项");
	}
	virtual void initOptions() override
	{
		options.push_back("+1，然后均分 [ 选择 B 选项人数 ] 的分数。");
		options.push_back("+0，然后均分 [ 选择 C 选项人数 ] 的分数。");
		options.push_back("+3。");
	}
	virtual void initExpects() override
	{
		expects.push_back("abbbccccccccc");
	}
	virtual void calc(vector<Player>& players) override
	{
		tempScore[0] = optionCount[1] / optionCount[0] + 1;
		tempScore[1] = optionCount[2] / optionCount[1] + 0;
		tempScore[2] = 3;
	}
};

class Q21 : public Question
{
public:
	Q21()
	{
		id = 21;
		author = "403";
	}
	
	virtual void initTexts() override
	{
		texts.push_back("选择一项");
	}
	virtual void initOptions() override
	{
		options.push_back("委婉：-2");
		options.push_back("强硬：如果所有玩家全部选择该项，则所有玩家分数取反。");
	}
	virtual void initExpects() override
	{
		expects.push_back("abbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
	}
	virtual void calc(vector<Player>& players) override
	{
		tempScore[0] = -2;
		
		if(optionCount[1] == playerNum)
		{
			for(int i = 0; i < playerNum; i++)
			{
				players[i].score = -players[i].score;
			}
		}
	}
};

class Q22 : public Question
{
public:
	Q22()
	{
		id = 22;
		author = "luna";
	}
	
	virtual void initTexts() override
	{
		texts.push_back("负分大转盘，选择一项。");
		texts.push_back("扣分最多的玩家获胜，获得（1 + 没有选择该选项玩家的数量 / 2）分。");
	}
	virtual void initOptions() override
	{
		vars["E"] = - 4 - playerNum / 4;
		options.push_back("+0");
		options.push_back("-1");
		options.push_back("-2");
		options.push_back("-3");
		options.push_back(str(vars["E"]));
	}
	virtual void initExpects() override
	{
		expects.push_back("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaabbbbbbbbbcccccdddeeeeeeee");
	}
	virtual void calc(vector<Player>& players) override
	{
		for(int i = 4; i >= 0; i--)
		{
			if(optionCount[i] != 0)
			{
				tempScore[i] = 1 + (playerNum - optionCount[i]) / 2;
				break;
			}
		}
		tempScore[0] += 0;
		tempScore[1] += -1;
		tempScore[2] += -2;
		tempScore[3] += -3;
		tempScore[4] += vars["E"];
	}
};

class Q23 : public Question
{
public:
	Q23()
	{
		id = 23;
		author = "YAMI";
	}
	
	virtual void initTexts() override
	{
		texts.push_back("选择一项。");
	}
	virtual void initOptions() override
	{
		options.push_back("同化：+1，然后所有选择此项的玩家会均分他们的分数。");
		options.push_back("排斥：+0。");
	}
	virtual void initExpects() override
	{
		expects.push_back("aaaaaaaaaaaaaaaaab");
	}
	virtual void calc(vector<Player>& players) override
	{
		double sum = 0;
		for(int i = 0; i < playerNum; i++)
		{
			if(players[i].select == 0)
			{
				sum += players[i].score;
				players[i].score = 0; 
			}
		}
		
		tempScore[0] = 1 + sum / optionCount[0];
	}
};

class Q24 : public Question
{
public:
	Q24()
	{
		id = 24;
		author = "Mutsuki";
	}
	
	virtual void initTexts() override
	{
		texts.push_back("选择一项。");
		texts.push_back("只有人数最多的选项会产生效果。");
	}
	virtual void initOptions() override
	{
		options.push_back("乞丐：-2。");
		options.push_back("慈善家：选 A 或 C 的玩家 +2。");
		options.push_back("乞丐：-2。");
		options.push_back("路人：-0.5。");
	}
	virtual void initExpects() override
	{
		expects.push_back("aabbbccddd");
	}
	virtual void calc(vector<Player>& players) override
	{
		if(optionCount[0] == maxSelect) tempScore[0] += -2;
		if(optionCount[1] == maxSelect)
		{
			tempScore[0] += 2;
			tempScore[2] += 2;
		}
		if(optionCount[2] == maxSelect) tempScore[2] += -0.5;
	}
};

class Q25 : public Question
{
public:
	Q25()
	{
		id = 25;
		author = "Mutsuki";
	}
	
	virtual void initTexts() override
	{
		texts.push_back("选择一项。");
		texts.push_back("选择完毕后，会统计所有玩家所选数字的平均值（下取整）。然后选择了数字为平均值选项的玩家，获得该数字的分数。");
	}
	virtual void initOptions() override
	{
		options.push_back("0");
		options.push_back("1");
		options.push_back("2");
		options.push_back("3");
		options.push_back("4");
		options.push_back("5");
		options.push_back("6");
		options.push_back("7");
	}
	virtual void initExpects() override
	{
		expects.push_back("abbbccccccccccccccccccccccccccccddddddddddddddddddddddddeeeeffgh");
	}
	virtual void calc(vector<Player>& players) override
	{
		int sum = 0, ave = 0;
		for(int i = 0; i < playerNum; i++)
		{
			sum += players[i].select;
		}
		ave = (int)(sum / playerNum);
		
		tempScore[ave] = ave;
	}
};

class Q26 : public Question
{
public:
	Q26()
	{
		id = 26;
		author = "Mutsuki";
	}
	
	virtual void initTexts() override
	{
		texts.push_back("选择一项。");
		texts.push_back("超过一半人选择某一选项时，该选项会改为不同的（括号中的）效果。");
	}
	virtual void initOptions() override
	{
		options.push_back("拔河：-1 ( +1.5 )");
		options.push_back("过桥：+1 ( +0 )");
		options.push_back("坐船：+2 ( -2 )");
	}
	virtual void initExpects() override
	{
		expects.push_back("aabbbbbbbccccc");
	}
	virtual void calc(vector<Player>& players) override
	{
		if(optionCount[0] > playerNum / 2) tempScore[0] = 1.5;
		else tempScore[0] = -1;
		
		if(optionCount[1] > playerNum / 2) tempScore[1] = 0;
		else tempScore[1] = 1;
				
		if(optionCount[2] > playerNum / 2) tempScore[2] = -2;
		else tempScore[2] = 2;
	}
};
