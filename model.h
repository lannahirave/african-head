#pragma once
#include <vector>
#include <string>
#include <opencv2/core.hpp>
#include "geometry.h"

// Пункт 1: читання OBJ-моделі з підтримкою текстур
class Model {
    std::vector<vec4> verts_;
    std::vector<vec4> norms_;
    std::vector<vec2> tex_;
    std::vector<int> facet_vrt_;
    std::vector<int> facet_nrm_;
    std::vector<int> facet_tex_;
    cv::Mat diffusemap_;
    cv::Mat normalmap_;
    cv::Mat specularmap_;

    void load_texture(const std::string &filename, const std::string &suffix, cv::Mat &tex);

public:
    Model(const std::string &filename);

    int nverts() const;
    int nfaces() const;

    vec4 vert(int i) const;
    vec4 vert(int iface, int nthvert) const;
    vec4 normal(int iface, int nthvert) const;
    vec4 normal(const vec2 &uv) const;
    vec2 uv(int iface, int nthvert) const;

    cv::Vec3b sample_diffuse(const vec2 &uv) const;
    cv::Vec3b sample_normal(const vec2 &uv) const;
    double sample_specular(const vec2 &uv) const;

    bool has_diffuse() const;
    bool has_normalmap() const;
    bool has_specular() const;
};
