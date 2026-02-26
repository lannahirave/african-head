#include <iostream>
#include <chrono>
#include <cmath>
#include <vector>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include "renderer.h"
#include "model.h"

// пункти 7+8: шейдер Фонга з текстурою та normal mapping

struct PhongShader : IShader {
    const Model &model;
    vec4 light_dir;                   // напрямок світла в координатах камери
    vec2 varying_uv[3];              // UV на вершинах (записує VS, читає FS)
    vec4 varying_nrm[3];             // нормалі на вершинах
    vec4 tri[3];                     // трикутник у view-координатах
    mat4 mv;                         // кешована матриця ModelView
    mat4 mv_it;                      // кешований обернений транспонований ModelView

    // тангенти precompute на трикутник (щоб уникнути інверсії матриці на кожному пікселі)
    vec4 tangent, bitangent;
    bool darboux_valid = false;

    PhongShader(const vec4 &light_eye, const Model &m, const mat4 &modelview)
        : model(m), mv(modelview) {
        mv_it = mv.invert_transpose();
        light_dir = normalized(light_eye);
    }

    vec4 vertex(int face, int vert) override {
        varying_uv[vert] = model.uv(face, vert);
        varying_nrm[vert] = mv_it * model.normal(face, vert);
        vec4 gl_Position = mv * model.vert(face, vert);
        tri[vert] = gl_Position;
        return Perspective * gl_Position;
    }

    // викликається після обробки 3 вершин грані: рахує tangent frame
    void precompute_face() {
        darboux_valid = false;
        if (!model.has_normalmap()) return;

        vec2 duv1 = varying_uv[1] - varying_uv[0];
        vec2 duv2 = varying_uv[2] - varying_uv[0];
        double det = duv1.x * duv2.y - duv2.x * duv1.y;
        if (std::abs(det) < 1e-10) return;

        double inv_det = 1.0 / det;
        vec4 e1 = tri[1] - tri[0];
        vec4 e2 = tri[2] - tri[0];

        tangent   = normalized((e1 * duv2.y - e2 * duv1.y) * inv_det);
        bitangent = normalized((e2 * duv1.x - e1 * duv2.x) * inv_det);
        darboux_valid = true;
    }

    std::pair<bool, cv::Vec3b> fragment(const vec3 &bar) const override {
        // Інтерполяція UV
        vec2 uv = varying_uv[0] * bar[0] + varying_uv[1] * bar[1] + varying_uv[2] * bar[2];

        // освітлення з normal mapping
        vec4 n;
        if (darboux_valid) {
            vec4 interp_nrm = normalized(varying_nrm[0] * bar[0] + varying_nrm[1] * bar[1] + varying_nrm[2] * bar[2]);
            mat<4, 4> D = {tangent, bitangent, interp_nrm, {0, 0, 0, 1}};
            n = normalized(D.transpose() * model.normal(uv));
        } else {
            n = normalized(varying_nrm[0] * bar[0] + varying_nrm[1] * bar[1] + varying_nrm[2] * bar[2]);
        }

        // модель освітлення Фонга
        vec4 l = light_dir;
        vec4 r = normalized(n * (n * l) * 2 - l);

        double ambient  = 0.15;
        double diffuse  = std::max(0., n * l);
        double spec_val = model.has_specular() ? (0.5 + 2.0 * model.sample_specular(uv)) : 0.3;
        double specular = spec_val * std::pow(std::max(r.z, 0.), 35);

        double intensity = ambient + diffuse + specular;

        // п.8: вибірка кольору з diffuse-текстури
        cv::Vec3b tex_color = model.sample_diffuse(uv);
        cv::Vec3b frag_color;
        for (int c = 0; c < 3; c++) {
            frag_color[c] = std::min(255, (int)(tex_color[c] * intensity));
        }
        return {false, frag_color};
    }
};

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
    const vec3 eye{0, 0, 3};
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
