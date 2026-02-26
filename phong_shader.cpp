#include "phong_shader.h"

#include <algorithm>
#include <cmath>

PhongShader::PhongShader(const vec4 &light_eye, const Model &m, const mat4 &modelview)
    : model(m), mv(modelview) {
    mv_it = mv.invert_transpose();
    light_dir = normalized(light_eye);
}

vec4 PhongShader::vertex(int face, int vert) {
    varying_uv[vert] = model.uv(face, vert);
    varying_nrm[vert] = mv_it * model.normal(face, vert);
    vec4 gl_Position = mv * model.vert(face, vert);
    tri[vert] = gl_Position;
    return Perspective * gl_Position;
}

void PhongShader::precompute_face() {
    darboux_valid = false;
    if (!model.has_normalmap()) return;

    vec2 duv1 = varying_uv[1] - varying_uv[0];
    vec2 duv2 = varying_uv[2] - varying_uv[0];
    double det = duv1.x * duv2.y - duv2.x * duv1.y;
    if (std::abs(det) < 1e-10) return;

    double inv_det = 1.0 / det;
    vec4 e1 = tri[1] - tri[0];
    vec4 e2 = tri[2] - tri[0];

    tangent = normalized((e1 * duv2.y - e2 * duv1.y) * inv_det);
    bitangent = normalized((e2 * duv1.x - e1 * duv2.x) * inv_det);
    darboux_valid = true;
}

std::pair<bool, cv::Vec3b> PhongShader::fragment(const vec3 &bar) const {
    vec2 uv = varying_uv[0] * bar[0] + varying_uv[1] * bar[1] + varying_uv[2] * bar[2];

    vec4 n;
    if (darboux_valid) {
        vec4 interp_nrm = normalized(varying_nrm[0] * bar[0] + varying_nrm[1] * bar[1] + varying_nrm[2] * bar[2]);
        mat<4, 4> D = {tangent, bitangent, interp_nrm, {0, 0, 0, 1}};
        n = normalized(D.transpose() * model.normal(uv));
    } else {
        n = normalized(varying_nrm[0] * bar[0] + varying_nrm[1] * bar[1] + varying_nrm[2] * bar[2]);
    }

    vec4 l = light_dir;
    vec4 r = normalized(n * (n * l) * 2 - l);

    double ambient = 0.15;
    double diffuse = std::max(0.0, n * l);
    double spec_val = model.has_specular() ? (0.5 + 2.0 * model.sample_specular(uv)) : 0.3;
    double specular = spec_val * std::pow(std::max(r.z, 0.0), 35);

    double intensity = ambient + diffuse + specular;

    cv::Vec3b tex_color = model.sample_diffuse(uv);
    cv::Vec3b frag_color;
    for (int c = 0; c < 3; c++) {
        frag_color[c] = std::min(255, static_cast<int>(tex_color[c] * intensity));
    }
    return {false, frag_color};
}
