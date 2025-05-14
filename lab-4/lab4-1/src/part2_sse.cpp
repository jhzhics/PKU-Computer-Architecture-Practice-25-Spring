#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <sys/time.h>
#include <algorithm>
#include <xmmintrin.h>

#include "standard.hpp"

constexpr int width = 1920;
constexpr int height = 1080;

// Utility clamp for SSE
inline __m128 clamp_ps(__m128 v, __m128 minv, __m128 maxv)
{
    v = _mm_max_ps(v, minv);
    v = _mm_min_ps(v, maxv);
    return v;
}

void process1(uint8_t *yuv_buffer, uint8_t *rgb_buffer)
{
    clk.start();
    uint8_t *y = yuv_buffer;
    uint8_t *u = y + width * height;
    uint8_t *v = u + (width * height) / 4;
    uint8_t *r = rgb_buffer;
    uint8_t *g = r + width * height;
    uint8_t *b = g + width * height;

    __m128 offset0 = _mm_set1_ps(BT601::YUV2RGB[0].offset);
    __m128 s10 = _mm_set1_ps(BT601::YUV2RGB[0].scale1);
    __m128 s03 = _mm_set1_ps(BT601::YUV2RGB[0].scale3);
    __m128 offset1 = _mm_set1_ps(BT601::YUV2RGB[1].offset);
    __m128 s11 = _mm_set1_ps(BT601::YUV2RGB[1].scale1);
    __m128 s12 = _mm_set1_ps(BT601::YUV2RGB[1].scale2);
    __m128 s13 = _mm_set1_ps(BT601::YUV2RGB[1].scale3);
    __m128 offset2 = _mm_set1_ps(BT601::YUV2RGB[2].offset);
    __m128 s21 = _mm_set1_ps(BT601::YUV2RGB[2].scale1);
    __m128 s22 = _mm_set1_ps(BT601::YUV2RGB[2].scale2);

    __m128 zero = _mm_setzero_ps();
    __m128 minv = zero;
    __m128 maxv = _mm_set1_ps(255.0f);

    for (int i = 0; i < height; ++i)
    {
        for (int j = 0; j < width; j += 4)
        {
            int base = i * width + j;
            int uv_index = (i / 2) * (width / 2) + (j / 2);

            // Load Y values
            __m128 y_ps = _mm_cvtepi32_ps(_mm_set_epi32(
                y[base + 3], y[base + 2], y[base + 1], y[base + 0]));
            // Load U and V (2x2 block -> replicate each)
            float u_val = u[uv_index];
            float v_val = v[uv_index];
            __m128 u_ps = _mm_set1_ps(u_val);
            __m128 v_ps = _mm_set1_ps(v_val);

            // R = off0 + y*s10 + v*s03
            __m128 r_ps = _mm_add_ps(
                _mm_add_ps(offset0, _mm_mul_ps(y_ps, s10)),
                _mm_mul_ps(v_ps, s03));
            // G = off1 + y*s11 + u*s12 + v*s13
            __m128 g_ps = _mm_add_ps(
                _mm_add_ps(_mm_add_ps(offset1, _mm_mul_ps(y_ps, s11)), _mm_mul_ps(u_ps, s12)),
                _mm_mul_ps(v_ps, s13));
            // B = off2 + y*s21 + u*s22
            __m128 b_ps = _mm_add_ps(
                _mm_add_ps(offset2, _mm_mul_ps(y_ps, s21)),
                _mm_mul_ps(u_ps, s22));

            // Clamp
            r_ps = clamp_ps(r_ps, minv, maxv);
            g_ps = clamp_ps(g_ps, minv, maxv);
            b_ps = clamp_ps(b_ps, minv, maxv);

            // Store
            alignas(16) float rf[4], gf[4], bf_[4];
            _mm_store_ps(rf, r_ps);
            _mm_store_ps(gf, g_ps);
            _mm_store_ps(bf_, b_ps);
            for (int k = 0; k < 4; ++k)
            {
                r[base + k] = static_cast<uint8_t>(rf[k]);
                g[base + k] = static_cast<uint8_t>(gf[k]);
                b[base + k] = static_cast<uint8_t>(bf_[k]);
            }
        }
    }
    clk.stop();
}

void process2(uint8_t alpha, uint8_t *rgb_buffer)
{
    char file_name[32]; // Increased buffer size for filename
    sprintf(file_name, "%d.yuv", static_cast<int>(alpha)); // Cast alpha for sprintf
    void *yuv_buffer = malloc(width * height * 3 / 2);
    clk.start();

    uint8_t *y = (uint8_t *)yuv_buffer;
    uint8_t *u = y + width * height;
    uint8_t *v = u + (width * height) / 4;
    uint8_t *r = rgb_buffer;
    uint8_t *g = r + width * height;
    uint8_t *b = g + width * height;

    float af = alpha / 255.0f;
    __m128 alpha_ps = _mm_set1_ps(af);
    __m128 offsetY = _mm_set1_ps(BT601::RGB2YUV[0].offset);
    __m128 y_s1 = _mm_set1_ps(BT601::RGB2YUV[0].scale1);
    __m128 y_s2 = _mm_set1_ps(BT601::RGB2YUV[0].scale2);
    __m128 y_s3 = _mm_set1_ps(BT601::RGB2YUV[0].scale3);
    __m128 offsetU = _mm_set1_ps(BT601::RGB2YUV[1].offset);
    __m128 u_s1 = _mm_set1_ps(BT601::RGB2YUV[1].scale1);
    __m128 u_s2 = _mm_set1_ps(BT601::RGB2YUV[1].scale2);
    __m128 u_s3 = _mm_set1_ps(BT601::RGB2YUV[1].scale3);
    __m128 offsetV = _mm_set1_ps(BT601::RGB2YUV[2].offset);
    __m128 v_s1 = _mm_set1_ps(BT601::RGB2YUV[2].scale1);
    __m128 v_s2 = _mm_set1_ps(BT601::RGB2YUV[2].scale2);
    __m128 v_s3 = _mm_set1_ps(BT601::RGB2YUV[2].scale3);
    __m128 minv = _mm_setzero_ps();
    __m128 maxv = _mm_set1_ps(255.0f);

    for (int i = 0; i < height; ++i)
    {
        for (int j = 0; j < width; j += 4)
        {
            int idx = i * width + j;
            // load and scale R, G, B
            __m128 r_ps = _mm_mul_ps(_mm_cvtepi32_ps(_mm_set_epi32(r[idx + 3], r[idx + 2], r[idx + 1], r[idx])), alpha_ps);
            __m128 g_ps = _mm_mul_ps(_mm_cvtepi32_ps(_mm_set_epi32(g[idx + 3], g[idx + 2], g[idx + 1], g[idx])), alpha_ps);
            __m128 b_ps = _mm_mul_ps(_mm_cvtepi32_ps(_mm_set_epi32(b[idx + 3], b[idx + 2], b[idx + 1], b[idx])), alpha_ps);

            // Y
            __m128 y_ps = _mm_add_ps(
                _mm_add_ps(offsetY, _mm_mul_ps(r_ps, y_s1)),
                _mm_add_ps(_mm_mul_ps(g_ps, y_s2), _mm_mul_ps(b_ps, y_s3)));
            y_ps = clamp_ps(y_ps, minv, maxv);
            alignas(16) float yf[4];
            _mm_store_ps(yf, y_ps);
            for (int k = 0; k < 4; ++k)
                y[idx + k] = static_cast<uint8_t>(yf[k]);

            if ((i % 2) == 0)
            {
                int uv_idx = (i / 2) * (width / 2) + (j / 2);

                // U = offsetU + r*u_s1 + g*u_s2 + b*u_s3
                __m128 u_ps = _mm_add_ps(
                    _mm_add_ps(offsetU, _mm_mul_ps(r_ps, u_s1)),
                    _mm_add_ps(_mm_mul_ps(g_ps, u_s2), _mm_mul_ps(b_ps, u_s3)));

                // V = offsetV + r*v_s1 + g*v_s2 + b*v_s3
                __m128 v_ps = _mm_add_ps(
                    _mm_add_ps(offsetV, _mm_mul_ps(r_ps, v_s1)),
                    _mm_add_ps(_mm_mul_ps(g_ps, v_s2), _mm_mul_ps(b_ps, v_s3)));

                // Clamp
                u_ps = clamp_ps(u_ps, minv, maxv);
                v_ps = clamp_ps(v_ps, minv, maxv);

                // Store values for both 2x2 blocks within the 4-pixel group
                alignas(16) float uf[4], vf[4];
                _mm_store_ps(uf, u_ps);
                _mm_store_ps(vf, v_ps);

                // First 2x2 block
                u[uv_idx] = static_cast<uint8_t>(uf[0]);
                v[uv_idx] = static_cast<uint8_t>(vf[0]);

                // Second 2x2 block
                u[uv_idx + 1] = static_cast<uint8_t>(uf[2]);
                v[uv_idx + 1] = static_cast<uint8_t>(vf[2]);
            }
        }
    }
    clk.stop();
    FILE *fp = fopen(file_name, "wb");
    fwrite(yuv_buffer, 1, width * height * 3 / 2, fp);
    fclose(fp);
    free(yuv_buffer);
}

int main(int, char *argv[])
{
    const char *filename = argv[1];
    FILE *fp = fopen(filename, "rb");
    void *yuv_buffer = malloc(width * height * 3 / 2);
    fread(yuv_buffer, 1, width * height * 3 / 2, fp);
    void *rgb_buffer = malloc(width * height * 3);
    process1((uint8_t *)yuv_buffer, (uint8_t *)rgb_buffer);

    for (int alpha = 1; alpha < 256; alpha += 3)
    {
        process2(alpha, (uint8_t *)rgb_buffer);
    }
    fclose(fp);
    free(rgb_buffer);
    free(yuv_buffer);
    printf("Elapsed time: %f ms\n", clk.elapsed_time);
}