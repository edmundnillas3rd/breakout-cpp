#include "Game.h"
#include "Main.h"

SDL_Window* window;
SDL_Renderer* renderer;


int main(int argc, char* argv[])
{
    const uint32_t SCREEN_WIDTH = 640;
    const uint32_t SCREEN_HEIGHT = 480;

    window = SDL_CreateWindow( "SDL Tutorial", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN );
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC);

    SDL_Init(SDL_INIT_VIDEO);
    IMG_Init(IMG_INIT_PNG);

    GameState game_state;
    game_state.SCREEN_WIDTH = SCREEN_WIDTH;
    game_state.SCREEN_HEIGHT = SCREEN_HEIGHT;
    GameOnStart(&game_state);

    bool running = true;
    float ticks_count = 0.0f;

    while (running)
    {
        float delta_time = (SDL_GetTicks() - ticks_count) / 1000.0f;
        ticks_count = SDL_GetTicks();
        game_state.delta_time = delta_time;


        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_QUIT)
                running = false;

            GameOnEvent(&game_state);
        }

        SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xFF);
        SDL_RenderClear(renderer);

        GameOnUpdate(&game_state);

        SDL_RenderPresent(renderer);
    }

    GameOnShutdown(&game_state);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    
    IMG_Quit();
    SDL_Quit();
    return 0;
}