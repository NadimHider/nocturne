#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "defines.h"
#include "api.h"
#include "typography.h"

static int current_size = 1;
static int fail_font_load = 0;
int CFG_getFontSize(void) { return current_size; }
int CFG_getFontStyle(void) { return 0; }

TTF_Font *__real_TTF_OpenFont(const char *path, int points);
TTF_Font *__wrap_TTF_OpenFont(const char *path, int points)
{
    return fail_font_load ? NULL : __real_TTF_OpenFont(path, points);
}

int GFX_loadSystemFont(const char *path);

static void check_fit(const char *input, size_t capacity, int width)
{
    char output[64];
    memset(output, 0x7f, sizeof(output));
    int measured = GFX_truncateText(font.small, input, output, capacity, width, 0);
    assert((unsigned char)output[capacity] == 0x7f);
    if (!capacity) return;
    assert(strlen(output) < capacity);
    assert(measured <= width);
    int actual = 0;
    if (output[0]) assert(TTF_SizeUTF8(font.small, output, &actual, NULL) == 0);
    assert(actual <= width);
}

int main(void)
{
    assert(TTF_Init() == 0);
    const char *path = "skeleton/SYSTEM/res/DepartureMono-Regular.otf";
    for (current_size = 0; current_size < 4; current_size++) {
        assert(GFX_loadSystemFont(path) == 0);
        assert(TTF_FontHeight(font.large) == TTF_FontHeight(font.small));
        assert(TTF_FontHeight(font.medium) == TTF_FontHeight(font.small));
        assert(Typography_points(current_size, 0) == Typography_points(current_size, 2));
        int row = GFX_getRowHeight(font.small, 20);
        assert(row >= TTF_FontHeight(font.small) + SCALE1(8));
        int y = GFX_centerTextY(font.small, 0, row);
        assert(y >= SCALE1(4));
        assert(y + TTF_FontHeight(font.small) <= row - SCALE1(4));
        const char *inputs[] = {"", "A", "AB", "...", "A very long game title", "Pok\xc3\xa9mon \xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e"};
        for (size_t i = 0; i < sizeof(inputs) / sizeof(inputs[0]); i++)
            for (size_t capacity = 0; capacity < 32; capacity++)
                for (int width = 0; width < 100; width += 7)
                    check_fit(inputs[i], capacity, width);
    }
    assert(Typography_points(1, 2) == 10);
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, 640, 480, 32, SDL_PIXELFORMAT_ARGB8888);
    assert(surface);
    SDL_Color white = {255, 255, 255, 255};
    int leading = TTF_FontLineSkip(font.tiny);
    assert(GFX_blitWrappedText(font.tiny, "First\nSecond", 600, 2, white, surface, 0) == leading * 2);
    assert(GFX_blitWrappedText(font.tiny, "A long description that wraps over the screen", 100, 2, white, surface, 0) == leading * 2);
    assert(GFX_blitWrappedText(font.tiny, "No width", 0, 2, white, surface, 0) == 0);
    SDL_FreeSurface(surface);
    // A failed live font change must preserve all working font handles.
    TTF_Font *previous = font.small;
    // Inject failure for both the requested font and its fallback, regardless
    // of whether the developer has desktop resources installed already.
    fail_font_load = 1;
    assert(GFX_loadSystemFont(path) == -1);
    assert(font.small == previous);
    TTF_CloseFont(font.large);
    TTF_CloseFont(font.medium);
    TTF_CloseFont(font.small);
    TTF_CloseFont(font.tiny);
    TTF_CloseFont(font.micro);
    TTF_Quit();
    puts("typography: ok");
    return 0;
}
