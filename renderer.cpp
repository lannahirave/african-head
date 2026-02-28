#include <algorithm>
#include <cmath>
#include "renderer.h"

// глобальний стан рендерера

mat4 ModelView, Viewport, Perspective;
std::vector<double> zbuffer;

// пункт 5: налаштування проєкцій
void lookat(const vec3 &eye, const vec3 &center, const vec3 &up) {
    vec3 n = normalized(eye - center);
    vec3 l = normalized(cross(up, n));
    vec3 m = normalized(cross(n, l));
    ModelView = mat4{{{l.x, l.y, l.z, 0},
                      {m.x, m.y, m.z, 0},
                      {n.x, n.y, n.z, 0},
                      {0,   0,   0,   1}}} *
                mat4{{{1, 0, 0, -center.x},
                      {0, 1, 0, -center.y},
                      {0, 0, 1, -center.z},
                      {0, 0, 0, 1}}};
}

// перспективна проєкція
void init_perspective(double f) {
    Perspective = {{{1, 0, 0,     0},
                    {0, 1, 0,     0},
                    {0, 0, 1,     0},
                    {0, 0, -1/f,  1}}};
}

// аксонометрична (ортографічна) проєкція
void init_orthographic() {
    Perspective = mat4::identity();
}

void init_viewport(int x, int y, int w, int h) {
    Viewport = {{{w / 2., 0,     0, x + w / 2.},
                 {0,      h / 2., 0, y + h / 2.},
                 {0,      0,     1, 0},
                 {0,      0,     0, 1}}};
}

// пункт 6: Z-буфер
void init_zbuffer(int width, int height) {
    zbuffer.assign(width * height, -1e9);
}

// пункт 2: малювання відрізків — алгоритм Брезенгема
void draw_line(int x0, int y0, int x1, int y1, cv::Mat &framebuffer, const cv::Vec3b &color) {
    bool steep = false;
    if (std::abs(x0 - x1) < std::abs(y0 - y1)) {
        std::swap(x0, y0);
        std::swap(x1, y1);
        steep = true;
    }
    if (x0 > x1) {
        std::swap(x0, x1);
        std::swap(y0, y1);
    }

    int dx = x1 - x0;
    int dy = y1 - y0;
    int derror2 = std::abs(dy) * 2;
    int error2 = 0;
    int y = y0;
    int ystep = (y1 > y0) ? 1 : -1;

    for (int x = x0; x <= x1; x++) {
        int px = steep ? y : x;
        int py = steep ? x : y;
        if (px >= 0 && px < framebuffer.cols && py >= 0 && py < framebuffer.rows) {
            framebuffer.at<cv::Vec3b>(py, px) = color;
        }
        error2 += derror2;
        if (error2 > dx) {
            y += ystep;
            error2 -= dx * 2;
        }
    }
}

// очищення Z-буфера
void clear_zbuffer(int width, int height) {
    zbuffer.assign(width * height, -1e9);
}

// пункти 3 + 6: растеризація трикутника із Z-буфером
void rasterize(const Triangle &clip, const IShader &shader, cv::Mat &framebuffer) {
    int width = framebuffer.cols;
    int height = framebuffer.rows;

    // нормалізовані координати пристрою (perspective divide)
    vec4 ndc[3] = {clip[0] / clip[0].w,
                   clip[1] / clip[1].w,
                   clip[2] / clip[2].w};

    // екранні координати
    vec2 screen[3] = {(Viewport * ndc[0]).xy(),
                      (Viewport * ndc[1]).xy(),
                      (Viewport * ndc[2]).xy()};

    const mat<3,3> ABC = {{
        {screen[0].x, screen[0].y, 1.0},
        {screen[1].x, screen[1].y, 1.0},
        {screen[2].x, screen[2].y, 1.0}
    }};
    if (ABC.det() < 1) return; // відсікання зворотних і вироджених трикутників

    // bounding box
    int xmin = std::max<int>(std::min({screen[0].x, screen[1].x, screen[2].x}), 0);
    int xmax = std::min<int>(std::max({screen[0].x, screen[1].x, screen[2].x}), width - 1);
    int ymin = std::max<int>(std::min({screen[0].y, screen[1].y, screen[2].y}), 0);
    int ymax = std::min<int>(std::max({screen[0].y, screen[1].y, screen[2].y}), height - 1);

    for (int x = xmin; x <= xmax; x++) {
        for (int y = ymin; y <= ymax; y++) {
            vec3 bc_screen = ABC.invert_transpose() * vec3{(double)x, (double)y, 1.0}; // барицентричні координати
            if (bc_screen.x < 0 || bc_screen.y < 0 || bc_screen.z < 0) continue;

            // перевірка глибини через Z-буфер (п.6)
            double z = ndc[0].z * bc_screen[0] + ndc[1].z * bc_screen[1] + ndc[2].z * bc_screen[2];
            int idx = x + y * width;
            if (z <= zbuffer[idx]) continue;

            vec3 bc_clip = {bc_screen.x / clip[0].w, bc_screen.y / clip[1].w, bc_screen.z / clip[2].w};
            bc_clip = bc_clip / (bc_clip.x + bc_clip.y + bc_clip.z);

            auto [discard, color] = shader.fragment(bc_clip);
            if (discard) continue;

            zbuffer[idx] = z;
            framebuffer.at<cv::Vec3b>(y, x) = color;
        }
    }
}
