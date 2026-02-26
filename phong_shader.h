#pragma once

#include "model.h"
#include "renderer.h"

struct PhongShader : IShader {
    const Model &model;
    vec4 light_dir;
    vec2 varying_uv[3];
    vec4 varying_nrm[3];
    vec4 tri[3];
    mat4 mv;
    mat4 mv_it;

    vec4 tangent, bitangent;
    bool darboux_valid = false;

    PhongShader(const vec4 &light_eye, const Model &m, const mat4 &modelview);

    vec4 vertex(int face, int vert) override;
    void precompute_face();
    std::pair<bool, cv::Vec3b> fragment(const vec3 &bar) const override;
};
