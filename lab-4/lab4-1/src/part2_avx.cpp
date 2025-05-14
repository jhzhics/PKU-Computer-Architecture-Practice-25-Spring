#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <sys/time.h>
#include <algorithm>
#include <immintrin.h>

#include "standard.hpp"


constexpr int width = 1920;
constexpr int height = 1080;

void process2(uint8_t alpha, uint8_t *rgb_buffer)
{
    char file_name[32]; // Increased buffer size for filename
    sprintf(file_name, "%d.yuv", static_cast<int>(alpha)); // Cast alpha for sprintf
    // Memory map the YUV file
    void *yuv_mmap_addr = malloc(width * height * 3 / 2);

    clk.start();
    // Pointers to Y, U, V planes in the memory-mapped file
    uint8_t *y_plane_out = static_cast<uint8_t *>(yuv_mmap_addr);
    uint8_t *u_plane_out = y_plane_out + (static_cast<size_t>(width) * height);
    uint8_t *v_plane_out = u_plane_out + (static_cast<size_t>(width) * height / 4);

    // Pointers to R, G, B planes in the input RGB buffer
    uint8_t *r_plane_in = rgb_buffer;
    uint8_t *g_plane_in = r_plane_in + (static_cast<size_t>(width) * height);
    uint8_t *b_plane_in = g_plane_in + (static_cast<size_t>(width) * height);

    // --- AVX2 Setup ---
    __m256 alpha_factor_ps = _mm256_set1_ps(static_cast<float>(alpha) / 255.0f);
    __m256 const_zero_ps   = _mm256_setzero_ps();
    __m256 const_max255_ps = _mm256_set1_ps(255.0f);

    __m128 const_zero_ps128   = _mm_setzero_ps(); // For U/V processing (4 floats)
    __m128 const_max255_ps128 = _mm_set1_ps(255.0f);


    // Y coefficients (for 8 pixels)
    const auto& Y_coeffs = BT601::RGB2YUV[0];
    __m256 y_offset_ps = _mm256_set1_ps(Y_coeffs.offset);
    __m256 y_s1_r_ps   = _mm256_set1_ps(Y_coeffs.scale1); // Coeff for R'
    __m256 y_s2_g_ps   = _mm256_set1_ps(Y_coeffs.scale2); // Coeff for G'
    __m256 y_s3_b_ps   = _mm256_set1_ps(Y_coeffs.scale3); // Coeff for B'

    // U coefficients (for 4 pixels)
    const auto& U_coeffs = BT601::RGB2YUV[1];
    __m128 u_offset_ps128 = _mm_set1_ps(U_coeffs.offset);
    __m128 u_s1_r_ps128   = _mm_set1_ps(U_coeffs.scale1);
    __m128 u_s2_g_ps128   = _mm_set1_ps(U_coeffs.scale2);
    __m128 u_s3_b_ps128   = _mm_set1_ps(U_coeffs.scale3);

    // V coefficients (for 4 pixels)
    const auto& V_coeffs = BT601::RGB2YUV[2];
    __m128 v_offset_ps128 = _mm_set1_ps(V_coeffs.offset);
    __m128 v_s1_r_ps128   = _mm_set1_ps(V_coeffs.scale1);
    __m128 v_s2_g_ps128   = _mm_set1_ps(V_coeffs.scale2);
    __m128 v_s3_b_ps128   = _mm_set1_ps(V_coeffs.scale3);
    
    // Permutation indices for selecting R/G/B values for U/V calculation
    // Input: [px0, px1, px2, px3, px4, px5, px6, px7]
    // Output (lower 128 bits): [px0, px2, px4, px6]
    __m256i uv_select_indices = _mm256_set_epi32(0, 0, 0, 0, 6, 4, 2, 0);

    for (int i = 0; i < height; ++i) {
        for (int j = 0; j < width; j += 8) { // Process 8 pixels horizontally
            int y_plane_idx = i * width + j;
            int uv_plane_idx = (i / 2) * (width / 2) + (j / 2);

            // 1. Load 8 R, G, B uint8_t values
            __m128i r_vals_epi8 = _mm_loadu_si64(reinterpret_cast<const void*>(r_plane_in + y_plane_idx));
            __m128i g_vals_epi8 = _mm_loadu_si64(reinterpret_cast<const void*>(g_plane_in + y_plane_idx));
            __m128i b_vals_epi8 = _mm_loadu_si64(reinterpret_cast<const void*>(b_plane_in + y_plane_idx));

            // 2. Convert uint8_t to float32 (8 values each)
            __m256i r_vals_epi32 = _mm256_cvtepu8_epi32(r_vals_epi8);
            __m256i g_vals_epi32 = _mm256_cvtepu8_epi32(g_vals_epi8);
            __m256i b_vals_epi32 = _mm256_cvtepu8_epi32(b_vals_epi8);

            __m256 r_vals_ps = _mm256_cvtepi32_ps(r_vals_epi32);
            __m256 g_vals_ps = _mm256_cvtepi32_ps(g_vals_epi32);
            __m256 b_vals_ps = _mm256_cvtepi32_ps(b_vals_epi32);

            // 3. Apply alpha scaling: scaled_color = color * (alpha / 255.0f)
            __m256 r_scaled_ps = _mm256_mul_ps(r_vals_ps, alpha_factor_ps);
            __m256 g_scaled_ps = _mm256_mul_ps(g_vals_ps, alpha_factor_ps);
            __m256 b_scaled_ps = _mm256_mul_ps(b_vals_ps, alpha_factor_ps);

            // 4. Y Calculation (for 8 pixels)
            __m256 y_calc_ps = y_offset_ps;
            y_calc_ps = _mm256_fmadd_ps(r_scaled_ps, y_s1_r_ps, y_calc_ps);
            y_calc_ps = _mm256_fmadd_ps(g_scaled_ps, y_s2_g_ps, y_calc_ps);
            y_calc_ps = _mm256_fmadd_ps(b_scaled_ps, y_s3_b_ps, y_calc_ps);

            // 5. Clamp Y, convert to uint8_t, and store
            y_calc_ps = _mm256_max_ps(const_zero_ps, y_calc_ps);    // Clamp min 0
            y_calc_ps = _mm256_min_ps(const_max255_ps, y_calc_ps);  // Clamp max 255
            
            __m256i y_clamped_epi32 = _mm256_cvtps_epi32(y_calc_ps); // Convert float to int32 (truncates)

            // Pack 8x int32 to 8x uint8_t
            __m128i y_out_epi16_lane0 = _mm256_castsi256_si128(y_clamped_epi32);        // Lower 4 int32s
            __m128i y_out_epi16_lane1 = _mm256_extracti128_si256(y_clamped_epi32, 1); // Upper 4 int32s
            __m128i y_packed_epi16 = _mm_packus_epi32(y_out_epi16_lane0, y_out_epi16_lane1); // 8x signed int32 -> 8x unsigned int16
            __m128i y_packed_epi8  = _mm_packus_epi16(y_packed_epi16, _mm_setzero_si128());   // 8x signed int16 -> 8x unsigned int8 (lower 8 bytes of result)
            
            _mm_storeu_si64(y_plane_out + y_plane_idx, y_packed_epi8);

            // 6. U and V Calculation and Storage (conditional: only for even rows 'i')
            if (i % 2 == 0) {
                // Select scaled R', G', B' for pixels 0, 2, 4, 6 from the current block of 8
                // These will be used to calculate 4 U and 4 V values.
                __m128 r_for_uv_ps = _mm256_castps256_ps128(_mm256_permutevar8x32_ps(r_scaled_ps, uv_select_indices));
                __m128 g_for_uv_ps = _mm256_castps256_ps128(_mm256_permutevar8x32_ps(g_scaled_ps, uv_select_indices));
                __m128 b_for_uv_ps = _mm256_castps256_ps128(_mm256_permutevar8x32_ps(b_scaled_ps, uv_select_indices));

                // U calculation (for 4 pixels)
                __m128 u_calc_ps128 = u_offset_ps128;
                u_calc_ps128 = _mm_fmadd_ps(r_for_uv_ps, u_s1_r_ps128, u_calc_ps128);
                u_calc_ps128 = _mm_fmadd_ps(g_for_uv_ps, u_s2_g_ps128, u_calc_ps128);
                u_calc_ps128 = _mm_fmadd_ps(b_for_uv_ps, u_s3_b_ps128, u_calc_ps128);

                // V calculation (for 4 pixels)
                __m128 v_calc_ps128 = v_offset_ps128;
                v_calc_ps128 = _mm_fmadd_ps(r_for_uv_ps, v_s1_r_ps128, v_calc_ps128);
                v_calc_ps128 = _mm_fmadd_ps(g_for_uv_ps, v_s2_g_ps128, v_calc_ps128);
                v_calc_ps128 = _mm_fmadd_ps(b_for_uv_ps, v_s3_b_ps128, v_calc_ps128);
                
                // Clamp U and V values
                u_calc_ps128 = _mm_max_ps(const_zero_ps128, u_calc_ps128);
                u_calc_ps128 = _mm_min_ps(const_max255_ps128, u_calc_ps128);
                v_calc_ps128 = _mm_max_ps(const_zero_ps128, v_calc_ps128);
                v_calc_ps128 = _mm_min_ps(const_max255_ps128, v_calc_ps128);

                // Convert U, V floats to int32
                __m128i u_clamped_epi32 = _mm_cvtps_epi32(u_calc_ps128);
                __m128i v_clamped_epi32 = _mm_cvtps_epi32(v_calc_ps128);

                // Pack 4x int32 to 4x uint8_t for U
                __m128i u_packed_epi16 = _mm_packus_epi32(u_clamped_epi32, _mm_setzero_si128()); // 4 int32 -> 4 uint16 (in lower half of 8 uint16s)
                __m128i u_packed_epi8  = _mm_packus_epi16(u_packed_epi16, _mm_setzero_si128());   // 4 uint16 -> 4 uint8 (in lower half of 8 uint8s)
                // Store the 4 uint8_t U values using _mm_cvtsi128_si32 (gets the lowest 32 bits as int)
                *reinterpret_cast<uint32_t*>(u_plane_out + uv_plane_idx) = _mm_cvtsi128_si32(u_packed_epi8);

                // Pack 4x int32 to 4x uint8_t for V
                __m128i v_packed_epi16 = _mm_packus_epi32(v_clamped_epi32, _mm_setzero_si128());
                __m128i v_packed_epi8  = _mm_packus_epi16(v_packed_epi16, _mm_setzero_si128());
                // Store the 4 uint8_t V values
                *reinterpret_cast<uint32_t*>(v_plane_out + uv_plane_idx) = _mm_cvtsi128_si32(v_packed_epi8);
            }
        }
    }
    clk.stop();
    FILE *file = fopen(file_name, "wb");
    fwrite(yuv_mmap_addr, 1, width * height * 3 / 2, file);
    fclose(file);
    free(yuv_mmap_addr);
}

void process1(uint8_t *yuv_buffer, uint8_t *rgb_buffer)
{
    clk.start();
    uint8_t *y_plane = yuv_buffer;
    uint8_t *u_plane = y_plane + width * height;
    uint8_t *v_plane = u_plane + (width * height) / 4;

    uint8_t *r_plane_out = rgb_buffer;
    uint8_t *g_plane_out = r_plane_out + width * height;
    uint8_t *b_plane_out = g_plane_out + width * height;

    const auto &R_coeffs = BT601::YUV2RGB[0];
    const auto &G_coeffs = BT601::YUV2RGB[1];
    const auto &B_coeffs = BT601::YUV2RGB[2];

    __m256 offR = _mm256_set1_ps(R_coeffs.offset);
    __m256 s1R  = _mm256_set1_ps(R_coeffs.scale1);
    __m256 s2R  = _mm256_set1_ps(R_coeffs.scale2);
    __m256 s3R  = _mm256_set1_ps(R_coeffs.scale3);

    __m256 offG = _mm256_set1_ps(G_coeffs.offset);
    __m256 s1G  = _mm256_set1_ps(G_coeffs.scale1);
    __m256 s2G  = _mm256_set1_ps(G_coeffs.scale2);
    __m256 s3G  = _mm256_set1_ps(G_coeffs.scale3);

    __m256 offB = _mm256_set1_ps(B_coeffs.offset);
    __m256 s1B  = _mm256_set1_ps(B_coeffs.scale1);
    __m256 s2B  = _mm256_set1_ps(B_coeffs.scale2);
    __m256 s3B  = _mm256_set1_ps(B_coeffs.scale3);

    __m256 zero   = _mm256_setzero_ps();
    __m256 max255 = _mm256_set1_ps(255.0f);

    for (int i = 0; i < height; ++i) {
        for (int j = 0; j < width; j += 8) {
            int y_idx = i * width + j;
            int uv_col_idx = j / 2;
            int uv_idx = (i / 2) * (width / 2) + uv_col_idx;

            // 1. Load Y values (8 values)
            __m128i y_epi8 = _mm_loadu_si64(reinterpret_cast<const void*>(y_plane + y_idx)); // Loads 8 bytes

            // Convert 8x uint8_t Y values to 8x float32
            __m256i y_epi32 = _mm256_cvtepu8_epi32(y_epi8);
            __m256 y_ps = _mm256_cvtepi32_ps(y_epi32);

            // 2. Load U and V values (4 values each)
            // These 4 U/V values correspond to the 8 Y values (U0 for Y0,Y1; U1 for Y2,Y3; etc.)
            __m128i u_epi8_chunk = _mm_cvtsi32_si128(*reinterpret_cast<const int*>(u_plane + uv_idx)); // Loads 4 bytes (U0,U1,U2,U3) into low dword
            __m128i v_epi8_chunk = _mm_cvtsi32_si128(*reinterpret_cast<const int*>(v_plane + uv_idx)); // Loads 4 bytes (V0,V1,V2,V3) into low dword

            // Convert 4x uint8_t U/V values to 4x int32_t, then to 4x float32
            __m128i u_epi32_0123 = _mm_cvtepu8_epi32(u_epi8_chunk); // Expands 4 bytes from u_epi8_chunk to 4 int32s
            __m128i v_epi32_0123 = _mm_cvtepu8_epi32(v_epi8_chunk); // Expands 4 bytes from v_epi8_chunk to 4 int32s
                                                                   // These are already __m128i, so no casting needed
            __m128 u_ps_0123 = _mm_cvtepi32_ps(u_epi32_0123);
            __m128 v_ps_0123 = _mm_cvtepi32_ps(v_epi32_0123);


            // 3. Duplicate U and V values to match 8 Y values: U0,U0,U1,U1,U2,U2,U3,U3
            // u_ps_0123 = [u0, u1, u2, u3]
            // Corrected duplication:
            __m128 u_ps_low_dup  = _mm_shuffle_ps(u_ps_0123, u_ps_0123, _MM_SHUFFLE(1,1,0,0)); // [u0,u0,u1,u1]
            __m128 u_ps_high_dup = _mm_shuffle_ps(u_ps_0123, u_ps_0123, _MM_SHUFFLE(3,3,2,2)); // [u2,u2,u3,u3]
            __m256 u_ps = _mm256_insertf128_ps(_mm256_castps128_ps256(u_ps_low_dup), u_ps_high_dup, 1);

            __m128 v_ps_low_dup  = _mm_shuffle_ps(v_ps_0123, v_ps_0123, _MM_SHUFFLE(1,1,0,0)); // [v0,v0,v1,v1]
            __m128 v_ps_high_dup = _mm_shuffle_ps(v_ps_0123, v_ps_0123, _MM_SHUFFLE(3,3,2,2)); // [v2,v2,v3,v3]
            __m256 v_ps = _mm256_insertf128_ps(_mm256_castps128_ps256(v_ps_low_dup), v_ps_high_dup, 1);

            // 4. YUV to RGB calculation
            // R = offsetR + y_ps * s1R + u_ps * s2R + v_ps * s3R
            // G = offsetG + y_ps * s1G + u_ps * s2G + v_ps * s3G
            // B = offsetB + y_ps * s1B + u_ps * s2B + v_ps * s3B
            // Note: Your reference formula implies s2R (U-coeff for R) and s3B (V-coeff for B) might be zero.
            // The FMA will handle this correctly if the coefficient is indeed 0.0f.

            __m256 r_val_temp = offR;
            r_val_temp = _mm256_fmadd_ps(y_ps, s1R, r_val_temp);
            if (R_coeffs.scale2 != 0.0f) r_val_temp = _mm256_fmadd_ps(u_ps, s2R, r_val_temp); // Optional based on actual coeff
            __m256 r_val = _mm256_fmadd_ps(v_ps, s3R, r_val_temp);

            __m256 g_val_temp = offG;
            g_val_temp = _mm256_fmadd_ps(y_ps, s1G, g_val_temp);
            g_val_temp = _mm256_fmadd_ps(u_ps, s2G, g_val_temp);
            __m256 g_val = _mm256_fmadd_ps(v_ps, s3G, g_val_temp);

            __m256 b_val_temp = offB;
            b_val_temp = _mm256_fmadd_ps(y_ps, s1B, b_val_temp);
            b_val_temp = _mm256_fmadd_ps(u_ps, s2B, b_val_temp);
            if (B_coeffs.scale3 != 0.0f) b_val_temp = _mm256_fmadd_ps(v_ps, s3B, b_val_temp); // Optional based on actual coeff
             __m256 b_val = b_val_temp; // If scale3B is zero, no final FMA with v_ps

            // 5. Clamp to [0, 255]
            r_val = _mm256_max_ps(zero, r_val);
            r_val = _mm256_min_ps(max255, r_val);
            g_val = _mm256_max_ps(zero, g_val);
            g_val = _mm256_min_ps(max255, g_val);
            b_val = _mm256_max_ps(zero, b_val);
            b_val = _mm256_min_ps(max255, b_val);

            // 6. Convert float32 to int32
            __m256i r_epi32 = _mm256_cvtps_epi32(r_val);
            __m256i g_epi32 = _mm256_cvtps_epi32(g_val);
            __m256i b_epi32 = _mm256_cvtps_epi32(b_val);

            // 7. Pack 8x int32 to 8x uint8_t
            __m128i r_epi16_low  = _mm256_castsi256_si128(r_epi32);
            __m128i r_epi16_high = _mm256_extracti128_si256(r_epi32, 1);
            __m128i r_epi16 = _mm_packus_epi32(r_epi16_low, r_epi16_high);
            __m128i r_epi8 = _mm_packus_epi16(r_epi16, _mm_setzero_si128()); // extract lower 8 bytes

            __m128i g_epi16_low  = _mm256_castsi256_si128(g_epi32);
            __m128i g_epi16_high = _mm256_extracti128_si256(g_epi32, 1);
            __m128i g_epi16 = _mm_packus_epi32(g_epi16_low, g_epi16_high);
            __m128i g_epi8 = _mm_packus_epi16(g_epi16, _mm_setzero_si128());

            __m128i b_epi16_low  = _mm256_castsi256_si128(b_epi32);
            __m128i b_epi16_high = _mm256_extracti128_si256(b_epi32, 1);
            __m128i b_epi16 = _mm_packus_epi32(b_epi16_low, b_epi16_high);
            __m128i b_epi8 = _mm_packus_epi16(b_epi16, _mm_setzero_si128());

            // 8. Store RGB values
            _mm_storeu_si64(reinterpret_cast<void*>(r_plane_out + y_idx), r_epi8);
            _mm_storeu_si64(reinterpret_cast<void*>(g_plane_out + y_idx), g_epi8);
            _mm_storeu_si64(reinterpret_cast<void*>(b_plane_out + y_idx), b_epi8);
        }
    }
    clk.stop();
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