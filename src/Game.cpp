#include "Game.h"

#include <iostream>
#include <vector>
#include <random>
#include <tuple>

#include <glm/glm.hpp>

#include "Main.h"

struct Entity
{
    glm::vec2 position;
    glm::vec2 direction;
};

enum class Direction
{
    UP,
    RIGHT,
    DOWN,
    LEFT,
};

struct Ball
{
    glm::vec2 position;
    glm::vec2 velocity;
    float radius;
    bool stuck;
};

struct Brick
{
    Brick(glm::vec2 p, uint8_t t) : position(p), type(t), destroyed(false) {}

    glm::vec2 position;
    bool destroyed;
    uint8_t type;
};

// Game State
bool isGameOver;

static Entity player;
static Ball ball;

static SDL_Rect brick_rect;
static std::vector<Brick> bricks;

// Texture and clips
static SDL_Texture* breakout_spritesheet;
static SDL_Texture* background_texture;
static SDL_Rect brick_sprite_rects[4];
static SDL_Rect ball_rect_clip;
static SDL_Rect paddle_rect_clip;
static SDL_Rect background_rect;

std::random_device rd;
std::default_random_engine eng(rd());
std::uniform_int_distribution<int> uni_dest(0, 3);

using Collision = std::tuple<bool, Direction, glm::vec2>;

static Direction VectorDirection(glm::vec2 target)
{
    glm::vec2 compass[] = {
        glm::vec2(0.0f, 1.0f),
        glm::vec2(1.0f, 0.0f),
        glm::vec2(0.0f, -1.0f),
        glm::vec2(-1.0f, 0.0f),
    };

    float max = 0.0f;
    unsigned int best_match = -1;

    for (int i = 0; i < 4; i++)
    {
        float dot_product = glm::dot(glm::normalize(target), compass[i]);
        if (dot_product > max)
        {
            max = dot_product;
            best_match = static_cast<int>(i);
        }
    }
    
    return static_cast<Direction>(best_match);
}

static bool CheckCollision(SDL_Rect rectA, SDL_Rect rectB)
{
    bool collision_x = rectA.x + rectA.w >= rectB.x && rectB.x + rectB.w >= rectA.x;
    bool collision_y = rectA.y + rectA.h >= rectB.y && rectB.y + rectB.h >= rectA.y;

    return collision_x && collision_y;
}

static Collision CheckCollision(Ball ball, SDL_Rect brick)
{
    glm::vec2 center(ball.position + ball.radius);
    glm::vec2 aabb_half_extents(brick.w / 2.0f, brick.h / 2.0f);
    glm::vec2 aabb_center(
        brick.x + aabb_half_extents.x,
        brick.y + aabb_half_extents.y
    );

    glm::vec2 difference = center - aabb_center;
    glm::vec2 clamped = glm::clamp(difference, -aabb_half_extents, aabb_half_extents);
    glm::vec2 closest = aabb_center + clamped;

    difference = closest - center;

    if (glm::length(difference) < ball.radius)
        return std::make_tuple(true, VectorDirection(difference), difference);
    else
        return std::make_tuple(false, Direction::UP, glm::vec2(0.0f, 0.0f));
}

void Reset(GameState* state)
{
    isGameOver = false;
    
    player.position.x = state->SCREEN_WIDTH / 2;
    player.position.y = state->SCREEN_HEIGHT - 32;

    ball.position.x = player.position.x + 26;
    ball.position.y = player.position.y - 32;
    ball.velocity = { 100, -350.0f };
    ball.radius = 6.0f;
    ball.stuck = true;

    const uint32_t MAX_BRICKS_PER_ROW = state->SCREEN_WIDTH / brick_rect.w;
    const uint32_t NUM_ROWS = 6;

    for (int j = 0; j < NUM_ROWS; j++)
    {
        for (int i = 0; i < MAX_BRICKS_PER_ROW; i++)
        {
            int type = uni_dest(eng);
            float offset_x = i * (brick_rect.w + 3);
            float offset_y = (j + 2) * (brick_rect.h + 3);
            bricks.emplace_back(glm::vec2(offset_x, offset_y), type);
        }
    }
}

void GameOnStart(GameState* state)
{
    SDL_Surface* loaded_surface = IMG_Load("assets/images/breakout.png");

    SDL_Rect spritesheet_rect;
    spritesheet_rect.w = loaded_surface->w;
    spritesheet_rect.h = loaded_surface->h;
    breakout_spritesheet = SDL_CreateTextureFromSurface(renderer, loaded_surface);

    const int BRICK_SPRITE_WIDTH = 38;
    const int BRICK_SPRITE_HEIGHT = spritesheet_rect.h / 4;

    for (int i = 0; i < 4; i++)
    {
        brick_sprite_rects[i].x = 0;
        brick_sprite_rects[i].y = BRICK_SPRITE_HEIGHT * i;
        brick_sprite_rects[i].w = BRICK_SPRITE_WIDTH;
        brick_sprite_rects[i].h = BRICK_SPRITE_HEIGHT;
    }

    brick_rect.w = brick_sprite_rects[0].w * 2;
    brick_rect.h = brick_sprite_rects[0].h * 2;

    const int BALL_SPRITE_WIDTH = 6;
    const int BALL_SPRITE_HEIGHT = 6;

    ball_rect_clip.x = 38;
    ball_rect_clip.y = 0;
    ball_rect_clip.w = BALL_SPRITE_WIDTH;
    ball_rect_clip.h = BALL_SPRITE_HEIGHT;

    const int PADDLE_SPRITE_WIDTH = 50;
    const int PADDLE_SPRITE_HEIGHT = 12;
    paddle_rect_clip.x = 44;
    paddle_rect_clip.y = 0;
    paddle_rect_clip.w = PADDLE_SPRITE_WIDTH;
    paddle_rect_clip.h = PADDLE_SPRITE_HEIGHT;

    SDL_FreeSurface(loaded_surface);

    loaded_surface = IMG_Load("assets/images/breakout_bg.png");
    background_rect.w = loaded_surface->w;
    background_rect.h = loaded_surface->h;
    background_texture = SDL_CreateTextureFromSurface(renderer, loaded_surface);

    SDL_FreeSurface(loaded_surface);

    Reset(state);
}

void GameOnUpdate(GameState* state)
{
    if (isGameOver == true || bricks.empty())
    {
        Reset(state);
        return;
    }

    SDL_RenderCopy(renderer, background_texture, &background_rect, nullptr);

    auto destroyed_bricks = std::remove_if(bricks.begin(), bricks.end(), [](Brick b){
        return b.destroyed == true;
    });
    
    const Uint8* key = SDL_GetKeyboardState(nullptr);

    float speed = 400.0f;
    float velocity = speed * state->delta_time;
    if (key[SDL_SCANCODE_LEFT])
    {
        player.position.x -= velocity;

        if (ball.stuck)
            ball.position.x -= velocity;
    }
    else if (key[SDL_SCANCODE_RIGHT])
    {
        player.position.x += velocity;

        if (ball.stuck)
            ball.position.x += velocity;
    }

    if (key[SDL_SCANCODE_SPACE])
        ball.stuck = false;

    SDL_Rect paddle_rect;

    paddle_rect.x = player.position.x;
    paddle_rect.y = player.position.y;
    paddle_rect.w = paddle_rect_clip.w * 2;
    paddle_rect.h = paddle_rect_clip.h * 2;

    if (player.position.x <= 0.0f)
    {
        player.position.x = 0.0f;
    }
    else if (player.position.x + paddle_rect.w >= state->SCREEN_WIDTH)
    {
        player.position.x = state->SCREEN_WIDTH - paddle_rect.w;
    }

    SDL_RenderCopy(renderer, breakout_spritesheet, &paddle_rect_clip, &paddle_rect);


    if (!ball.stuck)
    {
        ball.position += ball.velocity * state->delta_time;

        if (ball.position.x <= 0.0f)
        {
            ball.velocity.x = -ball.velocity.x;
            ball.position.x = 0.0f;
        }
        else if (ball.position.x + ball.radius >= state->SCREEN_WIDTH )
        {
            ball.velocity.x = -ball.velocity.x;
            ball.position.x = ball.position.x - ball.radius;
        }

        if (ball.position.y <= 0)
        {
            ball.velocity.y = -ball.velocity.y;
            ball.position.y = 0.0f;
        }

        if (ball.position.y + ball.radius >= state->SCREEN_WIDTH)
        {
            isGameOver = true;
        }
    }


    bricks.erase(destroyed_bricks, bricks.end());

    Collision result = CheckCollision(ball, paddle_rect);
    if (!ball.stuck && std::get<0>(result))
    {
        float centerBoard = player.position.x + paddle_rect.w / 2.0f;
        float distance = (ball.position.x - ball.radius) - centerBoard;
        float percentage = distance / (paddle_rect.w / 2.0f);

        float strength = 2.0f;
        glm::vec2 oldVelocity = ball.velocity;
        ball.velocity.x = 100.0f * percentage * strength;
        ball.velocity.y = -ball.velocity.y;
        ball.velocity = glm::normalize(ball.velocity) * glm::length(oldVelocity);
        ball.velocity.y = -1.0f * abs(ball.velocity.y);
    }

    SDL_Rect ball_rect;
    ball_rect.x = ball.position.x;
    ball_rect.y = ball.position.y;
    ball_rect.w = ball_rect_clip.w * 2;
    ball_rect.h = ball_rect_clip.h * 2;
    SDL_RenderCopy(renderer, breakout_spritesheet, &ball_rect_clip, &ball_rect);


    for (auto& b : bricks)
    {
        brick_rect.x = b.position.x;
        brick_rect.y = b.position.y;
        brick_rect.w = brick_sprite_rects[b.type].w * 2;
        brick_rect.h = brick_sprite_rects[b.type].h * 2;

        Collision collision = CheckCollision(ball, brick_rect);
        if (std::get<0>(collision))
        {
            b.destroyed = true;

            Direction dir = std::get<1>(collision);
            glm::vec2 diff_vector = std::get<2>(collision);
            if (dir == Direction::LEFT || dir == Direction::RIGHT)
            {
                ball.velocity.x = -ball.velocity.x;

                float penetration = ball.radius - std::abs(diff_vector.x);

                if (dir == Direction::LEFT)
                {
                    ball.position.x += penetration;
                }
                else
                {
                    ball.position.x -= penetration;
                }
            }
            else
            {
                ball.velocity.y = -ball.velocity.y;
                float penetration = ball.radius - std::abs(diff_vector.y);
                if (dir == Direction::UP)
                    ball.position.y -= penetration;
                else
                    ball.position.y += penetration;
            }
        }

        SDL_RenderCopy(renderer, breakout_spritesheet, &brick_sprite_rects[b.type], &brick_rect);
    }

    
}

void GameOnEvent(GameState* state)
{
}

void GameOnShutdown(GameState* state)
{
    
}