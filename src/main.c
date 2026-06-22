#include <SDL3/SDL.h>

const int LOGICAL_WIDTH = 320;
const int LOGICAL_HEIGHT = 200;
const int INITIAL_WINDOW_WIDTH = 1280;
const int INITIAL_WINDOW_HEIGHT = 800;

int main(int argc, char* argv[]) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return -1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "F-15 SE2 EX v0.0.1", 
        INITIAL_WINDOW_WIDTH, 
        INITIAL_WINDOW_HEIGHT, 
        SDL_WINDOW_RESIZABLE
    );
    if (!window) {
        SDL_Log("Window creation failed: %s", SDL_GetError());
        SDL_Quit();
        return -1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, NULL);
    if (!renderer) {
        SDL_Log("Renderer creation failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }
    SDL_SetRenderVSync(renderer, 1);

    // SDL_LOGICAL_PRESENTATION_STRETCH will stretch your 320x200 to fill the window.
    if (!SDL_SetRenderLogicalPresentation(renderer, LOGICAL_WIDTH, LOGICAL_HEIGHT, 
                                          SDL_LOGICAL_PRESENTATION_STRETCH)) {
        SDL_Log("SetRenderLogicalPresentation failed: %s", SDL_GetError());
    }

    bool quit = false;
    SDL_Event event;

    while (!quit) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                quit = true;
            } else if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_ESCAPE) {
                    quit = true;
                }
            }
        }

        // Clear screen (Dark Blue)
        SDL_SetRenderDrawColor(renderer, 20, 20, 40, 255);
        SDL_RenderClear(renderer);

        // Draw a red rectangle inside our 320x200 space
        SDL_FRect rect = { 40.0f, 40.0f, 100.0f, 60.0f };
        SDL_SetRenderDrawColor(renderer, 255, 50, 50, 255);
        SDL_RenderFillRect(renderer, &rect);

        // Draw a green pixel at the center
        SDL_SetRenderDrawColor(renderer, 50, 255, 50, 255);
        SDL_RenderPoint(renderer, 160.0f, 100.0f);

        SDL_RenderPresent(renderer);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}