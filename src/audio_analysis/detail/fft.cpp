/*
  LICENSE
  -------
Copyright 2005-2013 Nullsoft, Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

  * Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

  * Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.

  * Neither the name of Nullsoft nor the names of its contributors may be used to
    endorse or promote products derived from this software without specific prior written
permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include "fft.h"

#include <cmath>
#include <stdexcept>

namespace rhythm::audio::detail {

namespace {
constexpr auto kPi = 3.141592653589793238462643383279502884197169399f;
}

std::vector<float> MakeHannWindow(std::size_t size, float power) {
    std::vector<float> window(size);
    if (power < 0.0f) {
        std::fill(window.begin(), window.end(), 1.0f);
        return window;
    }

    float const multiplier = 1.0f / static_cast<float>(size) * 2.0f * kPi;
    for (std::size_t i = 0; i < size; i++) {
        float const base = 0.5f + 0.5f * std::sin(static_cast<float>(i) * multiplier - kPi * 0.5f);
        window[i] = (power == 1.0f) ? base : std::pow(base, power);
    }
    return window;
}

std::vector<float> MakeEqualizeTable(std::size_t num_bins) {
    // projectM MilkdropFFT::InitEqualizeTable
    float const scaling = -0.02f;
    float const inverse_num_bins = 1.0f / static_cast<float>(num_bins);

    std::vector<float> equalize(num_bins);
    for (std::size_t i = 0; i < num_bins; i++) {
        equalize[i] = scaling * std::log(static_cast<float>(num_bins - i) * inverse_num_bins);
    }
    return equalize;
}

Fft::Fft(std::size_t size) : size_(size) {
    if (size < 2 || size > 4096 || (size & (size - 1)) != 0) {
        throw std::invalid_argument("Fft size must be a power of two");
    }

    // bit-reversal 表
    bit_rev_.resize(size);
    for (std::size_t i = 0; i < size; i++) {
        bit_rev_[i] = i;
    }
    std::size_t j = 0;
    for (std::size_t i = 0; i < size; i++) {
        if (j > i) {
            std::swap(bit_rev_[i], bit_rev_[j]);
        }
        std::size_t m = size >> 1;
        while (m >= 1 && j >= m) {
            j -= m;
            m >>= 1;
        }
        j += m;
    }

    // 每级 DFT 的 twiddle 步进表
    std::size_t tabsize = 0;
    for (std::size_t dftsize = 2; dftsize <= size; dftsize <<= 1) {
        tabsize++;
    }
    cos_sin_table_.resize(tabsize);
    std::size_t index = 0;
    for (std::size_t dftsize = 2; dftsize <= size; dftsize <<= 1) {
        auto const theta = -2.0f * kPi / static_cast<float>(dftsize);
        cos_sin_table_[index++] = std::polar(1.0f, theta);
    }

    window_ = MakeHannWindow(size);
    equalize_ = MakeEqualizeTable(size / 2);
    scratch_.resize(size);
}

void Fft::Transform(std::span<std::complex<float>> data) const {
    // bit-reversal 重排
    if (data.size() != size_) throw std::invalid_argument("audio.fft_size");
    for (std::size_t i = 0; i < size_; i++) {
        if (i < bit_rev_[i]) std::swap(data[i], data[bit_rev_[i]]);
    }

    // 迭代基-2 蝶形
    std::size_t dft_size = 2;
    std::size_t octave = 0;
    while (dft_size <= size_) {
        std::complex<float> w{1.0f, 0.0f};
        std::complex<float> const wp = cos_sin_table_[octave];
        std::size_t const half_size = dft_size >> 1;

        for (std::size_t m = 0; m < half_size; m++) {
            for (std::size_t i = m; i < size_; i += dft_size) {
                std::size_t const k = i + half_size;
                std::complex<float> const t = data[k] * w;
                data[k] = data[i] - t;
                data[i] = data[i] + t;
            }
            w *= wp;
        }

        dft_size <<= 1;
        octave++;
    }
}

void Fft::MagnitudeSpectrum(std::span<const float> samples, std::vector<float>& mag_out,
                            bool apply_window, bool apply_equalize) {
    if (samples.size() != size_) throw std::invalid_argument("audio.fft_samples");
    auto& spectrum_data = scratch_;
    std::fill(spectrum_data.begin(), spectrum_data.end(), std::complex<float>{});
    for (std::size_t i = 0; i < size_; i++) {
        float const w = apply_window ? window_[i] : 1.0f;
        spectrum_data[i].real(samples[i] * w);
    }

    Transform(spectrum_data);

    mag_out.resize(NumBins());
    for (std::size_t i = 0; i < NumBins(); i++) {
        float const eq = apply_equalize ? equalize_[i] : 1.0f;
        mag_out[i] = eq * std::abs(spectrum_data[i]);
    }
}

}  // namespace rhythm::audio::detail
