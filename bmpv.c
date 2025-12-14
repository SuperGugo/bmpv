#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#pragma pack(push, 1)
typedef struct
{
    uint16_t bfType;
    uint32_t bfSize;
    uint16_t bfReserved1;
    uint16_t bfReserved2;
    uint32_t bfOffBits;
} BITMAPFILEHEADER;

typedef struct
{
    uint32_t biSize;
    int32_t biWidth;
    int32_t biHeight;
    uint16_t biPlanes;
    uint16_t biBitCount;
    uint32_t biCompression;
    uint32_t biSizeImage;
    int32_t biXPelsPerMeter;
    int32_t biYPelsPerMeter;
    uint32_t biClrUsed;
    uint32_t biClrImportant;
} BITMAPINFOHEADER;
#pragma pack(pop)

int width;
int height;
int depth;
uint8_t **rows;

bool parseBmp(char *filename)
{
    FILE *fptr = fopen(filename, "rb");
    if (!fptr)
    {
        printf("file not found");
        return 1;
    }

    BITMAPFILEHEADER fileHeader;
    BITMAPINFOHEADER infoHeader;

    fread(&fileHeader, sizeof(fileHeader), 1, fptr);
    fread(&infoHeader, sizeof(infoHeader), 1, fptr);
    width = infoHeader.biWidth;
    height = infoHeader.biHeight;
    depth = infoHeader.biBitCount;
    rows = malloc(height * sizeof(uint8_t *));

    fseek(fptr, fileHeader.bfOffBits, SEEK_SET);

    int row_padded = (width * 3 + 3) & (~3);

    for (int y = 0; y < height; y++)
    {
        rows[y] = malloc(row_padded);
        fread(rows[y], 1, row_padded, fptr);
    }

    fclose(fptr);
    return 0;
}

void render(SDL_Window *window, SDL_Renderer *renderer)
{
    SDL_Surface *surface = SDL_CreateSurface(width, height, SDL_PIXELFORMAT_RGB24);
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            uint8_t b = rows[y][x * 3 + 0];
            uint8_t g = rows[y][x * 3 + 1];
            uint16_t r = rows[y][x * 3 + 2];
            SDL_WriteSurfacePixel(surface, x, height - y, r, g, b, 0xFF);
        }
    }
    SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_RenderTexture(renderer, tex, NULL, NULL);
    SDL_DestroyTexture(tex);
    SDL_DestroySurface(surface);
    SDL_RenderPresent(renderer);
}

int main(int argc, char **argv)
{
    char *filename;
    if (argc >= 2)
        filename = argv[1];
    else {
        printf("no filename input, aborting\n");
        return 1;
    }
    if (parseBmp(filename))
    {
        printf(", aborting\n");
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow("bmpv", 500, 500, SDL_WINDOW_RESIZABLE);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL);
    SDL_SetRenderLogicalPresentation(renderer, width, height, SDL_LOGICAL_PRESENTATION_LETTERBOX);

    if (window == NULL)
    {
        printf("window couldnt be created, aborting\n");
        return 1;
    }

    render(window, renderer);

    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);
    bool exit = false;
    while (!exit)
    {
        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_EVENT_QUIT)
            {
                exit = true;
            }
            else if (e.type == SDL_EVENT_WINDOW_RESIZED)
            {
                SDL_WindowEvent wev = e.window;
                render(window, renderer);

                printf("%dx%d\n", wev.data1, wev.data2);
            }
        }
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
