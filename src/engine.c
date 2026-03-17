/* 
Copyright (C) 2026 qwertdim <dmitry.pimenoff@gmail.com>
This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by
the Free Software Foundation; version 2 of the License.
*/

#include <stdbool.h>
#include "engine.h"

int SDL_rand(int);
float SDL_randf(void);

void new_tile(Tile* ctx)
{
    char empty_tile_index[SIZE_BOARD*SIZE_BOARD];
    int count = 0;
    for (int i = 0; i != SIZE_BOARD*SIZE_BOARD; ++i) {
        if (ctx[i].state == HIDDEN) {
            empty_tile_index[count++] = i;
        }
    }
    int i = empty_tile_index[SDL_rand(count)];
    ctx[i].state = GENER;
    ctx[i].num = SDL_randf() > 0.9f;
}


void tiles_initialize(Tile *tiles)
{
    for (int i = 0; i != SIZE_BOARD*SIZE_BOARD; ++i){
        tiles[i].state = HIDDEN;
        tiles[i].num = 0;
        tiles[i].pos = i;
    }
    new_tile(tiles);
    new_tile(tiles);
}


bool merge(Tile **line)
{
    bool merged = false;
    Tile *empty_tiles[SIZE_BOARD];
    Tile *tile;
    int start_empty = 0, count_empy = 0;
    for (int left = 0; left != SIZE_BOARD; ++left) {
        if (line[left]->state != HIDDEN) {
            line[left]->state = IDLE;
            int right = left + 1;
            while (right < SIZE_BOARD && line[right]->state == HIDDEN)
                right += 1;
            if (right < SIZE_BOARD && line[left]->num == line[right]->num) {
                if (start_empty != count_empy) {
                    tile = empty_tiles[start_empty++];
                    tile->num = line[left]->num;
                    line[left]->state = HIDDEN;
                    empty_tiles[count_empy++] = line[left];
                } else {
                    tile = line[left];
                }
                tile->state = MERGE;
                ++tile->num;
                tile->from = line[right]->pos;
                line[right]->state = HIDDEN;
                merged = true;
            } else if (start_empty != count_empy) {
                tile = empty_tiles[start_empty++];
                tile->state = MOVE;
                tile->num = line[left]->num;
                tile->from = line[left]->pos;
                line[left]->state = HIDDEN;
                empty_tiles[count_empy++] = line[left];
                merged = true;
            }
        } else {
            empty_tiles[count_empy++] = line[left];
        }
    }
    return merged;
}


bool check_lose(Tile *ctx)
{
    int i, j;
    for (i = 0; i != SIZE_BOARD*SIZE_BOARD; ++i) {
        if (ctx[i].state == HIDDEN) {
            return false;
        }
    }
    for (i = 0; i != SIZE_BOARD*SIZE_BOARD; i += SIZE_BOARD) {
        for (j = 0; j != SIZE_BOARD-1; ++j) {
            if (ctx[i+j].num == ctx[i+j+1].num) {
                return false;
            }
        }
    }
    for (i = 0; i != SIZE_BOARD; ++i) {
        for (j = 0; j < SIZE_BOARD*(SIZE_BOARD-1); j += SIZE_BOARD) {
            if (ctx[i+j].num == ctx[i+j+SIZE_BOARD].num) {
                return false;
            }
        }
    }
    return true;
}


bool move(Tile *ctx, int dir)
{
    bool moved = false;
    Tile *line[SIZE_BOARD];
    switch (dir)
    {
    case DIR_LEFT:
    case DIR_RIGHT:
        for (int i = 0; i != SIZE_BOARD*SIZE_BOARD; i += SIZE_BOARD) {
            for (int j = 0; j != SIZE_BOARD; ++j) {
                line[j] = dir == DIR_LEFT ? ctx+i+j : ctx+i+SIZE_BOARD-1-j;
            }
            if (merge(line)) {
                moved = true;
            }
        }
        break;
    case DIR_UP:
    case DIR_DOWN:
        for (int i = 0; i != SIZE_BOARD; ++i) {
            for (int j = 0; j != SIZE_BOARD; ++j) {
                line[j] = dir == DIR_UP ? ctx+i+(SIZE_BOARD*j) : ctx+i+(SIZE_BOARD*(SIZE_BOARD-1-j));
            }
            if (merge(line)) {
                moved = true;
            }
        }
        break;
    default:
        break;
    }
    if (moved) {
        new_tile(ctx);
    }
    return moved;
}


bool check_won(Tile *ctx)
{
    for (int i = 0; i != SIZE_BOARD*SIZE_BOARD; ++i) {
        if (ctx[i].num == 10) {
            return true;
        }
    }
    return false;
}