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
    std::uint32_t flags = SDL_WINDOW_RESIZABLE;
    bool on_top = false;
    bool bordered = true;
};

struct Camera {
    SDL_CameraID camera_id;
    SDL_CameraID *p_camera_ids = nullptr;
    SDL_Camera *p_camera = nullptr;
    std::int32_t count_cameras;
    std::int32_t selected_index = 0;
};

struct CameraApp {
    SDL_Event event;
    Window window;
    Camera camera;
    SDL_Window *p_sdlwindow = nullptr;
    SDL_Renderer *p_renderer = nullptr;
    SDL_Texture *p_texture = nullptr;
    SDL_Texture *p_ascii_texture = nullptr;
    bool quit = false;
    bool ascii_mode = false;
    bool reset_to_larger_side_of_window = false;
    bool reset_to_smaller_side_of_window = false;
    bool fullsize_snapshot_requested = false;
    bool screensize_snapshot_requested = false;

    ~CameraApp () {
        if (p_renderer) SDL_DestroyRenderer(p_renderer);
        if (p_texture) SDL_DestroyTexture(p_texture);
        if (p_ascii_texture) SDL_DestroyTexture(p_ascii_texture);
        if (camera.p_camera) SDL_CloseCamera(camera.p_camera);
        if (camera.p_camera_ids) SDL_free(camera.p_camera_ids);
        if (p_sdlwindow) SDL_DestroyWindow(p_sdlwindow);
        SDL_Quit();
    }
};

void change_camera(CameraApp *app, std::int8_t delta) {
    SDL_Log("Camera is %s, with id %u", SDL_GetCameraName(app->camera.p_camera_ids[app->camera.selected_index]), app->camera.camera_id);

    SDL_CloseCamera(app->camera.p_camera);

    if (app->p_texture) {
        SDL_DestroyTexture(app->p_texture);
        app->p_texture = nullptr;
    }

    app->camera.selected_index += delta;
    app->camera.camera_id = app->camera.p_camera_ids[app->camera.selected_index];

    app->camera.p_camera = SDL_OpenCamera(app->camera.camera_id, NULL);
    if (app->camera.p_camera == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Could not open camera: %s", SDL_GetError());
        exit(1);
    }
    SDL_Log("Changed camera to %s, with id %u", SDL_GetCameraName(app->camera.p_camera_ids[app->camera.selected_index]), app->camera.camera_id);
}

void take_fullsize_snapshot(CameraApp *app, SDL_Surface *p_camera_surface) {

    SDL_Surface *p_snapshot = nullptr;

    if (app->ascii_mode && app->p_ascii_texture) {
        // No CPU-side surface exists for the ASCII render; it only lives as
        // draw calls baked into the p_ascii_texture. Bind it as the render
        // target so we can read its pixels back into a surface.
        SDL_SetRenderTarget(app->p_renderer, app->p_ascii_texture);
        p_snapshot = SDL_RenderReadPixels(app->p_renderer, NULL);
        // Then reset the render target
        SDL_SetRenderTarget(app->p_renderer, NULL);
    }
    else {
        p_snapshot = SDL_DuplicateSurface(p_camera_surface);
    }

    if (p_snapshot) {
        std::chrono::time_point time_now = std::chrono::system_clock::now();
        std::string filename = "snapshot_" + std::format("{:%Y-%m-%d_%H:%M:%S}", time_now) + ".bmp";

        if (SDL_SaveBMP(p_snapshot, filename.c_str())) {
            SDL_Log("Snapshot saved successfully to %s", filename.c_str());
        }
        else {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to save snapshot: %s", SDL_GetError());
        }
        SDL_DestroySurface(p_snapshot);
    }
    app->fullsize_snapshot_requested = false;
}

void reset_window_size(CameraApp *app, SDL_Surface *p_cam_surface, bool to_larger_side) {

    // Get camera surface size ratio
    int sw = p_cam_surface->w;
    int sh = p_cam_surface->h;

    // Get window surface size ratio
    int ww;
    int wh;
    if (SDL_GetWindowSize(app->p_sdlwindow, &ww, &wh) == false) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Could not get window size: %s", SDL_GetError());
        return;
    }

    float normal_ratio = static_cast<float>(sw) / sh;
    float current_ratio = static_cast<float>(ww) / wh;

    if (to_larger_side) {
        // When window is too wide -> Increase height
        if (current_ratio > normal_ratio) {
            wh = static_cast<int>(ww / normal_ratio);
        }
        // When window is too tall -> Increase width
	    else {
	        ww = static_cast<int>(wh * normal_ratio);
        }
	    app->reset_to_larger_side_of_window = false;
	}
    else {
        // When window is too wide -> Decrease width
        if (current_ratio > normal_ratio) {
            ww = static_cast<int>(wh * normal_ratio);
        }
        // When window is too tall -> Decrease height
        else {
            wh = static_cast<int>(ww / normal_ratio);
        }
	    app->reset_to_smaller_side_of_window = false;
    }

    // Set window size to camera size
    if (SDL_SetWindowSize(app->p_sdlwindow, ww, wh) == false) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Could not reset window size: %s", SDL_GetError());
        return;
    }

}

void take_screensize_snapshot(CameraApp *app, const SDL_Rect *rect) {
    SDL_Surface *p_snapshot = SDL_RenderReadPixels(app->p_renderer, rect);
    if (p_snapshot) {
        std::chrono::time_point time_now = std::chrono::system_clock::now();
        std::string filename = "snapshot_" + std::format("{:%Y-%m-%d_%H:%M:%S}", time_now) + ".bmp";

        if (SDL_SaveBMP(p_snapshot, filename.c_str())) {
            SDL_Log("Snapshot saved successfully to %s", filename.c_str());
        }
        else {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to save snapshot: %s", SDL_GetError());
        }
        SDL_DestroySurface(p_snapshot);
    }
    app->screensize_snapshot_requested = false;
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
        break;
    case SDL_SCANCODE_B:
        app->window.bordered = !app->window.bordered;
        SDL_SetWindowBordered(app->p_sdlwindow, app->window.bordered);
        break;
    case SDL_SCANCODE_F:
        app->fullsize_snapshot_requested = true;
        break;
    case SDL_SCANCODE_S:
        app->screensize_snapshot_requested = true;
        break;
    case SDL_SCANCODE_A:
        app->ascii_mode = !app->ascii_mode;
        break;
    case SDL_SCANCODE_R:
        if (app->event.key.mod & SDL_KMOD_SHIFT) {
            app->reset_to_smaller_side_of_window = true;
        }
        else {
            app->reset_to_larger_side_of_window = true;
        }
        break;
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

        while (SDL_PollEvent(&app->event)) {
            handle_event(app);
        }

        // Clear every frame
        SDL_SetRenderDrawColor(app->p_renderer, 18, 18, 18, 255);
        SDL_RenderClear(app->p_renderer);

        std::uint64_t timestamp_ns;
        SDL_Surface* p_cam_surface = SDL_AcquireCameraFrame(app->camera.p_camera, &timestamp_ns);

        // If no texture, create new texture from new surface data
        // Else if new surface dimensions is different from previous texture dimensions, destroy texture and create a new one from new surface
        // Else update previous texture values with new surface data
        if (p_cam_surface) {

            if (app->reset_to_larger_side_of_window) {
                reset_window_size(app, p_cam_surface, true);
            }

            if (app->reset_to_smaller_side_of_window) {
                reset_window_size(app, p_cam_surface, false);
            }

            if (!app->ascii_mode) {

                if (!app->p_texture) {
                    app->p_texture = SDL_CreateTextureFromSurface(app->p_renderer, p_cam_surface);
                }
                else {
                    float tw, th;
                    SDL_GetTextureSize(app->p_texture, &tw, &th);

                    if (p_cam_surface->w != (int)tw || p_cam_surface->h != (int)th) {
                        SDL_DestroyTexture(app->p_texture);
                        app->p_texture = SDL_CreateTextureFromSurface(app->p_renderer, p_cam_surface);
                    }
                    else {
                        SDL_UpdateTexture(app->p_texture, NULL, p_cam_surface->pixels, p_cam_surface->pitch);
                    }
                }

                if (app->fullsize_snapshot_requested) {
                    take_fullsize_snapshot(app, p_cam_surface);
                }

            }

            else if (app->ascii_mode) {

                // Convert pixel buffer into standard SDL_PIXELFORMAT_RGB24
                SDL_Surface *p_rgb_surface = SDL_ConvertSurface(p_cam_surface, SDL_PIXELFORMAT_RGB24);

                if (p_rgb_surface) {

                    std::string ascii_palette = " .:-=+*#%@"; // Darkest to brightest

                    int font_w = 8;
                    int font_h = 8;

                    std::uint8_t *p_rgb_pixels = (std::uint8_t*)p_rgb_surface->pixels;
                    int rgb_pitch = p_rgb_surface->pitch;

                    float atw = 0;
                    float ath = 0;

                    // Fetch the ascii target texture size if it exists already
                    if (app->p_ascii_texture) {
                        SDL_GetTextureSize(app->p_ascii_texture, &atw, &ath);
                    }
                    // Recreate the ascii target texture if it is missing or if the size of the camera frame has changed
                    if (!app->p_ascii_texture || p_rgb_surface->w != (int)atw || p_rgb_surface->h != (int)ath) {
                        if (app->p_ascii_texture) {
                            SDL_DestroyTexture(app->p_ascii_texture);
                        }
                        app->p_ascii_texture = SDL_CreateTexture(app->p_renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, p_rgb_surface->w, p_rgb_surface->h);
                    }

                    // Draw the ascii frame into the texture
                    SDL_SetRenderTarget(app->p_renderer, app->p_ascii_texture);
                    SDL_SetRenderDrawColor(app->p_renderer, 18, 18, 18, 255);
                    SDL_RenderClear(app->p_renderer);
                    SDL_SetRenderDrawColor(app->p_renderer, 255, 255, 255, 255);

                    for (int y = 0; y < p_rgb_surface->h; y += font_h) {
                        for (int x = 0; x < p_rgb_surface->w; x += font_w) {
                            // Calculate precise offsets for the 24-bit RGB pixel data
                            std::uint8_t *p_converted_pixel = p_rgb_pixels + (y * rgb_pitch) + (x * 3);
                            std::uint8_t converted_r = p_converted_pixel[0];
                            std::uint8_t converted_g = p_converted_pixel[1];
                            std::uint8_t converted_b = p_converted_pixel[2];

                            // Grayscale luminance formula
                            std::uint8_t brightness = (std::uint8_t)(0.299f * converted_r + 0.587f * converted_g + 0.114f * converted_b);
                            int char_index = (brightness * (ascii_palette.length() - 1)) / 255;

                            char glyph[2] = { ascii_palette[char_index], '\0' };

                            SDL_RenderDebugText(app->p_renderer, (float)x, (float)y, glyph);
                        }
                    }

                    if (app->fullsize_snapshot_requested) {
                        take_fullsize_snapshot(app, p_rgb_surface);
                    }

                    // Hand rendering back to the window
                    SDL_SetRenderTarget(app->p_renderer, NULL);

                    SDL_DestroySurface(p_rgb_surface);

                }
                else {
                    SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Could not create SDL_PIXELFORMAT_RGB24 surface: %s", SDL_GetError());
                }
            }

            SDL_ReleaseCameraFrame(app->camera.p_camera, p_cam_surface);
        }

        // Render the stored texture
        if (!app->ascii_mode) {
            if (app->p_texture) {

                SDL_RenderTexture(app->p_renderer, app->p_texture, NULL, NULL);

                if (app->screensize_snapshot_requested) {
                    take_screensize_snapshot(app, NULL);
                }
            }
        }
        else {
            if (app->p_ascii_texture) {
                SDL_RenderTexture(app->p_renderer, app->p_ascii_texture, NULL, NULL);

                if (app->screensize_snapshot_requested) {
                    take_screensize_snapshot(app, NULL);
                }
            }
        }

        SDL_RenderPresent(app->p_renderer);

        std::chrono::microseconds duration(16667);
        std::this_thread::sleep_for(duration);

    }

}

int main() {
    CameraApp app;

    SDL_Init(SDL_INIT_CAMERA | SDL_INIT_VIDEO);

    init_window(&app);
    init_camera(&app);
    init_camera_renderer(&app);
    camera_render_loop(&app);

    return 0;
}
