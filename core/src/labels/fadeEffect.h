#pragma once

#include <cmath>

namespace Tangram {

struct FadeEffect {

public:

    enum Interpolation {
        linear = 0,
        pow,
        sine
    };

    FadeEffect() {}

    FadeEffect(bool in, Interpolation interpolation, float duration)
        : m_interpolation(interpolation), m_duration(duration), m_in(in)
    {}

    float update(float dt) {
        m_step += dt;
        float st = m_step / m_duration;

        switch (m_interpolation) {
            case Interpolation::linear:
                return m_in ? st : -st + 1;
            case Interpolation::pow:
                return m_in ? st * st : -(st * st) + 1;
            case Interpolation::sine:
                return m_in ? sin(st * M_PI * 0.5) : cos(st * M_PI * 0.5);
        }

        return st;
    }

    bool isFinished() {
        return m_step > m_duration;
    }

private:

    Interpolation m_interpolation = Interpolation::linear;
    float m_duration;
    float m_step = 0.0;
    bool m_in;
};

}

