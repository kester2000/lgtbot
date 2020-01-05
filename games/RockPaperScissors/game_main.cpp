
#include <memory>
#include <map>
#include <functional>
#include <vector>
#include <iostream>
#include "game.h"
#include "dllmain.h"
#include "mygame.h"

typedef void (*request_f)(char const* msg);

bool __cdecl Init(const boardcast boardcast, const tell tell, const at at, const game_over game_over)
{

  return true;
}

char* __cdecl GameInfo(uint64_t* min_player, uint64_t* max_player)
{

  return NULL;
}

GameBase* __cdecl NewGame(const uint64_t match_id, const uint64_t player_num)
{
  return new Game<StageEnum, GameEnv>(match_id, MakeGameEnv(player_num), MakeMainStage());
}

void __cdecl DeleteGame(GameBase* const game)
{
  delete(game);
}
