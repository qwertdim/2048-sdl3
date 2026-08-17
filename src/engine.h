/*
Copyright (C) 2026 qwertdim <dmitry.pimenoff@gmail.com>
This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by
the Free Software Foundation; version 2 of the License.
*/

#pragma once

#define SIZE_BOARD 4

enum Direction
{
    DIR_RIGHT,
    DIR_LEFT,
    DIR_UP,
    DIR_DOWN
};

enum TileState
{
    HIDDEN,
    GENER,
    GENER2,
    IDLE,
    MOVE,
    MERGE,
    MERGE2
};

typedef struct Tile
{
    char state;
    char num;
    char pos;
    char from;
} Tile;

bool move(Tile *ctx, int dir);
void tiles_initialize(Tile *tiles);
bool check_lose(Tile *ctx);
bool check_won(Tile *ctx);
