#include <algorithm>
#include <cmath>
#include "renderer.h"

// ─── Глобальний стан рендерера ────────────────────────────────────────────────

mat4 ModelView, Viewport, Perspective;
std::vector<double> zbuffer;

// ─── Пункт 5: налаштування проєкцій (5 балів) ────────────────────────────────

void lookat(const vec3 &eye, const vec3 &center, const vec3 &up) {
    vec3 n = normalized(eye - center);        // вперед (камера дивиться вздовж -n)
    vec3 l = normalized(cross(up, n));        // вправо
    vec3 m = normalized(cross(n, l));         // скоригований вектор "вгору"
    ModelView = mat4{{{l.x, l.y, l.z, 0},
                      {m.x, m.y, m.z, 0},
                      {n.x, n.y, n.z, 0},
                      {0,   0,   0,   1}}} *
                mat4{{{1, 0, 0, -center.x},
                      {0, 1, 0, -center.y},
                      {0, 0, 1, -center.z},
                      {0, 0, 0, 1}}};
}

// Перспективна проєкція: далекі об'єкти виглядають меншими
void init_perspective(double f) {
    Perspective = {{{1, 0, 0,     0},
                    {0, 1, 0,     0},
                    {0, 0, 1,     0},
                    {0, 0, -1/f,  1}}};
}

// Аксонометрична (ортографічна) проєкція: без перспективних спотворень
void init_orthographic() {
    Perspective = mat4::identity();
}

void init_viewport(int x, int y, int w, int h) {
    Viewport = {{{w / 2., 0,     0, x + w / 2.},
                 {0,      h / 2., 0, y + h / 2.},
                 {0,      0,     1, 0},
                 {0,      0,     0, 1}}};
}

// ─── Пункт 6: Z-буфер (5 балів) ──────────────────────────────────────────────

void init_zbuffer(int width, int height) {
    zbuffer.assign(width * height, -1e9);
}

// ─── Пункт 2: малювання відрізків — алгоритм Брезенгема (5 балів) ───────────

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

// ─── Очищення Z-буфера (без перевиділення пам'яті) ───────────────────────────

void clear_zbuffer(int width, int height) {
    if ((int)zbuffer.size() != width * height)
        zbuffer.resize(width * height);
    std::fill(zbuffer.begin(), zbuffer.end(), -1e9);
}

// ─── Пункти 3 + 6: растеризація трикутника із Z-буфером (5+5 балів) ─────────

void rasterize(const Triangle &clip, const IShader &shader, cv::Mat &framebuffer) {
    int width = framebuffer.cols;
    int height = framebuffer.rows;

    // Нормалізовані координати пристрою (perspective divide)
    vec4 ndc[3] = {clip[0] / clip[0].w,
                   clip[1] / clip[1].w,
                   clip[2] / clip[2].w};

    // Екранні координати
    vec2 screen[3] = {(Viewport * ndc[0]).xy(),
                      (Viewport * ndc[1]).xy(),
                      (Viewport * ndc[2]).xy()};

    // Попередній розрахунок коефакторів барицентричної матриці:
    // ключова оптимізація, що прибирає інверсію 3x3 на кожному пікселі
    double sx0 = screen[0].x, sy0 = screen[0].y;
    double sx1 = screen[1].x, sy1 = screen[1].y;
    double sx2 = screen[2].x, sy2 = screen[2].y;

    double det = (sx1 - sx0) * (sy2 - sy0) - (sx2 - sx0) * (sy1 - sy0);
    if (det < 1) return; // відсікання зворотних і вироджених трикутників

    double inv_det = 1.0 / det;

    // Ряди коефакторів: bc[i] = (ci0*x + ci1*y + ci2) * inv_det
    double c00 = (sy1 - sy2) * inv_det, c01 = (sx2 - sx1) * inv_det, c02 = (sx1*sy2 - sy1*sx2) * inv_det;
    double c10 = (sy2 - sy0) * inv_det, c11 = (sx0 - sx2) * inv_det, c12 = (sy0*sx2 - sx0*sy2) * inv_det;
    double c20 = (sy0 - sy1) * inv_det, c21 = (sx1 - sx0) * inv_det, c22 = (sx0*sy1 - sy0*sx1) * inv_det;

    // Попередній розрахунок ваг для perspective-correct інтерполяції
    double inv_w0 = 1.0 / clip[0].w, inv_w1 = 1.0 / clip[1].w, inv_w2 = 1.0 / clip[2].w;
    double nz0 = ndc[0].z, nz1 = ndc[1].z, nz2 = ndc[2].z;

    // Обмежувальний прямокутник, обрізаний межами екрана
    int xmin = std::max<int>(std::min({sx0, sx1, sx2}), 0);
    int xmax = std::min<int>(std::max({sx0, sx1, sx2}), width - 1);
    int ymin = std::max<int>(std::min({sy0, sy1, sy2}), 0);
    int ymax = std::min<int>(std::max({sy0, sy1, sy2}), height - 1);

    for (int y = ymin; y <= ymax; y++) {
        // Частини bc, що залежать лише від y
        double bc0_base = c01 * y + c02;
        double bc1_base = c11 * y + c12;
        double bc2_base = c21 * y + c22;
        int row_offset = y * width;

        for (int x = xmin; x <= xmax; x++) {
            double bc0 = c00 * x + bc0_base;
            double bc1 = c10 * x + bc1_base;
            double bc2 = c20 * x + bc2_base;

            if (bc0 < 0 || bc1 < 0 || bc2 < 0) continue;

            // Перевірка глибини через Z-буфер (п.6)
            double z = bc0 * nz0 + bc1 * nz1 + bc2 * nz2;
            int idx = x + row_offset;
            if (z <= zbuffer[idx]) continue;

            // Perspective-correct барицентрична інтерполяція
            double pc0 = bc0 * inv_w0, pc1 = bc1 * inv_w1, pc2 = bc2 * inv_w2;
            double pc_sum_inv = 1.0 / (pc0 + pc1 + pc2);
            vec3 bc_clip = {pc0 * pc_sum_inv, pc1 * pc_sum_inv, pc2 * pc_sum_inv};

            auto [discard, color] = shader.fragment(bc_clip);
            if (discard) continue;

            zbuffer[idx] = z;
            framebuffer.at<cv::Vec3b>(y, x) = color;
        }
    }
}
