#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <sys/time.h>
#include <algorithm>

#include "standard.hpp"


constexpr int width = 1920;
constexpr int height = 1080;
static const char *file1 = "dem1.yuv";
static const char *file2 = "dem2.yuv";

void process2(uint8_t alpha1, uint alpha2,uint8_t *rgb_buffer1, uint8_t *rgb_buffer2)
{
    char file_name[32]; // Increased buffer size for filename
    sprintf(file_name, "%d.yuv", static_cast<int>(alpha2)); // Cast alpha for sprintf
    void *yuv_buffer = malloc(width * height * 3 / 2);
    float alpha_factor1 = static_cast<float>(alpha1) / 255.0f;
    float alpha_factor2 = static_cast<float>(alpha2) / 255.0f;
    clk.start();

    uint8_t *y = (uint8_t *)yuv_buffer;
    uint8_t *u = y + width * height;
    uint8_t *v = u + (width * height) / 4;
    uint8_t *r1 = (uint8_t *)rgb_buffer1;
    uint8_t *g1 = r1 + width * height;
    uint8_t *b1= g1 + width * height;
    uint8_t *r2 = (uint8_t *)rgb_buffer2;
    uint8_t *g2 = r2 + width * height;
    uint8_t *b2= g2 + width * height;

    for (int i = 0; i < height; ++i)
    {
        for (int j = 0; j < width; ++j)
        {
            int y_index = i * width + j;
            int uv_index = (i / 2) * (width / 2) + (j / 2);
            float r_val = static_cast<float>(r1[y_index]) * alpha_factor1 + static_cast<float>(r2[y_index]) * alpha_factor2;
            float g_val = static_cast<float>(g1[y_index]) * alpha_factor1 + static_cast<float>(g2[y_index]) * alpha_factor2;
            float b_val = static_cast<float>(b1[y_index]) * alpha_factor1 + static_cast<float>(b2[y_index]) * alpha_factor2;

            float y_val = BT601::RGB2YUV[0].offset + r_val * BT601::RGB2YUV[0].scale1 + g_val * BT601::RGB2YUV[0].scale2 + b_val * BT601::RGB2YUV[0].scale3;
            y[y_index] = std::clamp(y_val, 0.0f, 255.0f);

            if (i % 2 == 0 && j % 2 == 0)
            {
                float u_val = BT601::RGB2YUV[1].offset + r_val * BT601::RGB2YUV[1].scale1 + g_val * BT601::RGB2YUV[1].scale2 + b_val * BT601::RGB2YUV[1].scale3;
                float v_val = BT601::RGB2YUV[2].offset + r_val * BT601::RGB2YUV[2].scale1 + g_val * BT601::RGB2YUV[2].scale2 + b_val * BT601::RGB2YUV[2].scale3;
                u[uv_index] = std::clamp(u_val, 0.0f, 255.0f);
                v[uv_index] = std::clamp(v_val, 0.0f, 255.0f);
            }
        }
    }
    clk.stop();
    FILE *fp = fopen(file_name, "wb");
    fwrite(yuv_buffer, 1, width * height * 3 / 2, fp);
    fclose(fp);
    free(yuv_buffer);
}

void process1(uint8_t *yuv_buffer, uint8_t *rgb_buffer)
{
    clk.start();
    uint8_t *y = (uint8_t *)yuv_buffer;
    uint8_t *u = y + width * height;
    uint8_t *v = u + (width * height) / 4;
    uint8_t *r = (uint8_t *)rgb_buffer;
    uint8_t *g = r + width * height;
    uint8_t *b = g + width * height;

    for (int i = 0; i < height; ++i)
    {
        for (int j = 0; j < width; ++j)
        {
            int y_index = i * width + j;
            int uv_index = (i / 2) * (width / 2) + (j / 2);
            int y_value = y[y_index];
            int u_value = u[uv_index];
            int v_value = v[uv_index];

            float r_value = BT601::YUV2RGB[0].offset + y_value * BT601::YUV2RGB[0].scale1 + 0 + v_value * BT601::YUV2RGB[0].scale3;
            float g_value = BT601::YUV2RGB[1].offset + y_value * BT601::YUV2RGB[1].scale1 + u_value * BT601::YUV2RGB[1].scale2 + v_value * BT601::YUV2RGB[1].scale3;
            float b_value = BT601::YUV2RGB[2].offset + y_value * BT601::YUV2RGB[2].scale1 + u_value * BT601::YUV2RGB[2].scale2 + 0;
            r[y_index] = std::clamp(r_value, 0.0f, 255.0f);
            g[y_index] = std::clamp(g_value, 0.0f, 255.0f);
            b[y_index] = std::clamp(b_value, 0.0f, 255.0f);
        }
    }
    clk.stop();
}



int main(int, char *argv[])
{
    FILE *fp1 = fopen(file1, "rb");
    FILE *fp2 = fopen(file2, "rb");
    void *yuv_buffer1 = malloc(width * height * 3 / 2);
    void *yuv_buffer2 = malloc(width * height * 3 / 2);
    fread(yuv_buffer1, 1, width * height * 3 / 2, fp1);
    fread(yuv_buffer2, 1, width * height * 3 / 2, fp2);
    void *rgb_buffer1 = malloc(width * height * 3);
    void *rgb_buffer2 = malloc(width * height * 3);
    process1((uint8_t *)yuv_buffer1, (uint8_t *)rgb_buffer1);
    process1((uint8_t *)yuv_buffer2, (uint8_t *)rgb_buffer2);

    for (int alpha = 1; alpha < 256; alpha += 3)
    {
        process2(255 - alpha, alpha, (uint8_t *)rgb_buffer1, (uint8_t *)rgb_buffer2);
    }

    free(rgb_buffer1);
    free(rgb_buffer2);
    free(yuv_buffer1);
    free(yuv_buffer2);
    fclose(fp1);
    fclose(fp2);

    printf("Elapsed time: %f ms\n", clk.elapsed_time);
}