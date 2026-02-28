#include <iostream>
#include <chrono>
#include <cmath>
#include <vector>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include "renderer.h"
#include "model.h"
#include "phong_shader.h"

// основна програма з анімованим циклом рендеру
int main() {
    constexpr int WIDTH  = 800;
    constexpr int HEIGHT = 800;

    // завантаження моделей (голова + внутрішня частина очей)
    const std::vector<std::string> obj_files = {
        "models/african_head.obj",
        "models/african_head_eye_inner.obj",
    };
    std::vector<Model> models;
    models.reserve(obj_files.size());
    for (const auto &f : obj_files) {
        models.emplace_back(f);
        if (models.back().nfaces() == 0) {
            std::cerr << "Failed to load " << f << std::endl;
            return 1;
        }
    }

    // параметри камери і світла
    const vec3 eye{0, 0, 1};
    const vec3 center{0, 0, 0};
    const vec3 up{0, 1, 0};
    // напрямок світла: зліва, трохи зверху і трохи спереду
    const vec3 light_world{-1.0, 0.35, 0.25};

    // колір фону (BGR)
    const cv::Scalar bg_color(209, 195, 177);

    // стан сцени
    bool use_perspective = true;
    bool paused = false;
    double angle = 0.0;
    const double rotation_speed = 0.6; // рад/с

    const std::string window_name = "African Head 3D Renderer";
    cv::namedWindow(window_name, cv::WINDOW_AUTOSIZE);

    auto prev_time = std::chrono::high_resolution_clock::now();
    while (true) {
        // дельта часу між кадрами
        auto now = std::chrono::high_resolution_clock::now();
        double dt = std::chrono::duration<double>(now - prev_time).count();
        prev_time = now;

        // оновлення кута обертання
        if (!paused) {
            angle += rotation_speed * dt;
        }

        // налаштування матриць
        // п.4: 3D-перетворення — lookat + обертання
        lookat(eye, center, up);
        mat4 view = ModelView; // камера без моделі
        mat4 rot = rotation_y(angle);
        ModelView = ModelView * rot;

        // напрямок світла у координатах камери (світло фіксоване над камерою).
        vec4 light_eye = normalized(view * vec4{light_world.x, light_world.y, light_world.z, 0.});

        // проєкція
        if (use_perspective)
            init_perspective(norm(eye - center));
        else
            init_orthographic();

        init_viewport(WIDTH / 16, HEIGHT / 16, WIDTH * 7 / 8, HEIGHT * 7 / 8);

        // очищення буферів
        cv::Mat framebuffer(HEIGHT, WIDTH, CV_8UC3);
        framebuffer.setTo(bg_color);
        init_zbuffer(WIDTH, HEIGHT);

        // рендер усіх моделей
        for (const auto &m : models) {
            PhongShader shader(light_eye, m, ModelView);

            #pragma omp parallel for firstprivate(shader) schedule(dynamic)
            for (int f = 0; f < m.nfaces(); f++) {
                Triangle clip = {shader.vertex(f, 0),
                                 shader.vertex(f, 1),
                                 shader.vertex(f, 2)};
                shader.precompute_face();
                rasterize(clip, shader, framebuffer);
            }
        }

        // вертикальний переворот (в OpenCV Y=0 зверху, у рендері знизу)

        cv::flip(framebuffer, framebuffer, 0);

        // HUD-оверлей
        std::string proj_text = use_perspective ? "Projection: Perspective" : "Projection: Axonometric";
        cv::rectangle(framebuffer, cv::Point(0, 0), cv::Point(WIDTH, 36), cv::Scalar(20, 20, 20), cv::FILLED);
        cv::rectangle(framebuffer, cv::Point(0, HEIGHT - 34), cv::Point(WIDTH, HEIGHT), cv::Scalar(20, 20, 20), cv::FILLED);
        cv::putText(framebuffer, proj_text, cv::Point(10, 24),
                    cv::FONT_HERSHEY_SIMPLEX, 0.55, cv::Scalar(80, 255, 80), 1, cv::LINE_AA);
        cv::putText(framebuffer, "[P] toggle projection  [S] pause  [ESC] quit", cv::Point(10, HEIGHT - 12),
                    cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(230, 230, 230), 1, cv::LINE_AA);

        // відображення кадру
        cv::imshow(window_name, framebuffer);

        // вихід по ESC або закриттю вікна
        int key = cv::waitKeyEx(1);
        // ESC = 27
        if (key == 27) break;
        if (cv::getWindowProperty(window_name, cv::WND_PROP_VISIBLE) < 1) break;

        if (key == 'p' || key == 'P') {
            use_perspective = !use_perspective;
        }
        if (key == 's' || key == 'S') {
            paused = !paused;
        }
    }

    cv::destroyAllWindows();
    return 0;
}

