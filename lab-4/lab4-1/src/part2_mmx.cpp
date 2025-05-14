#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <sys/time.h>
#include <algorithm>
#include <mmintrin.h>

static const int YUV2RGB[3][3] = {
    { 298, 0, 409 },
    { 298, -100, -208 },
    { 298, 516, 0 }
};
static const int RGB2YUV[3][3] = {
    { 66, 129, 25 },
    { -38, -74, 112 },
    { 112, -94, -18 }
};


constexpr int width = 1920;
constexpr int height = 1080;

void process2(uint8_t alpha, uint8_t *rgb_buffer)
{
    char file_name[20];
    sprintf(file_name, "%d.yuv", alpha);
    int fd = open(file_name, O_RDWR | O_CREAT, 0666);
    ftruncate(fd, width * height * 3 / 2);
    void *yuv_buffer = mmap(NULL, width * height * 3 / 2, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    uint8_t *y = (uint8_t *)yuv_buffer;
    uint8_t *u = y + width * height;
    uint8_t *v = u + (width * height) / 4;
    uint8_t *r = (uint8_t *)rgb_buffer;
    uint8_t *g = r + width * height;
    uint8_t *b = g + width * height;

    __m64 zero = _mm_setzero_si64();
    __m64 alpha_val = _mm_set1_pi16(alpha + 1);
    for (int i = 0; i < height; ++i)
    {
        for (int j = 0; j < width; j += 4)
        {
            int y_index = i * width + j;
            int uv_index = (i / 2) * (width / 2) + (j / 2);
            __m64 r_raw = _mm_cvtsi32_si64(*(int32_t *)&r[y_index]);
            __m64 g_raw = _mm_cvtsi32_si64(*(int32_t *)&g[y_index]);
            __m64 b_raw = _mm_cvtsi32_si64(*(int32_t *)&b[y_index]);
            // Convert to 16-bit
            __m64 r_val = _mm_unpacklo_pi8(r_raw, zero);
            __m64 g_val = _mm_unpacklo_pi8(g_raw, zero);
            __m64 b_val = _mm_unpacklo_pi8(b_raw, zero);
            // Apply alpha factor
            r_val = _mm_mullo_pi16(r_val, alpha_val);
            g_val = _mm_mullo_pi16(g_val, alpha_val);
            b_val = _mm_mullo_pi16(b_val, alpha_val);

            r_val = _mm_srai_pi16(r_val, 8);
            g_val = _mm_srai_pi16(g_val, 8);
            b_val = _mm_srai_pi16(b_val, 8);

            // Convert to YUV
            // Calculate Y = (R*66 + G*129 + B*25) >> 8 + 16
            __m64 y_r = _mm_mullo_pi16(r_val, _mm_set1_pi16(RGB2YUV[0][0]));
            __m64 y_g = _mm_mullo_pi16(g_val, _mm_set1_pi16(RGB2YUV[0][1]));
            __m64 y_b = _mm_mullo_pi16(b_val, _mm_set1_pi16(RGB2YUV[0][2]));
            __m64 y_tmp = _mm_add_pi16(_mm_add_pi16(y_r, y_g), y_b);
            y_tmp = _mm_srai_pi16(y_tmp, 8);
            y_tmp = _mm_add_pi16(y_tmp, _mm_set1_pi16(16));
            __m64 y_out = _mm_packs_pu16(y_tmp, zero);

            // Store Y values (4 pixels)
            *((int32_t*)&y[y_index]) = _mm_cvtsi64_si32(y_out);

            // For U and V, only compute and store when on the top row of a 2x2 block
            if (i % 2 == 0) {
                // U = (R*-38 + G*-74 + B*112) >> 8 + 128
                __m64 u_r = _mm_mullo_pi16(r_val, _mm_set1_pi16(RGB2YUV[1][0]));
                __m64 u_g = _mm_mullo_pi16(g_val, _mm_set1_pi16(RGB2YUV[1][1]));
                __m64 u_b = _mm_mullo_pi16(b_val, _mm_set1_pi16(RGB2YUV[1][2]));
                __m64 u_tmp = _mm_add_pi16(_mm_add_pi16(u_r, u_g), u_b);
                u_tmp = _mm_srai_pi16(u_tmp, 8);
                u_tmp = _mm_add_pi16(u_tmp, _mm_set1_pi16(128));
                __m64 u_out = _mm_packs_pu16(u_tmp, zero);

                // V = (R*112 + G*-94 + B*-18) >> 8 + 128
                __m64 v_r = _mm_mullo_pi16(r_val, _mm_set1_pi16(RGB2YUV[2][0]));
                __m64 v_g = _mm_mullo_pi16(g_val, _mm_set1_pi16(RGB2YUV[2][1]));
                __m64 v_b = _mm_mullo_pi16(b_val, _mm_set1_pi16(RGB2YUV[2][2]));
                __m64 v_tmp = _mm_add_pi16(_mm_add_pi16(v_r, v_g), v_b);
                v_tmp = _mm_srai_pi16(v_tmp, 8);
                v_tmp = _mm_add_pi16(v_tmp, _mm_set1_pi16(128));
                __m64 v_out = _mm_packs_pu16(v_tmp, zero);

                // Store U and V values
                u[uv_index] = _mm_cvtsi64_si32(u_out) & 0xFF;
                v[uv_index] = _mm_cvtsi64_si32(v_out) & 0xFF;
                
                // For pixel 2
                u[uv_index + 1] = (_mm_cvtsi64_si32(u_out) >> 16) & 0xFF;
                v[uv_index + 1] = (_mm_cvtsi64_si32(v_out) >> 16) & 0xFF;
            }
        }
    }
    msync(yuv_buffer, width * height * 3 / 2, MS_SYNC);
    munmap(yuv_buffer, width * height * 3 / 2);
    close(fd);
}

void process1(uint8_t *yuv_buffer, uint8_t *rgb_buffer)
{
    uint8_t *y_plane = yuv_buffer;
    uint8_t *u_plane = y_plane + width * height;
    uint8_t *v_plane = u_plane + (width * height) / 4;
    uint8_t *r_plane = rgb_buffer;
    uint8_t *g_plane = r_plane + width * height;
    uint8_t *b_plane = g_plane + width * height;

    __m64 y_center = _mm_set1_pi16(16);
    __m64 uv_center = _mm_set1_pi16(128);
    __m64 zero = _mm_setzero_si64();

    for (int i = 0; i < height; ++i) {
        for (int j = 0; j < width; j += 4) {
            int idx     = i * width + j;
            int uv_idx  = (i / 2) * (width / 2) + (j / 2);
            // Load Y values (4 pixels) from memory
            __m64 y_raw = _mm_cvtsi32_si64(*(int32_t *)&y_plane[idx]);
            // Convert 8-bit to 16-bit by unpacking with zero
            __m64 y_val = _mm_unpacklo_pi8(y_raw, zero);
            
            // Convert u_raw16 from [a,b] to 32-bit [a,a,b,b]
            __m64 u_raw = _mm_cvtsi32_si64((int32_t)*(uint16_t*)(u_plane + uv_idx));
            u_raw = _mm_unpacklo_pi8(u_raw, zero); // Convert to 16-bit [a,0,b,0]
            __m64 u_val = _mm_unpacklo_pi16(u_raw, u_raw); // Duplicate to get [a,a,b,b]

            // Convert v_raw16 from [a,b] to 32-bit [a,a,b,b]
            __m64 v_raw = _mm_cvtsi32_si64((int32_t)*(uint16_t*)(v_plane + uv_idx));
            v_raw = _mm_unpacklo_pi8(v_raw, zero); // Convert to 16-bit [a,0,b,0]
            __m64 v_val = _mm_unpacklo_pi16(v_raw, v_raw); // Duplicate to get [a,a,b,b]

            y_val = _mm_sub_pi16(y_val, y_center);
            u_val = _mm_sub_pi16(u_val, uv_center);
            v_val = _mm_sub_pi16(v_val, uv_center);

            // Calculate R = Y*298 + V*409
            __m64 r_y = _mm_mullo_pi16(y_val, _mm_set1_pi16(YUV2RGB[0][0]));
            __m64 r_v = _mm_mullo_pi16(v_val, _mm_set1_pi16(YUV2RGB[0][2]));
            __m64 r_tmp = _mm_add_pi16(r_y, r_v);
            // Shift right by 8 bits (divide by 256)
            r_tmp = _mm_srai_pi16(r_tmp, 8);
            // Convert to 8 bits and clamp to 0-255 range
            __m64 r_out = _mm_packs_pu16(r_tmp, zero);

            // Calculate G = Y*298 - U*100 - V*208
            __m64 g_y = _mm_mullo_pi16(y_val, _mm_set1_pi16(YUV2RGB[1][0]));
            __m64 g_u = _mm_mullo_pi16(u_val, _mm_set1_pi16(YUV2RGB[1][1]));
            __m64 g_v = _mm_mullo_pi16(v_val, _mm_set1_pi16(YUV2RGB[1][2]));
            __m64 g_tmp = _mm_add_pi16(_mm_add_pi16(g_y, g_u), g_v);
            // Shift right by 8 bits
            g_tmp = _mm_srai_pi16(g_tmp, 8);
            // Convert to 8 bits and clamp to 0-255 range
            __m64 g_out = _mm_packs_pu16(g_tmp, zero);

            // Calculate B = Y*298 + U*516
            __m64 b_y = _mm_mullo_pi16(y_val, _mm_set1_pi16(YUV2RGB[2][0]));
            __m64 b_u = _mm_mullo_pi16(u_val, _mm_set1_pi16(YUV2RGB[2][1]));
            __m64 b_tmp = _mm_add_pi16(b_y, b_u);
            // Shift right by 8 bits
            b_tmp = _mm_srai_pi16(b_tmp, 8);
            // Convert to 8 bits and clamp to 0-255 range
            __m64 b_out = _mm_packs_pu16(b_tmp, zero);

            // Store results to memory (4 bytes each)
            *((int32_t*)&r_plane[idx]) = _mm_cvtsi64_si32(r_out);
            *((int32_t*)&g_plane[idx]) = _mm_cvtsi64_si32(g_out);
            *((int32_t*)&b_plane[idx]) = _mm_cvtsi64_si32(b_out);
        }
    }
}
int main(int, char *argv[])
{
    struct timeval start, end;

    gettimeofday(&start, NULL); // Record start time
    const char *filename = argv[1];
    int fd = open(filename, O_RDONLY);
    void *yuv_buffer = mmap(NULL, width * height * 3 / 2, PROT_READ, MAP_PRIVATE, fd, 0);
    void *rgb_buffer = malloc(width * height * 3);
    process1((uint8_t *)yuv_buffer, (uint8_t *)rgb_buffer);

    for (int alpha = 1; alpha < 256; alpha += 3)
    {
        process2(alpha, (uint8_t *)rgb_buffer);
    }

    munmap(yuv_buffer, 0);
    close(fd);
    free(rgb_buffer);

    gettimeofday(&end, NULL); // Record end time

    long seconds = end.tv_sec - start.tv_sec;
    long microseconds = end.tv_usec - start.tv_usec;
    if (microseconds < 0) {
        seconds--;
        microseconds += 1000000;
    }
    double elapsed = seconds + microseconds / 1000000.0;

    printf("Elapsed time: %f seconds\n", elapsed);
}