// Atmosphere.hlsl
// Precomputed atmospheric scattering based on Bruneton & Neyret (2008).
// 4D scattering LUT parameterized by (h, mu, muS, nu).
// Includes transmittance LUT, single scattering LUT, and sky dome VS/PS.
//
// Coordinate convention: Y-up world space. Planet center at origin.
// All distances in meters.

// ---------------------------------------------------------------------------
// Physical constants
// ---------------------------------------------------------------------------

static const float PI                 = 3.14159265359f;
static const float INV_4PI            = 1.0f / (4.0f * PI);

// Sun disk angular radius (radians) — for limb darkening
static const float  SUN_ANGULAR_RADIUS = 0.004675f;

// ---------------------------------------------------------------------------
// LUT dimensions
// ---------------------------------------------------------------------------

static const uint TRANSMITTANCE_W     = 256u;
static const uint TRANSMITTANCE_H     = 64u;

// 4D scattering LUT packed as a 3D texture:
//   x = height       [0..SCATTER_W-1]
//   y = mu (view)    [0..SCATTER_H-1]
//   z = muS*nu packed [0..SCATTER_D-1]  (muS = sun zenith, nu = view-sun angle)
static const uint SCATTER_W           = 32u;
static const uint SCATTER_H           = 128u;
static const uint SCATTER_MUS         = 32u;
static const uint SCATTER_NU          = 8u;
// Actual z depth = SCATTER_MUS * SCATTER_NU
static const uint SCATTER_D           = SCATTER_MUS * SCATTER_NU;

// Sample counts (precomputed once, crank them high)
static const int  TRANSMITTANCE_STEPS = 500;
static const int  SCATTER_VIEW_STEPS  = 50;
static const int  SCATTER_SUN_STEPS   = 50;

// ---------------------------------------------------------------------------
// Utility
// ---------------------------------------------------------------------------

float SafeSqrt(float x) { return sqrt(max(0.0f, x)); }
float ClampCosine(float c) { return clamp(c, -1.0f, 1.0f); }

// ---------------------------------------------------------------------------
// Atmosphere constant buffer — shared by both compute and graphics paths
// ---------------------------------------------------------------------------

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

float GetRG2()
{
    return Rg * Rg;
}

float GetRT2()
{
    return Rt * Rt;
}

float DistToAtmTop(float r, float mu)
{
    // r    = radial distance from planet center
    // mu   = cos(view zenith angle)
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

// ---------------------------------------------------------------------------
// Density profiles
// ---------------------------------------------------------------------------

float DensityRayleigh(float altitude) { return exp(-altitude / HR); }
float DensityMie     (float altitude) { return exp(-altitude / HM); }

// ---------------------------------------------------------------------------
// Phase functions
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// LUT parameterization  (Bruneton 2008, mapping functions)
// ---------------------------------------------------------------------------

// --- Transmittance LUT: UV <-> (r, mu) ---

float2 TransmittanceUV(float r, float mu)
{
    // Remap r: [Rg, Rt] -> [0, 1]
    float uR = SafeSqrt((r - Rg) / (Rt - Rg));

    // Remap mu: account for horizon by using discriminant
    float rho    = SafeSqrt(r * r - GetRG2());
    float H      = SafeSqrt(GetRT2() - GetRG2());
    float delta  = r * r * mu * mu - r * r + GetRG2();
    float muHor  = -rho / r;

    float uMu;
    if (mu >= muHor)
    {
        // Above horizon
        float dAtm = DistToAtmTop(r, mu);
        float dMax = rho + H;
        uMu = 0.5f + 0.5f * (dAtm / dMax);
    }
    else
    {
        // Below horizon
        float dGnd = max(0.0f, -r * mu - SafeSqrt(delta));
        float dMax = rho;
        uMu = 0.5f * (dGnd / max(dMax, 1.0f));
    }

    return float2(uMu, uR);
}

void TransmittanceFromUV(float2 uv, out float r, out float mu)
{
    float uR  = uv.y;
    float uMu = uv.x;

    r         = Rg + uR * uR * (Rt - Rg);
    float rho = SafeSqrt(r * r - GetRG2());

    float H     = SafeSqrt(GetRT2() - GetRG2());
    float muHor = -rho / r;

    if (uMu >= 0.5f)
    {
        float dMax = rho + H;
        float dAtm = (uMu - 0.5f) * 2.0f * dMax;
        mu = (dAtm == 0.0f) ? 1.0f : ClampCosine((H * H - rho * rho - dAtm * dAtm) / (2.0f * r * dAtm));
    }
    else
    {
        float dMax = rho;
        float dGnd = uMu * 2.0f * dMax;
        mu = (dGnd == 0.0f) ? -1.0f : ClampCosine(-(rho * rho + dGnd * dGnd) / (2.0f * r * dGnd));
    }
}

// --- Scattering LUT: UVWZ <-> (r, mu, muS, nu) ---

float4 ScatteringUVWZ(float r, float mu, float muS, float nu)
{
    // r -> z in [0,1]
    float uR = SafeSqrt((r - Rg) / (Rt - Rg));

    // mu
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

    // muS: use non-linear mapping to give more resolution near horizon
    float uMuS = max(0.0f, 1.0f - 0.5f * (log(max(1e-4f, 1.0f + muS)) / log(2.0f)));
    uMuS = clamp(uMuS, 0.0f, 1.0f);

    // nu: linear
    float uNu = (nu + 1.0f) * 0.5f;

    return float4(uNu, uMuS, uMu, uR);
}

void ScatteringFromUVWZ(float4 uvwz, out float r, out float mu, out float muS, out float nu)
{
    float uNu  = uvwz.x;
    float uMuS = uvwz.y;
    float uMu  = uvwz.z;
    float uR   = uvwz.w;

    r         = Rg + uR * uR * (Rt - Rg);
    float rho = SafeSqrt(r * r - GetRG2());
    float H   = SafeSqrt(GetRT2() - GetRG2());
    float muHor = -rho / r;

    if (uMu >= 0.5f)
    {
        float dMax = rho + H;
        float dAtm = (uMu - 0.5f) * 2.0f * dMax;
        mu = (dAtm == 0.0f) ? 1.0f : ClampCosine((H * H - rho * rho - dAtm * dAtm) / (2.0f * r * dAtm));
    }
    else
    {
        float dMax = rho;
        float dGnd = uMu * 2.0f * dMax;
        mu = (dGnd == 0.0f) ? -1.0f : ClampCosine(-(rho * rho + dGnd * dGnd) / (2.0f * r * dGnd));
    }

    // Invert muS non-linear mapping
    float t = (1.0f - uMuS) * 2.0f;
    muS = ClampCosine(pow(2.0f, t) - 1.0f - 1e-4f);

    nu  = ClampCosine(uNu * 2.0f - 1.0f);
}

// Helper to sample 4D LUT stored as 3D (nu packed into z with muS)
float4 SampleScatteringLUT(Texture3D<float4> lut, SamplerState smp, float r, float mu, float muS, float nu)
{
    float4 uvwz = ScatteringUVWZ(r, mu, muS, nu);

    // Pack muS and nu into z coordinate
    float uNu   = uvwz.x;
    float uMuS  = uvwz.y;
    float uMu   = uvwz.z;
    float uR    = uvwz.w;

    // z = floor(uNu * (SCATTER_NU-1)) * SCATTER_MUS + uMuS * SCATTER_MUS
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

    // Remap u to include height in w
    uvw0 = float3(uR, uMu, (nuIndex0 / float(SCATTER_NU)) + uMuS / float(SCATTER_NU));
    uvw1 = float3(uR, uMu, (nuIndex1 / float(SCATTER_NU)) + uMuS / float(SCATTER_NU));

    return lerp(lut.SampleLevel(smp, uvw0, 0), lut.SampleLevel(smp, uvw1, 0), nuFrac);
}

// ---------------------------------------------------------------------------
// Optical depth integration (used only in precompute passes)
// ---------------------------------------------------------------------------

struct OpticalDepthResult { float3 rayleigh; float mie; };

OpticalDepthResult IntegrateOpticalDepth(float3 startPos, float3 dir, float dist, int steps)
{
    OpticalDepthResult result;
    result.rayleigh = 0.0f;
    result.mie      = 0.0f;
    if (dist <= 0.0f) return result;

    float dt = dist / float(steps);
    for (int i = 0; i < steps; ++i)
    {
        float3 p = startPos + dir * (dt * (float(i) + 0.5f));
        float alt = max(0.0f, length(p) - Rg);
        result.rayleigh += DensityRayleigh(alt) * dt;
        result.mie      += DensityMie(alt)      * dt;
    }
    return result;
}

// ---------------------------------------------------------------------------
// Transmittance between two points (sampled from LUT)
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// Compute passes
// ---------------------------------------------------------------------------

#if defined(COMPUTE_SHADER)

RWTexture2D<float4> TransmittanceLUT : register(u0);
SamplerState        LinearClamp        : register(s0);

[numthreads(8, 8, 1)]
void CS_Transmittance(uint3 id : SV_DispatchThreadID)
{
    uint w, h;
    TransmittanceLUT.GetDimensions(w, h);
    if (id.x >= w || id.y >= h) return;

    float2 uv = (float2(id.xy) + 0.5f) / float2(w, h);
    float r, mu;
    TransmittanceFromUV(uv, r, mu);

    float dist = DistToAtmTop(r, mu);
    float3 pos = float3(0.0f, r, 0.0f);
    float3 dir = float3(SafeSqrt(1.0f - mu * mu), mu, 0.0f);

    OpticalDepthResult od = IntegrateOpticalDepth(pos, dir, dist, TRANSMITTANCE_STEPS);

    float3 tau = BetaR * od.rayleigh + BetaMExtinct * od.mie;
    TransmittanceLUT[id.xy] = float4(exp(-tau), 1.0f);
}

RWTexture3D<float4> ScatteringLUT    : register(u0);
Texture2D<float4>   TransmittanceLUTin : register(t0);

[numthreads(4, 4, 4)]
void CS_Scattering(uint3 id : SV_DispatchThreadID)
{
    uint sw, sh, sd;
    ScatteringLUT.GetDimensions(sw, sh, sd);
    if (id.x >= sw || id.y >= sh || id.z >= sd) return;

    // Unpack z: outer = nu index, inner = muS index
    uint nuIdx  = id.z / SCATTER_MUS;
    uint muSIdx = id.z % SCATTER_MUS;

    float4 uvwz = float4(
        (float(id.x) + 0.5f) / float(sw),       // uR
        (float(id.y) + 0.5f) / float(sh),       // uMu
        (float(muSIdx) + 0.5f) / float(SCATTER_MUS),
        (float(nuIdx)  + 0.5f) / float(SCATTER_NU)
    );
    // Reorder to match ScatteringFromUVWZ(uNu, uMuS, uMu, uR)
    float4 uvwzReordered = float4(uvwz.w, uvwz.z, uvwz.y, uvwz.x);

    float r, mu, muS, nu;
    ScatteringFromUVWZ(uvwzReordered, r, mu, muS, nu);

    float3 pos = float3(0.0f, r, 0.0f);
    float3 viewDir;
    {
        float sinMu = SafeSqrt(1.0f - mu * mu);
        viewDir = float3(sinMu, mu, 0.0f);
    }

    // Reconstruct sunDir from muS and nu
    //   nu = dot(viewDir, sunDir)
    //   muS = sunDir.y
    float3 sunDir;
    {
        float sinMuS = SafeSqrt(1.0f - muS * muS);
        // nu = sinMu * sinMuS * cos(azimuthDiff) + mu * muS
        // solve for cos(azimuthDiff)
        float sinMu = SafeSqrt(1.0f - mu * mu);
        float cosAz = (sinMu > 1e-4f && sinMuS > 1e-4f)
                      ? clamp((nu - mu * muS) / (sinMu * sinMuS), -1.0f, 1.0f)
                      : 0.0f;
        sunDir = float3(sinMuS * cosAz, muS, sinMuS * SafeSqrt(1.0f - cosAz * cosAz));
    }

    bool hitGround  = RayHitsGround(r, mu);
    float viewDist  = hitGround ? DistToGround(r, mu) : DistToAtmTop(r, mu);
    if (viewDist <= 0.0f)
    {
        ScatteringLUT[id] = float4(0.0f, 0.0f, 0.0f, 0.0f);
        return;
    }

    float3 scatterR = 0.0f;
    float  scatterM = 0.0f;
    float  dt       = viewDist / float(SCATTER_VIEW_STEPS);

    for (int i = 0; i < SCATTER_VIEW_STEPS; ++i)
    {
        float t   = dt * (float(i) + 0.5f);
        float3 p  = pos + viewDir * t;
        float  rp = length(p);
        float  alt = max(0.0f, rp - Rg);

        // Transmittance from camera to sample point
        float3 T_cam = GetTransmittance(TransmittanceLUTin, LinearClamp, r, mu, t, hitGround);

        // Check if sun is visible from p
        float muS_p = dot(p, sunDir) / rp;
        float2 sunIsect;
        {
            float a = 1.0f;
            float b = 2.0f * dot(p, sunDir);
            float c = dot(p, p) - GetRG2();
            float d = b * b - 4.0f * a * c;
            sunIsect = (d < 0.0f) ? float2(1e5f, -1e5f)
                                  : float2((-b - sqrt(d)) * 0.5f, (-b + sqrt(d)) * 0.5f);
        }
        bool sunBlocked = (sunIsect.x < sunIsect.y && sunIsect.x > 0.0f);
        if (sunBlocked) continue;

        float distSun;
        {
            float disc = rp * rp * (muS_p * muS_p - 1.0f) + GetRT2();
            distSun = max(0.0f, -rp * muS_p + SafeSqrt(disc));
        }
        float3 T_sun = GetTransmittance(TransmittanceLUTin, LinearClamp, rp, muS_p, distSun, false);

        float3 atten = T_cam * T_sun;

        scatterR += DensityRayleigh(alt) * dt * atten;
        scatterM += DensityMie(alt)      * dt * atten.x; // store single channel
    }

    // Store pre-multiplied by beta so PS only needs to apply phase
    ScatteringLUT[id] = float4(BetaR * scatterR, BetaMScatter * scatterM);
}

// ---------------------------------------------------------------------------
#else // Graphics shaders: sky dome VS/PS
// ---------------------------------------------------------------------------

cbuffer FrameCB : register(b0)
{
    float4x4 ViewMatrix;
    float4x4 ProjMatrix;
    float4   LightDir;    // xyz = direction toward sun (normalized), w unused
    float4   LightColor;  // xyz = color, w = intensity
    float3   CamPos;
    uint     Padding0;
};

cbuffer MeshCB : register(b1)
{
    float4x4 WorldMatrix;
    float3x4 NormalMatrix;
    float4   BaseColor;
    float    Metallic;
    float    Roughness;
    float    Opacity;
    uint     TextureFlags;
};

Texture2D<float4>   TransmittanceLUT : register(t0);
Texture3D<float4>   ScatteringLUT    : register(t1);
SamplerState        LinearClamp      : register(s0);

// ---------------------------------------------------------------------------
// Vertex shader
// ---------------------------------------------------------------------------

struct VSInput
{
    float3 Position  : POSITION;
    float3 Normal    : NORMAL;
    float3 Tangent   : TANGENT;
    float3 Bitangent : BITANGENT;
    float2 TexCoord  : TEXCOORD;
};

struct VSOutput
{
    float4 PositionCS  : SV_POSITION;
    float3 WorldPos    : TEXCOORD0;
};

VSOutput VS_Sky(VSInput input)
{
    VSOutput output;

    // Pass local position as direction for the pixel shader
    output.WorldPos = input.Position;

    // w=0 treats position as a direction — ViewMatrix applies only rotation,
    // never translation, so no vertex is ever "behind" the camera
    float4 viewDir = mul(ViewMatrix, float4(input.Position, 0.0f));
    float4 clipPos = mul(ProjMatrix, viewDir);
    clipPos.z = clipPos.w; // Force to far plane
    output.PositionCS = clipPos;

    return output;
}

// ---------------------------------------------------------------------------
// Pixel shader — single scattering from precomputed LUTs
// ---------------------------------------------------------------------------

// Sun disk with smooth limb darkening
float SunDisk(float3 viewDir, float3 sunDir)
{
    float cosAngle = dot(viewDir, sunDir);
    float cosLimit = cos(SUN_ANGULAR_RADIUS);
    if (cosAngle < cosLimit) return 0.0f;

    float u = (cosAngle - cosLimit) / (1.0f - cosLimit);
    // Limb darkening approximation: I(u) = 1 - 0.6*(1-sqrt(u))
    return saturate(1.0f - 0.6f * (1.0f - sqrt(u)));
}

float4 PS_Sky(VSOutput input) : SV_Target
{
    float3 viewDir = normalize(input.WorldPos);
    float3 sunDir  = normalize(-LightDir.xyz);
    float  sunIntensity = SunIntensity;

    // Camera parameters in planet frame
    // Camera is typically near sea level: CamPos.y + Rg gives the radial distance
    // Clamp to just above ground to avoid numerical issues
    float r  = clamp(length(CamPos) + Rg, Rg + 1.0f, Rt - 1.0f);

    float3 up  = float3(0.0f, 1.0f, 0.0f); // Planet up in world space
    float  mu  = ClampCosine(dot(viewDir, up));
    float  muS = ClampCosine(dot(up, sunDir));
    float  nu  = ClampCosine(dot(viewDir, sunDir));

    // --- Sample scattering LUT ---
    float4 scatter = SampleScatteringLUT(ScatteringLUT, LinearClamp, r, mu, muS, nu);
    float3 scatterR = scatter.rgb;
    float  scatterM = scatter.a;

    // Apply phase functions
    float phaseR = PhaseRayleigh(nu);
    float phaseM = PhaseMie(nu);
    float3 inScatter = (scatterR * phaseR + scatterM * phaseM) * sunIntensity;

    // --- Sun disk ---
    // Only draw sun disk if ray doesn't hit the ground
    float3 sunColor = 0.0f;
    if (!RayHitsGround(r, mu))
    {
        float disk = SunDisk(viewDir, sunDir);
        // Attenuate sun by transmittance along sun direction
        float2 sunTransUV = TransmittanceUV(r, muS);
        float3 sunTrans   = TransmittanceLUT.SampleLevel(LinearClamp, sunTransUV, 0).rgb;
        sunColor = disk * sunTrans * sunIntensity * LightColor.rgb;
    }

    float3 color = inScatter + sunColor;

    // Tone mapping: simple Reinhard on luminance to keep HDR sky in [0,1]
    float lum = dot(color, float3(0.2126f, 0.7152f, 0.0722f));
    color *= 1.0f / (1.0f + lum);

    // Gamma correction (assume linear rendering pipeline, apply 2.2)
    color = pow(max(color, 0.0f), 1.0f / 2.2f);

    return float4(color, 1.0f);
}

#endif // COMPUTE_SHADER
