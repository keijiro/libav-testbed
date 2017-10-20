#include "encoder.h"

int main()
{
    avtest::Encoder::initGlobal();

    const auto kWidth = 800;
    const auto kHeight = 480;

    avtest::Encoder encoder("test.mov", kWidth, kHeight, 30);

    for (auto i = 0; i < 100; i++)
    {
        auto* p = encoder.getRGBFrameBuffer();

        for (auto y = 0; y < kHeight; y++)
        {
            for (auto x = 0; x < kWidth; x++)
            {
                *p++ = x * 8 + i * 8;
                *p++ = y * 8 + i * 8;
                *p++ = i * 8;
            }
        }

        encoder.processFrame();
    }

    encoder.close();

    return 0;
}
