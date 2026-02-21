#include <SDL3/SDL.h>
#include <SDL3/SDL_camera.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_messagebox.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_surface.h>
#include <SDL3/SDL_video.h>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>

struct Window {
    std::string name = "Camera";
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t w = 800;
    std::int32_t h = 600;
    std::uint32_t flags = 0;
};

struct Camera {
    SDL_Camera *p_camera;
    SDL_CameraID *p_camera_ids;
    std::int32_t count_cameras;
    SDL_CameraID camera_id;
};

struct CameraApp {
    Window window;
    Camera camera;
    bool quit = false;
    SDL_Window *p_sdlwindow;
    SDL_Renderer *p_renderer;
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

    app.camera.p_camera_ids = SDL_GetCameras(&app.camera.count_cameras);
    if (app.camera.p_camera_ids == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Could not find cameras: %s\n", SDL_GetError());
    }

    std::cout << "number of cameras: " << app.camera.count_cameras << "\n";
    app.camera.camera_id = *app.camera.p_camera_ids;

    app.camera.p_camera = SDL_OpenCamera(app.camera.camera_id, NULL);
    if (app.camera.p_camera == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Could not open camera: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Log("Using SDL camera driver: %s", SDL_GetCurrentCameraDriver());

    app.p_renderer = SDL_CreateRenderer(app.p_sdlwindow, NULL);
    SDL_SetRenderDrawColor(app.p_renderer, 18, 18, 18, 0);
    SDL_RenderClear(app.p_renderer);
    SDL_RenderPresent(app.p_renderer);


    while (app.quit != true) {
        app.event = {}; // TODO: make sure this zero initializes
        while (SDL_PollEvent(&app.event)) {
            handle_event(&app);
        }

        std::uint64_t timestamp_ns;
        SDL_Surface* p_cam_surface = SDL_AcquireCameraFrame(app.camera.p_camera, &timestamp_ns);
        if (p_cam_surface) {
            SDL_Texture* p_texture = SDL_CreateTextureFromSurface(app.p_renderer, p_cam_surface);
            SDL_RenderTexture(app.p_renderer, p_texture, NULL, NULL);

            SDL_RenderPresent(app.p_renderer);

            SDL_DestroyTexture(p_texture);
            SDL_ReleaseCameraFrame(app.camera.p_camera, p_cam_surface);
        }

        std::chrono::seconds duration(1/60);
        std::this_thread::sleep_for(duration);

    }

    SDL_DestroyWindow(app.p_sdlwindow);
    SDL_Quit();
    return 0;
}
