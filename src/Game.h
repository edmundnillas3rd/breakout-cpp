#pragma once

#include <SDL.h>
#include <SDL_image.h>

struct GameState
{
    SDL_Event event;
    uint32_t SCREEN_WIDTH, SCREEN_HEIGHT;
    float delta_time;
};

void GameOnStart(GameState* state);
void GameOnUpdate(GameState* state);
void GameOnEvent(GameState* state);
void GameOnShutdown(GameState* state);