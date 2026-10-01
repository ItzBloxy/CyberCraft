// BlockRender's shaders: Minecraft section vertices (SkyCraft's 32-byte format), read from a buffer and drawn into
// the game's frame. The plugin compiles this file in the game with D3DCompile (shader model 5.1); "reload" in
// blockrender.request compiles it again. src/render/FrameConstants.h mirrors the constant buffers.

struct Vertex
{
    float3 position;  // from the section's minimum corner, in Minecraft axes (x east, y up, z south)
    float2 uv;        // atlas coordinates
    uint   colour;    // RGBA8, red in the low byte: tint and ambient occlusion
    uint   light;     // block light in bits 0-7, sky light in bits 8-15
    uint   flags;     // bit 0 alpha-tested, bit 1 translucent, bits 4-7 the face's direction + 1
};

cbuffer Frame : register(b0)
{
    row_major float4x4 cameraToClip;  // camera-relative world position to clip
    float4 target;                    // width, height, 1 / width, 1 / height of the image drawn into
    float4 depthExtent;               // left, top, width, height of the depth image within its buffer
    uint4  depthInfo;                 // floats per row of the depth copy; 1 if there is a copy; 1 if depth is reversed
    uint4  atlasInfo;                 // width, height, levels
    uint4  atlasLevels[2];            // where each level's texels start
    float4 tuning;                    // brightness; depth tolerance; exposure; the motion vectors' sign
    row_major float4x4 clipToPrevClip;  // Streamline's: a clip position to where it was a frame before
    float4 jitter;                    // the jitter as an offset per unit of clip w: x, y
    float4 material;                  // the G-buffer's material values: metalness, roughness, B; 0
};

cbuffer Section : register(b1)
{
    float3 sectionOffset;  // the section's minimum corner: camera-relative, world axes
    float  sectionUnused;
};

StructuredBuffer<Vertex> vertices : register(t0);
StructuredBuffer<uint>   atlas    : register(t1);
StructuredBuffer<float>  depthCopy : register(t2);

struct VSOut
{
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
    float4 colour   : COLOR0;
    nointerpolation uint face : TEXCOORD1;
    float4 clip     : TEXCOORD2;  // without jitter
    float4 prevClip : TEXCOORD3;  // where it was a frame before
};

VSOut BlocksVS(uint id : SV_VertexID)
{
    const Vertex v = vertices[id];
    VSOut o;
    // Minecraft (x, y, z) is world (x, -z, y).
    const float3 world = sectionOffset + float3(v.position.x, -v.position.z, v.position.y);
    const float4 clip = mul(float4(world, 1.0), cameraToClip);
    o.position = float4(clip.xy + jitter.xy * clip.w, clip.zw);
    o.clip = clip;
    o.prevClip = mul(clip, clipToPrevClip);
    o.uv = v.uv;
    o.colour = float4(v.colour & 0xFF, (v.colour >> 8) & 0xFF, (v.colour >> 16) & 0xFF, v.colour >> 24) / 255.0;
    o.face = (v.flags >> 4) & 15;
    return o;
}

// The atlas texel nearest uv, in the mip level that uv's rate of change across the screen picks.
float4 SampleAtlas(float2 uv)
{
    const float2 size = float2(atlasInfo.x, atlasInfo.y);
    const float2 dx = ddx(uv * size);
    const float2 dy = ddy(uv * size);
    const float  lod = 0.5 * log2(max(max(dot(dx, dx), dot(dy, dy)), 1e-8));
    const uint   level = (uint)clamp(floor(lod + 0.5), 0.0, (float)atlasInfo.z - 1.0);
    const uint2  levelSize = max(uint2(atlasInfo.x >> level, atlasInfo.y >> level), uint2(1, 1));
    const uint2  texel = min((uint2)(uv * float2(levelSize)), levelSize - 1);
    const uint   start = atlasLevels[level / 4][level % 4];
    const uint   packed = atlas[start + texel.y * levelSize.x + texel.x];
    return float4(packed & 0xFF, (packed >> 8) & 0xFF, (packed >> 16) & 0xFF, packed >> 24) / 255.0;
}

// Minecraft's brightness per face, by the flags' direction + 1: none, down, up, north, south, west, east.
static const float kFaceShade[7] = { 1.0, 0.5, 1.0, 0.8, 0.8, 0.6, 0.6 };

// Step 2: on the finished frame, before Present. Hidden where the game's depth (copied at the DLSS evaluate) says
// something nearer is there; blocks sort among themselves through a depth buffer of the plugin's own.
float4 PresentPS(VSOut i) : SV_Target0
{
    const float4 texel = SampleAtlas(i.uv);
    if (texel.a < 0.5)
    {
        discard;
    }
    if (depthInfo.y != 0)
    {
        const float2 at = depthExtent.xy + i.position.xy * target.zw * depthExtent.zw;
        const uint2  last = (uint2)(depthExtent.xy + depthExtent.zw) - 1;
        const uint2  where = min((uint2)at, last);
        const float  scene = depthCopy[where.y * depthInfo.x + where.x];
        const bool   hidden = depthInfo.z != 0 ? scene > i.position.z + tuning.y : scene < i.position.z - tuning.y;
        if (hidden)
        {
            discard;
        }
    }
    return float4(texel.rgb * i.colour.rgb * kFaceShade[min(i.face, 6)] * tuning.x, 1.0);
}

// The sRGB curve, undone: the atlas holds Minecraft's colours as they look, and the scene before DLSS is linear.
float3 Linear(float3 aColour)
{
    return aColour <= 0.04045 ? aColour / 12.92 : pow(abs((aColour + 0.055) / 1.055), 2.4);
}

// As Camera.cpp's MotionVector: a fraction of the screen, from where the point is to where it was.
float2 MotionVector(VSOut i)
{
    const float2 now = i.clip.xy / i.clip.w;
    const float2 before = i.prevClip.xy / i.prevClip.w;
    return (before - now) * float2(0.5, -0.5) * tuning.w;
}

struct PredlssOut
{
    float4 colour : SV_Target0;
    float2 motion : SV_Target1;
};

// Step 3: into the scene's colour before DLSS, which then tone maps, blooms and grades the blocks with the rest. The
// game's own depth hides what's behind, and the motion vectors let DLSS follow the blocks as the camera moves.
PredlssOut PredlssPS(VSOut i)
{
    const float4 texel = SampleAtlas(i.uv);
    if (texel.a < 0.5)
    {
        discard;
    }
    PredlssOut o;
    o.colour = float4(Linear(texel.rgb * i.colour.rgb) * kFaceShade[min(i.face, 6)] * tuning.z, 1.0);
    o.motion = MotionVector(i);
    return o;
}

// Each face's world-space normal, by the flags' direction + 1: none, down, up, north, south, west, east. As FaceNormal
// in src/render/Gbuffer.cpp: Minecraft's directions in world axes (x east, y north, z up).
static const float3 kFaceNormal[7] = { float3(0, 0, 1), float3(0, 0, -1), float3(0, 0, 1), float3(0, 1, 0), float3(0, -1, 0),
                                       float3(-1, 0, 0), float3(1, 0, 0) };

struct GbufferOut
{
    float4 base     : SV_Target0;  // the base colour, sRGB-encoded as the atlas is
    float4 normal   : SV_Target1;  // the world-space normal, as n * 0.5 + 0.5
    float4 material : SV_Target2;  // metalness, roughness, B (likely translucency), emission
    float2 motion   : SV_Target3;
};

// Step 4: into the G-buffer, which the game then lights, shadows and fogs as its own. The colour is Minecraft's
// without its face brightness: the game's light does that, from the normal. Alphas are 0, as on the game's ordinary
// surfaces.
GbufferOut GbufferPS(VSOut i)
{
    const float4 texel = SampleAtlas(i.uv);
    if (texel.a < 0.5)
    {
        discard;
    }
    GbufferOut o;
    o.base = float4(saturate(texel.rgb * i.colour.rgb * tuning.x), 0.0);
    o.normal = float4(kFaceNormal[min(i.face, 6)] * 0.5 + 0.5, 0.0);
    o.material = float4(material.xyz, 0.0);
    o.motion = MotionVector(i);
    return o;
}

// Step 4's second pass: the blocks' depth into the tagged depth, the copy of the G-buffer's depth that DLSS reads.
void DepthPS(VSOut i)
{
    if (SampleAtlas(i.uv).a < 0.5)
    {
        discard;
    }
}
