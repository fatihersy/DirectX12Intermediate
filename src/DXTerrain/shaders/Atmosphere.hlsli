// Atmosphere.hlsli
// Shared Bruneton & Neyret (2008) single-scattering model: constants, LUT parameterization, and
// sampling helpers. These MUST stay byte-for-byte consistent with how the transmittance/scattering
// LUTs are generated (currently in SkyDome.hlsl) or the lookups are wrong.
//
// Also provides AerialPerspective(): physically-based distance fog reconstructed from the LUTs, used
// by the post-process pass.
#ifndef ATMOSPHERE_HLSLI
#define ATMOSPHERE_HLSLI

static const float PI                 = 3.14159265359f;
static const float INV_4PI            = 1.0f / (4.0f * PI);

static const float  SUN_ANGULAR_RADIUS = 0.004675f;

static const uint TRANSMITTANCE_W     = 256u;
static const uint TRANSMITTANCE_H     = 64u;

static const uint SCATTER_W           = 32u;
static const uint SCATTER_H           = 128u;
static const uint SCATTER_MUS         = 32u;
static const uint SCATTER_NU          = 8u;
static const uint SCATTER_D           = SCATTER_MUS * SCATTER_NU;

float SafeSqrt(float x) { return sqrt(max(0.0f, x)); }
float ClampCosine(float c) { return clamp(c, -1.0f, 1.0f); }

cbuffer AtmosCB : register(b2)
{
    float3 BetaR;
    float Pad0;
    float BetaMScatter;
    float BetaMExtinct;
    float MieG;
    float HR;
    float HM;
    float Rg;
    float Rt;
    float SunIntensity;
    float3 SunDir;
    float Pad1;
};

float GetRG2() { return Rg * Rg; }
float GetRT2() { return Rt * Rt; }

float DistToAtmTop(float r, float mu)
{
    float disc = r * r * (mu * mu - 1.0f) + GetRT2();
    return max(0.0f, -r * mu + SafeSqrt(disc));
}

float DistToGround(float r, float mu)
{
    float disc = r * r * (mu * mu - 1.0f) + GetRG2();
    return (disc >= 0.0f) ? max(0.0f, -r * mu - SafeSqrt(disc)) : -1.0f;
}

bool RayHitsGround(float r, float mu)
{
    return (mu < 0.0f) && (r * r * (mu * mu - 1.0f) + GetRG2() >= 0.0f);
}

float DensityRayleigh(float altitude) { return exp(-altitude / HR); }
float DensityMie     (float altitude) { return exp(-altitude / HM); }

float PhaseRayleigh(float cosTheta)
{
    return (3.0f / (16.0f * PI)) * (1.0f + cosTheta * cosTheta);
}

float PhaseMie(float cosTheta)
{
    float g  = MieG;
    float g2 = g * g;
    float denom = pow(abs(1.0f + g2 - 2.0f * g * cosTheta), 1.5f);
    return (1.0f - g2) / (4.0f * PI * denom);
}

// --- Transmittance LUT: UV <-> (r, mu) ---
float2 TransmittanceUV(float r, float mu)
{
    float uR = SafeSqrt((r - Rg) / (Rt - Rg));

    float rho    = SafeSqrt(r * r - GetRG2());
    float H      = SafeSqrt(GetRT2() - GetRG2());
    float delta  = r * r * mu * mu - r * r + GetRG2();
    float muHor  = -rho / r;

    float uMu;
    if (mu >= muHor)
    {
        float dAtm = DistToAtmTop(r, mu);
        float dMax = rho + H;
        uMu = 0.5f + 0.5f * (dAtm / dMax);
    }
    else
    {
        float dGnd = max(0.0f, -r * mu - SafeSqrt(delta));
        float dMax = rho;
        uMu = 0.5f * (dGnd / max(dMax, 1.0f));
    }

    return float2(uMu, uR);
}

// --- Scattering LUT: UVWZ <-> (r, mu, muS, nu) ---
float4 ScatteringUVWZ(float r, float mu, float muS, float nu)
{
    float uR = SafeSqrt((r - Rg) / (Rt - Rg));

    float rho    = SafeSqrt(r * r - GetRG2());
    float H      = SafeSqrt(GetRT2() - GetRG2());
    float muHor  = -rho / r;
    float uMu;
    if (mu >= muHor)
    {
        float dAtm = DistToAtmTop(r, mu);
        float dMax = rho + H;
        uMu = 0.5f + 0.5f * (dAtm / dMax);
    }
    else
    {
        float dGnd = max(0.0f, -r * mu - SafeSqrt(r * r * mu * mu - r * r + GetRG2()));
        float dMax = rho;
        uMu = 0.5f * (dGnd / max(dMax, 1.0f));
    }

    float uMuS = max(0.0f, 1.0f - 0.5f * (log(max(1e-4f, 1.0f + muS)) / log(2.0f)));
    uMuS = clamp(uMuS, 0.0f, 1.0f);

    float uNu = (nu + 1.0f) * 0.5f;

    return float4(uNu, uMuS, uMu, uR);
}

float4 SampleScatteringLUT(Texture3D<float4> lut, SamplerState smp, float r, float mu, float muS, float nu)
{
    float4 uvwz = ScatteringUVWZ(r, mu, muS, nu);

    float uNu   = uvwz.x;
    float uMuS  = uvwz.y;
    float uMu   = uvwz.z;
    float uR    = uvwz.w;

    float nuIndex0 = floor(uNu * float(SCATTER_NU - 1));
    float nuIndex1 = nuIndex0 + 1.0f;
    float nuFrac   = uNu * float(SCATTER_NU - 1) - nuIndex0;

    float3 uvw0 = float3(
        uMuS,
        uMu,
        (nuIndex0 + uMuS) / float(SCATTER_NU)
    );
    float3 uvw1 = float3(
        uMuS,
        uMu,
        (nuIndex1 + uMuS) / float(SCATTER_NU)
    );

    uvw0 = float3(uR, uMu, (nuIndex0 / float(SCATTER_NU)) + uMuS / float(SCATTER_NU));
    uvw1 = float3(uR, uMu, (nuIndex1 / float(SCATTER_NU)) + uMuS / float(SCATTER_NU));

    return lerp(lut.SampleLevel(smp, uvw0, 0), lut.SampleLevel(smp, uvw1, 0), nuFrac);
}

float3 GetTransmittance(Texture2D<float4> transmLUT, SamplerState smp, float r, float mu, float dist, bool rayHitsGround)
{
    float r1  = SafeSqrt(r * r + dist * dist + 2.0f * r * mu * dist);
    float mu1 = ClampCosine((r * mu + dist) / r1);

    float3 t0, t1;
    if (rayHitsGround)
    {
        t0 = transmLUT.SampleLevel(smp, TransmittanceUV(r1, -mu1), 0).rgb;
        t1 = transmLUT.SampleLevel(smp, TransmittanceUV(r,  -mu),  0).rgb;
        return min(t0 / max(t1, 1e-6f), 1.0f);
    }
    else
    {
        t0 = transmLUT.SampleLevel(smp, TransmittanceUV(r,   mu),  0).rgb;
        t1 = transmLUT.SampleLevel(smp, TransmittanceUV(r1,  mu1), 0).rgb;
        return min(t0 / max(t1, 1e-6f), 1.0f);
    }
}

// Physically-based aerial perspective for a surface point: attenuate the surface radiance by the
// transmittance camera->point and add the in-scattering accumulated over that segment (S(cam) -
// T*S(point)). Flat-earth frame at terrain scale, matching the sky's PS_Sky setup. Density comes
// entirely from the atmosphere constants, so fog stays consistent with the sky / time of day.
float3 AerialPerspective(
    float3 surfaceColor, float3 camPos, float3 worldPos, float3 sunDir,
    Texture2D<float4> transLUT, Texture3D<float4> scatterLUT, SamplerState smp)
{
    float3 toPoint = worldPos - camPos;
    float d = length(toPoint);
    if (d < 1.0f) return surfaceColor;
    float3 viewDir = toPoint / d;

    float r   = clamp(Rg + camPos.y, Rg + 1.0f, Rt - 1.0f);
    float mu  = ClampCosine(viewDir.y);
    float muS = ClampCosine(sunDir.y);
    float nu  = ClampCosine(dot(viewDir, sunDir));

    bool hitGround = RayHitsGround(r, mu);

    float rP  = clamp(SafeSqrt(r * r + d * d + 2.0f * r * mu * d), Rg + 1.0f, Rt - 1.0f);
    float muP = ClampCosine((r * mu + d) / rP);

    float3 T = GetTransmittance(transLUT, smp, r, mu, d, hitGround);

    float4 sCam = SampleScatteringLUT(scatterLUT, smp, r,  mu,  muS, nu);
    float4 sPt  = SampleScatteringLUT(scatterLUT, smp, rP, muP, muS, nu);

    float3 scatterR = max(sCam.rgb - T * sPt.rgb, 0.0f);
    float  scatterM = max(sCam.a   - T.x * sPt.a, 0.0f);

    float3 inScatter = (scatterR * PhaseRayleigh(nu) + scatterM * PhaseMie(nu)) * SunIntensity;

    return surfaceColor * T + inScatter;
}

#endif // ATMOSPHERE_HLSLI
