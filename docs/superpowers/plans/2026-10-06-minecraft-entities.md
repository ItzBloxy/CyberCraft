# Minecraft's Entities Implementation Plan, Part 1 (step 0)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
>
> In this project, plans run inline with superpowers:executing-plans, never through sub-agents.

**Goal:** Part 1 of spike 7, step 0 of the spec: read what SkyCraft really sends of Minecraft's entities before anything
is drawn. BlockRender's link takes Minecraft's entity textures (`kRenTexture`), its scenes of entities, block entities
and particles (`kRenScene`) and its world-entity table, checks them against their layouts, and hands them on; `mclink`
reports them against the user's Minecraft, without the game, with a script that makes each test thing the spec names.

**Architecture:** all of it is plain C++ in `render_core`, tested without the game or Minecraft.
- **`link/Messages`:** `ParseTexture` and `ParseScene`, beside `ParseSection` and `ParseAtlas`, each checking its
  payload against its layout, as the render ring's other messages are checked.
- **`link/Rings` and `link/WorldTable`:** `ReadWorldEntities` copies the world-entity table under its sequence lock, as
  `ReadMcState` copies Minecraft's state; `CheckWorldTable` keeps its usable records and its outline.
- **`link/Link`:** the sink's three new calls, `Texture`, `Scene` and `WorldEntities`; textures and scenes once the
  atlas has been seen, as sections are; the table each update while Minecraft is in its world.
- **`mclink`:** reports each texture, sums up the scenes every five seconds, the largest among them included, and gains
  `scene`, which prints the newest scene batch by batch with how many of each batch's vertices glow, and the table;
  and `save-textures`, which saves every texture as DDS. Step 0's script is `tools/mclink/entities.txt`.

Nothing changes in the game: BlockRender's own sink takes none of the new calls yet, and nothing is deployed.

**Tech stack:**
- C++20, MSVC 14.44 (Visual Studio 2022), CMake 3.31 (bundled with Visual Studio);
- SkyCraft's protocol (version 11) and its Fabric mod 0.1.2, unchanged, in the user's `CyberCraft` instance (Minecraft
  26.3, Fabric Loader 0.19.5, CyberCraft Tweaks 1.3.0).

**Spec:** [docs/superpowers/specs/2026-10-06-minecraft-entities-design.md](../specs/2026-10-06-minecraft-entities-design.md)

**Parts 2 and 3** (steps A and B, and step C if run A needs it) are planned after step 0, from its results: step 0
settles the sizes, flags and textures they build on.

---

## Before you start

- **The branch:** work on `spike/minecraft-entities`, which holds the spec and this plan.
- **The shell:** commands are for Git Bash.
  - CMake isn't on PATH, and shell variables don't persist between tool calls, so every command block sets it:
    ```bash
    CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
    ```
  - Pass MSBuild switches as `-m`, never `/m`: Git Bash rewrites `/m` into a file path.
- **Edit steps:** an Edit step's code blocks come in pairs, applied in order: the text to find, which occurs exactly
  once in the file as the pairs before it left it, then the text that replaces it. A Write step's block is the whole
  file.
- **Already verified:** all the code in this plan has been compiled and tested once already, in a scratch clone.
  - Tasks 1 to 4 were then replayed in a fresh clone by a script that applied every file step exactly as written, and
    checked every build, test and `mclink` step's expected result. The results:
    - zero compiler warnings at `/W4`;
    - 465 C++ tests passing;
    - the replayed files identical to the scratch clone's.
  - Each new test was broken once, by a mutation of the code it covers, and failed as it should: 40 mutations.
  - Step 0's commands were checked against Minecraft 26.3's own classes: the entity ids, and the data each `/summon`
    and `/setblock` gives (`VillagerData`, `patterns`, `PickupDelay`, `FireworksItem`, `potion_contents`, `DropItem`,
    `DeathLootTable` and the rest).
  - Before Part 1 ran, step 0's script was also played through against SkyCraft's source and spike 1's dump, for
    where each thing lands and what it leaves behind ("Found before executing Part 1").

  So a failure while following the plan most likely means a transcription slip: compare against the plan's code first.
- **Test counts:** 450 at the start, then 455, 461, 464 and 465 after Tasks 1 to 4.
- **What only Minecraft can test:** what SkyCraft really sends, in Task 5, with the user.
- **The user:**
  - **does everything in Minecraft and the Modrinth App:** starting and stopping it, and anything on its screen. Never
    drive their mouse or keyboard: ask, then wait for them to say they're done;
  - **approves downloads first.** Nothing in Part 1 downloads anything;
  - **approves changes to their Minecraft world:** Task 5's script summons and places test things at x 880 in the
    `SkyCraft` world, SkyCraft's own in the `CyberCraft` instance, and clears them at its end, leaving no drops.
- **The game:** Part 1 deploys nothing. `mclink` needs the game closed, since the game would hold the mapping.
- **No faults:** shared memory is written by another process: every count, size and offset from it is checked before
  use, as the render ring's other messages are.
- **Commits** go on `spike/minecraft-entities`, and end with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## File map (Part 1)

All paths under `src/`, `tests/` and `tools/` are relative to `spikes/block-render/`.

| File | Responsibility |
|---|---|
| `CMakeLists.txt` | The new source and test |
| `src/link/Messages.*` | `ParseTexture`, `ParseScene` and `SceneVertices`, beside the section and atlas parsers |
| `src/link/Rings.*` | `ReadWorldEntities`: the world-entity table, copied under its sequence lock |
| `src/link/WorldTable.*` | `CheckWorldTable`: the table's usable records and its outline; `WorldKindName` |
| `src/link/Link.*` | The sink's `Texture`, `Scene` and `WorldEntities`; the status's counts |
| `tools/mclink/*` | Textures, scenes and the table reported; `scene` and `save-textures`; step 0's `entities.txt` |
| `tests/TestMessages.cpp`, `tests/TestRings.cpp`, `tests/TestLink.cpp`, `tests/TestMcLinkScript.cpp` | Their units' new tests |
| `tests/TestWorldTable.cpp` | `WorldTable`'s tests |

---

### Task 1: The render ring's textures and scenes, checked against their layouts

SkyCraft's `AvatarExporter` sends two messages BlockRender steps over (the spec's "The link: what comes in"):
- **`kRenTexture`:** `{u32 id, width, height, pad}`, then the RGBA8 pixels, top row first: an entity's skin, or a whole
  atlas of Minecraft's other than the blocks' and items'. `ParseTexture` takes it if the id is at least 1, each side is
  1 to 4,096 (SkyCraft's own game side's limit), and the payload holds the pixels.
- **`kRenScene`:** the origin as three doubles, the batch and vertex counts, the batches `{u32 texture, first, count,
  flags}`, then the vertices in SkyCraft's format, which is `Vertex`. `ParseScene` takes it if the origin is finite,
  there are at most 65,536 batches and 1,048,576 vertices (32 MB: SkyCraft's ring takes no message over half its 64
  MB), the payload holds them, and every batch is whole triangles within the vertices, its end worked out in 64 bits so
  a huge `first` can't wrap. A scene of no batches says nothing is there.

The vertices stay in the payload, which is valid only during the sink's call: `SceneVertices` copies them out.

**Files:**
- Modify: `src/link/Messages.h`, `src/link/Messages.cpp`, `tests/TestMessages.cpp`

- [x] **Step 1: Edit `tests/TestMessages.cpp` to add the failing tests**

```cpp
#include <cstring>
```
```cpp
#include <array>
#include <cmath>
#include <cstring>
```
```cpp
	CHECK(error.find("needs 16 bytes") != std::string::npos);
}
```
```cpp
	CHECK(error.find("needs 16 bytes") != std::string::npos);
}

namespace
{
// AvatarExporter.textureId's header, {id, width, height, 0}, then aPixels bytes.
std::vector<uint8_t> Texture(uint32_t aId, uint32_t aWidth, uint32_t aHeight, size_t aPixels)
{
	Bytes bytes;
	bytes.Put(aId).Put(aWidth).Put(aHeight).Put(uint32_t(0));
	for (size_t i = 0; i < aPixels; ++i)
	{
		bytes.Put(uint8_t(i + 1));
	}
	return bytes.data;
}

// AvatarExporter.send's scene: the origin as three doubles, the counts, the batches {texture, first, count, flags},
// then aVertices vertices, the i-th at x = i with block light i.
std::vector<uint8_t> Scene(double aX, uint32_t aVertices, const std::vector<std::array<uint32_t, 4>>& aBatches, uint32_t aClaimed)
{
	Bytes bytes;
	bytes.Put(aX).Put(6.0).Put(2007.0).Put(uint32_t(aBatches.size())).Put(aClaimed);
	for (const auto& batch : aBatches)
	{
		bytes.Put(batch[0]).Put(batch[1]).Put(batch[2]).Put(batch[3]);
	}
	for (uint32_t i = 0; i < aVertices; ++i)
	{
		bytes.Put(float(i)).Put(2.0f).Put(3.0f).Put(0.25f).Put(0.5f);
		bytes.Put(uint8_t(255)).Put(uint8_t(255)).Put(uint8_t(255)).Put(uint8_t(255));
		bytes.Put(uint32_t(i | (15 << 8))).Put(uint32_t(1 | 8 | (7 << 4)));
	}
	return bytes.data;
}
}

TEST(MessagesTextureAsAvatarExporterWritesIt)
{
	const auto  payload = Texture(3, 2, 1, 8);
	std::string error;
	const auto  texture = ParseTexture(payload, error);
	CHECK(texture.has_value());
	if (!texture)
	{
		return;
	}
	CHECK_EQ(texture->id, 3u);
	CHECK_EQ(texture->width, 2u);
	CHECK_EQ(texture->height, 1u);
	CHECK_EQ(texture->rgba.size(), size_t(8));
	CHECK_EQ(int(texture->rgba[0]), 1);
	CHECK_EQ(int(texture->rgba[7]), 8);
}

TEST(MessagesTextureRejections)
{
	std::string error;
	CHECK(!ParseTexture(std::vector<uint8_t>(15), error).has_value());
	CHECK(error.find("no room for its header") != std::string::npos);
	CHECK(!ParseTexture(Texture(0, 2, 1, 8), error).has_value());
	CHECK(error.find("id 0") != std::string::npos);
	CHECK(!ParseTexture(Texture(3, 0, 1, 0), error).has_value());
	CHECK(error.find("out of range") != std::string::npos);
	CHECK(!ParseTexture(Texture(3, 4097, 1, 4097 * 4), error).has_value());
	CHECK(error.find("out of range") != std::string::npos);
	CHECK(!ParseTexture(Texture(3, 2, 2, 12), error).has_value());
	CHECK(error.find("needs 16 bytes, and has 12") != std::string::npos);
}

TEST(MessagesSceneAsAvatarExporterWritesIt)
{
	const auto  payload = Scene(-120.0, 9, { { 0, 0, 3, 0 }, { 5, 3, 6, 1 } }, 9);
	std::string error;
	const auto  scene = ParseScene(payload, error);
	CHECK(scene.has_value());
	if (!scene)
	{
		return;
	}
	CHECK_EQ(scene->origin.x, -120.0);
	CHECK_EQ(scene->origin.y, 6.0);
	CHECK_EQ(scene->origin.z, 2007.0);
	CHECK_EQ(scene->batches.size(), size_t(2));
	if (scene->batches.size() != 2)
	{
		return;
	}
	CHECK_EQ(scene->batches[1].texture, 5u);
	CHECK_EQ(scene->batches[1].first, 3u);
	CHECK_EQ(scene->batches[1].count, 6u);
	CHECK_EQ(scene->batches[1].flags, 1u);
	CHECK_EQ(scene->vertexCount, 9u);
	const std::vector<Vertex> vertices = SceneVertices(*scene);
	CHECK_EQ(vertices.size(), size_t(9));
	if (vertices.size() != 9)
	{
		return;
	}
	CHECK_EQ(vertices[4].x, 4.0f);
	CHECK_EQ(vertices[4].light, uint32_t(4 | (15 << 8)));
	CHECK_EQ(vertices[4].flags, uint32_t(1 | 8 | (7 << 4)));
}

TEST(MessagesEmptySceneSaysNothingIsThere)
{
	std::string error;
	const auto  scene = ParseScene(Scene(0.0, 0, {}, 0), error);
	CHECK(scene.has_value());
	if (!scene)
	{
		return;
	}
	CHECK(scene->batches.empty());
	CHECK_EQ(scene->vertexCount, 0u);
	CHECK(SceneVertices(*scene).empty());
}

TEST(MessagesSceneRejections)
{
	std::string error;
	CHECK(!ParseScene(std::vector<uint8_t>(31), error).has_value());
	CHECK(error.find("no room for its header") != std::string::npos);
	CHECK(!ParseScene(Scene(std::nan(""), 3, { { 0, 0, 3, 0 } }, 3), error).has_value());
	CHECK(error.find("isn't finite") != std::string::npos);
	CHECK(!ParseScene(Scene(0.0, 0, std::vector<std::array<uint32_t, 4>>(65537, { 0, 0, 0, 0 }), 0), error).has_value());
	CHECK(error.find("65537 batches and 0 vertices is too big") != std::string::npos);
	CHECK(!ParseScene(Scene(0.0, 0, {}, 1048577), error).has_value());
	CHECK(error.find("0 batches and 1048577 vertices is too big") != std::string::npos);
	CHECK(!ParseScene(Scene(0.0, 3, { { 0, 0, 3, 0 } }, 4), error).has_value());
	CHECK(error.find("needs") != std::string::npos);
	CHECK(!ParseScene(Scene(0.0, 6, { { 0, 0, 4, 0 } }, 6), error).has_value());
	CHECK(error.find("batch 0") != std::string::npos);
	CHECK(!ParseScene(Scene(0.0, 6, { { 0, 0, 3, 0 }, { 0, 3, 6, 0 } }, 6), error).has_value());
	CHECK(error.find("batch 1") != std::string::npos);
	CHECK(!ParseScene(Scene(0.0, 6, { { 0, 0xFFFFFFFF, 3, 0 } }, 6), error).has_value());
	CHECK(error.find("batch 0") != std::string::npos);
}
```

- [x] **Step 2: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C3861: 'ParseTexture': identifier not found`.

- [x] **Step 3: Edit `src/link/Messages.h` to declare the parsers**

```cpp
#include "world/Vertex.h"
```
```cpp
#include "Coords.h"
#include "world/Vertex.h"
```
```cpp
constexpr uint32_t kMaxAtlasSide = 16384;
```
```cpp
constexpr uint32_t kMaxAtlasSide = 16384;
constexpr uint32_t kMaxTextureSide = 4096;       // SkyCraft's own game side refuses a bigger texture
constexpr uint32_t kMaxSceneBatches = 65536;
constexpr uint32_t kMaxSceneVertices = 1u << 20;  // 32 MB: SkyCraft's ring takes no message over half its 64 MB
```
```cpp
std::optional<AtlasImage> ParseAtlas(std::span<const uint8_t> aPayload, std::string& aError);
```
```cpp
std::optional<AtlasImage> ParseAtlas(std::span<const uint8_t> aPayload, std::string& aError);

// Spike 7: a texture besides the atlas (kRenTexture), for the scene's batches that name its id: an entity's skin, or a
// whole atlas of Minecraft's other than the blocks' and items' (chests', banners', signs', particles'). RGBA8, top row
// first. SkyCraft sends each once, and all of them again only after its clear.
struct TextureImage
{
	uint32_t                 id = 0;
	uint32_t                 width = 0;
	uint32_t                 height = 0;
	std::span<const uint8_t> rgba;  // width * height * 4 bytes, inside the payload
};

// A kRenTexture payload: {u32 id, width, height, pad}, then the pixels. Nothing, with aError saying why, if the id is
// 0, a side is 0 or over 4,096, or the payload is too short.
std::optional<TextureImage> ParseTexture(std::span<const uint8_t> aPayload, std::string& aError);

// One of a scene's batches: a run of its vertices drawn with one texture.
struct SceneBatch
{
	uint32_t texture = 0;  // 0: Minecraft's combined atlas; else a texture's id
	uint32_t first = 0;    // its first vertex
	uint32_t count = 0;    // its vertices, whole triangles
	uint32_t flags = 0;    // bit 0: translucent, drawn blended
};

// Spike 7: everything Minecraft draws this frame besides its blocks and its player (kRenScene): entities, block
// entities and particles, posed, their positions in Minecraft axes from the origin. Each Minecraft frame sends one.
struct SceneMessage
{
	Vec3d                    origin;  // a whole block near Minecraft's camera, Minecraft axes
	std::vector<SceneBatch>  batches;
	uint32_t                 vertexCount = 0;
	std::span<const uint8_t> vertices;  // vertexCount vertices in SkyCraft's format, which is Vertex, inside the payload
};

// A kRenScene payload: {f64 origin x, y, z; u32 batches, vertices}, the batches {u32 texture, first, count, flags},
// then the vertices. Nothing, with aError saying why, if the origin isn't finite, there are over 65,536 batches or
// 1,048,576 vertices, the payload is too short for them, or a batch isn't whole triangles within the vertices. A scene
// of no batches says nothing is there.
std::optional<SceneMessage> ParseScene(std::span<const uint8_t> aPayload, std::string& aError);

// The scene's vertices, copied out of the payload.
std::vector<Vertex> SceneVertices(const SceneMessage& aScene);
```

- [x] **Step 4: Edit `src/link/Messages.cpp` to add them**

```cpp
#include <cstdlib>
```
```cpp
#include <cmath>
#include <cstdlib>
```
```cpp
static_assert(sizeof(proto::RenSection) == 16 && sizeof(proto::RenAtlas) == 8);
```
```cpp
static_assert(sizeof(proto::RenSection) == 16 && sizeof(proto::RenAtlas) == 8);
static_assert(sizeof(proto::RenTexture) == 16 && sizeof(proto::RenScene) == 32 && sizeof(proto::RenBatch) == 16);
```
```cpp
	return AtlasImage{ header.width, header.height, aPayload.subspan(sizeof header, size_t(bytes)) };
}
}
```
```cpp
	return AtlasImage{ header.width, header.height, aPayload.subspan(sizeof header, size_t(bytes)) };
}

std::optional<TextureImage> ParseTexture(std::span<const uint8_t> aPayload, std::string& aError)
{
	proto::RenTexture header;
	if (aPayload.size() < sizeof header)
	{
		aError = std::format("a texture message of {} bytes has no room for its header", aPayload.size());
		return std::nullopt;
	}
	std::memcpy(&header, aPayload.data(), sizeof header);
	if (header.id == 0)
	{
		aError = std::format("a texture of {} x {} has id 0", header.width, header.height);
		return std::nullopt;
	}
	if (header.width == 0 || header.height == 0 || header.width > kMaxTextureSide || header.height > kMaxTextureSide)
	{
		aError = std::format("texture {} of {} x {} is out of range", header.id, header.width, header.height);
		return std::nullopt;
	}
	const uint64_t bytes = uint64_t(header.width) * header.height * 4;
	if (aPayload.size() - sizeof header < bytes)
	{
		aError = std::format("texture {} of {} x {} needs {} bytes, and has {}", header.id, header.width, header.height, bytes,
		                     aPayload.size() - sizeof header);
		return std::nullopt;
	}
	return TextureImage{ header.id, header.width, header.height, aPayload.subspan(sizeof header, size_t(bytes)) };
}

std::optional<SceneMessage> ParseScene(std::span<const uint8_t> aPayload, std::string& aError)
{
	proto::RenScene header;
	if (aPayload.size() < sizeof header)
	{
		aError = std::format("a scene message of {} bytes has no room for its header", aPayload.size());
		return std::nullopt;
	}
	std::memcpy(&header, aPayload.data(), sizeof header);
	if (!std::isfinite(header.originX) || !std::isfinite(header.originY) || !std::isfinite(header.originZ))
	{
		aError = "a scene's origin isn't finite";
		return std::nullopt;
	}
	if (header.batchCount > kMaxSceneBatches || header.vertexCount > kMaxSceneVertices)
	{
		aError = std::format("a scene of {} batches and {} vertices is too big", header.batchCount, header.vertexCount);
		return std::nullopt;
	}
	const uint64_t batchBytes = uint64_t(header.batchCount) * sizeof(proto::RenBatch);
	const uint64_t vertexBytes = uint64_t(header.vertexCount) * sizeof(Vertex);
	if (aPayload.size() - sizeof header < batchBytes + vertexBytes)
	{
		aError = std::format("a scene of {} batches and {} vertices needs {} bytes, and has {}", header.batchCount, header.vertexCount,
		                     batchBytes + vertexBytes, aPayload.size() - sizeof header);
		return std::nullopt;
	}
	SceneMessage scene;
	scene.origin = { header.originX, header.originY, header.originZ };
	scene.vertexCount = header.vertexCount;
	scene.batches.reserve(header.batchCount);
	for (uint32_t b = 0; b < header.batchCount; ++b)
	{
		proto::RenBatch batch;
		std::memcpy(&batch, aPayload.data() + sizeof header + size_t(b) * sizeof batch, sizeof batch);
		if (batch.count % 3 != 0 || uint64_t(batch.first) + batch.count > header.vertexCount)
		{
			aError = std::format("a scene's batch {} has {} vertices from {}, of {}", b, batch.count, batch.first, header.vertexCount);
			return std::nullopt;
		}
		scene.batches.push_back({ batch.texture, batch.first, batch.count, batch.flags });
	}
	scene.vertices = aPayload.subspan(sizeof header + size_t(batchBytes), size_t(vertexBytes));
	return scene;
}

std::vector<Vertex> SceneVertices(const SceneMessage& aScene)
{
	std::vector<Vertex> vertices(aScene.vertexCount);
	if (!vertices.empty())
	{
		std::memcpy(vertices.data(), aScene.vertices.data(), vertices.size() * sizeof(Vertex));
	}
	return vertices;
}
}
```

- [x] **Step 5: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `455 test(s), 0 failure(s)`.

- [x] **Step 6: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "minecraft-entities: the render ring's textures and scenes, checked against their layouts" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 2: The world-entity table, read under its lock and checked

SkyCraft's `WorldExporter` writes the world-entity table at `0x1C000` each Minecraft frame, its sequence odd while it
writes and even after (the spec's "The world-entity table"):
- **`ReadWorldEntities`** copies it as `ReadMcState` copies Minecraft's state: while the sequence is even and the same
  after the copy, 64 tries at most.
- **`CheckWorldTable`** reads at most 160 records whatever the count says, keeps those whose kind is 1 to 6 and whose
  numbers (position, yaw, pitch, scale, the box's extent and the 12 atlas coordinates) are finite, in the table's
  order, counts the rest as left out, and takes the outline only when `hasSelection` is set and its six numbers are
  finite. `WorldKindName` names a kind for the reports.

**Files:**
- Create: `src/link/WorldTable.h`, `src/link/WorldTable.cpp`, `tests/TestWorldTable.cpp`
- Modify: `src/link/Rings.h`, `src/link/Rings.cpp`, `tests/TestRings.cpp`, `CMakeLists.txt`

- [x] **Step 1: Write the failing test, `tests/TestWorldTable.cpp`**

```cpp
#include "Harness.h"

#include "link/WorldTable.h"

#include <algorithm>
#include <limits>

using namespace cpmc::link;

namespace
{
// A record as WorldExporter.exportEntities makes one: a kind, an id, a position near spike 5's spot B.
proto::WorldEntity Record(uint32_t aKind, uint32_t aId)
{
	proto::WorldEntity record{};
	record.kind = aKind;
	record.id = aId;
	record.x = 480.5f;
	record.y = 6.14f;
	record.z = 2007.5f;
	record.scale = 0.5f;
	record.uv[0][2] = 0.25f;
	return record;
}

proto::WorldEntities Table(std::initializer_list<proto::WorldEntity> aRecords)
{
	proto::WorldEntities table{};
	for (const proto::WorldEntity& record : aRecords)
	{
		table.entities[table.count++] = record;
	}
	return table;
}
}

TEST(WorldTableKeepsGoodRecordsInOrder)
{
	proto::WorldEntities table = Table({ Record(proto::kWeItem, 7), Record(proto::kWeArrow, 9), Record(proto::kWeShadow, 3) });
	table.hasSelection = 1;
	const float box[6] = { 480.0f, 6.0f, 2007.0f, 481.0f, 7.0f, 2008.0f };
	std::copy(box, box + 3, table.selMin);
	std::copy(box + 3, box + 6, table.selMax);
	const WorldTable checked = CheckWorldTable(table);
	CHECK_EQ(checked.entities.size(), size_t(3));
	CHECK_EQ(checked.leftOut, 0u);
	CHECK(checked.outline.has_value());
	if (checked.entities.size() != 3 || !checked.outline)
	{
		return;
	}
	CHECK_EQ(checked.entities[0].id, 7u);
	CHECK_EQ(checked.entities[1].kind, uint32_t(proto::kWeArrow));
	CHECK_EQ(checked.entities[2].id, 3u);
	CHECK_EQ((*checked.outline)[0], 480.0f);
	CHECK_EQ((*checked.outline)[5], 2008.0f);
}

TEST(WorldTableLeavesOutBadRecords)
{
	proto::WorldEntity unknown = Record(0, 1);
	proto::WorldEntity tooHigh = Record(7, 2);
	proto::WorldEntity lost = Record(proto::kWeItem, 3);
	lost.x = std::numeric_limits<float>::quiet_NaN();
	proto::WorldEntity badUv = Record(proto::kWeBlock, 4);
	badUv.uv[2][3] = std::numeric_limits<float>::infinity();
	proto::WorldEntity badExtent = Record(proto::kWeCrack, 5);
	badExtent.ext[1] = std::numeric_limits<float>::quiet_NaN();
	const WorldTable checked = CheckWorldTable(Table({ unknown, Record(proto::kWeTrident, 6), tooHigh, lost, badUv, badExtent }));
	CHECK_EQ(checked.entities.size(), size_t(1));
	CHECK_EQ(checked.leftOut, 5u);
	if (checked.entities.size() == 1)
	{
		CHECK_EQ(checked.entities[0].id, 6u);
	}
}

TEST(WorldTableHoldsTheCountToItsRecords)
{
	proto::WorldEntities table{};
	for (uint32_t i = 0; i < proto::kMaxWorldEntities; ++i)
	{
		table.entities[i] = Record(proto::kWeShadow, i + 1);
	}
	table.count = 1000;  // more than the table holds: only its records are read
	const WorldTable checked = CheckWorldTable(table);
	CHECK_EQ(checked.entities.size(), size_t(proto::kMaxWorldEntities));
	CHECK_EQ(checked.leftOut, 0u);
}

TEST(WorldTableHasAnOutlineOnlyWhenOneIsWhole)
{
	proto::WorldEntities none = Table({});
	none.selMax[0] = 1.0f;  // a box, but hasSelection is 0
	CHECK(!CheckWorldTable(none).outline.has_value());
	proto::WorldEntities broken = Table({});
	broken.hasSelection = 1;
	broken.selMin[1] = std::numeric_limits<float>::quiet_NaN();
	CHECK(!CheckWorldTable(broken).outline.has_value());
}

TEST(WorldTableNamesTheKinds)
{
	CHECK_STR(WorldKindName(proto::kWeArrow), "arrow");
	CHECK_STR(WorldKindName(proto::kWeItem), "item");
	CHECK_STR(WorldKindName(proto::kWeTrident), "trident");
	CHECK_STR(WorldKindName(proto::kWeBlock), "block");
	CHECK_STR(WorldKindName(proto::kWeCrack), "crack");
	CHECK_STR(WorldKindName(proto::kWeShadow), "shadow");
	CHECK_STR(WorldKindName(0), "other");
	CHECK_STR(WorldKindName(7), "other");
}
```

- [x] **Step 2: Edit `tests/TestRings.cpp` to add the failing test of the table's read**

```cpp
	CHECK(!ReadMcState(memory.Bytes()).has_value());
```
```cpp
	CHECK(!ReadMcState(memory.Bytes()).has_value());
}

TEST(RingsWorldEntitiesReadWholeTables)
{
	Memory               memory(proto::kOffWorldEntities + sizeof(proto::WorldEntities));
	proto::WorldEntities table{};
	table.seq = 4;  // even: as SkyLink.writeWorldEntities leaves it
	table.count = 2;
	table.entities[1].kind = proto::kWeArrow;
	table.entities[1].x = 480.5f;
	std::memcpy(memory.Bytes() + proto::kOffWorldEntities, &table, sizeof table);
	const auto read = ReadWorldEntities(memory.Bytes());
	CHECK(read.has_value());
	if (!read)
	{
		return;
	}
	CHECK_EQ(read->count, 2u);
	CHECK_EQ(read->entities[1].kind, uint32_t(proto::kWeArrow));
	CHECK_EQ(read->entities[1].x, 480.5f);
	const uint32_t writing = 5;  // odd: Minecraft is mid-write
	std::memcpy(memory.Bytes() + proto::kOffWorldEntities, &writing, sizeof writing);
	CHECK(!ReadWorldEntities(memory.Bytes()).has_value());
```

- [x] **Step 3: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestCombatInbox.cpp
	tools/mclink/DumpObj.cpp
```
```cmake
	tests/TestCombatInbox.cpp
	tests/TestWorldTable.cpp
	tools/mclink/DumpObj.cpp
```

- [x] **Step 4: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'link/WorldTable.h'`.

- [x] **Step 5: Write `src/link/WorldTable.h`**

```cpp
#pragma once

#include <skycraft_protocol.h>

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

// Spike 7: Minecraft's world entities (SkyCraft's WorldExporter), the light path for small things SkyCraft's game side
// builds itself: arrows, dropped and thrown items, tridents, dropped blocks, block-breaking cracks and contact shadows,
// and the targeted block's outline. Checked before use: shared memory is written by another process.
namespace cpmc::link
{
namespace proto = skycraft::proto;

struct WorldTable
{
	std::vector<proto::WorldEntity>     entities;  // the records that passed the checks, in the table's order
	std::optional<std::array<float, 6>> outline;   // the targeted block's box, its low corner then its high, Minecraft axes
	uint32_t                            leftOut = 0;  // records with a kind it doesn't have, or a number that isn't finite
};

// The table's records, at most kMaxWorldEntities whatever its count says, and its outline when it has a whole one.
WorldTable CheckWorldTable(const proto::WorldEntities& aTable);

// A record's kind as a word: "arrow", "item", "trident", "block", "crack" or "shadow"; "other" for anything else.
const char* WorldKindName(uint32_t aKind);
}
```

- [x] **Step 6: Write `src/link/WorldTable.cpp`**

```cpp
#include "link/WorldTable.h"

#include <algorithm>
#include <cmath>

namespace cpmc::link
{
namespace
{
bool Finite(const float* aValues, size_t aCount)
{
	return std::all_of(aValues, aValues + aCount, [](float aValue) { return std::isfinite(aValue); });
}

bool Usable(const proto::WorldEntity& aRecord)
{
	const float numbers[] = { aRecord.x, aRecord.y, aRecord.z, aRecord.yaw, aRecord.pitch, aRecord.scale };
	return aRecord.kind >= proto::kWeArrow && aRecord.kind <= proto::kWeShadow && Finite(numbers, std::size(numbers)) &&
	       Finite(aRecord.ext, 3) && Finite(&aRecord.uv[0][0], 12);
}
}

WorldTable CheckWorldTable(const proto::WorldEntities& aTable)
{
	WorldTable     table;
	const uint32_t count = std::min(aTable.count, proto::kMaxWorldEntities);
	for (uint32_t i = 0; i < count; ++i)
	{
		if (Usable(aTable.entities[i]))
		{
			table.entities.push_back(aTable.entities[i]);
		}
		else
		{
			++table.leftOut;
		}
	}
	if (aTable.hasSelection && Finite(aTable.selMin, 3) && Finite(aTable.selMax, 3))
	{
		table.outline = std::array<float, 6>{ aTable.selMin[0], aTable.selMin[1], aTable.selMin[2],
			                                  aTable.selMax[0], aTable.selMax[1], aTable.selMax[2] };
	}
	return table;
}

const char* WorldKindName(uint32_t aKind)
{
	switch (aKind)
	{
	case proto::kWeArrow: return "arrow";
	case proto::kWeItem: return "item";
	case proto::kWeTrident: return "trident";
	case proto::kWeBlock: return "block";
	case proto::kWeCrack: return "crack";
	case proto::kWeShadow: return "shadow";
	default: return "other";
	}
}
}
```

- [x] **Step 7: Edit `src/link/Rings.h` to declare the table's read**

```cpp
std::optional<proto::McState> ReadMcState(const uint8_t* aBase);
```
```cpp
std::optional<proto::McState> ReadMcState(const uint8_t* aBase);

// Spike 7: Minecraft's world entities, at aBase + kOffWorldEntities, as SkyLink.writeWorldEntities leaves them: the
// sequence odd while it writes, even after. Nothing if it was mid-write all 64 times we looked.
std::optional<proto::WorldEntities> ReadWorldEntities(const uint8_t* aBase);
```

- [x] **Step 8: Edit `src/link/Rings.cpp` to add it**

```cpp
			return state;
```
```cpp
			return state;
		}
	}
	return std::nullopt;
}

std::optional<proto::WorldEntities> ReadWorldEntities(const uint8_t* aBase)
{
	uint8_t* const source = const_cast<uint8_t*>(aBase) + proto::kOffWorldEntities;  // only loaded from
	auto           seq = std::atomic_ref<uint32_t>(*reinterpret_cast<uint32_t*>(source));
	for (int attempt = 0; attempt < 64; ++attempt)
	{
		const uint32_t before = seq.load(std::memory_order_acquire);
		if (before & 1)
		{
			_mm_pause();
			continue;
		}
		proto::WorldEntities table;
		std::memcpy(&table, source, sizeof table);
		std::atomic_thread_fence(std::memory_order_acquire);
		if (seq.load(std::memory_order_relaxed) == before)
		{
			return table;
```

- [x] **Step 9: Edit `CMakeLists.txt` to add the source**

```cmake
	src/link/Combat.cpp
	src/physx/GameAbi.cpp
```
```cmake
	src/link/Combat.cpp
	src/link/WorldTable.cpp
	src/physx/GameAbi.cpp
```

- [x] **Step 10: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `461 test(s), 0 failure(s)`.

- [x] **Step 11: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "minecraft-entities: the world-entity table, read under its lock and checked" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 3: The link passes on textures, scenes and the world-entity table

The link takes the two messages and reads the table (the spec's "Coming and going"):
- **Textures and scenes** go to the sink once the atlas has been seen, as sections do. Before it, they're from a link
  this one never saw, or from the frame Minecraft writes before it sees a new game process, with ids SkyCraft gives
  again after its clear: they're counted (`entitiesBeforeAtlas`) and stepped over. A bad one is rejected and counted,
  as a bad section is. Both now count toward the ring's budget, as the messages the link takes do.
- **The table** is read after the ring, each update while Minecraft is in its world, where SkyCraft writes it each
  frame. Found mid-write every time, the last good table stays (`worldTorn`). Out of its world, or gone, nothing of it
  is kept, so nothing stale reaches the sink, which gets the table every update.
- **The status** gains the table's records, those left out, and whether it has an outline.

`LinkStepsOverOtherMessagesAndKeepsToTheBudget` stepped over scenes; it now steps over atlas patches, which stay
stepped over.

**Files:**
- Modify: `src/link/Link.h`, `src/link/Link.cpp`, `tests/TestLink.cpp`

- [x] **Step 1: Edit `tests/TestLink.cpp` to add the failing tests**

```cpp
	proto::ActorTable Actors()
```
```cpp
	// Spike 7: the world-entity table as SkyLink.writeWorldEntities leaves it.
	void World(const proto::WorldEntities& aTable) { std::memcpy(base + proto::kOffWorldEntities, &aTable, sizeof aTable); }

	proto::ActorTable Actors()
```
```cpp
	}
	void Log(const std::string& aLine) override { log.push_back(aLine); }
```
```cpp
	}
	void Texture(const TextureImage& aTexture) override { textures.push_back({ aTexture.id, aTexture.width }); }
	void Scene(const SceneMessage& aScene) override { scenes.push_back({ aScene.origin.x, aScene.batches.size(), aScene.vertexCount }); }
	void WorldEntities(const WorldTable& aTable) override { worlds.push_back(aTable); }
	void Log(const std::string& aLine) override { log.push_back(aLine); }
```
```cpp
	const uint8_t*                             hudPixels = nullptr;  // the last HUD image's
```
```cpp
	const uint8_t*                             hudPixels = nullptr;  // the last HUD image's
	struct SeenScene
	{
		double   originX;
		size_t   batches;
		uint32_t vertices;
	};
	std::vector<std::pair<uint32_t, uint32_t>> textures;  // id and width
	std::vector<SeenScene>                     scenes;
	std::vector<WorldTable>                    worlds;  // one an update
```
```cpp
	std::memcpy(payload.data() + 12, &aClaimed, 4);
```
```cpp
	std::memcpy(payload.data() + 12, &aClaimed, 4);
	return payload;
}

// Spike 7: a kRenTexture payload, aId of aSide x 1, as AvatarExporter.textureId sends it.
std::vector<uint8_t> TexturePayload(uint32_t aId, uint32_t aSide)
{
	std::vector<uint8_t> payload(16 + aSide * 4, 0x7F);
	const uint32_t       header[4] = { aId, aSide, 1, 0 };
	std::memcpy(payload.data(), header, sizeof header);
	return payload;
}

// A kRenScene payload from origin x aX, its one batch of texture aTexture holding aCount of its aVertices vertices.
std::vector<uint8_t> ScenePayload(double aX, uint32_t aTexture, uint32_t aCount, uint32_t aVertices)
{
	std::vector<uint8_t> payload(32 + 16 + aVertices * sizeof(Vertex));
	const double         origin[3] = { aX, 6.0, 2007.0 };
	const uint32_t       counts[2] = { 1, aVertices };
	const uint32_t       batch[4] = { aTexture, 0, aCount, 0 };
	std::memcpy(payload.data(), origin, sizeof origin);
	std::memcpy(payload.data() + 24, counts, sizeof counts);
	std::memcpy(payload.data() + 32, batch, sizeof batch);
```
```cpp
		minecraft.Send(proto::kRenScene, std::vector<uint8_t>(4000));
```
```cpp
		minecraft.Send(proto::kRenAtlasRegion, std::vector<uint8_t>(4000));
```
```cpp
	CHECK_EQ(link.State().messages[proto::kRenScene], 1ull);  // stepped over, without counting toward the budget
	link.Update(At(1.1), kSettings, sink);
	CHECK_EQ(sink.sections.size(), size_t(3));
	CHECK_EQ(link.State().messages[proto::kRenScene], 3ull);
```
```cpp
	CHECK_EQ(link.State().messages[proto::kRenAtlasRegion], 1ull);  // stepped over, without counting toward the budget
	link.Update(At(1.1), kSettings, sink);
	CHECK_EQ(sink.sections.size(), size_t(3));
	CHECK_EQ(link.State().messages[proto::kRenAtlasRegion], 3ull);
```
```cpp
	CHECK_EQ(link.State().resyncs, 0ull);
```
```cpp
	CHECK_EQ(link.State().resyncs, 0ull);
}

TEST(LinkPassesOnTexturesAndScenesOnceTheAtlasIsSeen)
{
	FakeMinecraft minecraft;
	Link          link(minecraft.base);
	Sink          sink;
	minecraft.Send(proto::kRenTexture, TexturePayload(1, 4));  // left from a link we never saw: its ids aren't this one's
	minecraft.Send(proto::kRenScene, ScenePayload(-130.0, 1, 3, 3));
	link.Update(At(1.0), kSettings, sink);
	CHECK(sink.textures.empty());
	CHECK(sink.scenes.empty());
	CHECK_EQ(link.State().entitiesBeforeAtlas, 2ull);
	minecraft.Send(proto::kRenClearAll, {});
	minecraft.Send(proto::kRenAtlas, Atlas2x2());
	minecraft.Send(proto::kRenTexture, TexturePayload(1, 8));
	minecraft.Send(proto::kRenScene, ScenePayload(-120.0, 1, 6, 6));
	link.Update(At(1.1), kSettings, sink);
	CHECK_EQ(sink.textures.size(), size_t(1));
	CHECK_EQ(sink.scenes.size(), size_t(1));
	CHECK_EQ(link.State().entitiesBeforeAtlas, 2ull);
	CHECK_EQ(link.State().messages[proto::kRenScene], 2ull);
	if (sink.textures.size() != 1 || sink.scenes.size() != 1)
	{
		return;
	}
	CHECK_EQ(sink.textures[0].first, 1u);
	CHECK_EQ(sink.textures[0].second, 8u);
	CHECK_EQ(sink.scenes[0].originX, -120.0);
	CHECK_EQ(sink.scenes[0].batches, size_t(1));
	CHECK_EQ(sink.scenes[0].vertices, 6u);
}

TEST(LinkRejectsABadSceneAndCarriesOn)
{
	FakeMinecraft minecraft;
	Link          link(minecraft.base);
	Sink          sink;
	minecraft.Send(proto::kRenAtlas, Atlas2x2());
	minecraft.Send(proto::kRenScene, ScenePayload(-120.0, 0, 9, 6));  // its batch runs past its 6 vertices
	minecraft.Send(proto::kRenTexture, TexturePayload(2, 4));
	link.Update(At(1.0), kSettings, sink);
	CHECK(sink.scenes.empty());
	CHECK_EQ(link.State().rejected, 1ull);
	CHECK(link.State().lastRejected.find("batch 0") != std::string::npos);
	CHECK_EQ(sink.textures.size(), size_t(1));
}

TEST(LinkReadsTheWorldTableWhileMinecraftIsInItsWorld)
{
	FakeMinecraft minecraft;
	Link          link(minecraft.base);
	Sink          sink;
	minecraft.Beat(77, 1000);
	minecraft.State(proto::kMcInWorld);
	proto::WorldEntities table{};
	table.seq = 2;
	table.count = 3;
	table.entities[0].kind = proto::kWeItem;
	table.entities[1].kind = 9;  // no such kind: left out
	table.entities[2].kind = proto::kWeShadow;
	table.hasSelection = 1;
	minecraft.World(table);
	link.Update(At(1.0), kSettings, sink);
	CHECK_EQ(link.State().worldEntities, size_t(2));
	CHECK_EQ(link.State().worldLeftOut, 1u);
	CHECK(link.State().outline);
	table.seq = 3;  // mid-write every time it's looked at: the last good table stays
	minecraft.World(table);
	link.Update(At(1.1), kSettings, sink);
	CHECK_EQ(link.State().worldTorn, 1ull);
	minecraft.State(0);  // out of its world: nothing of the table is kept
	link.Update(At(1.2), kSettings, sink);
	CHECK_EQ(link.State().worldEntities, size_t(0));
	CHECK(!link.State().outline);
	CHECK_EQ(sink.worlds.size(), size_t(3));  // one an update
	if (sink.worlds.size() != 3)
	{
		return;
	}
	CHECK_EQ(sink.worlds[0].entities.size(), size_t(2));
	CHECK(sink.worlds[0].outline.has_value());
	CHECK_EQ(sink.worlds[1].entities.size(), size_t(2));
	CHECK(sink.worlds[2].entities.empty());
	CHECK(!sink.worlds[2].outline.has_value());
```

- [x] **Step 2: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C2065: 'WorldTable': undeclared identifier`.

- [x] **Step 3: Edit `src/link/Link.h` to add the sink's calls and the status's counts**

```cpp
#include "link/Ticks.h"
```
```cpp
#include "link/Ticks.h"
#include "link/WorldTable.h"
```
```cpp
// SkyCraft's actor table, and passes on Minecraft's hits on them and its player's death. It works on the mapping's
// memory, so the tests give it plain memory with a fake Minecraft.
```
```cpp
// SkyCraft's actor table, and passes on Minecraft's hits on them and its player's death. Spike 7 passes on Minecraft's
// entity textures, its scene of entities and particles, and its world-entity table. It works on the mapping's memory,
// so the tests give it plain memory with a fake Minecraft.
```
```cpp
	virtual void Hud(const HudFrame&) {}
```
```cpp
	virtual void Hud(const HudFrame&) {}
	virtual void Texture(const TextureImage&) {}      // spike 7: an entity texture, once the atlas has been seen
	virtual void Scene(const SceneMessage&) {}        // spike 7: this Minecraft frame's entities and particles, likewise
	virtual void WorldEntities(const WorldTable&) {}  // spike 7: every update; empty while Minecraft isn't in its world
```
```cpp
		uint64_t                     solidBlocks = 0;    // and their solid blocks
```
```cpp
		uint64_t                     solidBlocks = 0;    // and their solid blocks
		uint64_t                     entitiesBeforeAtlas = 0;  // spike 7: textures and scenes from before the atlas, skipped
		// Spike 7: the world-entity table, as the last update read it
		size_t                       worldEntities = 0;  // its records that passed the checks
		uint32_t                     worldLeftOut = 0;   // and those that didn't
		uint64_t                     worldTorn = 0;      // updates that found it mid-write every time, since the start
		bool                         outline = false;    // it has the targeted block's outline
```
```cpp
	Solids                solids_;
```
```cpp
	Solids                solids_;
	WorldTable            world_;  // the last good table read, while Minecraft is in its world
```

- [x] **Step 4: Edit `src/link/Link.cpp` to take textures and scenes, and read the table**

```cpp
	if (aFrame.takeHud && there)
```
```cpp
	// Spike 7: the world-entity table, read while Minecraft is in its world, where it writes the table each frame.
	// Otherwise nothing of it is kept, so nothing stale is drawn.
	if (status_.inWorld)
	{
		if (const auto table = ReadWorldEntities(base_))
		{
			world_ = CheckWorldTable(*table);
		}
		else
		{
			++status_.worldTorn;  // mid-write every time: the last good table stays
		}
	}
	else
	{
		world_ = {};
	}
	status_.worldEntities = world_.entities.size();
	status_.worldLeftOut = world_.leftOut;
	status_.outline = world_.outline.has_value();
	aSink.WorldEntities(world_);

	if (aFrame.takeHud && there)
```
```cpp
		return true;
	default:
		return false;  // atlas patches, lights, digging, textures, avatar, scene, ragdoll: stepped over
```
```cpp
		return true;
	case proto::kRenTexture:
	case proto::kRenScene:
		if (!status_.atlasSeen)
		{
			++status_.entitiesBeforeAtlas;  // from before Minecraft's clear: SkyCraft gives its ids again after it
			return false;
		}
		if (aMessage.type == proto::kRenTexture)
		{
			if (const auto texture = ParseTexture(aMessage.payload, error))
			{
				aSink.Texture(*texture);
			}
			else
			{
				reject();
			}
		}
		else if (const auto scene = ParseScene(aMessage.payload, error))
		{
			aSink.Scene(*scene);
		}
		else
		{
			reject();
		}
		return true;
	default:
		return false;  // atlas patches, lights, digging, avatar, ragdoll: stepped over
```

- [x] **Step 5: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `464 test(s), 0 failure(s)`.

- [x] **Step 6: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "minecraft-entities: the link passes on Minecraft's entity textures, its scenes and its world-entity table" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 4: `mclink` reports textures, scenes and the table, and runs step 0's script

`mclink`'s report takes the new calls (the spec's "Step 0"):
- **each texture** is said as it comes (`texture 3: 64 x 64, 0.02 MB`) and kept, to be saved; a clear drops them, as
  SkyCraft sends them again;
- **the scenes** are summed up every five seconds: how many a second and MB a second, and the largest since the last
  line, its vertices, how many have block light 15 (which step A will take as glowing) and how many are in translucent
  batches; with the textures kept, and the table's records counted by kind;
- **`scene`** prints the newest scene batch by batch (its texture, by id and size; its vertices; translucent or not;
  how many glow), then the table record by record, and its outline;
- **`save-textures <folder>`** saves every texture kept as `<folder>\texture-<id>.dds`, so step 0 can tell which id is
  which;
- **the status** gains an entities line.

Step 0's script, `tools/mclink/entities.txt`, follows `blast.txt`: the stand-in V on spike 1's dumped ground, with the
city's ground so that things stand on it, in creative, at a fresh spot 400 blocks east of `blast.txt`'s. It makes each
test thing 3 to 7 blocks ahead, the stand-in V looking at it, since Minecraft extracts only the particles in its view:
a pig and a villager with a profession and a level; a spider and a blaze with their AI off; a falling sand, dropped
from 12 blocks up so a scene catches it in the air; a chest and a banner with two patterns; a dropped diamond and a
dropped grass block, and an arrow; a firework, looking up, and a splash potion. Then a TNT 18 blocks ahead, beyond
the 8 blocks its blast hurts, and beyond the dump's raised ground, which ends 6 blocks ahead, 6 to 9 m above the
ground past it: an obsidian block holds the TNT at the stand-in V's level. It flashes white every quarter second of
its 4 s fuse, so three scenes a quarter second apart catch it with and without its flash, and one more comes just
after its blast. Everything summoned is tagged `sp7`, and each block placed has a marker tagged `sp7b`, so the end
clears them all and spares the NPCs' stand-ins.

Nothing it makes leaves a drop that the clear would miss, since Night City's ground is only collision to Minecraft
("Found before executing Part 1"):
- **the mobs** have empty loot tables (`DeathLootTable:"minecraft:empty"`), since `/kill` runs them;
- **the sand** drops no item (`DropItem:0b`): it can't land as a block on the city's ground;
- **the TNT** stands on obsidian, which its blast can't break: a block it broke would drop as an item.

The last scene comes 7 s after the clear, once the killed mobs' death puffs and the blaze's smoke are gone: its world
line saying `nothing` shows that nothing is left.

**Files:**
- Create: `tools/mclink/entities.txt`
- Modify: `tools/mclink/McLinkScript.h`, `tools/mclink/McLinkScript.cpp`, `tools/mclink/Main.cpp`,
  `tests/TestMcLinkScript.cpp`

- [x] **Step 1: Edit `tests/TestMcLinkScript.cpp` to add the failing tests**

```cpp
	for (const char* name : { "step1.txt", "again.txt", "eye.txt", "drive.txt", "relink.txt", "city.txt", "npcs.txt", "hurts.txt", "blasts.txt" })
```
```cpp
	for (const char* name : { "step1.txt", "again.txt", "eye.txt", "drive.txt", "relink.txt", "city.txt", "npcs.txt", "hurts.txt", "blasts.txt",
	                          "entities.txt" })
```
```cpp
TEST(ScriptParsesHealth)
```
```cpp
TEST(ScriptParsesScenesAndTextures)
{
	std::string error;
	const auto  actions = ParseScript("scene\nsave-textures build\\step0\\textures\n", error);
	CHECK(actions.has_value());
	CHECK_EQ(actions ? actions->size() : 0, size_t(2));
	if (actions && actions->size() == 2)
	{
		CHECK((*actions)[0].kind == Kind::Scene);
		CHECK((*actions)[1].kind == Kind::SaveTextures);
		CHECK_STR((*actions)[1].text, "build\\step0\\textures");
	}
	CHECK(!ParseScript("scene now", error).has_value());
	CHECK_STR(error, "line 1: scene takes nothing: \"scene now\"");
	CHECK(!ParseScript("save-textures", error).has_value());
	CHECK_STR(error, "line 1: save-textures takes a folder: \"save-textures\"");
}

TEST(ScriptParsesHealth)
```

- [x] **Step 2: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C2065: 'Scene': undeclared identifier`.

- [x] **Step 3: Edit `tools/mclink/McLinkScript.h` to add the two actions**

```cpp
//                                  plugin sends V's as build mode starts (spike 6's follow-up): CyberCraft Tweaks 1.3.0 sets it
```
```cpp
//                                  plugin sends V's as build mode starts (spike 6's follow-up): CyberCraft Tweaks 1.3.0 sets it
//   scene                          print the newest scene, batch by batch (spike 7's step 0), and the world-entity table
//   save-textures <folder>         save every entity texture received so far as <folder>\texture-<id>.dds
```
```cpp
		Health,
```
```cpp
		Health,
		Scene,
		SaveTextures,
```
```cpp
	std::string text;        // the command, the file, the ground, or an actor's name
```
```cpp
	std::string text;        // the command, the file, the folder, the ground, or an actor's name
```

- [x] **Step 4: Edit `tools/mclink/McLinkScript.cpp` to parse them**

```cpp
		else if (verb == "floor" || verb == "status" || verb == "quit")
		{
			action.kind = verb == "floor" ? Action::Kind::Floor : verb == "status" ? Action::Kind::Status : Action::Kind::Quit;
```
```cpp
		else if (verb == "save-textures")
		{
			action.kind = Action::Kind::SaveTextures;
			action.text = std::string(rest);
			if (action.text.empty())
			{
				return fail("save-textures takes a folder");
			}
		}
		else if (verb == "floor" || verb == "scene" || verb == "status" || verb == "quit")
		{
			action.kind = verb == "floor"    ? Action::Kind::Floor
			              : verb == "scene"  ? Action::Kind::Scene
			              : verb == "status" ? Action::Kind::Status
			                                 : Action::Kind::Quit;
```

- [x] **Step 5: Edit `tools/mclink/Main.cpp` to report textures, scenes and the table, and run the actions**

```cpp
// and spike 6's steps 1, 3 and 5a. It runs BlockRender's link code against the real Minecraft, without the game
// (docs/superpowers/specs/2026-10-02-minecraft-link-design.md, 2026-10-03-collision-and-movement-design.md and
// 2026-10-05-npcs-and-combat-design.md).
```
```cpp
// spike 6's steps 1, 3 and 5a, and spike 7's step 0. It runs BlockRender's link code against the real Minecraft, without
// the game (docs/superpowers/specs/2026-10-02-minecraft-link-design.md, 2026-10-03-collision-and-movement-design.md,
// 2026-10-05-npcs-and-combat-design.md and 2026-10-06-minecraft-entities-design.md).
```
```cpp
// feet, and when each blast's push would come; and refuses to start while another game side uses the mapping.
```
```cpp
// feet, and when each blast's push would come; its entity textures, scenes and world-entity table; and refuses to start
// while another game side uses the mapping.
```
```cpp
#include <cstring>
```
```cpp
#include <cstring>
#include <filesystem>
```
```cpp
// What Minecraft sends, reported as it comes.
```
```cpp
// Spike 7: a scene as step 0 reports it, with how many of each batch's vertices have block light 15, which the plugin
// takes as glowing (the spec's "Step A: into the G-buffer").
struct SceneSummary
{
	struct Batch
	{
		SceneBatch batch;
		uint32_t   glow = 0;
	};
	Vec3d              origin;
	std::vector<Batch> batches;
	uint32_t           vertices = 0;
	uint32_t           glow = 0;
	uint32_t           translucent = 0;  // vertices in translucent batches
	uint64_t           bytes = 0;        // the message's payload
};

SceneSummary Summarize(const SceneMessage& aScene)
{
	SceneSummary summary;
	summary.origin = aScene.origin;
	summary.vertices = aScene.vertexCount;
	summary.bytes = sizeof(proto::RenScene) + aScene.batches.size() * sizeof(proto::RenBatch) + aScene.vertices.size();
	const std::vector<Vertex> vertices = SceneVertices(aScene);
	for (const SceneBatch& batch : aScene.batches)
	{
		SceneSummary::Batch entry{ batch };
		for (uint32_t i = batch.first; i < batch.first + batch.count; ++i)
		{
			entry.glow += (vertices[i].light & 0xFF) >= 15 ? 1 : 0;
		}
		summary.glow += entry.glow;
		summary.translucent += (batch.flags & 1) ? batch.count : 0;
		summary.batches.push_back(entry);
	}
	return summary;
}

// The world-entity table's records counted by kind, and its outline: "2 items, 1 shadow; an outline", or "nothing".
std::string WorldWords(const WorldTable& aTable)
{
	std::map<std::string, int> kinds;
	for (const proto::WorldEntity& record : aTable.entities)
	{
		++kinds[WorldKindName(record.kind)];
	}
	std::string words;
	for (const auto& [kind, count] : kinds)
	{
		words += std::format("{}{} {}{}", words.empty() ? "" : ", ", count, kind, count == 1 ? "" : "s");
	}
	if (aTable.outline)
	{
		words += words.empty() ? "an outline" : "; an outline";
	}
	return words.empty() ? "nothing" : words;
}

// What Minecraft sends, reported as it comes.
```
```cpp
	void ClearAll() override { Say("clear all"); }
```
```cpp
	void ClearAll() override
	{
		Say("clear all");
		textures.clear();  // SkyCraft sends its textures again, its ids from 1
	}

	// Spike 7: each entity texture, kept to be saved.
	void Texture(const TextureImage& aTexture) override
	{
		textures[aTexture.id] = { aTexture.width, aTexture.height, { aTexture.rgba.begin(), aTexture.rgba.end() } };
		Say("texture {}: {} x {}, {:.2f} MB", aTexture.id, aTexture.width, aTexture.height, double(aTexture.rgba.size()) / 1048576.0);
	}

	// Spike 7: the newest scene, summed up, and the largest since the last report.
	void Scene(const SceneMessage& aScene) override
	{
		scene = Summarize(aScene);
		++scenes;
		sceneBytes += scene.bytes;
		if (scene.bytes > largest.bytes)
		{
			largest = scene;
		}
	}

	void WorldEntities(const WorldTable& aTable) override { world = aTable; }
```
```cpp
	const std::map<uint32_t, proto::ActorRecord>* actors = nullptr;  // the made-up NPCs, by number
};
```
```cpp
	const std::map<uint32_t, proto::ActorRecord>* actors = nullptr;  // the made-up NPCs, by number
	struct KeptTexture
	{
		uint32_t             width = 0;
		uint32_t             height = 0;
		std::vector<uint8_t> rgba;
	};
	std::map<uint32_t, KeptTexture> textures;  // by id
	SceneSummary                    scene;     // the newest
	SceneSummary                    largest;   // the largest since the last report
	uint64_t                        scenes = 0;
	uint64_t                        sceneBytes = 0;
	WorldTable                      world;  // the newest
};

// Spike 7's step 0: the newest scene, batch by batch, and the world-entity table, record by record.
void PrintScene(const Report& aReport)
{
	const SceneSummary& s = aReport.scene;
	Say("scene: origin {:.0f} {:.0f} {:.0f}; {} batches, {} vertices ({} glow, {} translucent), {:.2f} MB", s.origin.x, s.origin.y, s.origin.z,
	    s.batches.size(), s.vertices, s.glow, s.translucent, double(s.bytes) / 1048576.0);
	for (size_t b = 0; b < s.batches.size(); ++b)
	{
		const SceneBatch& batch = s.batches[b].batch;
		const auto        texture = aReport.textures.find(batch.texture);
		const std::string what = batch.texture == 0                    ? std::string("the atlas")
		                         : texture != aReport.textures.end() ? std::format("texture {} ({} x {})", batch.texture, texture->second.width,
		                                                                            texture->second.height)
		                                                               : std::format("texture {} (not received)", batch.texture);
		Say("  batch {}: {}, {} vertices from {}{}, {} glow", b, what, batch.count, batch.first, (batch.flags & 1) ? ", translucent" : "",
		    s.batches[b].glow);
	}
	Say("world: {}", WorldWords(aReport.world));
	for (const proto::WorldEntity& record : aReport.world.entities)
	{
		Say("  {} {} at {:.2f} {:.2f} {:.2f}, yaw {:.1f}, pitch {:.1f}, scale {:.2f}", WorldKindName(record.kind), record.id, record.x, record.y,
		    record.z, record.yaw, record.pitch, record.scale);
	}
	if (const auto& box = aReport.world.outline)
	{
		Say("  outline from {:.3f} {:.3f} {:.3f} to {:.3f} {:.3f} {:.3f}", (*box)[0], (*box)[1], (*box)[2], (*box)[3], (*box)[4], (*box)[5]);
	}
}
```
```cpp
	    s.explosions, s.hitEvents, s.deathEvents, s.otherEvents, s.actors, s.inputsDropped, s.commandsWaiting, s.hudImages, s.hudRefused);
```
```cpp
	    s.explosions, s.hitEvents, s.deathEvents, s.otherEvents, s.actors, s.inputsDropped, s.commandsWaiting, s.hudImages, s.hudRefused);
	Say("entities: {} scenes and {} textures, {} of them from before the atlas; {} textures kept; the world table: {} records, {} left out, "
	    "{} updates found it mid-write{}",
	    s.messages[proto::kRenScene], s.messages[proto::kRenTexture], s.entitiesBeforeAtlas, aReport.textures.size(), s.worldEntities,
	    s.worldLeftOut, s.worldTorn, s.outline ? "; an outline" : "");
```
```cpp
	uint64_t reportedHuds = 0;
```
```cpp
	uint64_t reportedHuds = 0;
	uint64_t reportedScenes = 0;
	uint64_t reportedSceneBytes = 0;
```
```cpp
			    double(state.drainedBytes - reportedBytes) / 1048576.0 / seconds, report.sections, double(report.huds - reportedHuds) / seconds);
```
```cpp
			    double(state.drainedBytes - reportedBytes) / 1048576.0 / seconds, report.sections, double(report.huds - reportedHuds) / seconds);
			if (report.scenes != reportedScenes)
			{
				uint64_t textureBytes = 0;
				for (const auto& [id, texture] : report.textures)
				{
					textureBytes += texture.rgba.size();
				}
				const SceneSummary& top = report.largest;
				Say("scenes: {:.0f}/s, {:.2f} MB/s; the largest since the last line {:.2f} MB, {} vertices ({} glow, {} translucent) in {} "
				    "batches; {} textures ({:.2f} MB); world: {}",
				    double(report.scenes - reportedScenes) / seconds, double(report.sceneBytes - reportedSceneBytes) / 1048576.0 / seconds,
				    double(top.bytes) / 1048576.0, top.vertices, top.glow, top.translucent, top.batches.size(), report.textures.size(),
				    double(textureBytes) / 1048576.0, WorldWords(report.world));
				report.largest = {};
			}
```
```cpp
			reportedHuds = report.huds;
```
```cpp
			reportedHuds = report.huds;
			reportedScenes = report.scenes;
			reportedSceneBytes = report.sceneBytes;
```
```cpp
			case Action::Kind::Status: PrintStatus(link, report, feet); break;
```
```cpp
			case Action::Kind::Status: PrintStatus(link, report, feet); break;
			case Action::Kind::Scene: PrintScene(report); break;
			case Action::Kind::SaveTextures:
			{
				std::error_code ignored;
				std::filesystem::create_directories(action.text, ignored);
				size_t saved = 0;
				for (const auto& [id, texture] : report.textures)
				{
					saved += WriteDds(std::format("{}\\texture-{}.dds", action.text, id), texture.width, texture.height, texture.rgba.data()) ? 1 : 0;
				}
				Say("{} of {} textures saved in {}", saved, report.textures.size(), action.text);
				break;
			}
```

- [x] **Step 6: Write step 0's script, `tools/mclink/entities.txt`**

```text
# Spike 7's step 0: what Minecraft sends of its entities, block entities, particles and world entities, against
# SkyCraft's real mod, without the game. At a fresh spot 400 blocks east of blast.txt's, on spike 1's dumped ground, in
# creative, each test thing goes 3 to 7 blocks ahead of the stand-in V, who looks east at it: Minecraft extracts only
# the particles in its view. The dump's raised ground ends 6 blocks ahead, 6 to 9 m above the ground beyond. Everything
# summoned is tagged sp7, and each block placed gets a marker tagged sp7b, so the end clears them all and spares the
# NPCs' stand-ins. Nothing leaves a drop the clear would miss: the mobs have no loot, the sand drops no item, and the
# TNT stands on obsidian, which its blast can't break. The TNT goes last, 18 blocks ahead: its blast hurts up to 8
# blocks out. Start Minecraft first; this waits for its world.
status
wait-world 180
ground city
at 880.5 6.14 2007.5
look 270 0
dump E:\Cyberpunk2077\red4ext\plugins\PhysXProbe\dumps\2026-10-01_14-37-07\collision.obj
link on
type /gamemode creative
wait-ground 30
wait 2
scene
# Mobs: a pig, and a villager with a profession and a level: one over 1 keeps its profession. Every mob here has an
# empty loot table, so the clear's /kill drops nothing.
type /execute rotated ~ 0 positioned ^ ^ ^5 run summon minecraft:pig ~ ~ ~ {Tags:["sp7"],PersistenceRequired:1b,DeathLootTable:"minecraft:empty"}
type /execute rotated ~ 0 positioned ^-2 ^ ^5 run summon minecraft:villager ~ ~ ~ {Tags:["sp7"],PersistenceRequired:1b,DeathLootTable:"minecraft:empty",VillagerData:{type:"minecraft:plains",profession:"minecraft:farmer",level:2}}
wait 3
scene
# AI off: a spider, whose eyes glow in Minecraft by their render type, and a blaze, which has block light 15.
type /execute rotated ~ 0 positioned ^2 ^ ^5 run summon minecraft:spider ~ ~ ~ {Tags:["sp7"],PersistenceRequired:1b,NoAI:1b,DeathLootTable:"minecraft:empty"}
type /execute rotated ~ 0 positioned ^4 ^ ^5 run summon minecraft:blaze ~ ~1 ~ {Tags:["sp7"],PersistenceRequired:1b,NoAI:1b,DeathLootTable:"minecraft:empty"}
wait 3
scene
# A falling sand, dropped from 12 blocks up so a scene catches it in the air. Minecraft has no block under the city's
# ground, so the sand can't land as one: it would drop as an item, which DropItem stops.
type /execute rotated ~ 0 positioned ^-4 ^ ^5 run summon minecraft:falling_block ~ ~12 ~ {BlockState:{Name:"minecraft:sand"},DropItem:0b,Tags:["sp7"]}
wait 0.5
scene
wait 3
# Block entities, 7 blocks ahead: a chest and a banner with two patterns, each with a marker.
type /execute rotated ~ 0 positioned ^-1 ^ ^7 run setblock ~ ~ ~ minecraft:chest
type /execute rotated ~ 0 positioned ^-1 ^ ^7 run summon minecraft:marker ~ ~ ~ {Tags:["sp7b"]}
type /execute rotated ~ 0 positioned ^1 ^ ^7 run setblock ~ ~ ~ minecraft:white_banner{patterns:[{pattern:"minecraft:stripe_top",color:"red"},{pattern:"minecraft:creeper",color:"black"}]}
type /execute rotated ~ 0 positioned ^1 ^ ^7 run summon minecraft:marker ~ ~ ~ {Tags:["sp7b"]}
wait 2
scene
# World entities: a dropped diamond, a dropped grass block, and an arrow flying ahead (east).
type /execute rotated ~ 0 positioned ^ ^ ^3 run summon minecraft:item ~ ~1 ~ {Item:{id:"minecraft:diamond",count:1},PickupDelay:32767,Tags:["sp7"]}
type /execute rotated ~ 0 positioned ^1 ^ ^3 run summon minecraft:item ~ ~1 ~ {Item:{id:"minecraft:grass_block",count:1},PickupDelay:32767,Tags:["sp7"]}
type /execute rotated ~ 0 positioned ^-1 ^ ^3 run summon minecraft:arrow ~ ~1 ~ {Motion:[0.6,0.3,0.0],Tags:["sp7"]}
wait 2
scene
# Translucent particles: a firework bursting overhead, the stand-in V looking up at it; then a splash potion, which
# breaks on the ground about half a second after it's summoned.
look 270 -45
type /execute rotated ~ 0 positioned ^ ^ ^6 run summon minecraft:firework_rocket ~ ~ ~ {LifeTime:20,FireworksItem:{id:"minecraft:firework_rocket",count:1,components:{"minecraft:fireworks":{explosions:[{shape:"large_ball",colors:[I;16711680]}]}}}}
wait 1.5
scene
wait 1
look 270 0
type /execute rotated ~ 0 positioned ^2 ^ ^3 run summon minecraft:splash_potion ~ ~2 ~ {Item:{id:"minecraft:splash_potion",count:1,components:{"minecraft:potion_contents":{potion:"minecraft:regeneration"}}}}
wait 1
scene
wait 2
save-textures build\step0\textures
status
# Last, the TNT 18 blocks ahead, beyond the raised ground, held at the stand-in V's level by an obsidian block with a
# marker: TNT can't break obsidian, and a block it broke would drop as an item. Its fuse is 4 s, and it flashes white
# every quarter second: three scenes a quarter second apart catch it with and without its flash; one more comes just
# after its blast.
type /execute rotated ~ 0 positioned ^ ^ ^18 run setblock ~ ~-1 ~ minecraft:obsidian
type /execute rotated ~ 0 positioned ^ ^-1 ^18 run summon minecraft:marker ~ ~ ~ {Tags:["sp7b"]}
type /execute rotated ~ 0 positioned ^ ^ ^18 run summon minecraft:tnt
wait 2
scene
wait 0.25
scene
wait 0.25
scene
wait 1.7
scene
wait 3
status
# The clear: the placed blocks by their markers, then everything tagged. A killed mob takes a second to die, and the
# puffs it leaves, and a blaze's smoke, last up to 5 s more: the last scene, 7 s on, has nothing in it, and its world
# line says nothing is left.
type /execute at @e[tag=sp7b] run setblock ~ ~ ~ minecraft:air
type /kill @e[tag=sp7]
type /kill @e[tag=sp7b]
wait 7
scene
status
quit
```

- [x] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `465 test(s), 0 failure(s)`.

- [x] **Step 8: Run mclink without Minecraft**

Minecraft and the game must not be running.
```bash
cd /d/Cyberpunk-Minecraft/spikes/block-render && printf 'status\nquit\n' > build/selfcheck.txt \
  && ./build/RelWithDebInfo/mclink.exe build/selfcheck.txt; echo "exit: $?"
```
Expected: `the mapping: created (process …); waiting for Minecraft`, then the status with `Minecraft: absent` and
`entities: 0 scenes and 0 textures, 0 of them from before the atlas; 0 textures kept; the world table: 0 records, 0
left out, 0 updates found it mid-write`. Then `exit: 0`.

- [x] **Step 9: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "minecraft-entities: mclink reports Minecraft's entity textures, scenes and world-entity table, saves the textures, and runs step 0's script" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 5: In Minecraft: step 0

Step 0 of the spec's "The steps", with the user: what SkyCraft really sends of Minecraft's entities, from `mclink`,
against SkyCraft's unchanged mod in the `CyberCraft` instance, without the game.
- **The instance** has had SkyCraft 0.1.2, Fabric API and CyberCraft Tweaks since spike 5, and the Java arguments
  `--enable-native-access=ALL-UNNAMED -Dskycraft.quitWithSkyrim=false`, so Minecraft keeps running between `mclink`
  runs. Its particles are at All (`particles:0` in `options.txt`).
- **What SkyCraft logs:** `SkyCraft: sent texture <name> to Skyrim (id <n>)` for each texture it sends, which names
  each id, and `SkyCraft: texture <name> isn't available to Skyrim; what uses it is left out` for one it can't read.
  The chat's replies to the script's commands go into Minecraft's log after `[CHAT]`, errors included.
- **If Minecraft doesn't link,** its log says why, in lines starting `SkyCraft:`.
- **If a check fails,** stop and find the cause with superpowers:systematic-debugging before going on. Fixes become new
  tasks.

**Files:**
- Create: `docs/spikes/2026-10-06-minecraft-entities-results.md`

- [x] **Step 1: Check that the game isn't running**

```bash
tasklist | grep -i -c "cyberpunk2077.exe"
```
Expected: `0`. If it's `1`, ask the user to quit the game: it would hold the mapping.

- [x] **Step 2: Ask the user to start Minecraft**

With their leave to summon and place test things at x 880 in the `SkyCraft` world, SkyCraft's own in the `CyberCraft`
instance, which the script clears at its end, leaving no drops, the user starts the `CyberCraft` instance from the
Modrinth App, and says when its title screen shows. Then:
```bash
grep -E "SkyCraft:" "/c/Users/BLOXY/AppData/Roaming/ModrinthApp/profiles/CyberCraft/logs/latest.log" | tail -3
```
Expected: `SkyCraft: can't open Skyrim's shared memory yet (Windows error 2: …)`: waiting for a game side.

- [x] **Step 3: Run step 0's script**

```bash
L="/c/Users/BLOXY/AppData/Roaming/ModrinthApp/profiles/CyberCraft/logs/latest.log"; before=$(wc -l < "$L"); \
  cd /d/Cyberpunk-Minecraft/spikes/block-render && mkdir -p build/step0 \
  && ./build/RelWithDebInfo/mclink.exe tools/mclink/entities.txt | tee build/step0/entities.log; echo "exit: ${PIPESTATUS[0]}"; \
  tail -n +$((before + 1)) "$L" | grep -E "SkyCraft: (sent texture|texture)|\[CHAT\]" | tee build/step0/minecraft.log
```
It takes about a minute once Minecraft is in its world, and up to four if it opens the world first: give the command 6
minutes. Expected, against the spec's "Step 0":
- **Linking:** `Minecraft is there (process …)`, `clear all`, the atlas, then `the city around region … is all in the
  ring`.
- **Textures:** a `texture <id>: <width> x <height>` line for each, and in Minecraft's lines the name of each id: the
  pig's, the villager's (its body, its biome, its profession and its level), the spider's and its eyes', the blaze's,
  the chest's and the banner's atlases, the particles' atlas.
- **The scenes,** at each `scene`:
  - the pig and the villager: batches of their textures, none glowing;
  - the spider and the blaze: the blaze's batch with glowing vertices (block light 15), the spider's eyes' batch with
    none;
  - the falling sand: an atlas batch while it falls;
  - the chest and the banner: batches of their atlases;
  - the firework and the potion: translucent batches;
  - the lit TNT: an atlas batch, all its vertices glowing in a scene where it flashes and none in one where it doesn't;
  - just after its blast: the explosion's puffs, glowing (`HugeExplosionParticle` has full light), and its smoke, not;
  - after the clear: no batches.
- **The world table:** `1 arrow, 1 block, 1 item, 4 shadows` at the items' scene, the grass block being a dropped full
  block, which SkyCraft records as a `block`; then `nothing` after the clear: no drop is left.
- **The blast:** a `scenes:` line whose largest scene is the explosion's, near the 1 MB a frame spike 4 measured.
- **No `[CHAT]` line with an error,** and `exit: 0`.

- [x] **Step 4: Ask the user to stop Minecraft**

Its window is hidden while `mclink` drives it, so the user stops it from the Modrinth App.

- [x] **Step 5: Write step 0's results, `docs/spikes/2026-10-06-minecraft-entities-results.md`**

From `build/step0/entities.log`, `build/step0/minecraft.log` and the saved textures in `build/step0/textures/`, as spike
6's results doc does: the versions; each check above with its outcome and numbers (the textures' sizes and MB in all,
each test thing's batches, the glowing and translucent counts, the villager's batch order, the explosion's peak);
anything that surprised, and what it means for Part 2. The spike's answer goes at the top once step A is in.

- [x] **Step 6: Commit**

With the plan, its steps ticked up to this one: running Tasks 1 to 4 ticked theirs.
```bash
cd /d/Cyberpunk-Minecraft && git add docs/spikes/2026-10-06-minecraft-entities-results.md docs/superpowers/plans/2026-10-06-minecraft-entities.md \
  && git commit -q -m "minecraft-entities: step 0's results, and Part 1's steps ticked" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

## Found while planning Part 1

The spec's "Changes while planning Part 1" lists what planning settled in the design. In the plan itself:
- **A test stops at a failed check that guards an index or an optional.** In the first run of the mutations, 3 of 40
  crashed the tests instead of failing one: a scene the mutation made the parser reject, and a sink that got nothing,
  each read past its end by the checks that followed. A crash hides every later test, so each such check now returns
  when it fails, and all 40 mutations are caught.
- **The scene's limits are tested by their own words,** `is too big`: the size check's error names the counts too, so
  a limit moved by one still passed a test that looked only for the counts.
- **`LinkStepsOverOtherMessagesAndKeepsToTheBudget` steps over atlas patches,** since the link now takes scenes.
- **Step 0's spot, x 880,** is 400 blocks east of `blast.txt`'s, on spike 1's dump, whose raised ground ends 6 blocks
  east of the stand-in V: the things 3 to 5 blocks ahead stand on it, and the TNT, 18 ahead, on an obsidian block at
  its level. Planning had the dump's ground reaching 36 blocks east and the TNT on it; "Found before executing Part 1"
  corrects that.

## Found before executing Part 1

Before Part 1 ran, the user asked for a read of the plan, the spec and their context. Step 0's script, played through
against SkyCraft's source, Minecraft 26.3's classes and spike 1's dump, would have left untagged items in the user's
world past its clear, since Night City's ground is only collision to Minecraft, in SkyCraft's void world. Task 4's
script and Task 5's expectations now have these changes:
- **The dump's ground:** its raised level ends 6 blocks east of the stand-in V, 6 to 9 m above the ground beyond, which
  reaches about 30 blocks east. The TNT, 18 ahead, stands only on the block under it, which holds it at the stand-in
  V's level.
- **That block is obsidian, not stone:** a TNT blast drops every block it breaks (26.3's `tnt_explosion_drop_decay` is
  false by default), so the stone's cobblestone would have fallen to the ground below, untagged. TNT can't break
  obsidian.
- **The falling sand drops no item:** Minecraft has no block under the city's ground (`FallingBlock.isFree`), so the
  sand can't land as a block, and would drop as a sand item. `DropItem:0b` stops that, and its marker is gone.
- **The mobs have no loot:** `/kill` runs a mob's loot table, so the pig would have dropped porkchops and the spider
  string. Each mob has `DeathLootTable:"minecraft:empty"`, which 26.3's `Mob.getLootTable` honours.
- **The potion's scene comes after 1 s,** not 0.5: the potion breaks on the ground about half a second after it's
  summoned.
- **The last scene comes 7 s after the clear,** not 1: a killed mob takes a second to die, and its death puffs (up to
  4 s) and a blaze's smoke (up to 5 s; it smokes even with its AI off) outlast it. Its world line saying `nothing`
  then shows that nothing was left.
- **The world line** at the items' scene is `1 arrow, 1 block, 1 item, 4 shadows`: SkyCraft records a dropped full
  block, the grass block, as a `block`.
- **The world** is `SkyCraft`, SkyCraft's own void world in the `CyberCraft` instance.
- **Task 5's commit** takes the plan too, so the ticked steps are kept.

The changed script was checked as Part 1's code was: the scratch clone's tests parse it (465 passing, and one failing
when the script is broken), and the plan's text still makes each task's files.
