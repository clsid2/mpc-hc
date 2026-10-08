/*
 * (C) 2026 see Authors.txt
 *
 * This file is part of MPC-HC.
 *
 * MPC-HC is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * MPC-HC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#pragma once

// HLG (BT.2100) to SDR (BT.709). The internal renderers otherwise pass HDR video to the
// display unconverted, washed out and desaturated (e.g. iPhone videos). It runs first on
// the mixer's output, which is HLG-encoded R'G'B' with BT.2020 primaries: it decodes HLG,
// scales it for SDR, rolls off the highlights and converts the primaries to BT.709.
// GAIN, KNEE and DISP_GAMMA were fitted to how Chromium/Edge show HLG test patches
// (mean error 3/255).
static const char* const HLG_TO_SDR_SHADER = R"(
sampler s0 : register(s0);

#define REF_WHITE   0.2640  // HLG reference white (75% signal) in scene light: 203 cd/m2 at 1000 cd/m2
#define SYS_GAMMA   1.2     // HLG system gamma for a 1000 cd/m2 display
#define GAIN        0.57    // reference white lands at 57% linear, leaving headroom for highlights
#define KNEE        0.60    // SDR level where highlights start to roll off
#define DISP_GAMMA  2.0     // SDR encoding gamma

static const float3x3 BT2020_TO_BT709 = {
     1.6605, -0.5876, -0.0728,
    -0.1246,  1.1329, -0.0083,
    -0.0182, -0.1006,  1.1187 };

float hlg_inverse_oetf(float e)
{
    const float a = 0.17883277, b = 0.28466892, c = 0.55991073;
    return e <= 0.5 ? e * e / 3.0 : (exp((e - c) / a) + b) / 12.0;
}

float4 main(float2 tex : TEXCOORD0) : COLOR
{
    float3 hlg = saturate(tex2D(s0, tex).rgb);
    float3 scene = float3(hlg_inverse_oetf(hlg.r), hlg_inverse_oetf(hlg.g), hlg_inverse_oetf(hlg.b));
    float ys = dot(scene, float3(0.2627, 0.6780, 0.0593));
    float3 display = scene * pow(max(ys, 1e-6), SYS_GAMMA - 1.0);   // HLG OOTF, 1 = display peak
    float3 sdr = max(mul(BT2020_TO_BT709, display * (GAIN / pow(REF_WHITE, SYS_GAMMA))), 0.0);

    float m = max(sdr.r, max(sdr.g, sdr.b));                        // soft knee above KNEE
    if (m > KNEE) {
        float t = m - KNEE, room = 1.0 - KNEE;
        sdr *= (KNEE + room * t / (t + room)) / m;
    }
    return float4(pow(saturate(sdr), 1.0 / DISP_GAMMA), 1.0);
}
)";
