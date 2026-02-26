#pragma once
#include <vector>
#include <opencv2/core.hpp>
#include "geometry.h"

// Інтерфейс шейдера
struct IShader {
    virtual ~IShader() = default;
    virtual vec4 vertex(int face, int vert) = 0;
    virtual std::pair<bool, cv::Vec3b> fragment(const vec3 &bar) const = 0;
};

// Глобальний стан рендерера

extern mat4 ModelView, Viewport, Perspective;
extern std::vector<double> zbuffer;

// Пункт 5: проєкції
void lookat(const vec3 &eye, const vec3 &center, const vec3 &up);
void init_perspective(double f);
void init_orthographic();
void init_viewport(int x, int y, int w, int h);
void init_zbuffer(int width, int height);
void clear_zbuffer(int width, int height);

// Пункт 2: малювання відрізків (Брезенгем)
void draw_line(int x0, int y0, int x1, int y1, cv::Mat &framebuffer, const cv::Vec3b &color);

// Пункти 3 + 6: растеризація трикутників із Z-буфером
typedef vec4 Triangle[3];
void rasterize(const Triangle &clip, const IShader &shader, cv::Mat &framebuffer);
