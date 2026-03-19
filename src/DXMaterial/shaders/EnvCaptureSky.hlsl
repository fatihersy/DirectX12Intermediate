// EnvCaptureSky.hlsl
// -----------------------------------------------------------------
// Renders sky into a single cubemap face by sampling the precomputed
// transmittance and scattering LUTs directly. No sky dome geometry
// needed — a fullscreen triangle covers the face, the PS reconstructs
// the world-space view direction from the face's view/proj matrices
// and evaluates the atmospheric scattering inline.
//
// This is the same math as SkyDome.hlsl PS_Sky, but driven by the
// cubemap face camera instead of the main camera.
// -----------------------------------------------------------------

// =====================================================================
// Shared atmosphere constants (same layout as SkyDome.hlsl AtmosCB)
// =====================================================================

cbuffer CaptureCB : register(b0)
{
    float4x4 ViewMatrix;
    float4x4 ProjMatrix;
    float4 LightDir;
    float4 LightColor;
    float3 CapturePos;
    float Pad0;
    float3 CamPos;
    float Pad1;
};

cbuffer AtmosCB : register(b2)
{
    float3 BetaR;
    float PadA0;
    float BetaMScatter;
    float BetaMExtinct;
    float MieG;
    float HR;
    float HM;
    float Rg;
    float Rt;
    float SunIntensity;
    float3 SunDir;
    float PadA1;
};

Texture2D<float4> TransmittanceLUT : register(t0);
Texture3D<float4> ScatteringLUT : register(t1);
SamplerState LinearClamp : register(s0);

// =====================================================================
// Atmosphere helpers (duplicated from SkyDome.hlsl to keep self-contained)
// =====================================================================

static const float PI = 3.14159265359f;
static const float SUN_ANGULAR_RADIUS = 0.004675f;

static const uint SCATTER_MUS = 32u;
static const uint SCATTER_NU = 8u;

float SafeSqrt(float x)
{
    return sqrt(max(0.0f, x));
}
float ClampCosine(float c)
{
    return clamp(c, -1.0f, 1.0f);
}

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
    float disc = r * r * (mu * mu - 1.0f) + GetRT2();
    return max(0.0f, -r * mu + SafeSqrt(disc));
}

bool RayHitsGround(float r, float mu)
{
    return mu < 0.0f && (r * r * (mu * mu - 1.0f) + GetRG2()) >= 0.0f;
}

float2 TransmittanceUV(float r, float mu)
{
    float H = SafeSqrt(GetRT2() - GetRG2());
    float rho = SafeSqrt(r * r - GetRG2());
    float d = DistToAtmTop(r, mu);
    float dMin = Rt - r;
    float dMax = rho + H;
    float uMu = (d - dMin) / (dMax - dMin);
    float uR = rho / H;
    return float2(uMu, uR);
}

float4 ScatteringUVWZ(float r, float mu, float muS, float nu)
{
    float uR = SafeSqrt((r - Rg) / (Rt - Rg));

    float rho = SafeSqrt(r * r - GetRG2());
    float H = SafeSqrt(GetRT2() - GetRG2());
    float muHor = -rho / r;
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

    float uNu = uvwz.x;
    float uMuS = uvwz.y;
    float uMu = uvwz.z;
    float uR = uvwz.w;

    float nuIndex0 = floor(uNu * float(SCATTER_NU - 1));
    float nuIndex1 = nuIndex0 + 1.0f;
    float nuFrac = uNu * float(SCATTER_NU - 1) - nuIndex0;

    float3 uvw0 = float3(uR, uMu, (nuIndex0 / float(SCATTER_NU)) + uMuS / float(SCATTER_NU));
    float3 uvw1 = float3(uR, uMu, (nuIndex1 / float(SCATTER_NU)) + uMuS / float(SCATTER_NU));

    return lerp(lut.SampleLevel(smp, uvw0, 0), lut.SampleLevel(smp, uvw1, 0), nuFrac);
}

float PhaseRayleigh(float cosTheta)
{
    return (3.0f / (16.0f * PI)) * (1.0f + cosTheta * cosTheta);
}

float PhaseMie(float cosTheta)
{
    float g = MieG;
    float g2 = g * g;
    float num = (1.0f - g2);
    float den = pow(1.0f + g2 - 2.0f * g * cosTheta, 1.5f);
    return (1.0f / (4.0f * PI)) * num / den;
}

float SunDisk(float3 viewDir, float3 sunDir)
{
    float cosAngle = dot(viewDir, sunDir);
    float cosLimit = cos(SUN_ANGULAR_RADIUS);
    if (cosAngle < cosLimit)
        return 0.0f;
    float u = (cosAngle - cosLimit) / (1.0f - cosLimit);
    return saturate(1.0f - 0.6f * (1.0f - sqrt(u)));
}

// =====================================================================
// Vertex shader — fullscreen triangle (no vertex buffer)
// =====================================================================

struct VSOutput
{
    float4 posCS : SV_POSITION;
    float3 viewDir : TEXCOORD0; // World-space view direction for this pixel
};

VSOutput VS_EnvSky(uint vertexID : SV_VertexID)
{
    VSOutput output;

    // Generate fullscreen triangle in NDC
    float2 uv = float2((vertexID << 1) & 2, vertexID & 2);
    float4 posNDC = float4(uv * 2.0f - 1.0f, 1.0f, 1.0f); // z=1 (far plane)
    posNDC.y = -posNDC.y; // Flip Y for DX convention

    output.posCS = posNDC;

    // Unproject NDC back to world-space direction.
    // We need the inverse of (View * Proj). Since we're only interested in
    // direction (not position), we can use the inverse view rotation.
    float4x4 invProj = (float4x4) 0;
    // Approximate inverse of 90° FOV symmetric projection:
    // For a standard perspective matrix with fov=90°, aspect=1:
    //   proj[0][0] = 1/tan(45°) = 1,  proj[1][1] = 1
    // So inv is straightforward. But for robustness, we reconstruct from
    // the view matrix directly.

    // Instead, pass the direction through the inverse view-proj.
    // Use the fact that for a cubemap face camera, the view matrix is a
    // pure rotation + translation. We can reconstruct the world direction
    // from the clip-space position.

    // Reconstruct view-space ray direction from NDC
    // For 90° FOV, aspect 1:1, the projection maps x/z and y/z to [-1,1]
    float3 viewRay = float3(posNDC.x, posNDC.y, 1.0f); // In view space

    // Rotate from view space to world space using the inverse of the
    // upper-left 3x3 of the view matrix (view matrix is orthonormal).
    // HLSL default column-major reads the CPU's row-major data transposed,
    // so ViewMatrix[i].xyz gives row i of the logical view matrix V
    // (right, up, forward basis vectors). To invert the rotation (view→world),
    // we need V^T * viewRay, which equals mul(viewRay, V) in HLSL.
    float3x3 viewRot = float3x3(
        ViewMatrix[0].xyz,
        ViewMatrix[1].xyz,
        ViewMatrix[2].xyz
    );
    output.viewDir = mul(viewRay, viewRot);

    return output;
}

// =====================================================================
// Pixel shader — evaluate atmosphere from LUTs
// =====================================================================

float4 PS_EnvSky(VSOutput input) : SV_Target
{
    float3 viewDir = normalize(input.viewDir);
    float3 sunDir = normalize(-LightDir.xyz);
    float sunIntensity = SunIntensity;

    float r = clamp(length(CamPos) + Rg, Rg + 1.0f, Rt - 1.0f);

    float3 up = float3(0.0f, 1.0f, 0.0f);
    float mu = ClampCosine(dot(viewDir, up));
    float muS = ClampCosine(dot(up, sunDir));
    float nu = ClampCosine(dot(viewDir, sunDir));

    // Sample scattering
    float4 scatter = SampleScatteringLUT(ScatteringLUT, LinearClamp, r, mu, muS, nu);
    float3 scatterR = scatter.rgb;
    float scatterM = scatter.a;

    float phaseR = PhaseRayleigh(nu);
    float phaseM = PhaseMie(nu);
    float3 inScatter = (scatterR * phaseR + scatterM * phaseM) * sunIntensity;

    // Sun disk
    float3 sunColor = 0.0f;
    if (!RayHitsGround(r, mu))
    {
        float disk = SunDisk(viewDir, sunDir);
        float2 sunTransUV = TransmittanceUV(r, muS);
        float3 sunTrans = TransmittanceLUT.SampleLevel(LinearClamp, sunTransUV, 0).rgb;
        sunColor = disk * sunTrans * sunIntensity * LightColor.rgb;
    }

    float3 color = inScatter + sunColor;

    // NOTE: We store in LINEAR HDR — no tone mapping or gamma here.
    // Tone mapping happens in the final geometry pass when the cubemap is sampled.
    return float4(color, 1.0f);
}
