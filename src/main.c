/* 
Copyright (C) 2026 qwertdim <dmitry.pimenoff@gmail.com>
This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by
the Free Software Foundation; version 2 of the License.
*/

#include <stddef.h>
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "engine.h"
#ifdef __EMSCRIPTEN__
#include <emscripten/fetch.h>
static void* data = NULL;
static bool flag_attr = false;

void load_success(emscripten_fetch_t * fetch) {
    SDL_memcpy(data, fetch->data, fetch->numBytes);
    flag_attr = true;
    emscripten_fetch_close(fetch);
}

void load_failed(emscripten_fetch_t * fetch) {
    SDL_Log("Downloading %s failed, HTTP failure status code: %d.", fetch->url, fetch->status);
    flag_attr = false;
    emscripten_fetch_close(fetch);
}

void save_success(emscripten_fetch_t * fetch) {
    emscripten_fetch_close(fetch);
}

void save_failed(emscripten_fetch_t * fetch) {
    SDL_Log("IDB store failed.");
    emscripten_fetch_close(fetch);
}
#endif

#define DURATION 150
#define TILE_SIZE_IN_PIXELS 128.0f
#define SDL_WINDOW_WIDTH  576
#define SDL_WINDOW_HEIGHT 1024

typedef struct Sound {
    Uint8 *wav_data;
    Uint32 wav_data_len;
} Sound;

typedef struct AppState
{
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    SDL_Texture *bgtile_texture;
    SDL_Texture *tiles_texture;
    SDL_Texture *bg_texture;
    Uint64 stop_anim_at;
    SDL_Rect safe_area;
    SDL_AudioStream *stream;
    SDL_AudioStream *music_stream;
    Sound sounds[4];
    SDL_AudioDeviceID audio_device;
    Tile tiles[SIZE_BOARD*SIZE_BOARD];
    Uint32 score;
    Uint32 best;
    bool game_over;
    bool win;
    bool continue_game;
    bool pressed_button;
    bool mute;
} AppState;


static void draw_score(AppState* as, SDL_FPoint* point, Sint32 score)
{
    const float coord_x[] = {10.f, 32.f, 45.f, 65.f, 84.f, 106.f, 125.f, 145.f, 163.f, 185.f, 204.f};
    Sint32 num = score;
    float width = 0.f;
    while (num) {
        Sint32 i = num % 10;
        width += coord_x[i+1] - coord_x[i];
        num /= 10; 
    }

    SDL_FRect dst = {point->x + width/2.f, point->y, 0.f, 32.f};
    SDL_FRect src = {0.f, 475.f, 0.f, 32.f};
    num = score;
    if (num == 0) {
        src.x = coord_x[0] - 2.f;
        src.w = dst.w = coord_x[1] - coord_x[0] + 4.f;
        dst.x -= dst.w/2.f;
        SDL_RenderTexture(as->renderer, as->texture, &src, &dst);
    }
    while (num) {
        Sint32 i = num % 10;
        src.x = coord_x[i] + 9.f*i - 2.f;
        src.w = dst.w = coord_x[i+1] - coord_x[i] + 4.f;
        dst.x -= dst.w - 4.f;
        SDL_RenderTexture(as->renderer, as->texture, &src, &dst);
        num /= 10; 
    }
}


SDL_AppResult SDL_AppIterate(void *appstate)
{
    AppState *as = (AppState *)appstate;
    const Uint64 now = SDL_GetTicks();
    const SDL_FRect board = {32.f, 480.f, TILE_SIZE_IN_PIXELS*SIZE_BOARD, TILE_SIZE_IN_PIXELS*SIZE_BOARD};
    SDL_FRect src_rect = {0.f, 0.f, 512.f, 318.f};
    SDL_FRect dst_rect = {32.f, 32.f, 512.f, 318.f};
    float scale;
    const Tile *end_tile = as->tiles + SIZE_BOARD*SIZE_BOARD;
    Uint32 score = 0;
    static Uint32 delta = 0;

    if (!as->mute && SDL_GetAudioStreamQueued(as->music_stream) < 40960) {
        static int music_index = 0;
        int to_put = as->sounds[3].wav_data_len - music_index < 40960 ? as->sounds[3].wav_data_len - music_index : 40960;
        SDL_PutAudioStreamData(as->music_stream, as->sounds[3].wav_data + music_index, to_put);
        music_index += to_put;
        if (music_index >= as->sounds[3].wav_data_len) {
            music_index = 0;
        }
    }

    // clear screen
    SDL_RenderClear(as->renderer);

    // draw background
    SDL_SetRenderLogicalPresentation(as->renderer, SDL_WINDOW_WIDTH, SDL_WINDOW_HEIGHT, SDL_LOGICAL_PRESENTATION_OVERSCAN);
    SDL_RenderTexture(as->renderer, as->bg_texture, NULL, NULL);
    SDL_SetRenderLogicalPresentation(as->renderer, SDL_WINDOW_WIDTH, SDL_WINDOW_HEIGHT, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    dst_rect.y += as->safe_area.y;
    SDL_RenderTexture(as->renderer, as->texture, &src_rect, &dst_rect);
    if (as->mute) {
        SDL_FRect src = {461.f, 471.f, 38.f, 40.f};
        SDL_FRect dst = {493.f, 53.f+as->safe_area.y, 38.f, 40.f};
        SDL_RenderTexture(as->renderer, as->texture, &src, &dst);
    }
    
 
    // draw board
    SDL_RenderFillRect(as->renderer, &board);
    SDL_RenderTextureTiled(as->renderer, as->bgtile_texture, NULL, 1.f, &board);
    dst_rect.w = dst_rect.h = src_rect.w = src_rect.h = TILE_SIZE_IN_PIXELS;
    for (Tile *tile = as->tiles; tile != end_tile; ++tile) {
        dst_rect.x = (float)(tile->pos & 3) * TILE_SIZE_IN_PIXELS + 32.f;
        dst_rect.y = (float)(tile->pos >> 2) * TILE_SIZE_IN_PIXELS + 480.f;
        src_rect.x = (float)(tile->num & 3) * TILE_SIZE_IN_PIXELS;
        src_rect.y = (float)(tile->num >> 2) * TILE_SIZE_IN_PIXELS;
        switch (tile->state)
        {
        case HIDDEN:
            break;
        case IDLE:
            score += tile->num * (1 << (tile->num + 1));
            SDL_RenderTexture(as->renderer, as->tiles_texture, &src_rect, &dst_rect);
            break;
        case MOVE:
            score += tile->num * (1 << (tile->num + 1));
            if (now < as->stop_anim_at - DURATION) {
                scale = (float)(DURATION - (as->stop_anim_at - DURATION - now)) / DURATION;
                float from_x = (float)(tile->from & 3) * TILE_SIZE_IN_PIXELS + 32.f;
                float from_y = (float)(tile->from >> 2) * TILE_SIZE_IN_PIXELS + 480.f;
                dst_rect.x = from_x - (from_x - dst_rect.x) * scale;
                dst_rect.y = from_y - (from_y - dst_rect.y) * scale;
            } else {
                tile->state = IDLE;
            }
            SDL_RenderTexture(as->renderer, as->tiles_texture, &src_rect, &dst_rect);
            break;
        case MERGE:
            score += tile->num * (1 << (tile->num + 1));
            if (now < as->stop_anim_at - DURATION) {
                scale = (float)(DURATION - (as->stop_anim_at - DURATION - now)) / DURATION;
                float from_x = (float)(tile->from & 3) * TILE_SIZE_IN_PIXELS + 32.f;
                float from_y = (float)(tile->from >> 2) * TILE_SIZE_IN_PIXELS + 480.f;
                dst_rect.x = from_x - (from_x - dst_rect.x) * scale;
                dst_rect.y = from_y - (from_y - dst_rect.y) * scale;
                src_rect.x = (float)((tile->num - 1) & 3) * TILE_SIZE_IN_PIXELS;
                src_rect.y = (float)((tile->num - 1) >> 2) * TILE_SIZE_IN_PIXELS;
            } else if (now < as->stop_anim_at) {
                scale = 1.25f - 0.5f*(float)SDL_abs(DURATION / 2 - (as->stop_anim_at - now)) / DURATION;
                dst_rect.w *= scale;
                dst_rect.h *= scale;
                dst_rect.x += (TILE_SIZE_IN_PIXELS - dst_rect.w)/2.f;
                dst_rect.y += (TILE_SIZE_IN_PIXELS - dst_rect.h)/2.f;
            } else {
                tile->state = IDLE;
            }
            SDL_RenderTexture(as->renderer, as->tiles_texture, &src_rect, &dst_rect);
            dst_rect.w = dst_rect.h = TILE_SIZE_IN_PIXELS;
            break;
        case GENER:
            if (now < as->stop_anim_at - DURATION) {
                break;
            }
            if (now < as->stop_anim_at) {
                scale = (float)(DURATION - (as->stop_anim_at - now)) / DURATION;
                dst_rect.w *= scale;
                dst_rect.h *= scale;
                dst_rect.x += (TILE_SIZE_IN_PIXELS - dst_rect.w)/2.f;
                dst_rect.y += (TILE_SIZE_IN_PIXELS - dst_rect.h)/2.f;
            } else {
                tile->state = IDLE;
            }
            SDL_RenderTexture(as->renderer, as->tiles_texture, &src_rect, &dst_rect);
            dst_rect.w = dst_rect.h = TILE_SIZE_IN_PIXELS;
            break;
        default:
            break;
        }
    }

    //draw score
    SDL_FPoint p_score = {122.f, 208.f+as->safe_area.y};
    draw_score(as, &p_score, score);

    //draw best score
    if (score > as->best) {
        as->best = score;
    }
    p_score.x += 332.f;
    draw_score(as, &p_score, as->best);

    //draw delta score
    delta += score - as->score;
    if (delta && now < as->stop_anim_at) {
        as->score = score;
        p_score.x -= 280.f;
        p_score.y = 260.f+as->safe_area.y + 32.f * (2.f - (float)(as->stop_anim_at - now) / (2*DURATION));
        SDL_SetTextureColorMod(as->texture, 255, 145, 102);
        SDL_SetRenderScale(as->renderer, 0.8125f, 0.8125f);
        draw_score(as, &p_score, delta);
        SDL_SetTextureColorMod(as->texture, 255, 255, 255);
        SDL_SetRenderScale(as->renderer, 1.f, 1.f);
    } else {
        delta = 0;
    }
    
    //draw message
    if ((as->win && !as->continue_game) || as->game_over) {
        SDL_RenderFillRect(as->renderer, &board);
        SDL_FRect src = {0.f, 400.f, 512.f, 66.f};
        if (as->game_over){
            src.y -= 75.f;
        }
        SDL_FRect dst = {32.f, 701.f, 512.f, 66.f};
        SDL_RenderTexture(as->renderer, as->texture, &src, &dst);
    }

    //draw pressed button
    if (as->pressed_button) {
        if (now < as->stop_anim_at - DURATION) {
            SDL_FRect dst = {211.f, 134.f+as->safe_area.y, 154.f, 171.f};
            SDL_FRect src = {179.f, 102.f, 154.f, 171.f};
            scale = 1.05f - 0.1f*(float)SDL_abs(DURATION/2 - (as->stop_anim_at - now - DURATION)) / DURATION;
            dst.h *= scale;
            SDL_RenderTexture(as->renderer, as->texture, &src, &dst);
        } else {
            as->pressed_button = false;
        }
    }

    SDL_RenderPresent(as->renderer);
    return SDL_APP_CONTINUE;
}


SDL_AppResult ReadGameData(AppState *appstate)
{
    const char *fileNames[] = {
        "Text.png", "Tile_Background.png", "Tiles.png", "Background.png",
        "move.wav", "win.wav", "lose.wav", "sample.wav"
    };
    SDL_Surface *surface = NULL;

    SDL_Storage *title = SDL_OpenTitleStorage(NULL, 0);
    if (title == NULL) {
        SDL_Log("Couldn't open Title Storage: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    while (!SDL_StorageReady(title)) {
        SDL_Delay(1);
    }

    void* dst;
    Uint64 dstLen = 0;
    SDL_AudioSpec spec;
    int i, j = 0;

    for (i = 0; i != SDL_arraysize(fileNames); ++i) {
        if (SDL_GetStorageFileSize(title, fileNames[i], &dstLen) && dstLen > 0) {
            dst = SDL_malloc(dstLen);
            if (SDL_ReadStorageFile(title, fileNames[i], dst, dstLen)) {
                SDL_IOStream *stream =  SDL_IOFromConstMem(dst, dstLen);
                if (i < 4) {
                    surface = SDL_LoadPNG_IO(stream, true);
                    SDL_Texture **texture = &appstate->texture + i;
                    *texture = SDL_CreateTextureFromSurface(appstate->renderer, surface);
                    SDL_DestroySurface(surface);
                } else {
                    SDL_LoadWAV_IO(stream, true, &spec, &appstate->sounds[j].wav_data, &appstate->sounds[j].wav_data_len);
                    if (!appstate->stream) {
                        appstate->stream = SDL_CreateAudioStream(&spec, NULL);
                        SDL_BindAudioStream(appstate->audio_device, appstate->stream);
                    }
                    ++j;
                }
            } else {
                // Something bad happened!
            }
            SDL_free(dst);
        } else {
            SDL_Log("Couldn't open file: %s", SDL_GetError());
        }
    }
    appstate->music_stream = SDL_CreateAudioStream(&spec, NULL);
    SDL_BindAudioStream(appstate->audio_device, appstate->music_stream);
    SDL_CloseStorage(title);

    return SDL_APP_CONTINUE;
}


bool ReadSave(AppState *appstate)
{
    bool flag = true;
#ifdef __EMSCRIPTEN__
    emscripten_fetch_attr_t attr;
    emscripten_fetch_attr_init(&attr);
    strcpy(attr.requestMethod, "GET");
    attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY | EMSCRIPTEN_FETCH_PERSIST_FILE;
    attr.onsuccess = load_success;
    attr.onerror = load_failed;
    emscripten_fetch(&attr, "/qwertdim/game_2048/save0.sav");
    flag = flag_attr;
#else
    SDL_Storage *user = SDL_OpenUserStorage("qwertdim", "game_2048", 0);
    if (user == NULL) {
        SDL_Log("Couldn't open User Storage: %s", SDL_GetError());
        return false;
    }
    while (!SDL_StorageReady(user)) {
        SDL_Delay(1);
    }
    Uint64 saveLen = 0;
    if (SDL_GetStorageFileSize(user, "save0.sav", &saveLen) && saveLen == sizeof(AppState) - offsetof(AppState, tiles)) {
        void* dst = (void *)appstate->tiles;
        if (!SDL_ReadStorageFile(user, "save0.sav", dst, saveLen)) {
            SDL_Log("Couldn't read storage file: %s", SDL_GetError());
            flag = false;
        }
    } else {
        SDL_Log("Couldn't get file size: %s, or file size %lu != %lu", SDL_GetError(), saveLen, sizeof(AppState) - offsetof(AppState, tiles));
        flag = false;
    }

    SDL_CloseStorage(user);
#endif
    return flag;
}


void WriteSave(AppState *appstate)
{
#ifdef __EMSCRIPTEN__
    emscripten_fetch_attr_t attr;
    emscripten_fetch_attr_init(&attr);
    strcpy(attr.requestMethod, "EM_IDB_STORE");
    attr.attributes = EMSCRIPTEN_FETCH_REPLACE | EMSCRIPTEN_FETCH_PERSIST_FILE;
    attr.requestData = (void *)appstate->tiles;
    attr.requestDataSize = sizeof(AppState) - offsetof(AppState, tiles);
    attr.onsuccess = save_success;
    attr.onerror = save_failed;
    emscripten_fetch(&attr, "/qwertdim/game_2048/save0.sav");
#else
    SDL_Storage *user = SDL_OpenUserStorage("qwertdim", "game_2048", 0);
    if (user == NULL) {
        SDL_Log("Couldn't open User Storage: %s", SDL_GetError());
        return;
    }
    while (!SDL_StorageReady(user)) {
        SDL_Delay(1);
    }

    void *saveData = (void *)appstate->tiles;
    Uint64 saveLen = sizeof(AppState) - offsetof(AppState, tiles);
    if (!SDL_WriteStorageFile(user, "save0.sav", saveData, saveLen)) {
        SDL_Log("Couldn't write storage file: %s", SDL_GetError());
    }
    SDL_CloseStorage(user);
#endif
}


static const struct
{
    const char *key;
    const char *value;
} extended_metadata[] =
{
    { SDL_PROP_APP_METADATA_URL_STRING, "https://qwertdim.itch.io/game-2048" },
    { SDL_PROP_APP_METADATA_CREATOR_STRING, "qwertdim" },
    { SDL_PROP_APP_METADATA_COPYRIGHT_STRING, "© 2026 qwertdim. Licensed under GPLv2" },
    { SDL_PROP_APP_METADATA_TYPE_STRING, "game" }
};


SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    if (!SDL_SetAppMetadata("2048 Game", "1.0.0", "ru.qwertdim.game_2048")) {
        return SDL_APP_FAILURE;
    }

    for (size_t i = 0; i < SDL_arraysize(extended_metadata); ++i) {
        if (!SDL_SetAppMetadataProperty(extended_metadata[i].key, extended_metadata[i].value)) {
            return SDL_APP_FAILURE;
        }
    }
    
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    AppState *as = (AppState *)SDL_calloc(1, sizeof(AppState));
    if (!as) {
        return SDL_APP_FAILURE;
    }

    *appstate = as;
#ifdef __EMSCRIPTEN__
    data = (void *)as->tiles;
#endif

    if (!SDL_CreateWindowAndRenderer("Game 2048", SDL_WINDOW_WIDTH, SDL_WINDOW_HEIGHT, SDL_WINDOW_RESIZABLE, &as->window, &as->renderer)) {
        return SDL_APP_FAILURE;
    }
    SDL_SetRenderVSync(as->renderer, SDL_RENDERER_VSYNC_ADAPTIVE);
    SDL_SetRenderDrawBlendMode(as->renderer, SDL_BLENDMODE_BLEND);

    as->audio_device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
    if (as->audio_device == 0) {
        SDL_Log("Couldn't open audio device: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    
    ReadGameData(as);
    SDL_SetRenderLogicalPresentation(as->renderer, SDL_WINDOW_WIDTH, SDL_WINDOW_HEIGHT, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    SDL_GetRenderSafeArea(as->renderer, &as->safe_area);
    if (as->mute) {
        SDL_PauseAudioStreamDevice(as->stream);
    } else {
        SDL_ResumeAudioStreamDevice(as->stream);
    }

    if (!ReadSave(as)) {
        tiles_initialize(as->tiles);
    }
    as->stop_anim_at = SDL_GetTicks() + 2*DURATION;
    SDL_SetRenderDrawColor(as->renderer, 0, 0, 0, 96);

    return SDL_APP_CONTINUE;
}


static void reset_game(AppState *appstate) {
    if (!appstate->mute) {
        SDL_ClearAudioStream(appstate->stream);
        SDL_PutAudioStreamData(appstate->stream, appstate->sounds[0].wav_data, appstate->sounds[0].wav_data_len);
        SDL_FlushAudioStream(appstate->stream);
    }
    appstate->stop_anim_at = SDL_GetTicks() + 2*DURATION;
    appstate->score = 0;
    tiles_initialize(appstate->tiles);
    appstate->game_over = false;
    appstate->win = false;
    appstate->continue_game = false;
    appstate->pressed_button = true;
}


static void process_move_result(AppState *as)
{
    if (as->mute) {
        if (check_lose(as->tiles)) {
            as->game_over = true;
        }
        if (!as->win && check_won(as->tiles)) {
            as->win = true;
        }
    } else {
        SDL_ClearAudioStream(as->stream);
        SDL_PutAudioStreamData(as->stream, as->sounds[0].wav_data, as->sounds[0].wav_data_len);
        if (check_lose(as->tiles)) {
            SDL_PutAudioStreamData(as->stream, as->sounds[2].wav_data, as->sounds[2].wav_data_len);
            as->game_over = true;
        }
        if (!as->win && check_won(as->tiles)) {
            SDL_PutAudioStreamData(as->stream, as->sounds[1].wav_data, as->sounds[1].wav_data_len);
            as->win = true;
        }
        SDL_FlushAudioStream(as->stream);
    }
    as->stop_anim_at = SDL_GetTicks() + 2*DURATION;
}


static SDL_AppResult handle_key_event_(AppState *appstate, SDL_Scancode key_code)
{
    Tile *ctx = appstate->tiles;
    bool moved = false;
    if (appstate->win) {
        appstate->continue_game = true;
    }
    switch (key_code) {
    /* Quit. */
    // case SDL_SCANCODE_ESCAPE:
    case SDL_SCANCODE_Q:
        return SDL_APP_SUCCESS;
    /* Restart the game as if the program was launched. */
    case SDL_SCANCODE_RIGHT:
        moved = move(ctx, DIR_RIGHT);
        break;
    case SDL_SCANCODE_UP:
        moved = move(ctx, DIR_UP);
        break;
    case SDL_SCANCODE_LEFT:
        moved = move(ctx, DIR_LEFT);
        break;
    case SDL_SCANCODE_DOWN:
        moved = move(ctx, DIR_DOWN);
        break;
    case SDL_SCANCODE_R:
        reset_game(appstate);
        break;
    case SDL_SCANCODE_M:
        appstate->mute = !appstate->mute;
        if (appstate->mute) {
            SDL_PauseAudioStreamDevice(appstate->stream);
        } else {
            SDL_ResumeAudioStreamDevice(appstate->stream);
        }
        break;
    default:
        break;
    }
    if (moved) {
        process_move_result(appstate);
    }
    return SDL_APP_CONTINUE;
}


SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    AppState *as = (AppState *)appstate;
    SDL_ConvertEventToRenderCoordinates(as->renderer, event);
    static SDL_FPoint pressed_point = { };
    bool moved = false;

    switch (event->type) {
    case SDL_EVENT_QUIT:
        return SDL_APP_SUCCESS;
    case SDL_EVENT_WINDOW_RESIZED:
        SDL_GetRenderSafeArea(as->renderer, &as->safe_area);
        break;
    case SDL_EVENT_WILL_ENTER_BACKGROUND:
        WriteSave(as);
        break;
    case SDL_EVENT_KEY_DOWN:
        if (!event->key.repeat) {
            return handle_key_event_(as, event->key.scancode);
        }
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        const SDL_FRect mute_button = {493.f, 53.f+as->safe_area.y, 38.f, 40.f};
        const SDL_FRect button = {211.f, 180.f, 154.f, 171.f};
        pressed_point.x = event->button.x;
        pressed_point.y = event->button.y;
        if (SDL_PointInRectFloat(&pressed_point, &mute_button)) {
            as->mute = !as->mute;
            if (as->mute) {
                SDL_PauseAudioStreamDevice(as->stream);
            } else {
                SDL_ResumeAudioStreamDevice(as->stream);
            }
            break;
        }
        if (SDL_PointInRectFloat(&pressed_point, &button)) {
            reset_game(as);
        }
        if (as->win) {
            as->continue_game = true;
        }
        break;
    case SDL_EVENT_MOUSE_BUTTON_UP:
        float x = event->button.x - pressed_point.x;
        float y = event->button.y - pressed_point.y;
        float mag = SDL_sqrtf(x*x + y*y);
        if (mag > 40.f) {
            x /= mag;
            y /= mag;
            if (x > 0.5f) {
                moved = move(as->tiles, DIR_RIGHT);
            } else if (x < -0.5f) {
                moved = move(as->tiles, DIR_LEFT);
            }else if (y > 0.5f) {
                moved = move(as->tiles, DIR_DOWN);
            } else if (y < -0.5f) {
                moved = move(as->tiles, DIR_UP);
            }
            if (moved) {
                process_move_result(as);
            }
        }
        break;
    default:
        break;
    }
    return SDL_APP_CONTINUE;
}


void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    if (appstate != NULL) {
        AppState *as = (AppState *)appstate;
        WriteSave(appstate);
        SDL_DestroyTexture(as->texture);
        SDL_DestroyTexture(as->tiles_texture);
        SDL_DestroyTexture(as->bgtile_texture);
        SDL_DestroyTexture(as->bg_texture);
        SDL_DestroyAudioStream(as->stream);
        SDL_DestroyAudioStream(as->music_stream);
        for (size_t i = 0; i != SDL_arraysize(as->sounds); ++i) {
            SDL_free(as->sounds[i].wav_data);
        }
        SDL_free(as);
    }
}
