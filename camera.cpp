#include "SDL3/SDL_log.h"
#include <iostream>
#include <cstdint>
#include <SDL3/SDL.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_camera.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_messagebox.h>

struct Window {
    std::string name = "Camera";
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t w = 800;
    std::int32_t h = 600;
    std::uint32_t flags = 0;
};

struct CameraApp
{
    Window window;
    bool quit = false;
    SDL_Window *p_sdlwindow;
    SDL_Camera *p_camera;
    SDL_Event event;
};

void handle_event(CameraApp *app) {
    switch (app->event.type) {
        case SDL_EVENT_CAMERA_DEVICE_APPROVED:
            SDL_Log("Camera approved!");
            break;
        case SDL_EVENT_CAMERA_DEVICE_DENIED:
            SDL_Log("Camera denied!");

            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Camera permission denied!", "User denied access to the camera!", app->p_sdlwindow);
            app->quit = true;
            break;
        case SDL_EVENT_QUIT:
            app->quit = true;
            break;
        default:
            break;
    }
}

int main() {
    CameraApp app;

    SDL_Init(SDL_INIT_CAMERA | SDL_INIT_VIDEO | SDL_WINDOW_METAL);

    app.p_sdlwindow = SDL_CreateWindow(app.window.name.c_str(), app.window.w, app.window.h, app.window.flags);
    if (app.p_sdlwindow == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Could not create window: %s\n", SDL_GetError());
        return 1;
    }

    std::int32_t count_cameras;
    SDL_CameraID *p_camera_id = SDL_GetCameras(&count_cameras);
    if (p_camera_id == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Could not find cameras: %s\n", SDL_GetError());
    }

    std::cout << "number of cameras: " << count_cameras << "\n";
    SDL_CameraID cam_id = *p_camera_id;

    app.p_camera = SDL_OpenCamera(cam_id, NULL);
    if (app.p_camera == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Could not open camera: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Log("Using SDL camera driver: %s", SDL_GetCurrentCameraDriver());

    while (app.quit != true) {
        app.event = {}; // TODO: make sure this zero initializes
        while (SDL_PollEvent(&app.event)) {
            handle_event(&app);
        }
    }

    SDL_DestroyWindow(app.p_sdlwindow);
    SDL_Quit();
    return 0;
}
