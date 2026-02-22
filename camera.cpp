#include <SDL3/SDL.h>
#include <SDL3/SDL_camera.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_messagebox.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_scancode.h>
#include <SDL3/SDL_surface.h>
#include <SDL3/SDL_video.h>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <thread>

struct Window {
    std::string name = "Camera";
    std::int32_t x;
    std::int32_t y;
    std::int32_t w = 800;
    std::int32_t h = 600;
    std::uint32_t flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_BORDERLESS;
    bool on_top = false;
};

struct Camera {
    SDL_CameraID camera_id;
    SDL_CameraID *p_camera_ids;
    SDL_Camera *p_camera;
    std::int32_t count_cameras;
    std::int32_t selected_index = 0;
};

struct CameraApp {
    Window window;
    Camera camera;
    bool quit = false;
    SDL_Window *p_sdlwindow;
    SDL_Renderer *p_renderer;
    SDL_Event event;

    ~CameraApp () {
        if (p_renderer) SDL_DestroyRenderer(p_renderer);
        if (camera.p_camera) SDL_CloseCamera(camera.p_camera);
        if (camera.p_camera_ids) SDL_free(camera.p_camera_ids);
        if (p_sdlwindow) SDL_DestroyWindow(p_sdlwindow);
        SDL_Quit();
    }
};

void change_camera(CameraApp *app, std::int8_t delta) {
    SDL_Log("Camera is %s, with id %u", SDL_GetCameraName(app->camera.p_camera_ids[app->camera.selected_index]), app->camera.camera_id);

    SDL_CloseCamera(app->camera.p_camera);

    app->camera.selected_index += delta;
    app->camera.camera_id = app->camera.p_camera_ids[app->camera.selected_index];

    app->camera.p_camera = SDL_OpenCamera(app->camera.camera_id, NULL);
    if (app->camera.p_camera == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Could not open camera: %s", SDL_GetError());
        exit(1);
    }
    SDL_Log("Changed camera to %s, with id %u", SDL_GetCameraName(app->camera.p_camera_ids[app->camera.selected_index]), app->camera.camera_id);
}

void handle_keydown_keybinds(CameraApp *app) {
    switch (app->event.key.scancode) {
        case SDL_SCANCODE_UP:
            if (app->camera.selected_index > 0) {
                change_camera(app, -1);
            }
            break;
        case SDL_SCANCODE_DOWN:
            if (app->camera.selected_index < app->camera.count_cameras - 1) {
                change_camera(app, 1);
            }
            break;
        case SDL_SCANCODE_T:
            app->window.on_top = !app->window.on_top;
            SDL_SetWindowAlwaysOnTop(app->p_sdlwindow, app->window.on_top);
        default:
            break;
    }
}

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
        case SDL_EVENT_WINDOW_RESIZED:
            break;
        case SDL_EVENT_QUIT:
            app->quit = true;
            break;
        case SDL_EVENT_KEY_DOWN:
            handle_keydown_keybinds(app);
            break;
        default:
            break;
    }
}

void init_window(CameraApp *app) {
    app->p_sdlwindow = SDL_CreateWindow(app->window.name.c_str(), app->window.w, app->window.h, app->window.flags);
    if (app->p_sdlwindow == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Could not create window: %s", SDL_GetError());
        exit(1);
    }
}

void init_camera(CameraApp *app) {
    app->camera.p_camera_ids = SDL_GetCameras(&app->camera.count_cameras);
    if (app->camera.p_camera_ids == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Could not find cameras: %s", SDL_GetError());
    }

    SDL_Log("number of cameras: %d", app->camera.count_cameras);
    if (app->camera.count_cameras == 0) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "No cameras found");
        exit(1);
    }

    app->camera.camera_id = app->camera.p_camera_ids[app->camera.selected_index];
    app->camera.p_camera = SDL_OpenCamera(app->camera.camera_id, NULL);
    if (app->camera.p_camera == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Could not open camera: %s", SDL_GetError());
        exit(1);
    }

    SDL_Log("Using SDL camera driver: %s", SDL_GetCurrentCameraDriver());
}

void init_camera_renderer(CameraApp *app) {
    app->p_renderer = SDL_CreateRenderer(app->p_sdlwindow, NULL);
    SDL_SetRenderDrawColor(app->p_renderer, 18, 18, 18, 0);
    SDL_RenderClear(app->p_renderer);
    SDL_RenderPresent(app->p_renderer);
}

void camera_render_loop(CameraApp *app) {

    while (app->quit != true) {
        app->event = {}; // TODO: make sure this zero initializes
        while (SDL_PollEvent(&app->event)) {
            handle_event(app);
        }

        std::uint64_t timestamp_ns;
        SDL_Surface* p_cam_surface = SDL_AcquireCameraFrame(app->camera.p_camera, &timestamp_ns);
        if (p_cam_surface) {
            SDL_Texture* p_texture = SDL_CreateTextureFromSurface(app->p_renderer, p_cam_surface);
            SDL_RenderTexture(app->p_renderer, p_texture, NULL, NULL);

            SDL_RenderPresent(app->p_renderer);

            SDL_DestroyTexture(p_texture);
            SDL_ReleaseCameraFrame(app->camera.p_camera, p_cam_surface);
        }

        std::chrono::microseconds duration(16667);
        std::this_thread::sleep_for(duration);

    }

}

int main() {
    CameraApp app;

    SDL_Init(SDL_INIT_CAMERA | SDL_INIT_VIDEO | SDL_WINDOW_METAL);

    init_window(&app);
    init_camera(&app);
    init_camera_renderer(&app);
    camera_render_loop(&app);

    return 0;
}
