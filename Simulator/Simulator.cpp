// Simulator.cpp: 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include "../BotCore/bot_core.h"
#include <iostream>
#include <sstream>
#include <cassert>

void at_cq(const UserID& usr_qq, char* buf, const uint64_t len)
{
  sprintf_s(buf, len, "<@%llu>", usr_qq);
}

void send_private_msg(const UserID& usr_qq, const char* msg)
{
  std::cout << "[private -> user: " << usr_qq << "]" << std::endl << msg << std::endl;
}

void send_group_msg(const UserID& group_qq, const char* msg)
{
  std::cout << "[public -> group: " << group_qq << "]" << std::endl << msg << std::endl;
}

void lie_commands()
{
  BOT_API::HandlePublicRequest(1, 1, "#新游戏 LIE");
  BOT_API::HandlePublicRequest(2, 1, "#加入游戏");
  BOT_API::HandlePublicRequest(1, 1, "#开始游戏");
  for (int num = 1; num <= 6; ++num)
  {
    BOT_API::HandlePrivateRequest(2, std::to_string(num).c_str());
    BOT_API::HandlePrivateRequest(2, std::to_string(num).c_str());
    BOT_API::HandlePrivateRequest(1, "相信");
  }
}

void rps_commands()
{
  BOT_API::HandlePublicRequest(1, 1, "#新游戏 猜拳游戏");
  BOT_API::HandlePublicRequest(2, 1, "#加入游戏");
  BOT_API::HandlePublicRequest(1, 1, "#开始游戏");

}

void init_bot()
{
  const char* errmsg = nullptr;
  std::cout << "code: " << BOT_API::ConnectDatabase("cdb-e4pq5sf8.cd.tencentcdb.com:10083", "root", "i8349276i", "lgt_bot_test", false, &errmsg) << std::endl;
  std::cout << "errmsg: " << (errmsg ? errmsg : "(null)");
  BOT_API::Init(114514, send_private_msg, send_group_msg, at_cq);
  lie_commands();
}

int main()
{
  init_bot();

  std::cout << ">>> ";

  for (std::string s; std::getline(std::cin, s); )
  {
    std::stringstream ss(s);

    uint64_t uid = 0;
    uint64_t gid = 0;
    if (!(ss >> uid >> gid))
    {
      std::cout << "[ERROR] usage: <UserID> <GroupID | 0> <arg1> <arg2> ..." << std::endl << ">>> ";
      continue;
    }

    std::string args;
    for (std::string arg; ss >> arg; )
    {
      args += arg;
      args += " ";
    }

    if (gid == 0) { BOT_API::HandlePrivateRequest(uid, args.c_str()); }
    else { BOT_API::HandlePublicRequest(uid, gid, args.c_str()); }
    std::cout << ">>> ";
  }
  
  return 0;
}

