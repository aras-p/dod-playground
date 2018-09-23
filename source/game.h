#pragma once

#ifdef __cplusplus
extern "C" {
#endif


#define kMaxSpriteCount 1100000

typedef struct
{
    float posX, posY;
    float scale;
    float colR, colG, colB;
    float sprite;
} sprite_data_t;

void game_initialize(void);
void game_destroy(void);
// returns amount of sprites
int game_update(sprite_data_t* data, double time, float deltaTime);


#ifdef __cplusplus
}
#endif
