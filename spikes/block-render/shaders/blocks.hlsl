// BlockRender's shaders: Minecraft section vertices (SkyCraft's 32-byte format), read from a buffer and drawn into
// the game's frame. The plugin compiles this file in the game with D3DCompile (shader model 5.1); "reload" in
// blockrender.request compiles it again. src/render/FrameConstants.h mirrors the constant buffers.

struct Vertex
{
    float3 position;  // from the section's minimum corner, in Minecraft axes (x east, y up, z south)
    float2 uv;        // atlas coordinates
    uint   colour;    // RGBA8, red in the low byte: tint and ambient occlusion
    uint   light;     // block light in bits 0-7, sky light in bits 8-15
    uint   flags;     // bit 0 alpha-tested, bit 1 translucent, bit 3 no mipmaps, bits 4-7 the face's direction + 1
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
    float4 material;                  // the G-buffer's material values: metalness, roughness, B; plants' B (the polish pass)
    float4 entityTuning;              // spike 7: a glowing triangle's emission; the block light it glows from; the outline's
                                      // width in render pixels (step B); 0
};

cbuffer Section : register(b1)
{
    float3 sectionOffset;  // the section's minimum corner: camera-relative, world axes
    uint   sectionFirst;   // the draw's first vertex in the section's buffer: where its part starts (the polish pass)
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
    float2 light    : TEXCOORD4;  // block and sky light, 0 to 15
};

VSOut BlocksVS(uint id : SV_VertexID)
{
    const Vertex v = vertices[sectionFirst + id];
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
    o.light = float2(v.light & 0xFF, (v.light >> 8) & 0xFF);
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
float2 MotionBetween(float4 aClip, float4 aPrevClip)
{
    const float2 now = aClip.xy / aClip.w;
    const float2 before = aPrevClip.xy / aPrevClip.w;
    return (before - now) * float2(0.5, -0.5) * tuning.w;
}

float2 MotionVector(VSOut i)
{
    return MotionBetween(i.clip, i.prevClip);
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
// surfaces. A plant (no direction of its own) takes the plants' material B, as the game's own plants have a higher one.
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
    o.material = float4(material.xy, i.face == 0 ? material.w : material.z, 0.0);
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

// The polish pass: a translucent block (water, stained glass, ice, slime, honey), blended into the scene's colour just
// before DLSS as Minecraft's translucent_terrain blends it, its depth written as that writes it, and cut out under 0.1
// of alpha as its ALPHA_CUTOUT cuts. Lit as step B's particles are, by Minecraft's light curve of the brighter of its
// block and sky light, times exposure, and shaded per face as PredlssPS shades a block.
float4 BlendedBlockPS(VSOut i) : SV_Target0
{
    const float4 texel = SampleAtlas(i.uv);
    const float  alpha = texel.a * i.colour.a;
    if (alpha < 0.1)
    {
        discard;
    }
    const float level = min(max(i.light.x, i.light.y), 15.0) / 15.0;
    const float light = level / (4.0 - 3.0 * level);
    return float4(Linear(texel.rgb * i.colour.rgb) * kFaceShade[min(i.face, 6)] * light * tuning.z, alpha);
}

// Spike 7: Minecraft's entities, block entities, particles and world entities (the spec's "Step A: into the G-buffer"),
// from a snapshot of the frame's vertices, each draw with its texture: Minecraft's combined atlas, or an entity texture
// in entityTextures. A draw's constants, in b1 where the blocks have Section.
cbuffer Entity : register(b1)
{
    float3 entityOffset;   // the frame's origin: camera-relative, world axes
    uint   entityFirst;    // the draw's first vertex in the snapshot
    uint   textureStart;   // its texture's first texel in entityTextures
    uint   textureWidth;   // its size; 0 for Minecraft's combined atlas
    uint   textureHeight;
    uint   entityUnused;
};

StructuredBuffer<uint> entityTextures : register(t3);  // RGBA8, one texel per uint, red in the low byte

struct EntityOut
{
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
    float4 colour   : COLOR0;
    nointerpolation uint flags : TEXCOORD1;  // the triangle's first vertex's, as its light
    float4 clip     : TEXCOORD2;  // without jitter
    float4 prevClip : TEXCOORD3;  // where it was a frame before
    nointerpolation uint light : TEXCOORD4;
    float3 world    : TEXCOORD5;  // camera-relative, world axes: for the triangle's own normal
};

EntityOut EntityVS(uint id : SV_VertexID)
{
    const Vertex v = vertices[entityFirst + id];
    EntityOut o;
    // Minecraft (x, y, z) is world (x, -z, y).
    const float3 world = entityOffset + float3(v.position.x, -v.position.z, v.position.y);
    const float4 clip = mul(float4(world, 1.0), cameraToClip);
    o.position = float4(clip.xy + jitter.xy * clip.w, clip.zw);
    o.clip = clip;
    o.prevClip = mul(clip, clipToPrevClip);
    o.uv = v.uv;
    o.colour = float4(v.colour & 0xFF, (v.colour >> 8) & 0xFF, (v.colour >> 16) & 0xFF, v.colour >> 24) / 255.0;
    o.flags = v.flags;
    o.light = v.light;
    o.world = world;
    return o;
}

// The draw's texel nearest uv: Minecraft's combined atlas at its top level, as SkyCraft marks every entity vertex for no
// mipmaps, or the draw's entity texture. Root descriptors carry no bounds, so the UV is clamped to the texture first: a
// bad one reads an edge texel. [branch] keeps the other buffer unread.
float4 EntityTexel(float2 uv)
{
    uint packed;
    [branch] if (textureWidth == 0)
    {
        const uint2 size = uint2(atlasInfo.x, atlasInfo.y);
        const uint2 texel = min((uint2)(saturate(uv) * float2(size)), size - 1);
        packed = atlas[atlasLevels[0].x + texel.y * size.x + texel.x];
    }
    else
    {
        const uint2 size = uint2(textureWidth, textureHeight);
        const uint2 texel = min((uint2)(saturate(uv) * float2(size)), size - 1);
        packed = entityTextures[textureStart + texel.y * size.x + texel.x];
    }
    return float4(packed & 0xFF, (packed >> 8) & 0xFF, (packed >> 16) & 0xFF, packed >> 24) / 255.0;
}

// The triangle's own normal, from how its camera-relative position changes across the screen; straight up for none.
float3 OwnNormal(float3 aWorld)
{
    const float3 across = cross(ddy(aWorld), ddx(aWorld));
    const float  size = length(across);
    return size > 1e-12 ? across / size : float3(0.0, 0.0, 1.0);
}

// The normal a vertex's flags give, bits 4 to 6 its direction + 1: 7 the triangle's own (aOwn), turned toward the camera,
// as SkyCraft marks models, items and blocks drawn as entities; 0 (particles) straight up, as the blocks' plants are lit;
// 1 to 6 the face's.
float3 EntityNormal(uint aFlags, float3 aOwn, float3 aWorld)
{
    const uint face = (aFlags >> 4) & 7;
    if (face == 7)
    {
        return dot(aOwn, aWorld) > 0.0 ? -aOwn : aOwn;  // aWorld runs from the camera to the point
    }
    return kFaceNormal[min(face, 6)];
}

// Step A: an entity into the G-buffer, as GbufferPS draws a block, cut out where texel times vertex alpha is under 0.1,
// Minecraft's cut-out for entities, particles and items. A triangle whose first vertex has block light of at least
// entityTuning.y glows: it writes entityTuning.x into the material target's A, which spike 3 found above 0 only on the
// game's windows and signs, likely as emission.
GbufferOut EntityPS(EntityOut i)
{
    const float3 own = OwnNormal(i.world);  // before the discard, which ends the pixel's part in the derivatives
    const float4 texel = EntityTexel(i.uv);
    if (texel.a * i.colour.a < 0.1)
    {
        discard;
    }
    GbufferOut o;
    o.base = float4(saturate(texel.rgb * i.colour.rgb * tuning.x), 0.0);
    o.normal = float4(EntityNormal(i.flags, own, i.world) * 0.5 + 0.5, 0.0);
    o.material = float4(material.xyz, (i.light & 0xFF) >= (uint)entityTuning.y ? entityTuning.x : 0.0);
    o.motion = MotionBetween(i.clip, i.prevClip);
    return o;
}

// Step A's cracks over a block being mined, into the base colour alone, which their pipeline blends as Minecraft's
// crumbling does: DEST_COLOR and SRC_COLOR, the crack's colour times what's there, twice.
float4 CrackPS(EntityOut i) : SV_Target0
{
    const float4 texel = EntityTexel(i.uv);
    if (texel.a * i.colour.a < 0.1)
    {
        discard;
    }
    return float4(texel.rgb * i.colour.rgb, 1.0);
}

// Step A's second pass: the entities' depth into the tagged depth, cut out as EntityPS cuts them.
void EntityDepthPS(EntityOut i)
{
    if (EntityTexel(i.uv).a * i.colour.a < 0.1)
    {
        discard;
    }
}

// Step B (the spec's "Step B: before DLSS"): a translucent particle, blended into the scene's colour just before DLSS, as
// Minecraft blends it, over what the game has drawn and hidden behind what's nearer in the tagged depth. Lit by
// Minecraft's light curve, l / (4 - 3l) of the brighter of its block and sky light (l from 0 to 1), or unlit from
// entityTuning.y's block light, as a triangle glows in the G-buffer; times exposure, as predlss's blocks. Under 0.1 of
// alpha is left out, as Minecraft's particle shader leaves it.
float4 TranslucentPS(EntityOut i) : SV_Target0
{
    const float4 texel = EntityTexel(i.uv);
    const float  alpha = texel.a * i.colour.a;
    if (alpha < 0.1)
    {
        discard;
    }
    const uint  block = i.light & 0xFF;
    const float level = (float)min(max(block, (i.light >> 8) & 0xFF), 15u) / 15.0;
    const float light = block >= (uint)entityTuning.y ? 1.0 : level / (4.0 - 3.0 * level);
    return float4(Linear(texel.rgb * i.colour.rgb) * light * tuning.z, alpha);
}

// Step B: DLSS's bias hint under the translucent particles (set dlsshint 1), 1 where TranslucentPS draws, so DLSS takes
// their colour from this frame rather than from its history: the motion vectors there are the surface's behind them.
// Part 5's blended and additive layers too.
float HintPS(EntityOut i) : SV_Target0
{
    if (EntityTexel(i.uv).a * i.colour.a < 0.1)
    {
        discard;
    }
    return 1.0;
}

// Minecraft's two lights for entities in the overworld (Lighting.updateLevel's DIFFUSE_LIGHT_0 and _1), normalize(0.2,
// 1, -0.7) and normalize(-0.2, 1, 0.7) in its axes, here in the world's: Minecraft (x, y, z) is world (x, -z, y).
static const float3 kLight0 = float3(0.161690, 0.565916, 0.808452);
static const float3 kLight1 = float3(-0.161690, -0.565916, 0.808452);

// Part 5 (the spec's "Step B"): an entity layer Minecraft blends, blended into the scene's colour as a translucent
// particle is, but cut out where the texel's own alpha is under 0.1, as Minecraft's entity shader cuts (ALPHA_CUTOUT,
// before the vertex colour). Lit as a particle, unlit from entityTuning.y's block light (the emissive layers), and shaded
// per face as Minecraft's PER_FACE_LIGHTING shades it: min(1, (L0.n + L1.n) * 0.6 + 0.4) of its sRGB colour, each dot
// at least 0, with n the triangle's own normal turned toward the camera, which is Minecraft's back-face rule.
float4 BlendedPS(EntityOut i) : SV_Target0
{
    const float3 own = OwnNormal(i.world);  // before the discard, which ends the pixel's part in the derivatives
    const float4 texel = EntityTexel(i.uv);
    if (texel.a < 0.1)
    {
        discard;
    }
    const float3 normal = EntityNormal(i.flags, own, i.world);
    const float  face = min(1.0, (max(0.0, dot(kLight0, normal)) + max(0.0, dot(kLight1, normal))) * 0.6 + 0.4);
    const uint   block = i.light & 0xFF;
    const float  level = (float)min(max(block, (i.light >> 8) & 0xFF), 15u) / 15.0;
    const float  light = block >= (uint)entityTuning.y ? 1.0 : level / (4.0 - 3.0 * level);
    return float4(Linear(texel.rgb * i.colour.rgb * face) * light * tuning.z, texel.a * i.colour.a);
}

// Part 5: an entity layer Minecraft adds rather than blends (energy_swirl, a charged creeper's armour): its colour added
// to the scene's, whose pipeline adds ONE to ONE as Minecraft's ADDITIVE does, unlit and unshaded (EMISSIVE,
// NO_CARDINAL_LIGHTING), cut out where the texel's own alpha is under 0.1.
float4 AdditivePS(EntityOut i) : SV_Target0
{
    const float4 texel = EntityTexel(i.uv);
    if (texel.a < 0.1)
    {
        discard;
    }
    return float4(Linear(texel.rgb * i.colour.rgb) * tuning.z, 0.0);
}

struct OutlineOut
{
    float4 position : SV_Position;
    float4 colour   : COLOR0;
};

// A snapshot vertex's clip position, with the frame's jitter, as EntityVS places it.
float4 JitteredClip(float3 aPosition)
{
    // Minecraft (x, y, z) is world (x, -z, y).
    const float4 clip = mul(float4(entityOffset + float3(aPosition.x, -aPosition.z, aPosition.y), 1.0), cameraToClip);
    return float4(clip.xy + jitter.xy * clip.w, clip.zw);
}

// The strip's six corners, two triangles: which end of the edge each is at, and which side of it.
static const uint  kStripEnd[6] = { 0, 0, 1, 1, 0, 1 };
static const float kStripSide[6] = { 1.0, -1.0, 1.0, 1.0, -1.0, -1.0 };

// Step B: the targeted block's outline (set outline 1, in build mode), as Minecraft's lines shader draws its own: each
// edge, a pair of the snapshot's vertices, widened into a strip facing the camera, entityTuning.z render pixels across,
// six vertices an edge. An end behind the camera is first brought along the edge to just in front of it, so the strip's
// direction stays right; an edge wholly behind it draws nothing.
OutlineOut OutlineVS(uint id : SV_VertexID)
{
    const uint   first = entityFirst + id / 6 * 2;
    const Vertex a = vertices[first];
    float4       from = JitteredClip(a.position);
    float4       to = JitteredClip(vertices[first + 1].position);
    OutlineOut   o;
    o.colour = float4(a.colour & 0xFF, (a.colour >> 8) & 0xFF, (a.colour >> 16) & 0xFF, a.colour >> 24) / 255.0;
    const float ahead = 1e-3;  // clip w: metres in front of the camera
    if (from.w < ahead && to.w < ahead)
    {
        o.position = float4(2.0, 2.0, 2.0, 1.0);  // outside the view, so clipped
        return o;
    }
    if (from.w < ahead)
    {
        from = lerp(from, to, (ahead - from.w) / (to.w - from.w));
    }
    else if (to.w < ahead)
    {
        to = lerp(to, from, (ahead - to.w) / (from.w - to.w));
    }
    const float2 half = target.xy * 0.5;  // from clip units to pixels
    const float2 along = (to.xy / to.w - from.xy / from.w) * half;
    const float2 direction = dot(along, along) > 1e-8 ? normalize(along) : float2(1.0, 0.0);
    const float2 side = float2(-direction.y, direction.x) * entityTuning.z * 0.5 / half;
    const uint   corner = id % 6;
    const float4 at = kStripEnd[corner] == 0 ? from : to;
    o.position = float4(at.xy + side * kStripSide[corner] * at.w, at.zw);
    return o;
}

// Step B's outline, blended: black darkens the scene's linear colour to Linear(0.6), about a third, as Minecraft's 40%
// darkens the sRGB colours of its framebuffer to 0.6 of what they were.
float4 OutlinePS(OutlineOut i) : SV_Target0
{
    return float4(Linear(i.colour.rgb), 1.0 - Linear((1.0 - i.colour.a).xxx).x);
}

// Spike 4: Minecraft's HUD image (its hotbar, crosshair, chat, hand and held item), drawn over the finished frame at
// Present with premultiplied-alpha blending. The image is RGBA8 with premultiplied alpha, one texel per uint (red in the
// low byte), its rows stored as Minecraft sent them. Each texel covers hudInfo.z by hudInfo.z back-buffer pixels from
// the top left, and nothing shows outside the image. src/render/Hud.cpp's HudTexel does the same arithmetic.
cbuffer Hud : register(b1)
{
    uint4 hudInfo;    // the image's width and height; back-buffer pixels per texel; 1 if its rows are stored bottom-up
    uint4 hudCursor;  // spike 8: the cursor's tip in back-buffer pixels; its arrow's height, 0 for none; unused
};

StructuredBuffer<uint> hudImage : register(t1);

// One triangle covering the screen, from the vertex index alone.
float4 HudVS(uint id : SV_VertexID) : SV_Position
{
    const float2 corner = float2((id << 1) & 2, id & 2);  // (0, 0), (2, 0), (0, 2)
    return float4(corner * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
}

// Spike 8: the part of the cursor's arrow at a pixel's centre, SkyCraft's arrow: 0 none, 1 its black edge, 2 its white
// fill. src/render/Hud.cpp's CursorAt does the same arithmetic.
uint CursorPart(float2 position)
{
    if (hudCursor.z == 0)
    {
        return 0;
    }
    const float  height = (float)hudCursor.z;
    const float  edge = 1.5 * height / 18.0;
    const float2 p = position - (float2)hudCursor.xy;
    if (p.x < 0.0 || p.y < 0.0 || p.y >= height || p.x > p.y * 0.6)
    {
        return 0;
    }
    return p.x < edge || p.x > p.y * 0.6 - edge || p.y > height - edge ? 1 : 2;
}

float4 HudPS(float4 position : SV_Position) : SV_Target0
{
    const uint part = CursorPart(position.xy);
    if (part != 0)
    {
        return part == 1 ? float4(0.0, 0.0, 0.0, 1.0) : float4(1.0, 1.0, 1.0, 1.0);  // opaque, over the image
    }
    const uint2 texel = (uint2)position.xy / max(hudInfo.z, 1u);
    if (texel.x >= hudInfo.x || texel.y >= hudInfo.y)
    {
        return float4(0.0, 0.0, 0.0, 0.0);
    }
    const uint row = hudInfo.w != 0 ? hudInfo.y - 1 - texel.y : texel.y;
    const uint packed = hudImage[row * hudInfo.x + texel.x];
    return float4(packed & 0xFF, (packed >> 8) & 0xFF, (packed >> 16) & 0xFF, packed >> 24) / 255.0;
}

// Spike 7's Part 5: the atlas's animated sprites (the spec's "Animated textures on the GPU"). The upload list writes the
// areas its patches changed, at every level, into the atlas buffer, with its own root signature: b0 the pass's
// constants, t0 the areas, t1 their texels packed one area after another, u0 the atlas. src/render/FrameConstants.h
// mirrors PatchArea and Patch.
struct PatchArea
{
    uint  start;   // its first texel in the atlas buffer: the level's start, plus y times the level's width, plus x
    uint  stride;  // the level's width
    uint  width;
    uint  height;
    uint  source;  // its first texel in patchTexels
    uint3 unused;
};

cbuffer Patch : register(b0)
{
    uint4 patchInfo;  // the areas; the texels packed; the atlas buffer's length in texels; 0
};

StructuredBuffer<PatchArea> patchAreas  : register(t0);
StructuredBuffer<uint>      patchTexels : register(t1);
RWStructuredBuffer<uint>    patchAtlas  : register(u0);

// A group an area, its 64 threads taking its texels in turn. Root descriptors carry no bounds, so every read and write
// is checked against the counts the CPU gave as well.
[numthreads(64, 1, 1)]
void PatchCS(uint3 group : SV_GroupID, uint thread : SV_GroupIndex)
{
    if (group.x >= patchInfo.x)
    {
        return;
    }
    const PatchArea area = patchAreas[group.x];
    for (uint t = thread; t < area.width * area.height; t += 64)
    {
        const uint row = t / area.width;
        const uint to = area.start + row * area.stride + (t - row * area.width);
        const uint from = area.source + t;
        if (to < patchInfo.z && from < patchInfo.y)
        {
            patchAtlas[to] = patchTexels[from];
        }
    }
}
