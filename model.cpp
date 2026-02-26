#include <fstream>
#include <sstream>
#include <iostream>
#include <cstring>
#include "model.h"
// Власний завантажувач TGA у cv::Mat (для підтримки текстур у форматі .tga)

#pragma pack(push,1)
struct TGAHeader {
    uint8_t  idlength = 0;
    uint8_t  colormaptype = 0;
    uint8_t  datatypecode = 0;
    uint16_t colormaporigin = 0;
    uint16_t colormaplength = 0;
    uint8_t  colormapdepth = 0;
    uint16_t x_origin = 0;
    uint16_t y_origin = 0;
    uint16_t width = 0;
    uint16_t height = 0;
    uint8_t  bitsperpixel = 0;
    uint8_t  imagedescriptor = 0;
};
#pragma pack(pop)

static cv::Mat load_tga(const std::string &path) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) return {};

    TGAHeader header;
    in.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!in.good()) return {};

    int w = header.width, h = header.height;
    int bpp = header.bitsperpixel >> 3; // байт на піксель
    if (w <= 0 || h <= 0 || (bpp != 1 && bpp != 3 && bpp != 4)) return {};

    // Пропускаємо поле ID
    if (header.idlength > 0) in.seekg(header.idlength, std::ios::cur);

    size_t nbytes = (size_t)bpp * w * h;
    std::vector<uint8_t> data(nbytes);

    if (header.datatypecode == 2 || header.datatypecode == 3) {
        // Нестиснуте зображення
        in.read(reinterpret_cast<char*>(data.data()), nbytes);
        if (!in.good()) return {};
    } else if (header.datatypecode == 10 || header.datatypecode == 11) {
        // RLE-стиснення
        size_t pixelcount = (size_t)w * h;
        size_t currentpixel = 0, currentbyte = 0;
        uint8_t colorbuf[4];
        while (currentpixel < pixelcount) {
            uint8_t chunkheader = in.get();
            if (!in.good()) return {};
            if (chunkheader < 128) {
                chunkheader++;
                for (int i = 0; i < chunkheader; i++) {
                    in.read(reinterpret_cast<char*>(colorbuf), bpp);
                    if (!in.good()) return {};
                    for (int t = 0; t < bpp; t++) data[currentbyte++] = colorbuf[t];
                    currentpixel++;
                }
            } else {
                chunkheader -= 127;
                in.read(reinterpret_cast<char*>(colorbuf), bpp);
                if (!in.good()) return {};
                for (int i = 0; i < chunkheader; i++) {
                    for (int t = 0; t < bpp; t++) data[currentbyte++] = colorbuf[t];
                    currentpixel++;
                }
            }
        }
    } else {
        return {};
    }

    // Перетворення у BGR-формат cv::Mat
    cv::Mat result(h, w, CV_8UC3);
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            size_t src = ((size_t)i + (size_t)j * w) * bpp;
            // TGA зберігає BGRA; OpenCV використовує BGR — копіюємо B,G,R
            uint8_t b = data[src];
            uint8_t g = bpp >= 3 ? data[src + 1] : data[src];
            uint8_t r = bpp >= 3 ? data[src + 2] : data[src];
            result.at<cv::Vec3b>(j, i) = {b, g, r};
        }
    }

    // Якщо біт 5 у imagedescriptor не встановлено, початок знизу зліва -> flip
    if (!(header.imagedescriptor & 0x20))
        cv::flip(result, result, 0);

    return result;
}

// ─── Пункт 1: читання OBJ-моделі (5 балів) ───────────────────────────────────

Model::Model(const std::string &filename) {
    std::ifstream in(filename, std::ifstream::in);
    if (in.fail()) {
        std::cerr << "Failed to open " << filename << std::endl;
        return;
    }
    std::string line;
    while (!in.eof()) {
        std::getline(in, line);
        std::istringstream iss(line.c_str());
        char trash;

        if (!line.compare(0, 2, "v ")) {
            iss >> trash;
            vec4 v = {0, 0, 0, 1};
            for (int i : {0, 1, 2}) iss >> v[i];
            verts_.push_back(v);

        } else if (!line.compare(0, 3, "vn ")) {
            iss >> trash >> trash;
            vec4 n = {0, 0, 0, 0};
            for (int i : {0, 1, 2}) iss >> n[i];
            norms_.push_back(normalized(n));

        } else if (!line.compare(0, 3, "vt ")) {
            iss >> trash >> trash;
            vec2 uv;
            for (int i : {0, 1}) iss >> uv[i];
            tex_.push_back({uv.x, 1 - uv.y});

        } else if (!line.compare(0, 2, "f ")) {
            int f, t, n, cnt = 0;
            iss >> trash;
            while (iss >> f >> trash >> t >> trash >> n) {
                facet_vrt_.push_back(--f);
                facet_tex_.push_back(--t);
                facet_nrm_.push_back(--n);
                cnt++;
            }
            if (3 != cnt) {
                std::cerr << "Error: OBJ file must be triangulated" << std::endl;
                return;
            }
        }
    }
    std::cerr << "Model loaded: " << nverts() << " vertices, " << nfaces() << " faces" << std::endl;

    load_texture(filename, "_diffuse.tga", diffusemap_);
    load_texture(filename, "_nm_tangent.tga", normalmap_);
    load_texture(filename, "_spec.tga", specularmap_);
}

void Model::load_texture(const std::string &filename, const std::string &suffix, cv::Mat &tex) {
    size_t dot = filename.find_last_of(".");
    if (dot == std::string::npos) return;
    std::string texfile = filename.substr(0, dot) + suffix;
    tex = load_tga(texfile);
    if (tex.empty()) {
        std::cerr << "Texture " << texfile << " loading failed" << std::endl;
    } else {
        std::cerr << "Texture " << texfile << " loaded: " << tex.cols << "x" << tex.rows << std::endl;
    }
}

int Model::nverts() const { return verts_.size(); }
int Model::nfaces() const { return facet_vrt_.size() / 3; }

vec4 Model::vert(int i) const {
    return verts_[i];
}

vec4 Model::vert(int iface, int nthvert) const {
    return verts_[facet_vrt_[iface * 3 + nthvert]];
}

vec4 Model::normal(int iface, int nthvert) const {
    return norms_[facet_nrm_[iface * 3 + nthvert]];
}

vec4 Model::normal(const vec2 &uv) const {
    if (normalmap_.empty()) return {0, 0, 1, 0};
    int x = std::clamp((int)(uv[0] * normalmap_.cols), 0, normalmap_.cols - 1);
    int y = std::clamp((int)(uv[1] * normalmap_.rows), 0, normalmap_.rows - 1);
    cv::Vec3b c = normalmap_.at<cv::Vec3b>(y, x);
    return normalized(vec4{(double)c[2], (double)c[1], (double)c[0], 0} * 2.0 / 255.0 - vec4{1, 1, 1, 0});
}

vec2 Model::uv(int iface, int nthvert) const {
    return tex_[facet_tex_[iface * 3 + nthvert]];
}

cv::Vec3b Model::sample_diffuse(const vec2 &uv) const {
    if (diffusemap_.empty()) return {180, 180, 180};
    int x = std::clamp((int)(uv[0] * diffusemap_.cols), 0, diffusemap_.cols - 1);
    int y = std::clamp((int)(uv[1] * diffusemap_.rows), 0, diffusemap_.rows - 1);
    return diffusemap_.at<cv::Vec3b>(y, x);
}

cv::Vec3b Model::sample_normal(const vec2 &uv) const {
    if (normalmap_.empty()) return {128, 128, 255};
    int x = std::clamp((int)(uv[0] * normalmap_.cols), 0, normalmap_.cols - 1);
    int y = std::clamp((int)(uv[1] * normalmap_.rows), 0, normalmap_.rows - 1);
    return normalmap_.at<cv::Vec3b>(y, x);
}

double Model::sample_specular(const vec2 &uv) const {
    if (specularmap_.empty()) return 0.0;
    int x = std::clamp((int)(uv[0] * specularmap_.cols), 0, specularmap_.cols - 1);
    int y = std::clamp((int)(uv[1] * specularmap_.rows), 0, specularmap_.rows - 1);
    cv::Vec3b c = specularmap_.at<cv::Vec3b>(y, x);
    return c[0] / 255.0;
}

bool Model::has_diffuse() const { return !diffusemap_.empty(); }
bool Model::has_normalmap() const { return !normalmap_.empty(); }
bool Model::has_specular() const { return !specularmap_.empty(); }
