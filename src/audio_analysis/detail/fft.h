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
#pragma once

// 自包含基-2 FFT（参考 projectM Audio/MilkdropFFT.cpp 的迭代实现），
// 附带 Hann 窗表与高频均衡表（-0.02*log(剩余比例)）。
// 支持 1024 / 2048 / 4096 点。

#include <complex>
#include <cstddef>
#include <span>
#include <vector>

namespace rhythm::audio::detail {

// Hann 窗（power=1 即标准 Hann；power 控制窗的幂次，参考 MilkdropFFT）
std::vector<float> MakeHannWindow(std::size_t size, float power = 1.0f);

// projectM/MilkDrop 高频均衡表：eq[i] = -0.02 * log((num_bins - i) / num_bins)
// 最低桶≈0，越往高频增益越大（对幅度做 log10 域补偿）
std::vector<float> MakeEqualizeTable(std::size_t num_bins);

class Fft {
   public:
    explicit Fft(std::size_t size = 4096);  // size 必须为 2 的幂

    std::size_t Size() const { return size_; }
    std::size_t NumBins() const { return size_ / 2; }

    // 原地复数 FFT（输入输出均为 size 个复数）
    void Transform(std::span<std::complex<float>> data) const;

    // 实信号 → 幅度谱（num_bins 个值），可选加 Hann 窗与高频均衡
    void MagnitudeSpectrum(std::span<const float> samples, std::vector<float>& mag_out,
                           bool apply_window = true, bool apply_equalize = true);

    const std::vector<float>& Window() const { return window_; }
    const std::vector<float>& Equalize() const { return equalize_; }

   private:
    std::size_t size_ = 0;
    std::vector<std::size_t> bit_rev_{};
    std::vector<std::complex<float>> cos_sin_table_{};
    std::vector<float> window_{};
    std::vector<float> equalize_{};
    std::vector<std::complex<float>> scratch_{};
};

}  // namespace rhythm::audio::detail
