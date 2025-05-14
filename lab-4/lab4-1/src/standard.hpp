/**
 * The conversion follows the ITU-R BT.601 standard for YUV && RGB conversion.
 * https://en.wikipedia.org/wiki/YCbCr#ITU-R_BT.601_conversion
 */

#pragma once
namespace BT601
{
    struct Entry
    {
        float offset;
        float scale1;
        float scale2;
        float scale3;
    };

    static const Entry RGB2YUV[3] = {
        { 16.0f, 65.783f / 256, 129.057f / 256, 25.064f / 256 },
        { 128.0f, -37.945f / 256, -74.494f / 256, 112.439f / 256 },
        { 128.0f, 112.439f / 256, -94.154f / 256, -18.285f / 256 }
    };

    static const Entry YUV2RGB[3] = {
        { -222.921f, 298.082f / 256, 0.0f, 408.583f / 256},
        { 135.576f, 298.082f / 256, -100.291f / 256, -208.120f / 256 },
        { -276.836f, 298.082f / 256, 516.412f / 256, 0.0f }
    };
}
