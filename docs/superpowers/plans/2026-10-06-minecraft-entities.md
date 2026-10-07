# Minecraft's Entities Implementation Plan, Parts 1 and 2 (steps 0, A and C's draw order)

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

**Part 2** (Tasks 6 to 17) draws Minecraft's entities into the game's G-buffer (step A), after step C's draw order,
which step 0 showed is needed: CyberCraft Tweaks 1.4.0 keeps Minecraft's draw order in what SkyCraft sends. It was
planned after step 0, from its results, and its own section below says how it's built.
**Part 3** (step B, and what run A asks for) is planned after run A.

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

---

## Part 2: step A, and step C's draw order

Part 2 draws Minecraft's entities, block entities, particles and world entities into the game's G-buffer (the spec's
"Step A: into the G-buffer"), with Minecraft's draw order kept by CyberCraft Tweaks 1.4.0 (the spec's "Step C", its draw
order alone). It was planned after step 0 (Task 5), from what step 0 found (below); the spec's "Changes while planning
Part 2" lists what planning settled in the design, and "Found while planning Part 2", at the end, what it settled in
the plan.
- **Tasks 6 and 7** are step C's draw order, first: step 0 showed it's needed, and `mclink` can check it against the
  real Minecraft now. Task 6 makes CyberCraft Tweaks 1.4.0; Task 7 runs step 0's script again with it, in Minecraft,
  without the game.
- **Tasks 8 to 11** make the plain C++ of a frame's entities, tested without the game: the world entities' shapes,
  `Entities` (one frame's geometry), where each texture goes in one buffer (`EntityTextures`), and the snapshot ring
  (`Snapshots`).
- **Tasks 12 and 13** draw them: the shaders, then the renderer's textures, snapshots, pipelines and draws.
- **Task 14** hands them to the renderer: BlockRender 0.20.0's sink, each frame's entities, the `entities` request and
  the status's `entities` line. **Task 15** gives the overlay its Spawn rows, Clear, and `Entities` button and line.
- **Task 16** is run A, in the game. **Task 17** closes Part 2 and plans Part 3 (step B).

Test counts: 465 at the start of Part 2. Then 471 after Task 8, 478 after Task 9, 483 after Task 10, 488 after Task 11,
489 after Task 12 and 493 after Task 14. Tasks 6, 13 and 15 add none.

### What step 0 found

From the results' "Step 0" (`docs/spikes/2026-10-06-minecraft-entities-results.md`), what Part 2 builds on:
- **The sizes:** scenes of at most 6,222 vertices (0.19 MB) in step 0, and about 1 MB in `tnt-cube`'s blast; textures
  of 3.64 MB, 3.5 MB of it three atlases. The spec's 8 MB snapshots and 64 MB of textures hold them.
- **The glow rule takes what it should:** the blaze, the TNT's flash, the explosion's puffs and the firework's sparks
  have block light 15; the spider's eyes, smoke and spell particles don't.
- **A villager's layers come in the wrong order:** SkyCraft keeps its batches in a `HashMap`, so they go out by the
  textures' ids, which follow what Minecraft drew first in the session. Step C's draw order is the fix, so Part 2 makes
  it first.
- **Mobs don't walk on Night City's ground:** their paths need a Minecraft block under them, and under the city's
  ground there's air. Run A's smearing check uses a slime, which hops with no path.
- **SkyCraft sends a scene only while something is there,** and one empty scene once the last thing has gone:
  `Entities` keeps the last scene until then.
- **`/gamemode creative` came too soon** in step 0's script, while Minecraft's loading screen was open: Task 6 moves it.

## Before you start (Part 2)

- **Part 1's "Before you start" holds,** its shell, Edit steps and commits included.
- **Already verified:** all the code in Part 2 has been compiled and tested once already, in a scratch clone.
  - Tasks 6 and 8 to 15 were then replayed in a fresh clone by a script that applied every file step exactly as
    written, and checked every build, test, CyberCraft Tweaks and overlay step's expected result. The results:
    - zero compiler warnings at `/W4`, and CyberCraft Tweaks built with none;
    - 493 C++ tests passing;
    - the replayed files identical to the scratch clone's.
  - Each new test was broken once, by a mutation of the code it covers, and failed as it should: 62 mutations.
  - The mixin was checked against SkyCraft 0.1.2's bytecode: `AvatarExporter`'s `batches` field, `batch(int, int,
    int)` and the key it stores, `order(int)` handing back the exporter itself, and `submitModel` making its batch
    inside the call. The layers' orders were read from Minecraft 26.3's classes: `VillagerProfessionLayer` submits the
    biome, profession and level at `order(1)` to `order(3)`, and `EyesLayer` at `order(1)`.
  - The overlay's Spawn rows were played through against Minecraft 26.3's classes and SkyCraft's, for where each thing
    lands and what it leaves behind ("Found while planning Part 2"), then typed into Minecraft with `mclink` during
    Task 7 ("Found while executing Part 2").

  So a failure while following the plan most likely means a transcription slip: compare against the plan's code first.
- **What only Minecraft and the game can test:** the draw order with SkyCraft's real mod (Task 7), and everything drawn
  (Task 16).
- **The user:**
  - **does everything in Minecraft, the Modrinth App and the game,** as in Part 1: never drive their mouse or keyboard;
    ask, then wait for them to say they're done;
  - **approves installing CyberCraft Tweaks 1.4.0** into the `CyberCraft` instance, with Minecraft stopped (Task 7).
    1.3.0 stays in the mods folder, disabled, so it can be put back;
  - **approves changes to their Minecraft world:** Task 7 runs step 0's script again at x 880, which clears what it
    makes; in Task 16 the overlay's Spawn rows and the named sets make test things ahead of Minecraft's player, and
    Clear removes them, leaving no drops.
- **The game:** builds deploy only with `CP2077_DIR` set, which only Task 16's deploy step sets, with the game closed.
  `mclink` needs the game closed too, since the game would hold the mapping.
- **No faults:** as in Part 1, and on the GPU too. The shader clamps every index it works out from vertex data, since
  root descriptors carry no bounds; draws read only the vertices the CPU put in the snapshot; and nothing the GPU may
  still read is freed before the fence after the last list that reads it.

## File map (Part 2)

| File | Responsibility |
|---|---|
| `minecraft/cybercraft-tweaks/` (`KeepDrawOrder.java`, `cybercraft_tweaks.mixins.json`, `fabric.mod.json`, `README.md`) | CyberCraft Tweaks 1.4.0: what SkyCraft sends of Minecraft's entities, in Minecraft's draw order |
| `tools/mclink/entities.txt` | Step 0's script: creative once Minecraft's loading screen has closed (Task 6), and the sand's block as 26.3 reads it (Task 15) |
| `src/entities/WorldShapes.*` | The world entities' triangles: dropped items and blocks, arrows, tridents and cracks |
| `src/entities/Entities.*` | One frame's geometry from the newest scene and table; their counts, the nearest mob, and the `entities` request's lines |
| `src/render/EntityTextures.*` | Where each entity texture goes in one buffer, 64 MB at most |
| `src/render/Snapshots.*` | Which upload buffer a frame's entities go into, and when one is free again |
| `shaders/blocks.hlsl`, `src/render/Shaders.*`, `src/render/FrameConstants.h` | The entity shaders and their constants |
| `src/render/Renderer.*` | The textures' buffer, the snapshots, three pipelines, and the draws in the G-buffer list |
| `src/link/WorldTable.*`, `tools/mclink/Main.cpp` | `WorldWords`, moved from `mclink` |
| `src/Requests.*`, `src/Status.*` | The `entities` request, and the status's `entities` line |
| `src/Plugin.cpp`, `CMakeLists.txt` | 0.20.0: the sink's textures, scenes and table, each frame's entities, and their status |
| `src/link/Commands.cpp` | `tnt-ahead`'s TNT on obsidian, with a marker |
| `tools/blockrender_hud/init.lua` | The Spawn rows, Clear, and the `Entities` button and line |
| `tests/TestWorldShapes.cpp`, `tests/TestEntities.cpp`, `tests/TestEntityTextures.cpp`, `tests/TestSnapshots.cpp` | One test file per new unit; others gain tests |

---

### Task 6: CyberCraft Tweaks 1.4.0 keeps Minecraft's draw order

Minecraft draws an entity's layers each over the one before at the same depth (the spec's "Step C"): a villager's body,
then its biome's clothes at `order(1)`, its profession's at `order(2)` and its level's badge at `order(3)`; a spider's
eyes at `order(1)` over its body. SkyCraft's `AvatarExporter` answers `order(n)` with itself, and keeps its batches in a
`HashMap` keyed by texture, UV mode and flags, so they go out in an order that follows the textures' ids: step 0 had the
villager's badge first.
- **`KeepDrawOrder`** mixes into `AvatarExporter` by name, as `NoBlastBlocks` mixes into `SkyDigBlast`:
  - its batches become a `TreeMap`, so they go out in their keys' order;
  - `order(n)` hands back a stand-in, a `Proxy` of `OrderedSubmitNodeCollector` that calls the exporter itself, with n
    as the order while each call runs;
  - `batch` puts the order above SkyCraft's key (`order << 40`), so a batch is kept by its order too, and the lowest
    order goes first. SkyCraft makes a batch inside each submit call, so the order is the one in force.
- **The version** goes to 1.4.0, and the README says what it does.
- **Step 0's script** types `/gamemode creative` after the link's `wait-ground` and `wait 2`, once Minecraft's loading
  screen has closed: in step 0, `mclink`'s typer dropped it while the screen was open.

**Files:**
- Create: `minecraft/cybercraft-tweaks/src/main/java/dev/cybercraft/tweaks/mixin/KeepDrawOrder.java`
- Modify: `minecraft/cybercraft-tweaks/src/main/resources/cybercraft_tweaks.mixins.json`,
  `minecraft/cybercraft-tweaks/src/main/resources/fabric.mod.json`, `minecraft/cybercraft-tweaks/README.md`,
  `tools/mclink/entities.txt`

- [x] **Step 1: Write `minecraft/cybercraft-tweaks/src/main/java/dev/cybercraft/tweaks/mixin/KeepDrawOrder.java`**

```java
package dev.cybercraft.tweaks.mixin;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Proxy;
import java.util.HashMap;
import java.util.Map;
import java.util.TreeMap;

import net.minecraft.client.renderer.OrderedSubmitNodeCollector;

import org.spongepowered.asm.mixin.Final;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.Mutable;
import org.spongepowered.asm.mixin.Shadow;
import org.spongepowered.asm.mixin.Unique;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.ModifyVariable;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

/**
 * Minecraft's draw order kept in what SkyCraft sends the game. Minecraft draws an entity's layers in order, each over
 * the one before at the same depth: a villager's body, then its biome's clothes, its profession's and its level's badge
 * (orders 0 to 3); a spider's eyes over its body; a banner's patterns over its cloth. SkyCraft's {@code AvatarExporter}
 * takes every layer as Minecraft submits it, but its {@code order(n)} hands back the collector itself, and it keeps its
 * batches in a HashMap by texture, UV mode and flags, which sends them by the textures' ids: spike 7's step 0 had a
 * villager's badge before its body. Here {@code order(n)} hands back a stand-in that submits through the collector with
 * n as the order, each batch is kept by its order too, and the batches are kept sorted, lowest order first, so the game
 * draws each layer after those under it.
 *
 * <p>It targets SkyCraft 0.1.2's {@code dev.skycraft.client.render.AvatarExporter} by name, as {@link NoBlastBlocks}
 * targets {@code SkyDigBlast}.
 */
@Mixin(targets = "dev.skycraft.client.render.AvatarExporter", remap = false)
public abstract class KeepDrawOrder {
    @Shadow @Final @Mutable private Map<Long, ?> batches;
    /** The order of what's being submitted: 0 unless it comes through an order's stand-in. */
    @Unique private int cybercraft_tweaks$order;
    /** The stand-ins, one for each order asked for. */
    @Unique private Map<Integer, OrderedSubmitNodeCollector> cybercraft_tweaks$standIns;

    @Inject(method = "<init>", at = @At("RETURN"))
    private void cybercraft_tweaks$sortBatches(CallbackInfo info) {
        this.batches = new TreeMap<Long, Object>();  // in key order, and the key starts with the order
        this.cybercraft_tweaks$standIns = new HashMap<>();
    }

    @Inject(method = "order", at = @At("HEAD"), cancellable = true)
    private void cybercraft_tweaks$order(int order, CallbackInfoReturnable<OrderedSubmitNodeCollector> info) {
        info.setReturnValue(this.cybercraft_tweaks$standIns.computeIfAbsent(order, this::cybercraft_tweaks$standIn));
    }

    // SkyCraft's key is the texture's id << 16 | the UV mode << 8 | the flags: the order goes above it, signed, so a
    // layer below 0 comes first.
    @ModifyVariable(method = "batch", at = @At("STORE"), ordinal = 0)
    private long cybercraft_tweaks$keyByOrder(long key) {
        return ((long) this.cybercraft_tweaks$order << 40) + key;
    }

    /** Submits through the collector itself, with aOrder as the order while each call runs. */
    @Unique
    private OrderedSubmitNodeCollector cybercraft_tweaks$standIn(int aOrder) {
        Object collector = this;
        return (OrderedSubmitNodeCollector) Proxy.newProxyInstance(OrderedSubmitNodeCollector.class.getClassLoader(),
            new Class<?>[] { OrderedSubmitNodeCollector.class }, (proxy, method, args) -> {
                int before = this.cybercraft_tweaks$order;
                this.cybercraft_tweaks$order = aOrder;
                try {
                    return method.invoke(collector, args);
                } catch (InvocationTargetException e) {
                    throw e.getCause();
                } finally {
                    this.cybercraft_tweaks$order = before;
                }
            });
    }
}
```

- [x] **Step 2: Edit `minecraft/cybercraft-tweaks/src/main/resources/cybercraft_tweaks.mixins.json` to add it, on the client**

```json
  "mixins": ["NoBlastBlocks", "NoDigging", "SetHealth"],
```
```json
  "mixins": ["NoBlastBlocks", "NoDigging", "SetHealth"],
  "client": ["KeepDrawOrder"],
```

- [x] **Step 3: Edit `minecraft/cybercraft-tweaks/src/main/resources/fabric.mod.json` for 1.4.0**

```json
  "version": "1.3.0",
  "name": "CyberCraft Tweaks",
  "description": "SkyCraft adjusted for Cyberpunk 2077's Night City: nothing digs into the game's ground, so blasts leave no blocks floating above it; and V's health share carries into Minecraft's player as build mode starts.",
```
```json
  "version": "1.4.0",
  "name": "CyberCraft Tweaks",
  "description": "SkyCraft adjusted for Cyberpunk 2077's Night City: nothing digs into the game's ground, so blasts leave no blocks floating above it; V's health share carries into Minecraft's player as build mode starts; and what SkyCraft sends of Minecraft's entities keeps their layers in Minecraft's draw order.",
```

- [x] **Step 4: Edit `minecraft/cybercraft-tweaks/README.md` to say what it does**

```markdown
  max health, and Minecraft's log says so.
```
```markdown
  max health, and Minecraft's log says so.

What SkyCraft sends of Minecraft's entities keeps Minecraft's draw order:
- **An entity's layers come in order** (`KeepDrawOrder`, from 1.4.0). Minecraft draws an entity's layers each over the
  one before at the same depth: a villager's body, then its biome's clothes, its profession's and its level's badge
  (`order` 0 to 3), a spider's eyes over its body, a banner's patterns over its cloth. SkyCraft's `AvatarExporter`
  answers `order(n)` with itself and keeps its batches in a `HashMap`, so they go out in the textures' ids' order:
  spike 7's step 0 had a villager's badge before its body. With the mod, `order(n)` hands back a stand-in that submits
  with n as the order, each batch is kept by its order, and the batches go out lowest order first.
```
```markdown
player's health to half, all and a twentieth of its max, and reads each back with `/data`.
```
```markdown
player's health to half, all and a twentieth of its max, and reads each back with `/data`.
`spikes/block-render/tools/mclink/entities.txt` prints each scene's batches in the order they come: with the mod, a
villager's body comes before its biome's clothes, those before its profession's, and those before its level's badge.
```

- [x] **Step 5: Build CyberCraft Tweaks, and check that SkyCraft's exporter has what the mixin takes**

```bash
cd /d/Cyberpunk-Minecraft/minecraft/cybercraft-tweaks && out=$(bash build.sh 2>&1); echo "$out" | tail -2; \
  echo "warnings: $(echo "$out" | grep -c -i warning)"; \
  unzip -l build/cybercraft-tweaks-1.4.0.jar | grep -o "dev/cybercraft/tweaks/mixin/KeepDrawOrder.class"; \
  javap -p -cp /c/Users/BLOXY/AppData/Roaming/ModrinthApp/profiles/CyberCraft/mods/skycraft-fabric-0.1.2.jar \
    dev.skycraft.client.render.AvatarExporter | grep -E " batches;| batch\(int, int, int\);| order\(int\);"
```
Expected: `built build/cybercraft-tweaks-1.4.0.jar against 26.3-`, then `warnings: 0`,
`dev/cybercraft/tweaks/mixin/KeepDrawOrder.class`, and SkyCraft's
`java.util.Map<java.lang.Long, dev.skycraft.client.render.AvatarExporter$Batch> batches;`,
`AvatarExporter$Batch batch(int, int, int);` and `OrderedSubmitNodeCollector order(int);`.

- [x] **Step 6: Edit `tools/mclink/entities.txt` to type creative once the loading screen has closed**

```text
type /gamemode creative
wait-ground 30
wait 2
```
```text
wait-ground 30
wait 2
# Creative once Minecraft's loading screen has closed: the typer drops a command while a screen is open.
type /gamemode creative
```

- [x] **Step 7: Build and run the tests; they must pass**

The tests parse step 0's script.
```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `465 test(s), 0 failure(s)`.

- [x] **Step 8: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add minecraft/cybercraft-tweaks spikes/block-render \
  && git commit -q -m "minecraft-entities: CyberCraft Tweaks 1.4.0 keeps Minecraft's draw order in what SkyCraft sends of its entities, and step 0's script types creative once the loading screen has closed" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 7: In Minecraft: step C's draw order

Step C's draw order, with the user: step 0's script again, against SkyCraft's unchanged mod with CyberCraft Tweaks 1.4.0,
in the `CyberCraft` instance, without the game. Nothing else changes from step 0, so its scenes should be step 0's, their
batches in Minecraft's draw order.
- **Installing:** with Minecraft stopped, 1.3.0 is renamed `.jar.disabled`, as 1.2.0 was, and 1.4.0 copied in. A jar
  can't be renamed while Minecraft runs, so the rename failing means it's still running.
- **If Minecraft doesn't start, or crashes as it opens its world,** its log says why: a mixin that can't apply names
  `KeepDrawOrder`. Then put 1.3.0 back (rename 1.4.0's jar `.jar.disabled` and 1.3.0's back to `.jar`), and stop.
- **If a check fails,** stop and find the cause with superpowers:systematic-debugging before going on. Fixes become new
  tasks.

**Files:**
- Modify: `docs/spikes/2026-10-06-minecraft-entities-results.md`

- [x] **Step 1: Check that the game isn't running**

```bash
tasklist | grep -i -c "cyberpunk2077.exe"
```
Expected: `0`. If it's `1`, ask the user to quit the game: it would hold the mapping.

- [x] **Step 2: Ask the user to stop Minecraft, and for their leave**

Ask the user for their leave to install CyberCraft Tweaks 1.4.0 into the `CyberCraft` instance, keeping 1.3.0 disabled,
and to run step 0's script again at x 880 in the `SkyCraft` world, which clears what it makes; and, if Minecraft is
running, to stop it from the Modrinth App. Wait for both.

- [x] **Step 3: Install CyberCraft Tweaks 1.4.0**

```bash
M=/c/Users/BLOXY/AppData/Roaming/ModrinthApp/profiles/CyberCraft/mods
mv "$M/cybercraft-tweaks-1.3.0.jar" "$M/cybercraft-tweaks-1.3.0.jar.disabled" \
  && cp /d/Cyberpunk-Minecraft/minecraft/cybercraft-tweaks/build/cybercraft-tweaks-1.4.0.jar "$M/" && ls "$M"
```
Expected: `cybercraft-tweaks-1.2.0.jar.disabled`, `cybercraft-tweaks-1.3.0.jar.disabled`, `cybercraft-tweaks-1.4.0.jar`,
`fabric-api-0.161.0+26.3.jar` and `skycraft-fabric-0.1.2.jar`. If the rename fails, Minecraft is still running.

- [x] **Step 4: Ask the user to start Minecraft**

The user starts the `CyberCraft` instance from the Modrinth App, and says when its title screen shows. Then:
```bash
L="/c/Users/BLOXY/AppData/Roaming/ModrinthApp/profiles/CyberCraft/logs/latest.log"
grep -m1 "cybercraft_tweaks" "$L"; grep "SkyCraft:" "$L" | tail -2; echo "mixin errors: $(grep -c -i -E "mixin.*(error|fail)" "$L")"
```
Expected: `cybercraft_tweaks 1.4.0` in Fabric's list of mods, `SkyCraft: can't open Skyrim's shared memory yet (Windows
error 2: …)`, and `mixin errors: 0`.

- [x] **Step 5: Run step 0's script**

```bash
L="/c/Users/BLOXY/AppData/Roaming/ModrinthApp/profiles/CyberCraft/logs/latest.log"; before=$(wc -l < "$L"); \
  cd /d/Cyberpunk-Minecraft/spikes/block-render && mkdir -p build/stepc \
  && ./build/RelWithDebInfo/mclink.exe tools/mclink/entities.txt | tee build/stepc/entities.log; echo "exit: ${PIPESTATUS[0]}"; \
  tail -n +$((before + 1)) "$L" | grep -E "SkyCraft: (sent texture|texture)|\[CHAT\]|[Mm]ixin" | tee build/stepc/minecraft.log
```
Give it 6 minutes, as in step 0. Expected, against step 0's results:
- **Linking, the textures and the scenes as in step 0:** the same textures, most likely with the same ids (Minecraft's
  lines name each), and each scene with the same batches and vertex counts, give or take what moves.
- **The batches in Minecraft's draw order:** in every scene, the villager's body (`villager.png`) before its biome's
  (`type/plains.png`), that before its profession's (`profession/farmer.png`), and that before its level's
  (`profession_level/iron.png`); the spider's body (`spider.png`) before its eyes (`spider_eyes.png`). With step 0's
  ids, the first scene's batches are textures 4, 5, 6, 7 and 8, in that order.
- **Creative typed:** a `[CHAT]` line `Set own game mode to Creative Mode`.
- **No mixin line,** no `[CHAT]` line with an error, the last scene empty and its world line `nothing`, and `exit: 0`.

- [x] **Step 6: Ask the user to stop Minecraft**

Its window is hidden while `mclink` drives it, so the user stops it from the Modrinth App.

- [x] **Step 7: Record step C's draw order in `docs/spikes/2026-10-06-minecraft-entities-results.md`**

A "Step C's draw order, with `mclink`" section after step 0's: the versions (CyberCraft Tweaks 1.4.0); each scene's
batch order beside step 0's, the villager's and the spider's layers named; anything else that differs from step 0, and
creative typed; and the verdict.

- [x] **Step 8: Commit**

With the plan, its steps ticked up to this one.
```bash
cd /d/Cyberpunk-Minecraft && git add docs/spikes/2026-10-06-minecraft-entities-results.md docs/superpowers/plans/2026-10-06-minecraft-entities.md \
  && git commit -q -m "minecraft-entities: step C's draw order with mclink" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 8: The world entities' shapes

SkyCraft's world-entity table holds the small things it draws itself in Skyrim (the spec's "Entities: one frame's
geometry"): dropped items and blocks, arrows, tridents, cracks, and its contact shadows. `WorldShapes` makes their
triangles as SkyCraft's own game side does (`WorldRender.cpp`'s `Quad`, `Box` and `BuildArrow`), at Minecraft's size,
relative to the frame's origin, in Minecraft axes, with UVs into Minecraft's combined atlas:
- **a dropped item:** one quad of its icon, `scale` wide, turned by `yaw` about the vertical, centred on the record;
- **a dropped block:** a cube of side `scale` about the record, turned by `yaw`, its sides, top and bottom with their
  rectangles, the top tinted;
- **an arrow:** Minecraft's `ArrowModel`, two fins crossed at 45° and a back plate, along `(sin yaw cos pitch, sin
  pitch, cos yaw cos pitch)`, a block per 16 of its units scaled by 0.05625, not SkyCraft's 0.55 for Skyrim's people;
- **a trident:** its icon on two crossed quads, the diagonal along its flight;
- **a crack:** its box's six faces with the stage's texture, kept apart (`AppendCrack`), since cracks get their own
  blend;
- **a shadow, or a kind it doesn't know:** nothing (contact shadows wait for run A, the spec's check 6).

Every vertex is cut out, without mipmaps, lit by its triangle's own normal, with sky light 15 and block light 0, as
SkyCraft's are, so none of them glows.

**Files:**
- Create: `src/entities/WorldShapes.h`, `src/entities/WorldShapes.cpp`, `tests/TestWorldShapes.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the failing test, `tests/TestWorldShapes.cpp`**

```cpp
#include "Harness.h"

#include "entities/WorldShapes.h"

#include <cmath>

using namespace cpmc;
using namespace cpmc::entities;

namespace
{
const Vec3d kOrigin{ 880.0, 6.0, 2007.0 };

// A record of aKind at (aX, aY, aZ) from kOrigin, its atlas rectangles {0.1, 0.2, 0.3, 0.4} and on.
proto::WorldEntity Record(uint32_t aKind, float aX, float aY, float aZ)
{
	proto::WorldEntity record{};
	record.kind = aKind;
	record.x = float(kOrigin.x) + aX;
	record.y = float(kOrigin.y) + aY;
	record.z = float(kOrigin.z) + aZ;
	for (int r = 0; r < 3; ++r)
	{
		for (int k = 0; k < 4; ++k)
		{
			record.uv[r][k] = 0.1f * float(r * 4 + k + 1);
		}
	}
	return record;
}

void CheckAt(const Vertex& aVertex, float aX, float aY, float aZ)
{
	CHECK_NEAR(aVertex.x, aX, 1e-4);
	CHECK_NEAR(aVertex.y, aY, 1e-4);
	CHECK_NEAR(aVertex.z, aZ, 1e-4);
}
}

TEST(WorldShapesItemTurnsAboutTheVertical)
{
	proto::WorldEntity item = Record(proto::kWeItem, 3.5f, 0.75f, 0.5f);
	item.yaw = 90.0f;
	item.scale = 0.5f;
	std::vector<Vertex> vertices;
	AppendShape(item, kOrigin, vertices);
	CHECK_EQ(vertices.size(), size_t(6));
	if (vertices.size() != 6)
	{
		return;
	}
	// At yaw 90 the icon faces east: it spans z, 0.5 wide and tall, centred on the record.
	CheckAt(vertices[0], 3.5f, 1.0f, 0.25f);  // top left
	CheckAt(vertices[1], 3.5f, 1.0f, 0.75f);  // top right
	CheckAt(vertices[2], 3.5f, 0.5f, 0.75f);  // bottom right
	CheckAt(vertices[5], 3.5f, 0.5f, 0.25f);  // bottom left
	CHECK_NEAR(vertices[0].u, 0.1f, 1e-6);
	CHECK_NEAR(vertices[0].v, 0.2f, 1e-6);
	CHECK_NEAR(vertices[2].u, 0.3f, 1e-6);
	CHECK_NEAR(vertices[2].v, 0.4f, 1e-6);
	for (const Vertex& vertex : vertices)
	{
		CHECK_EQ(vertex.flags, kShapeFlags);
		CHECK_EQ(vertex.light, kShapeLight);
		CHECK_EQ(int(vertex.r) + vertex.g + vertex.b + vertex.a, 4 * 255);
	}
}

TEST(WorldShapesBlockIsACubeWithItsTopTinted)
{
	proto::WorldEntity block = Record(proto::kWeBlock, 1.0f, 2.0f, 3.0f);
	block.scale = 0.25f;
	block.tint = 0xFF3C9B5Bu;  // red 0x5B, green 0x9B, blue 0x3C: a grass top
	std::vector<Vertex> vertices;
	AppendShape(block, kOrigin, vertices);
	CHECK_EQ(vertices.size(), size_t(36));
	if (vertices.size() != 36)
	{
		return;
	}
	// The top, the fifth face: corners (-x, -z), (+x, -z), (+x, +z), (-x, +z) at the top, 0.125 above the centre.
	CheckAt(vertices[24], 0.875f, 2.125f, 2.875f);
	CheckAt(vertices[25], 1.125f, 2.125f, 2.875f);
	CheckAt(vertices[26], 1.125f, 2.125f, 3.125f);
	CheckAt(vertices[29], 0.875f, 2.125f, 3.125f);
	CHECK_NEAR(vertices[24].u, 0.5f, 1e-6);  // the top's rectangle, uv[1]
	CHECK_EQ(int(vertices[24].r), 0x5B);
	CHECK_EQ(int(vertices[24].g), 0x9B);
	CHECK_EQ(int(vertices[24].b), 0x3C);
	CHECK_EQ(int(vertices[0].g), 255);  // the sides are white
	CHECK_NEAR(vertices[0].u, 0.1f, 1e-6);  // with uv[0]
	CheckAt(vertices[30], 0.875f, 1.875f, 3.125f);  // the bottom, with uv[2]
	CHECK_NEAR(vertices[30].u, 0.9f, 1e-6);
	CHECK_EQ(int(vertices[30].g), 255);
}

TEST(WorldShapesArrowFliesAlongItsYawAndPitch)
{
	proto::WorldEntity arrow = Record(proto::kWeArrow, 0.0f, 0.0f, 0.0f);
	arrow.yaw = 180.0f;  // north
	arrow.pitch = 30.0f;  // and up
	std::vector<Vertex> vertices;
	AppendShape(arrow, kOrigin, vertices);
	CHECK_EQ(vertices.size(), size_t(18));  // two fins and the back plate
	if (vertices.size() != 18)
	{
		return;
	}
	const float k = kArrowUnit;
	const float dy = 0.5f;
	const float dz = -std::sqrt(3.0f) / 2.0f;
	// The first fin: its head end 4 units ahead along the flight, its fletching 12 behind, 4 wide.
	CheckAt({ (vertices[1].x + vertices[2].x) / 2, (vertices[1].y + vertices[2].y) / 2, (vertices[1].z + vertices[2].z) / 2 }, 0.0f,
	        4 * k * dy, 4 * k * dz);
	CheckAt({ (vertices[0].x + vertices[5].x) / 2, (vertices[0].y + vertices[5].y) / 2, (vertices[0].z + vertices[5].z) / 2 }, 0.0f,
	        -12 * k * dy, -12 * k * dz);
	CHECK_NEAR(std::hypot(vertices[2].x - vertices[1].x, vertices[2].y - vertices[1].y, vertices[2].z - vertices[1].z), 4 * k, 1e-5);
	CHECK_NEAR(vertices[0].u, 0.1f, 1e-6);  // the fins, the side view
	// The back plate, 11 units behind, its rectangle uv[1].
	CheckAt({ (vertices[12].x + vertices[13].x + vertices[14].x + vertices[17].x) / 4,
	          (vertices[12].y + vertices[13].y + vertices[14].y + vertices[17].y) / 4,
	          (vertices[12].z + vertices[13].z + vertices[14].z + vertices[17].z) / 4 },
	        0.0f, -11 * k * dy, -11 * k * dz);
	CHECK_NEAR(vertices[12].u, 0.5f, 1e-6);
}

TEST(WorldShapesTridentLiesAlongItsFlight)
{
	proto::WorldEntity trident = Record(proto::kWeTrident, 0.0f, 0.0f, 0.0f);
	trident.yaw = 90.0f;  // east, level
	std::vector<Vertex> vertices;
	AppendShape(trident, kOrigin, vertices);
	CHECK_EQ(vertices.size(), size_t(12));  // its icon on two crossed quads
	if (vertices.size() != 12)
	{
		return;
	}
	CheckAt(vertices[1], kTridentHalf, 0.0f, 0.0f);   // the tip, its icon's top right, ahead
	CheckAt(vertices[5], -kTridentHalf, 0.0f, 0.0f);  // the handle, its bottom left, behind
	CHECK_NEAR(vertices[1].u, 0.3f, 1e-6);
}

TEST(WorldShapesCrackCoversItsBox)
{
	proto::WorldEntity crack = Record(proto::kWeCrack, 5.0f, 1.0f, 7.0f);
	crack.ext[0] = 1.008f;
	crack.ext[1] = 0.508f;
	crack.ext[2] = 1.008f;
	std::vector<Vertex> vertices;
	AppendShape(crack, kOrigin, vertices);
	CHECK(vertices.empty());  // a crack isn't a shape
	AppendCrack(crack, kOrigin, vertices);
	CHECK_EQ(vertices.size(), size_t(36));
	if (vertices.size() != 36)
	{
		return;
	}
	CheckAt(vertices[24], 5.0f, 1.508f, 7.0f);  // the top, from the box's low corner
	CheckAt(vertices[26], 6.008f, 1.508f, 8.008f);
	for (const Vertex& vertex : vertices)
	{
		CHECK(vertex.u == crack.uv[0][0] || vertex.u == crack.uv[0][2]);  // every face the stage's, uv[0]
	}
}

TEST(WorldShapesShadowsAndOthersAddNothing)
{
	std::vector<Vertex> vertices;
	AppendShape(Record(proto::kWeShadow, 0.0f, 0.0f, 0.0f), kOrigin, vertices);
	AppendShape(Record(9, 0.0f, 0.0f, 0.0f), kOrigin, vertices);
	AppendCrack(Record(proto::kWeItem, 0.0f, 0.0f, 0.0f), kOrigin, vertices);
	CHECK(vertices.empty());
}
```

- [ ] **Step 2: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestWorldTable.cpp
	tools/mclink/DumpObj.cpp
```
```cmake
	tests/TestWorldTable.cpp
	tests/TestWorldShapes.cpp
	tools/mclink/DumpObj.cpp
```

- [ ] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'entities/WorldShapes.h'`.

- [ ] **Step 4: Write `src/entities/WorldShapes.h`**

```cpp
#pragma once

#include "Coords.h"
#include "world/Vertex.h"

#include <skycraft_protocol.h>

#include <cstdint>
#include <vector>

// Spike 7: Minecraft's world entities as triangles (the spec's "Entities: one frame's geometry"), shaped as SkyCraft's
// own game side shapes them (WorldRender.cpp's BuildEntities), at Minecraft's size: a dropped item's icon, a dropped
// block's spinning cube, an arrow's fins and back plate, a trident's icon on two crossed quads, and the cracks over a
// block being mined. Positions are relative to the frame's origin, a whole block, in Minecraft axes; UVs are into
// Minecraft's combined atlas. Pure.
namespace cpmc::entities
{
namespace proto = skycraft::proto;

// Every shape's vertices: cut-out, without mipmaps, lit by their triangle's own normal, with sky light 15 and block
// light 0, so none of them glows, as SkyCraft's are.
constexpr uint32_t kShapeFlags = kFlagAlphaTested | (1u << 3) | (7u << 4);
constexpr uint32_t kShapeLight = 15u << 8;

// A block per unit of Minecraft's ArrowModel, which ArrowRenderer scales by 0.05625. SkyCraft's own game side draws
// arrows at 0.55 of it, to suit Skyrim's people; Night City's are Minecraft's height.
constexpr float kArrowUnit = 0.9f / 16.0f;
constexpr float kTridentHalf = 0.9f;  // a trident's icon: its corners this far out along and across its flight

// The record's triangles, relative to aOrigin, appended to aOut: an item, a block, an arrow or a trident. A crack, a
// shadow or a kind it doesn't know adds none.
void AppendShape(const proto::WorldEntity& aRecord, const Vec3d& aOrigin, std::vector<Vertex>& aOut);

// A crack's box, its six faces with the stage's texture, relative to aOrigin, appended to aOut. Any other kind adds
// none.
void AppendCrack(const proto::WorldEntity& aRecord, const Vec3d& aOrigin, std::vector<Vertex>& aOut);
}
```

- [ ] **Step 5: Write `src/entities/WorldShapes.cpp`**

```cpp
#include "entities/WorldShapes.h"

#include <array>
#include <cmath>
#include <numbers>

namespace cpmc::entities
{
namespace
{
using Point = std::array<float, 3>;

constexpr float    kRadians = std::numbers::pi_v<float> / 180.0f;
constexpr uint32_t kWhite = 0xFFFFFFFFu;

Vertex At(const Point& aPoint, float aU, float aV, uint32_t aRgba)
{
	Vertex vertex;
	vertex.x = aPoint[0];
	vertex.y = aPoint[1];
	vertex.z = aPoint[2];
	vertex.u = aU;
	vertex.v = aV;
	vertex.r = uint8_t(aRgba & 0xFF);
	vertex.g = uint8_t((aRgba >> 8) & 0xFF);
	vertex.b = uint8_t((aRgba >> 16) & 0xFF);
	vertex.a = uint8_t(aRgba >> 24);
	vertex.light = kShapeLight;
	vertex.flags = kShapeFlags;
	return vertex;
}

// Two triangles over four corners, top left, top right, bottom right and bottom left as they're seen, with the atlas
// rectangle aUv {u0, v0, u1, v1}: WorldRender.cpp's Quad.
void Quad(const std::array<Point, 4>& aCorners, const float aUv[4], uint32_t aRgba, std::vector<Vertex>& aOut)
{
	const float uv[4][2] = { { aUv[0], aUv[1] }, { aUv[2], aUv[1] }, { aUv[2], aUv[3] }, { aUv[0], aUv[3] } };
	for (const int k : { 0, 1, 2, 0, 2, 3 })
	{
		aOut.push_back(At(aCorners[k], uv[k][0], uv[k][1], aRgba));
	}
}

// A box from aMin of aSize, turned aYaw (radians) about its vertical centre line: its sides aSide, its top aTop tinted
// aTopTint (RGBA8, red in the low byte; 0 for none) and its bottom aBottom. WorldRender.cpp's Box.
void Box(const Point& aMin, const Point& aSize, float aYaw, const float aSide[4], const float aTop[4], const float aBottom[4],
         uint32_t aTopTint, std::vector<Vertex>& aOut)
{
	const float cx = aMin[0] + aSize[0] * 0.5f;
	const float cz = aMin[2] + aSize[2] * 0.5f;
	const float c = std::cos(aYaw);
	const float s = std::sin(aYaw);
	const auto  corner = [&](int aIndex) {
        const float lx = ((aIndex & 1) ? 0.5f : -0.5f) * aSize[0];
        const float lz = ((aIndex & 4) ? 0.5f : -0.5f) * aSize[2];
        return Point{ cx + lx * c - lz * s, aMin[1] + ((aIndex & 2) ? aSize[1] : 0.0f), cz + lx * s + lz * c };
	};
	// The corners' bits: 1 is +x, 2 is +y, 4 is +z. Each face's top left, top right, bottom right and bottom left, seen
	// from outside: south (+z), north, east (+x), west, top and bottom.
	static constexpr int kFaces[6][4] = { { 6, 7, 5, 4 }, { 3, 2, 0, 1 }, { 7, 3, 1, 5 }, { 2, 6, 4, 0 }, { 2, 3, 7, 6 }, { 4, 5, 1, 0 } };
	for (int face = 0; face < 6; ++face)
	{
		const std::array<Point, 4> corners{ corner(kFaces[face][0]), corner(kFaces[face][1]), corner(kFaces[face][2]), corner(kFaces[face][3]) };
		const float*               uv = face == 4 ? aTop : face == 5 ? aBottom : aSide;
		Quad(corners, uv, face == 4 && aTopTint != 0 ? 0xFF000000u | aTopTint : kWhite, aOut);
	}
}

// Minecraft's arrow, or a trident, at aAt flying along aDir (unit length, Minecraft axes): WorldRender.cpp's BuildArrow,
// at Minecraft's size. Around the flight, aSide is level, and up the cross of it and the flight; the fins sit at 45
// degrees between them.
void Arrow(const Point& aAt, const Point& aDir, const float aUvSide[4], const float aUvBack[4], bool aTrident, std::vector<Vertex>& aOut)
{
	Point side{ aDir[2], 0.0f, -aDir[0] };
	float length = std::sqrt(side[0] * side[0] + side[2] * side[2]);
	if (length < 1e-3f)
	{
		side = { 1.0f, 0.0f, 0.0f };  // flying straight up or down
		length = 1.0f;
	}
	side[0] /= length;
	side[2] /= length;
	const Point     up{ side[1] * aDir[2] - side[2] * aDir[1], side[2] * aDir[0] - side[0] * aDir[2], side[0] * aDir[1] - side[1] * aDir[0] };
	constexpr float r = 0.70710678f;
	const Point     fins[2] = { { (up[0] + side[0]) * r, (up[1] + side[1]) * r, (up[2] + side[2]) * r },
		                        { (up[0] - side[0]) * r, (up[1] - side[1]) * r, (up[2] - side[2]) * r } };
	// aAlong the flight, aAcross aFin, and aAcross2 aFin2 too, if there is one.
	const auto point = [&](float aAlong, const Point& aFin, float aAcross, const Point* aFin2 = nullptr, float aAcross2 = 0.0f) {
		Point p;
		for (int k = 0; k < 3; ++k)
		{
			p[k] = aAt[k] + aDir[k] * aAlong + aFin[k] * aAcross + (aFin2 ? (*aFin2)[k] * aAcross2 : 0.0f);
		}
		return p;
	};
	if (aTrident)
	{
		// The item icon, its diagonal running from the handle (bottom left) to the tip (top right) along the flight.
		constexpr float h = kTridentHalf;
		for (const Point& fin : fins)
		{
			Quad({ point(0.0f, fin, h), point(h, fin, 0.0f), point(0.0f, fin, -h), point(-h, fin, 0.0f) }, aUvSide, kWhite, aOut);
		}
		return;
	}
	// Minecraft's ArrowModel: two fins 16 long and 4 wide from -12 (the fletching) to +4 (the head), and a 4 x 4 back
	// plate at -11.
	constexpr float k = kArrowUnit;
	for (const Point& fin : fins)
	{
		Quad({ point(-12 * k, fin, -2 * k), point(4 * k, fin, -2 * k), point(4 * k, fin, 2 * k), point(-12 * k, fin, 2 * k) }, aUvSide, kWhite, aOut);
	}
	Quad({ point(-11 * k, fins[0], -2 * k, &fins[1], -2 * k), point(-11 * k, fins[0], 2 * k, &fins[1], -2 * k),
	       point(-11 * k, fins[0], 2 * k, &fins[1], 2 * k), point(-11 * k, fins[0], -2 * k, &fins[1], 2 * k) },
	     aUvBack, kWhite, aOut);
}

Point Relative(const proto::WorldEntity& aRecord, const Vec3d& aOrigin)
{
	return { float(aRecord.x - aOrigin.x), float(aRecord.y - aOrigin.y), float(aRecord.z - aOrigin.z) };
}
}

void AppendShape(const proto::WorldEntity& aRecord, const Vec3d& aOrigin, std::vector<Vertex>& aOut)
{
	const Point at = Relative(aRecord, aOrigin);
	switch (aRecord.kind)
	{
	case proto::kWeItem:
	{
		// A flat icon turning about the vertical, centred on the record.
		const float spin = aRecord.yaw * kRadians;
		const float half = aRecord.scale * 0.5f;
		const float rx = std::cos(spin) * half;
		const float rz = std::sin(spin) * half;
		Quad({ Point{ at[0] - rx, at[1] + half, at[2] - rz }, Point{ at[0] + rx, at[1] + half, at[2] + rz },
		       Point{ at[0] + rx, at[1] - half, at[2] + rz }, Point{ at[0] - rx, at[1] - half, at[2] - rz } },
		     aRecord.uv[0], kWhite, aOut);
		break;
	}
	case proto::kWeBlock:
	{
		// A small cube spinning about its centre, the record.
		const float s = aRecord.scale;
		Box({ at[0] - s * 0.5f, at[1] - s * 0.5f, at[2] - s * 0.5f }, { s, s, s }, aRecord.yaw * kRadians, aRecord.uv[0], aRecord.uv[1],
		    aRecord.uv[2], aRecord.tint, aOut);
		break;
	}
	case proto::kWeArrow:
	case proto::kWeTrident:
	{
		// Arrows face (sin yaw cos pitch, sin pitch, cos yaw cos pitch): Minecraft sets their yaw and pitch from their flight.
		const float yaw = aRecord.yaw * kRadians;
		const float pitch = aRecord.pitch * kRadians;
		const Point dir{ std::sin(yaw) * std::cos(pitch), std::sin(pitch), std::cos(yaw) * std::cos(pitch) };
		Arrow(at, dir, aRecord.uv[0], aRecord.uv[1], aRecord.kind == proto::kWeTrident, aOut);
		break;
	}
	default: break;
	}
}

void AppendCrack(const proto::WorldEntity& aRecord, const Vec3d& aOrigin, std::vector<Vertex>& aOut)
{
	if (aRecord.kind == proto::kWeCrack)
	{
		Box(Relative(aRecord, aOrigin), { aRecord.ext[0], aRecord.ext[1], aRecord.ext[2] }, 0.0f, aRecord.uv[0], aRecord.uv[0], aRecord.uv[0], 0,
		    aOut);
	}
}
}
```

- [ ] **Step 6: Edit `CMakeLists.txt` to add the source**

```cmake
	src/link/WorldTable.cpp
	src/physx/GameAbi.cpp
```
```cmake
	src/link/WorldTable.cpp
	src/entities/WorldShapes.cpp
	src/physx/GameAbi.cpp
```

- [ ] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `471 test(s), 0 failure(s)`.

- [ ] **Step 8: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "minecraft-entities: the world entities' shapes, as SkyCraft's own game side builds them, at Minecraft's size" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 9: One frame's entities

`Entities` keeps what the link hands on, on the main thread, and makes each frame's geometry from it (the spec's
"Entities: one frame's geometry"). It calls no GPU, so every rule is tested here:
- **the newest scene,** copied: its origin, vertices and batches. A scene of no batches is SkyCraft's one empty scene,
  sent once the last thing has gone, so it clears what's kept; so do Minecraft's clear, and Minecraft out of its world
  (`InWorld(false)`), since SkyCraft stops sending then;
- **the newest world-entity table,** as the link hands it on each update (empty when Minecraft isn't in its world);
- **a frame** (`EntityFrame`): the scene's vertices as they came, then the world entities' shapes, then their cracks;
  and its draws, each a run of those vertices with one texture (0 for Minecraft's combined atlas) and a kind: the
  scene's batches in the order SkyCraft sent them, `Model` or, flagged translucent, `Translucent` (step B's), then one
  `Model` draw of every shape, then one `Crack` draw. Its origin is the scene's, a whole block; with no scene, the whole
  block under Minecraft's eye;
- **the counts** the status shows: the newest scene's batches and vertices, those with block light of at least
  `glowlight`, and those in translucent batches; scenes and their bytes since the start;
- **the nearest mob:** the nearest of the table's shadows, which SkyCraft puts under players' and mobs' feet.

**Files:**
- Create: `src/entities/Entities.h`, `src/entities/Entities.cpp`, `tests/TestEntities.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the failing test, `tests/TestEntities.cpp`**

```cpp
#include "Harness.h"

#include "entities/Entities.h"
#include "entities/WorldShapes.h"

using namespace cpmc;
using namespace cpmc::entities;

namespace
{
namespace proto = skycraft::proto;

// aCount vertices, the i-th at x = i with block light aLight.
std::vector<Vertex> Vertices(uint32_t aCount, uint32_t aLight = 0)
{
	std::vector<Vertex> vertices(aCount);
	for (uint32_t i = 0; i < aCount; ++i)
	{
		vertices[i].x = float(i);
		vertices[i].light = aLight | (15u << 8);
	}
	return vertices;
}

// A scene from aOrigin over aVertices, which must outlive it, as ParseScene makes one.
link::SceneMessage Scene(const std::vector<Vertex>& aVertices, std::vector<link::SceneBatch> aBatches, const Vec3d& aOrigin)
{
	link::SceneMessage scene;
	scene.origin = aOrigin;
	scene.batches = std::move(aBatches);
	scene.vertexCount = uint32_t(aVertices.size());
	scene.vertices = { reinterpret_cast<const uint8_t*>(aVertices.data()), aVertices.size() * sizeof(Vertex) };
	return scene;
}

proto::WorldEntity Record(uint32_t aKind, float aX, float aY, float aZ)
{
	proto::WorldEntity record{};
	record.kind = aKind;
	record.x = aX;
	record.y = aY;
	record.z = aZ;
	record.scale = 0.5f;
	record.ext[0] = record.ext[1] = record.ext[2] = 1.008f;
	return record;
}

const Vec3d kOrigin{ 880.0, 7.0, 2007.0 };
const Vec3d kEye{ 880.5, 7.76, 2007.5 };
}

TEST(EntitiesDrawTheSceneInItsOrder)
{
	const std::vector<Vertex> vertices = Vertices(12);
	Entities                  entities;
	entities.Scene(Scene(vertices, { { 5, 0, 6, 0 }, { 12, 6, 3, 1 }, { 0, 9, 3, 0 } }, kOrigin));
	const EntityFrame frame = entities.Frame(kEye);
	CHECK_EQ(frame.origin.x, 880.0);
	CHECK_EQ(frame.origin.y, 7.0);
	CHECK_EQ(frame.vertices.size(), size_t(12));
	CHECK_EQ(frame.draws.size(), size_t(3));
	if (frame.draws.size() != 3 || frame.vertices.size() != 12)
	{
		return;
	}
	CHECK_EQ(frame.vertices[7].x, 7.0f);
	CHECK_EQ(frame.draws[0].texture, 5u);
	CHECK(frame.draws[0].kind == EntityDraw::Kind::Model);
	CHECK_EQ(frame.draws[1].first, 6u);
	CHECK_EQ(frame.draws[1].count, 3u);
	CHECK(frame.draws[1].kind == EntityDraw::Kind::Translucent);  // flagged translucent: step B's
	CHECK_EQ(frame.draws[2].texture, 0u);  // the atlas: a falling block, say
	CHECK(frame.draws[2].kind == EntityDraw::Kind::Model);
}

TEST(EntitiesAddTheWorldEntitiesAfterTheScene)
{
	const std::vector<Vertex> vertices = Vertices(3);
	Entities                  entities;
	entities.Scene(Scene(vertices, { { 4, 0, 3, 0 } }, kOrigin));
	link::WorldTable table;
	table.entities = { Record(proto::kWeItem, 883.5f, 7.75f, 2007.5f), Record(proto::kWeCrack, 885.0f, 6.0f, 2010.0f),
		               Record(proto::kWeShadow, 885.5f, 6.12f, 2007.5f), Record(proto::kWeArrow, 890.0f, 7.0f, 2007.0f) };
	entities.World(table);
	const EntityFrame frame = entities.Frame(kEye);
	// The scene's draw, then the item's and the arrow's shapes in one draw, then the crack's: the shadow isn't drawn.
	CHECK_EQ(frame.draws.size(), size_t(3));
	CHECK_EQ(frame.vertices.size(), size_t(3 + 6 + 18 + 36));
	if (frame.draws.size() != 3 || frame.vertices.size() != 63)
	{
		return;
	}
	CHECK_EQ(frame.draws[1].first, 3u);
	CHECK_EQ(frame.draws[1].count, 24u);
	CHECK_EQ(frame.draws[1].texture, 0u);
	CHECK(frame.draws[1].kind == EntityDraw::Kind::Model);
	CHECK_EQ(frame.draws[2].first, 27u);
	CHECK_EQ(frame.draws[2].count, 36u);
	CHECK(frame.draws[2].kind == EntityDraw::Kind::Crack);
	// Relative to the scene's origin: the item's centre 3.5 east and 0.75 up of it.
	CHECK_NEAR((frame.vertices[3].y + frame.vertices[5].y) / 2.0f, 0.75f, 1e-4);
	CHECK_EQ(frame.vertices[3].flags, kShapeFlags);
}

TEST(EntitiesWithoutASceneUseMinecraftsEye)
{
	Entities         entities;
	link::WorldTable table;
	table.entities = { Record(proto::kWeItem, 883.5f, 7.75f, 2007.5f) };
	entities.World(table);
	const EntityFrame frame = entities.Frame(kEye);
	CHECK_EQ(frame.origin.x, 880.0);  // the whole block of Minecraft's eye
	CHECK_EQ(frame.origin.y, 7.0);
	CHECK_EQ(frame.origin.z, 2007.0);
	CHECK_EQ(frame.draws.size(), size_t(1));
	CHECK_EQ(frame.vertices.size(), size_t(6));
}

TEST(EntitiesEmptySceneMeansNothingIsThere)
{
	const std::vector<Vertex> vertices = Vertices(3);
	Entities                  entities;
	entities.Scene(Scene(vertices, { { 4, 0, 3, 0 } }, kOrigin));
	entities.Scene(Scene({}, {}, { 0.0, 0.0, 0.0 }));  // SkyCraft's one empty scene, origin 0
	const EntityFrame frame = entities.Frame({ 100.2, 64.5, -3.5 });
	CHECK(frame.draws.empty());
	CHECK(frame.vertices.empty());
	CHECK_EQ(frame.origin.x, 100.0);  // from the eye, not the empty scene's 0
	CHECK_EQ(frame.origin.z, -4.0);
	CHECK_EQ(entities.Scenes(), 2ull);
}

TEST(EntitiesDropTheSceneAtAClearAndOutOfTheWorld)
{
	const std::vector<Vertex> vertices = Vertices(3);
	Entities                  entities;
	entities.Scene(Scene(vertices, { { 4, 0, 3, 0 } }, kOrigin));
	entities.InWorld(true);
	CHECK_EQ(entities.Frame(kEye).draws.size(), size_t(1));
	entities.Clear();
	CHECK(entities.Frame(kEye).draws.empty());
	entities.Scene(Scene(vertices, { { 4, 0, 3, 0 } }, kOrigin));
	entities.InWorld(false);
	CHECK(entities.Frame(kEye).draws.empty());
}

TEST(EntitiesCountTheNewestScene)
{
	std::vector<Vertex> vertices = Vertices(9, 0);
	vertices[0].light = 15 | (15u << 8);  // block light 15: a blaze, say
	vertices[1].light = 15 | (15u << 8);
	vertices[2].light = 14 | (15u << 8);  // beside a torch
	Entities entities;
	entities.Scene(Scene(vertices, { { 11, 0, 3, 0 }, { 12, 3, 6, 1 } }, kOrigin));
	const Entities::SceneCounts at15 = entities.Counts(15);
	CHECK_EQ(at15.batches, size_t(2));
	CHECK_EQ(at15.vertices, 9u);
	CHECK_EQ(at15.glowing, 2u);
	CHECK_EQ(at15.translucent, 6u);
	CHECK_EQ(entities.Counts(14).glowing, 3u);
	CHECK_EQ(entities.SceneBytes(), uint64_t(32 + 2 * 16 + 9 * 32));
}

TEST(EntitiesFindTheNearestMob)
{
	Entities entities;
	CHECK(!entities.NearestMob(kEye).has_value());
	link::WorldTable table;
	table.entities = { Record(proto::kWeShadow, 885.5f, 7.76f, 2007.5f), Record(proto::kWeItem, 881.5f, 7.76f, 2007.5f),
		               Record(proto::kWeShadow, 880.5f, 7.76f, 2010.5f) };
	entities.World(table);
	const std::optional<double> nearest = entities.NearestMob(kEye);
	CHECK(nearest.has_value());
	CHECK_NEAR(nearest.value_or(0.0), 3.0, 1e-4);  // the second shadow's, 3 south: the item doesn't count
}
```

- [ ] **Step 2: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestWorldShapes.cpp
	tools/mclink/DumpObj.cpp
```
```cmake
	tests/TestWorldShapes.cpp
	tests/TestEntities.cpp
	tools/mclink/DumpObj.cpp
```

- [ ] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'entities/Entities.h'`.

- [ ] **Step 4: Write `src/entities/Entities.h`**

```cpp
#pragma once

#include "Coords.h"
#include "link/Messages.h"
#include "link/WorldTable.h"
#include "world/Vertex.h"

#include <cstdint>
#include <optional>
#include <vector>

// Spike 7: one frame's geometry of Minecraft's entities (the spec's "Entities: one frame's geometry"), from the newest
// scene and world-entity table the link handed on. Main thread; it calls no GPU, so every rule is tested without the
// game. The renderer copies each frame into a snapshot and draws its draws.
namespace cpmc::entities
{
// One draw: a run of the frame's vertices with one texture.
struct EntityDraw
{
	enum class Kind : uint8_t
	{
		Model,        // into the G-buffer: the scene's opaque batches, and the world entities' shapes
		Translucent,  // the scene's blended batches, for step B's forward pass
		Crack,        // multiplied into the G-buffer's base colour
	};

	uint32_t first = 0;
	uint32_t count = 0;
	uint32_t texture = 0;  // 0: Minecraft's combined atlas; else a texture's id
	Kind     kind = Kind::Model;
};

struct EntityFrame
{
	Vec3d                   origin;    // a whole block, in Minecraft axes: every position is relative to it
	std::vector<Vertex>     vertices;  // the scene's as they came, then the world entities' shapes, then their cracks
	std::vector<EntityDraw> draws;     // the scene's batches in the order they came, then one of shapes and one of cracks
};

class Entities
{
public:
	void Scene(const link::SceneMessage& aScene);  // the newest scene, copied; one of no batches says nothing is there
	void World(const link::WorldTable& aTable);    // the newest world-entity table, as the link hands it on each update
	void Clear();                                  // Minecraft's clear: the scene goes
	void InWorld(bool aInWorld);                   // out of its world, or gone, the scene goes: SkyCraft stops sending

	// This frame's geometry. aEye, Minecraft's eye (McState), gives the origin when there's no scene: its whole block.
	EntityFrame Frame(const Vec3d& aEye) const;

	// The newest scene: its batches and vertices, how many of those have block light of at least aGlowLight, and how many
	// are in translucent batches.
	struct SceneCounts
	{
		size_t   batches = 0;
		uint32_t vertices = 0;
		uint32_t glowing = 0;
		uint32_t translucent = 0;
	};
	SceneCounts                          Counts(uint32_t aGlowLight) const;
	const std::vector<link::SceneBatch>& Batches() const { return batches_; }
	const link::WorldTable&              Table() const { return world_; }
	uint64_t                             Scenes() const { return scenes_; }  // since the start, empty ones included
	uint64_t                             SceneBytes() const { return sceneBytes_; }
	// How far the nearest player's or mob's feet (the table's shadows) are from aFeet, Minecraft axes; nothing if none.
	std::optional<double> NearestMob(const Vec3d& aFeet) const;

private:
	std::optional<Vec3d>          origin_;  // the newest scene's, while one is there
	std::vector<Vertex>           vertices_;
	std::vector<link::SceneBatch> batches_;
	link::WorldTable              world_;
	uint64_t                      scenes_ = 0;
	uint64_t                      sceneBytes_ = 0;
};
}
```

- [ ] **Step 5: Write `src/entities/Entities.cpp`**

```cpp
#include "entities/Entities.h"

#include "entities/WorldShapes.h"

#include <skycraft_protocol.h>

#include <algorithm>
#include <cmath>

namespace cpmc::entities
{
void Entities::Scene(const link::SceneMessage& aScene)
{
	++scenes_;
	sceneBytes_ += sizeof(skycraft::proto::RenScene) + aScene.batches.size() * sizeof(skycraft::proto::RenBatch) + aScene.vertices.size();
	if (aScene.batches.empty())
	{
		Clear();  // SkyCraft's one empty scene, once the last thing has gone
		return;
	}
	origin_ = aScene.origin;
	vertices_ = link::SceneVertices(aScene);
	batches_ = aScene.batches;
}

void Entities::World(const link::WorldTable& aTable)
{
	world_ = aTable;
}

void Entities::Clear()
{
	origin_.reset();
	vertices_.clear();
	batches_.clear();
}

void Entities::InWorld(bool aInWorld)
{
	if (!aInWorld)
	{
		Clear();
	}
}

EntityFrame Entities::Frame(const Vec3d& aEye) const
{
	EntityFrame frame;
	frame.origin = origin_ ? *origin_ : Vec3d{ std::floor(aEye.x), std::floor(aEye.y), std::floor(aEye.z) };
	frame.vertices = vertices_;
	for (const link::SceneBatch& batch : batches_)
	{
		if (batch.count > 0)
		{
			frame.draws.push_back({ batch.first, batch.count, batch.texture, (batch.flags & 1) ? EntityDraw::Kind::Translucent : EntityDraw::Kind::Model });
		}
	}
	// The world entities: their shapes in one draw and their cracks in another, all in Minecraft's combined atlas.
	const uint32_t shapes = uint32_t(frame.vertices.size());
	for (const auto& record : world_.entities)
	{
		AppendShape(record, frame.origin, frame.vertices);
	}
	if (const uint32_t count = uint32_t(frame.vertices.size()) - shapes; count > 0)
	{
		frame.draws.push_back({ shapes, count, 0, EntityDraw::Kind::Model });
	}
	const uint32_t cracks = uint32_t(frame.vertices.size());
	for (const auto& record : world_.entities)
	{
		AppendCrack(record, frame.origin, frame.vertices);
	}
	if (const uint32_t count = uint32_t(frame.vertices.size()) - cracks; count > 0)
	{
		frame.draws.push_back({ cracks, count, 0, EntityDraw::Kind::Crack });
	}
	return frame;
}

Entities::SceneCounts Entities::Counts(uint32_t aGlowLight) const
{
	SceneCounts counts;
	counts.batches = batches_.size();
	counts.vertices = uint32_t(vertices_.size());
	counts.glowing = uint32_t(std::count_if(vertices_.begin(), vertices_.end(), [aGlowLight](const Vertex& aVertex) {
		return (aVertex.light & 0xFF) >= aGlowLight;
	}));
	for (const link::SceneBatch& batch : batches_)
	{
		counts.translucent += (batch.flags & 1) ? batch.count : 0;
	}
	return counts;
}

std::optional<double> Entities::NearestMob(const Vec3d& aFeet) const
{
	std::optional<double> nearest;
	for (const auto& record : world_.entities)
	{
		if (record.kind == skycraft::proto::kWeShadow)
		{
			const double distance = std::hypot(record.x - aFeet.x, record.y - aFeet.y, record.z - aFeet.z);
			nearest = std::min(nearest.value_or(distance), distance);
		}
	}
	return nearest;
}
}
```

- [ ] **Step 6: Edit `CMakeLists.txt` to add the source**

```cmake
	src/entities/WorldShapes.cpp
	src/physx/GameAbi.cpp
```
```cmake
	src/entities/WorldShapes.cpp
	src/entities/Entities.cpp
	src/physx/GameAbi.cpp
```

- [ ] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `478 test(s), 0 failure(s)`.

- [ ] **Step 8: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "minecraft-entities: one frame's entities, from the newest scene and world-entity table" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 10: Where each entity texture goes

The renderer keeps Minecraft's entity textures in one buffer, one `uint` per texel as the atlas has them (the spec's
"Entity textures on the GPU"). `EntityTextures` says where each goes, with no GPU, so it's tested here:
- **each texture after the last,** placed as it comes, before its upload, so the cap counts those waiting while drawing
  is off;
- **64 MB in all at most:** a texture past it gets no place, and what uses it isn't drawn;
- **an id placed again** (only after Minecraft's clear, which empties them all) gets a new place, the old one unused;
- **the buffer's size:** 4 MB at first, doubled as needed, up to 64 MB.

**Files:**
- Create: `src/render/EntityTextures.h`, `src/render/EntityTextures.cpp`, `tests/TestEntityTextures.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the failing test, `tests/TestEntityTextures.cpp`**

```cpp
#include "Harness.h"

#include "render/EntityTextures.h"

using cpmc::EntityTextures;

TEST(EntityTexturesGoOneAfterAnother)
{
	EntityTextures textures;
	const auto     pig = textures.Add(4, 64, 64);
	const auto     spider = textures.Add(9, 64, 32);
	CHECK(pig.has_value() && spider.has_value());
	CHECK_EQ(pig ? pig->start : 1u, 0u);
	CHECK_EQ(spider ? spider->start : 0u, 4096u);  // right after the pig's 64 x 64
	const EntityTextures::Place* found = textures.Find(9);
	CHECK(found != nullptr);
	if (found)
	{
		CHECK_EQ(found->start, 4096u);
		CHECK_EQ(found->width, 64u);
		CHECK_EQ(found->height, 32u);
	}
	CHECK(textures.Find(5) == nullptr);
	CHECK_EQ(textures.Used(), 6144ull);
	CHECK_EQ(textures.Count(), size_t(2));
}

TEST(EntityTexturesStopAtTheCap)
{
	EntityTextures textures;
	CHECK(textures.Add(1, 4096, 4096).has_value());  // 64 MB: the cap exactly
	CHECK(!textures.Add(2, 1, 1).has_value());
	CHECK(textures.Find(2) == nullptr);
	CHECK_EQ(textures.Used(), EntityTextures::kCapTexels);
	CHECK(!textures.Add(3, 0, 4).has_value());  // nothing to place
}

TEST(EntityTexturesBufferDoublesFrom4Mb)
{
	EntityTextures textures;
	CHECK_EQ(textures.Capacity(0), 0ull);  // nothing placed: no buffer yet
	textures.Add(1, 64, 64);
	CHECK_EQ(textures.Capacity(0), EntityTextures::kFirstTexels);
	CHECK_EQ(textures.Capacity(EntityTextures::kFirstTexels), EntityTextures::kFirstTexels);
	textures.Add(2, 1024, 1024);  // past 4 MB now
	CHECK_EQ(textures.Capacity(EntityTextures::kFirstTexels), 2 * EntityTextures::kFirstTexels);
	textures.Add(3, 4096, 3000);
	CHECK_EQ(textures.Capacity(2 * EntityTextures::kFirstTexels), EntityTextures::kCapTexels);  // doubled to the cap
}

TEST(EntityTexturesClearStartsAgain)
{
	EntityTextures textures;
	textures.Add(1, 64, 64);
	textures.Clear();
	CHECK_EQ(textures.Used(), 0ull);
	CHECK_EQ(textures.Count(), size_t(0));
	CHECK(textures.Find(1) == nullptr);
	const auto again = textures.Add(1, 32, 32);  // SkyCraft's ids start at 1 again after its clear
	CHECK_EQ(again ? again->start : 1u, 0u);
}

TEST(EntityTexturesPlaceAnIdAgainAfterTheLast)
{
	EntityTextures textures;
	textures.Add(1, 2, 2);
	const auto again = textures.Add(1, 4, 4);
	CHECK_EQ(again ? again->start : 0u, 4u);
	CHECK_EQ(textures.Find(1) ? textures.Find(1)->width : 0u, 4u);
	CHECK_EQ(textures.Used(), 20ull);
	CHECK_EQ(textures.Count(), size_t(1));
}
```

- [ ] **Step 2: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestEntities.cpp
	tools/mclink/DumpObj.cpp
```
```cmake
	tests/TestEntities.cpp
	tests/TestEntityTextures.cpp
	tools/mclink/DumpObj.cpp
```

- [ ] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'render/EntityTextures.h'`.

- [ ] **Step 4: Write `src/render/EntityTextures.h`**

```cpp
#pragma once

#include <cstdint>
#include <map>
#include <optional>

// Spike 7: where Minecraft's entity textures go in the renderer's one buffer (the spec's "Entity textures on the GPU"):
// each after the last, one uint per texel as the atlas has them, 64 MB in all at most. A texture is placed as it comes,
// before its upload, so the cap counts those waiting for drawing to come on too. Pure; the renderer locks around it.
namespace cpmc
{
class EntityTextures
{
public:
	static constexpr uint64_t kCapTexels = (64ull << 20) / 4;   // 64 MB
	static constexpr uint64_t kFirstTexels = (4ull << 20) / 4;  // the buffer's first size, 4 MB

	struct Place
	{
		uint32_t start = 0;  // its first texel in the buffer
		uint32_t width = 0;
		uint32_t height = 0;
	};

	// Texture aId placed after the last: nothing if it would take the textures past 64 MB in all. An id placed again
	// gets a new place, its old one unused until a clear.
	std::optional<Place> Add(uint32_t aId, uint32_t aWidth, uint32_t aHeight);
	const Place*         Find(uint32_t aId) const;
	void                 Clear();  // Minecraft's clear: every place goes, and the next texture starts the buffer again

	uint64_t Used() const { return used_; }  // texels placed
	size_t   Count() const { return places_.size(); }
	// The buffer's size in texels to hold what's placed, from aCurrent's: aCurrent if it holds it, else the first of
	// 4 MB doubled that does, at most 64 MB.
	uint64_t Capacity(uint64_t aCurrent) const;

private:
	std::map<uint32_t, Place> places_;
	uint64_t                  used_ = 0;
};
}
```

- [ ] **Step 5: Write `src/render/EntityTextures.cpp`**

```cpp
#include "render/EntityTextures.h"

#include <algorithm>

namespace cpmc
{
std::optional<EntityTextures::Place> EntityTextures::Add(uint32_t aId, uint32_t aWidth, uint32_t aHeight)
{
	const uint64_t texels = uint64_t(aWidth) * aHeight;
	if (texels == 0 || used_ + texels > kCapTexels)
	{
		return std::nullopt;
	}
	const Place place{ uint32_t(used_), aWidth, aHeight };
	places_[aId] = place;
	used_ += texels;
	return place;
}

const EntityTextures::Place* EntityTextures::Find(uint32_t aId) const
{
	const auto found = places_.find(aId);
	return found != places_.end() ? &found->second : nullptr;
}

void EntityTextures::Clear()
{
	places_.clear();
	used_ = 0;
}

uint64_t EntityTextures::Capacity(uint64_t aCurrent) const
{
	if (used_ <= aCurrent)
	{
		return aCurrent;
	}
	uint64_t capacity = std::max(aCurrent, kFirstTexels);
	while (capacity < used_)
	{
		capacity *= 2;
	}
	return std::min(capacity, kCapTexels);
}
}
```

- [ ] **Step 6: Edit `CMakeLists.txt` to add the source**

```cmake
	src/render/Hud.cpp
	src/Measurements.cpp
```
```cmake
	src/render/Hud.cpp
	src/render/EntityTextures.cpp
	src/Measurements.cpp
```

- [ ] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `483 test(s), 0 failure(s)`.

- [ ] **Step 8: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "minecraft-entities: where each entity texture goes in the renderer's one buffer, up to 64 MB" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 11: The snapshot ring

Each frame's entities are copied into one of 8 upload buffers, which the G-buffer list then reads (the spec's "The
snapshot ring"). `Snapshots` says which buffer the main thread may write and when one is free again, with no GPU:
- **the newest** is never written over, so a list can always take it;
- **a list takes the newest** as it's recorded, and holds it until the GPU passes the fence signalled after that list:
  after it, not before it, as `Retired` learnt from block-render's Task 52. Taken twice, it waits for both;
- **a list that isn't recorded after all** gives its hold back (`Abandon`);
- **with every buffer held,** the frame's geometry waits, and the next frame's replaces it.

In Part 2 only the G-buffer list reads a snapshot. Step B's forward list (Part 3) will hold the same snapshot to the
fence after it.

**Files:**
- Create: `src/render/Snapshots.h`, `src/render/Snapshots.cpp`, `tests/TestSnapshots.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the failing test, `tests/TestSnapshots.cpp`**

```cpp
#include "Harness.h"

#include "render/Snapshots.h"

using cpmc::Snapshots;

TEST(SnapshotsNothingToTakeAtFirst)
{
	Snapshots snapshots(3);
	CHECK(!snapshots.Newest().has_value());
	CHECK(!snapshots.TakeNewest().has_value());
	CHECK_EQ(snapshots.Writable(0).value_or(9), 0u);
}

TEST(SnapshotsTheNewestIsNeverWrittenOver)
{
	Snapshots snapshots(3);
	snapshots.Written(0);
	CHECK_EQ(snapshots.Newest().value_or(9), 0u);
	CHECK_EQ(snapshots.Writable(0).value_or(9), 1u);  // not 0: a list may take it any moment
	CHECK_EQ(snapshots.TakeNewest().value_or(9), 0u);
}

TEST(SnapshotsHeldUntilTheFenceAfterTheList)
{
	Snapshots snapshots(2);
	snapshots.Written(0);
	CHECK_EQ(snapshots.TakeNewest().value_or(9), 0u);  // a G-buffer list reads it
	snapshots.Written(1);                               // the next frame
	CHECK(!snapshots.Writable(100).has_value());        // 0 is on its way to the GPU, and 1 is the newest
	snapshots.Submitted(0, 7);                          // the fence after the list
	CHECK(!snapshots.Writable(6).has_value());          // the GPU may still be reading it
	CHECK_EQ(snapshots.Writable(7).value_or(9), 0u);
}

TEST(SnapshotsTakenTwiceWaitForBoth)
{
	Snapshots snapshots(2);
	snapshots.Written(0);
	snapshots.TakeNewest();  // two frames' lists, with no newer frame between them
	snapshots.TakeNewest();
	snapshots.Written(1);
	snapshots.Submitted(0, 5);
	CHECK(!snapshots.Writable(9).has_value());  // the second list is still on its way
	snapshots.Submitted(0, 8);
	CHECK(!snapshots.Writable(7).has_value());
	CHECK_EQ(snapshots.Writable(8).value_or(9), 0u);
}

TEST(SnapshotsAbandonedListHoldsNothing)
{
	Snapshots snapshots(2);
	snapshots.Written(0);
	snapshots.TakeNewest();
	snapshots.Written(1);
	CHECK(!snapshots.Writable(0).has_value());
	snapshots.Abandon(0);  // the list wasn't recorded after all
	CHECK_EQ(snapshots.Writable(0).value_or(9), 0u);
}
```

- [ ] **Step 2: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestEntityTextures.cpp
	tools/mclink/DumpObj.cpp
```
```cmake
	tests/TestEntityTextures.cpp
	tests/TestSnapshots.cpp
	tools/mclink/DumpObj.cpp
```

- [ ] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'render/Snapshots.h'`.

- [ ] **Step 4: Write `src/render/Snapshots.h`**

```cpp
#pragma once

#include <cstdint>
#include <optional>
#include <vector>

// Spike 7's snapshot ring (the spec's "The snapshot ring"): which of the renderer's upload buffers each frame's entities
// are copied into, and when one is free again. The main thread writes the newest frame into a buffer no list of ours may
// still read; a G-buffer list takes the newest, which is then held until the GPU passes the fence signalled after that
// list: after it, not before it, as Retired learnt from Task 52. The newest is never written over, so a list can always
// take it. Pure; the renderer locks around it.
namespace cpmc
{
class Snapshots
{
public:
	explicit Snapshots(uint32_t aCount);

	// A buffer the main thread may write: not the newest, not taken by a list on its way to the GPU, and with every list
	// that took it done, aCompleted (the fence's completed value) at or past its fence. Nothing if all are held.
	std::optional<uint32_t> Writable(uint64_t aCompleted) const;
	void                    Written(uint32_t aIndex);  // it holds the newest frame now
	std::optional<uint32_t> Newest() const { return newest_; }

	// A list of ours takes the newest to read: it's held from now on. Nothing if none has been written yet.
	std::optional<uint32_t> TakeNewest();
	void                    Submitted(uint32_t aIndex, uint64_t aFence);  // that list went to the GPU, aFence after it
	void                    Abandon(uint32_t aIndex);                     // that list wasn't recorded after all

	uint32_t Count() const { return uint32_t(holds_.size()); }

private:
	struct Hold
	{
		uint32_t onTheirWay = 0;  // lists that took it, recorded and not yet submitted
		uint64_t fence = 0;       // the latest fence after a submitted list that took it
	};
	std::vector<Hold>       holds_;
	std::optional<uint32_t> newest_;
};
}
```

- [ ] **Step 5: Write `src/render/Snapshots.cpp`**

```cpp
#include "render/Snapshots.h"

#include <algorithm>

namespace cpmc
{
Snapshots::Snapshots(uint32_t aCount)
	: holds_(aCount)
{
}

std::optional<uint32_t> Snapshots::Writable(uint64_t aCompleted) const
{
	for (uint32_t index = 0; index < holds_.size(); ++index)
	{
		const Hold& hold = holds_[index];
		if (index != newest_ && hold.onTheirWay == 0 && hold.fence <= aCompleted)
		{
			return index;
		}
	}
	return std::nullopt;
}

void Snapshots::Written(uint32_t aIndex)
{
	if (aIndex < holds_.size())
	{
		newest_ = aIndex;
	}
}

std::optional<uint32_t> Snapshots::TakeNewest()
{
	if (newest_)
	{
		++holds_[*newest_].onTheirWay;
	}
	return newest_;
}

void Snapshots::Submitted(uint32_t aIndex, uint64_t aFence)
{
	if (aIndex < holds_.size() && holds_[aIndex].onTheirWay > 0)
	{
		--holds_[aIndex].onTheirWay;
		holds_[aIndex].fence = std::max(holds_[aIndex].fence, aFence);
	}
}

void Snapshots::Abandon(uint32_t aIndex)
{
	if (aIndex < holds_.size() && holds_[aIndex].onTheirWay > 0)
	{
		--holds_[aIndex].onTheirWay;
	}
}
}
```

- [ ] **Step 6: Edit `CMakeLists.txt` to add the source**

```cmake
	src/render/EntityTextures.cpp
	src/Measurements.cpp
```
```cmake
	src/render/EntityTextures.cpp
	src/render/Snapshots.cpp
	src/Measurements.cpp
```

- [ ] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `488 test(s), 0 failure(s)`.

- [ ] **Step 8: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "minecraft-entities: the snapshot ring, each frame's entities held until the fence after the list that reads them" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 12: The entity shaders

New entry points beside the blocks' in `shaders/blocks.hlsl` (the spec's "The shader"), with the blocks' root signature,
grown in Task 13:
- **`EntityVS`** reads a draw's vertices from the snapshot (`vertices`, t0) from its first (`entityFirst`), and places
  them by the frame's origin's offset from the camera (`entityOffset`), Minecraft's (x, y, z) being the world's (x, −z,
  y). Its motion is the camera's, as the blocks' is (`MotionBetween`, shared now with `MotionVector`).
- **`EntityTexel`:** texture 0 is Minecraft's combined atlas at its top level, since SkyCraft marks every entity vertex
  for no mipmaps; any other the draw's texture in `entityTextures` (t3), from its start, width and height. The UV is
  clamped to the texture first, since root descriptors carry no bounds: a bad one reads an edge texel. `[branch]` keeps
  the other buffer unread.
- **`EntityPS`** writes the G-buffer's four targets as `GbufferPS` does, cut out where texel × vertex alpha is under
  0.1, Minecraft 26.3's cut-out for entities, particles and items. Its normal: direction 7, the triangle's own,
  `normalize(cross(ddy(p), ddx(p)))` of its camera-relative position, turned toward the camera; 0, straight up, as the
  blocks' plants are lit; 1 to 6, the table. A triangle whose first vertex has block light of at least `glowlight`
  writes `glow` into the material target's A, which spike 3 found above 0 only on the game's windows and signs; any
  other writes 0, as the blocks do.
- **`CrackPS`** writes the crack's colour alone, which its pipeline blends into the base colour as Minecraft's crumbling
  does; **`EntityDepthPS`** writes depth alone, cut out as `EntityPS`.
- **The constants:** the frame's grow by `entity` (`glow` and `glowlight`, as `entityTuning` in the shader), to 288
  bytes; a draw's (`EntityConstants`: the offset, the first vertex and its texture's start, width and height) take b1,
  where the blocks have a section's, eight 32-bit values. The test checks both against the compiled shaders'
  reflection, and the targets each entry point writes.

**Files:**
- Modify: `tests/TestShaders.cpp`, `src/render/FrameConstants.h`, `shaders/blocks.hlsl`, `src/render/Shaders.h`,
  `src/render/Shaders.cpp`

- [ ] **Step 1: Edit `tests/TestShaders.cpp` to add the failing test**

```cpp
TEST(ShadersReportCompileErrors)
```
```cpp
TEST(ShadersDrawMinecraftsEntities)
{
	std::string error;
	const auto  shaders = cpmc::CompileBlockShaders(ShaderSource(), error);
	CHECK(shaders.has_value());
	if (shaders)
	{
		CHECK(!shaders->entityVertex.empty());
		CHECK_EQ(ConstantBufferSize(shaders->entityVertex, "Frame"), UINT(sizeof(cpmc::FrameConstants)));
		CHECK_EQ(ConstantBufferSize(shaders->entityVertex, "Entity"), UINT(sizeof(cpmc::EntityConstants)));
		CHECK_EQ(ConstantBufferSize(shaders->entity, "Frame"), UINT(sizeof(cpmc::FrameConstants)));
		CHECK_EQ(ConstantBufferSize(shaders->entity, "Entity"), UINT(sizeof(cpmc::EntityConstants)));
		CHECK_EQ(Outputs(shaders->entity), 4u);       // the G-buffer's four targets, as GbufferPS
		CHECK_EQ(Outputs(shaders->crack), 1u);        // the base colour alone
		CHECK_EQ(Outputs(shaders->entityDepth), 0u);  // depth only
	}
}

TEST(ShadersReportCompileErrors)
```

- [ ] **Step 2: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `'entityVertex': is not a member of 'cpmc::BlockShaders'`.

- [ ] **Step 3: Edit `src/render/FrameConstants.h` to add the entities' constants**

```cpp
	float    material[4];         // the G-buffer's material values: metalness, roughness, B; 0
};
static_assert(sizeof(FrameConstants) == 272);
```
```cpp
	float    material[4];         // the G-buffer's material values: metalness, roughness, B; 0
	float    entity[4];           // spike 7: a glowing triangle's emission (set glow); the block light it glows from (set
	                              // glowlight); 0; 0
};
static_assert(sizeof(FrameConstants) == 288);
```
```cpp
static_assert(sizeof(SectionConstants) == 16);
```
```cpp
static_assert(sizeof(SectionConstants) == 16);

// Spike 7: one entity draw's root constants, in b1 where the blocks have SectionConstants.
struct EntityConstants
{
	float    offset[3];      // the frame's origin: camera-relative, world axes
	uint32_t first;          // the draw's first vertex in the snapshot
	uint32_t textureStart;   // its texture's first texel in the entity textures' buffer
	uint32_t textureWidth;   // 0 for Minecraft's combined atlas
	uint32_t textureHeight;
	uint32_t unused;
};
static_assert(sizeof(EntityConstants) == 32);
```

- [ ] **Step 4: Edit `shaders/blocks.hlsl` to add the entities' entry points**

```hlsl
    uint   flags;     // bit 0 alpha-tested, bit 1 translucent, bits 4-7 the face's direction + 1
```
```hlsl
    uint   flags;     // bit 0 alpha-tested, bit 1 translucent, bit 3 no mipmaps, bits 4-7 the face's direction + 1
```
```hlsl
    float4 material;                  // the G-buffer's material values: metalness, roughness, B; 0
```
```hlsl
    float4 material;                  // the G-buffer's material values: metalness, roughness, B; 0
    float4 entityTuning;              // spike 7: a glowing triangle's emission; the block light it glows from; 0; 0
```
```hlsl
// As Camera.cpp's MotionVector: a fraction of the screen, from where the point is to where it was.
float2 MotionVector(VSOut i)
{
    const float2 now = i.clip.xy / i.clip.w;
    const float2 before = i.prevClip.xy / i.prevClip.w;
    return (before - now) * float2(0.5, -0.5) * tuning.w;
```
```hlsl
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
```
```hlsl
// Spike 4: Minecraft's HUD image (its hotbar, crosshair, chat, hand and held item), drawn over the finished frame at
```
```hlsl
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

// Spike 4: Minecraft's HUD image (its hotbar, crosshair, chat, hand and held item), drawn over the finished frame at
```

- [ ] **Step 5: Edit `src/render/Shaders.h` to hold them**

```cpp
	std::vector<uint8_t> hud;        // HudPS
```
```cpp
	std::vector<uint8_t> hud;        // HudPS
	std::vector<uint8_t> entityVertex;  // EntityVS (spike 7)
	std::vector<uint8_t> entity;        // EntityPS
	std::vector<uint8_t> crack;         // CrackPS
	std::vector<uint8_t> entityDepth;   // EntityDepthPS
```

- [ ] **Step 6: Edit `src/render/Shaders.cpp` to compile them**

```cpp
	}
	return BlockShaders{ std::move(*vertex), std::move(*present), std::move(*predlss), std::move(*gbuffer),
		                 std::move(*depth), std::move(*hudVertex), std::move(*hud) };
```
```cpp
	}
	auto entityVertex = CompileShader(aSource, "blocks.hlsl", "EntityVS", "vs_5_1", aError);
	if (!entityVertex)
	{
		return std::nullopt;
	}
	auto entity = CompileShader(aSource, "blocks.hlsl", "EntityPS", "ps_5_1", aError);
	if (!entity)
	{
		return std::nullopt;
	}
	auto crack = CompileShader(aSource, "blocks.hlsl", "CrackPS", "ps_5_1", aError);
	if (!crack)
	{
		return std::nullopt;
	}
	auto entityDepth = CompileShader(aSource, "blocks.hlsl", "EntityDepthPS", "ps_5_1", aError);
	if (!entityDepth)
	{
		return std::nullopt;
	}
	return BlockShaders{ std::move(*vertex), std::move(*present), std::move(*predlss), std::move(*gbuffer),
		                 std::move(*depth), std::move(*hudVertex), std::move(*hud), std::move(*entityVertex),
		                 std::move(*entity), std::move(*crack), std::move(*entityDepth) };
```

- [ ] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `489 test(s), 0 failure(s)`.

- [ ] **Step 8: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "minecraft-entities: the entity shaders: into the G-buffer with Minecraft's glow as emission, cracks multiplied into the base colour, and the entities' depth" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 13: The renderer draws Minecraft's entities into the G-buffer

The renderer's side of step A (the spec's "The renderer" and "Step A: into the G-buffer"), only in `gbuffer` mode:
`predlss` and `present` keep drawing the blocks alone. Nothing here can run without the game, so it's checked by the
build here and by run A (Task 16).
- **The textures:** `AddEntityTexture` places a texture at once (`EntityTextures`) and keeps its pixels until the next
  `Update`'s upload, which copies them into the one buffer: past the 64 MB cap a texture is left out, logged once.
  When the buffer is too small, a bigger one is made and the textures already uploaded copied over on the GPU; the old
  buffer is kept until that upload is done, or until a list of ours still on its way to the GPU is, and each texture
  in it is drawn again once the copy is. `ClearEntityTextures`, at Minecraft's clear, drops them all, the buffer
  waiting for the lists that may read it (`Retire`).
- **The snapshots:** `SetEntities` takes the frame's geometry, which `Update` copies into a free snapshot
  (`Snapshots`): an upload buffer grown as needed, to 8 MB at most. A frame over 8 MB keeps its draws in order while
  they fit; the rest are counted (`snapshotCut`). With every snapshot held, the frame waits (`snapshotWaits`).
- **The root signature** grows to eight 32-bit constants in b1 and a fourth root SRV, t3, which every list of ours binds:
  the entity textures, or the atlas where nothing reads it.
- **Three pipelines,** made at their first use, with the blocks' root signature:
  - **entities:** no culling, as Minecraft's own `entity_cutout`; depth tested greater-or-equal (less-or-equal when
    the depth isn't reversed), so a later triangle on the same surface wins, as in Minecraft; depth written; the four
    G-buffer targets, the motion vectors' written as `gbmotion` says; the stencil class as the blocks';
  - **entities' depth:** the same tests, depth only, into the tagged depth;
  - **cracks:** no culling; depth tested, not written; the base colour's RGB blended `DEST_COLOR, SRC_COLOR`, its A and
    the other targets left alone; the stencil left alone.

  If they can't be made, entities aren't drawn and the blocks still are, logged once; `reload` tries again.
- **The draws,** in `RecordGbuffer` after the blocks, while `set entities` is 1: the newest snapshot with draws, taken
  until the fence after this list (`OnSubmitted`), or given back if the list can't be closed; its `Model` draws, then
  its `Crack` draws over the base colour alone; then, with the blocks, its `Model` draws into the tagged depth. Each
  draw sets its constants: the origin's offset from the camera, as a section's corner is computed, its first vertex,
  and its texture. A draw whose texture isn't on the GPU yet is skipped and counted: SkyCraft sends a texture before
  the scene that uses it, so this lasts a frame. Two timestamps around the entities' draws give their GPU time.
- **The settings:** `entities` (0 or 1, 1 at first), `glow` (0 to 1, 0.9 at first, near the 0.93 spike 3 found on the
  game's signs) and `glowlight` (a whole number from 1 to 15, 15 at first), each refused outside its range.
- **The stats:** whether entities are on, the frames drawn, draws skipped, snapshot waits and cuts, the textures and
  their MB, those left out, and the entities' GPU time.

**Files:**
- Modify: `src/render/Renderer.h`, `src/render/Renderer.cpp`

- [ ] **Step 1: Edit `src/render/Renderer.h` to add the entities**

```cpp
#include "LogQueue.h"
#include "hooks/HookEvents.h"
#include "render/Atlas.h"
#include "render/Camera.h"
```
```cpp
#include "LogQueue.h"
#include "entities/Entities.h"
#include "hooks/HookEvents.h"
#include "render/Atlas.h"
#include "render/Camera.h"
#include "render/EntityTextures.h"
```
```cpp
#include "render/Shaders.h"
```
```cpp
#include "render/Shaders.h"
#include "render/Snapshots.h"
```
```cpp
#include <optional>
```
```cpp
#include <optional>
#include <span>
```
```cpp
//   targets, its end by its transitions, and the states our barriers start from by both.
```
```cpp
//   targets, its end by its transitions, and the states our barriers start from by both. Spike 7 draws Minecraft's
//   entities in the same list, after the blocks: the newest snapshot's models, then their cracks, then their depth.
```
```cpp
	// metalness, roughness, materialb, gbmotion and gbdepth
```
```cpp
	// metalness, roughness, materialb, gbmotion and gbdepth; spike 7's entities, glow and glowlight
```
```cpp
	HudSize BackBufferSize() const { return { backWidth_.load(), backHeight_.load() }; }  // as the last Present saw it
```
```cpp
	HudSize BackBufferSize() const { return { backWidth_.load(), backHeight_.load() }; }  // as the last Present saw it

	// Spike 7: Minecraft's entities (the spec's "The renderer"), from the main thread. AddEntityTexture copies a texture
	// and places it in the one buffer at once, for the next Update to upload: past the 64 MB cap it's left out, logged
	// once. ClearEntityTextures drops them all, at Minecraft's clear: Unlink, the Clear request and drawing switched off
	// keep them, since SkyCraft sends each once. SetEntities takes this frame's geometry, which the next Update copies
	// into a free snapshot for the next G-buffer list to draw.
	void     AddEntityTexture(uint32_t aId, uint32_t aWidth, uint32_t aHeight, std::span<const uint8_t> aRgba);
	void     ClearEntityTextures();
	void     SetEntities(entities::EntityFrame aFrame);
	uint32_t GlowLight() const;  // set glowlight
```
```cpp
		bool        hudBottomUp = false;
```
```cpp
		bool        hudBottomUp = false;
		// Spike 7's entities
		bool        entitiesOn = true;          // set entities
		uint64_t    entityFrames = 0;           // G-buffer lists that drew them, in the last second
		uint64_t    entityDrawsSkipped = 0;     // their texture not on the GPU yet, or left out at the cap; in the last second
		uint64_t    snapshotWaits = 0;          // frames whose geometry waited for a free snapshot, in the last second
		uint64_t    snapshotCut = 0;            // draws past a snapshot's 8 MB, in the last second
		size_t      entityTextures = 0;         // placed: uploaded, or waiting to be
		double      entityTextureMb = 0.0;
		uint64_t    entityTexturesLeftOut = 0;  // at the cap, since the start
		double      entityGpuMsAverage = -1.0;  // the entities' draws in the G-buffer, on the GPU; below 0: none drawn
```
```cpp
	bool               EnsureDepthTarget(uint32_t aWidth, uint32_t aHeight);
```
```cpp
	bool               EnsureDepthTarget(uint32_t aWidth, uint32_t aHeight);
	bool               EnsureEntityPipelines(bool aReversed);  // spike 7's; false, logged once, if they can't be made
```
```cpp
	void               DrawSections(ID3D12GraphicsCommandList* aList, const CameraFrame& aCamera, uint64_t aCompleted);
```
```cpp
	void               DrawSections(ID3D12GraphicsCommandList* aList, const CameraFrame& aCamera, uint64_t aCompleted);
	// Spike 7: a snapshot's draws of aKind; those whose texture isn't on the GPU are skipped, and counted if aCount.
	struct SnapshotGpu;
	void DrawEntities(ID3D12GraphicsCommandList* aList, const CameraFrame& aCamera, uint64_t aCompleted, const SnapshotGpu& aSnapshot,
	                  entities::EntityDraw::Kind aKind, bool aCount);
	// Spike 7: the textures waiting, into the one buffer, grown first if they don't fit, recorded into Upload's list.
	void UploadEntityTextures(uint64_t aReady, std::vector<gpu::ComPtr<ID3D12Resource>>& aStaging, bool& aFailed);
	void CopyEntities();  // spike 7: Update's, the frame's geometry into a free snapshot
```
```cpp
	Stats    stats_;
```
```cpp
	// Spike 7's entities. The snapshots: upload buffers, written by Update into one no list may still read (Snapshots),
	// each with the draws that fit in it.
	struct SnapshotGpu
	{
		gpu::ComPtr<ID3D12Resource>       buffer;  // upload heap, mapped while it lives
		uint8_t*                          mapped = nullptr;
		uint64_t                          bytes = 0;
		Vec3d                             origin;  // the frame's, Minecraft axes
		std::vector<entities::EntityDraw> draws;
	};
	static constexpr uint32_t kSnapshots = 8;
	static constexpr uint64_t kSnapshotMaxBytes = 8ull << 20;  // 262,144 vertices: a big blast's scene is about 1 MB
	struct PendingTexture
	{
		uint32_t              id = 0;
		EntityTextures::Place place;
		std::vector<uint8_t>  rgba;
	};
	bool                                        entitiesOn_ = true;  // set entities
	float                                       glow_ = 0.9f;        // set glow: near the 0.93 spike 3 found on the game's signs
	uint32_t                                    glowLight_ = 15;     // set glowlight
	EntityTextures                              textures_;
	std::vector<PendingTexture>                 pendingTextures_;
	std::map<uint32_t, uint64_t>                textureReady_;     // each uploaded texture's fence
	gpu::ComPtr<ID3D12Resource>                 entityTextures_;   // default heap, entityTexels_ texels
	uint64_t                                    entityTexels_ = 0;
	bool                                        capLogged_ = false;
	uint64_t                                    texturesLeftOut_ = 0;
	std::optional<entities::EntityFrame>        pendingEntities_;  // SetEntities's, until Update copies it
	std::array<SnapshotGpu, kSnapshots>         snapshots_;
	Snapshots                                   snapshotRing_{ kSnapshots };
	std::array<std::optional<uint32_t>, kSlots> slotSnapshot_{};  // the snapshot each slot's list took
	gpu::ComPtr<ID3D12PipelineState>            entityPipeline_;
	gpu::ComPtr<ID3D12PipelineState>            entityDepthPipeline_;
	gpu::ComPtr<ID3D12PipelineState>            crackPipeline_;
	bool                                        entityReversed_ = false;
	bool                                        entityMotionWrites_ = false;
	bool                                        entityPipelinesFailed_ = false;  // until the shaders are reloaded
	bool                                        snapshotLogged_ = false;  // a snapshot buffer couldn't be made: logged once
	gpu::ComPtr<ID3D12QueryHeap>                entityQueries_;  // kSlots x 2 timestamps, around the entities' draws
	gpu::ComPtr<ID3D12Resource>                 entityTimings_;  // readback
	uint64_t*                                   entityTimingsMapped_ = nullptr;
	std::array<bool, kSlots>                    slotEntityTimed_{};
	double                                      entityGpuMsSum_ = 0.0;
	uint64_t                                    entityGpuSamples_ = 0;

	Stats    stats_;
```

- [ ] **Step 2: Edit `src/render/Renderer.cpp` to upload, copy and draw them**

```cpp
#include <algorithm>
```
```cpp
#include <algorithm>
#include <bit>
```
```cpp
#include <iterator>
```
```cpp
#include <iterator>
#include <utility>
```
```cpp
	hudPipeline_.Reset();
	hudShadersFailed_ = false;
```
```cpp
	hudPipeline_.Reset();
	entityPipeline_.Reset();
	entityDepthPipeline_.Reset();
	crackPipeline_.Reset();
	entityPipelinesFailed_ = false;
	hudShadersFailed_ = false;
```
```cpp
	}
	aError = std::format("there's no setting called \"{}\"", aName);
```
```cpp
	}
	if (aName == "entities")
	{
		if (aValue != 0.0 && aValue != 1.0)
		{
			aError = "entities is 1 to draw Minecraft's entities, or 0 to hide them";
			return false;
		}
		entitiesOn_ = aValue == 1.0;
		return true;
	}
	if (aName == "glow")
	{
		if (aValue < 0.0 || aValue > 1.0)
		{
			aError = "glow is from 0 to 1: what a glowing triangle writes into the G-buffer as emission";
			return false;
		}
		glow_ = float(aValue);
		return true;
	}
	if (aName == "glowlight")
	{
		if (aValue < 1.0 || aValue > 15.0 || aValue != std::floor(aValue))
		{
			aError = "glowlight is a whole number from 1 to 15: the block light from which a triangle glows";
			return false;
		}
		glowLight_ = uint32_t(aValue);
		return true;
	}
	aError = std::format("there's no setting called \"{}\"", aName);
```
```cpp
	pending_.Clear();
}
```
```cpp
	pending_.Clear();
}

void Renderer::AddEntityTexture(uint32_t aId, uint32_t aWidth, uint32_t aHeight, std::span<const uint8_t> aRgba)
{
	std::lock_guard                            lock(mutex_);
	const std::optional<EntityTextures::Place> place = textures_.Add(aId, aWidth, aHeight);
	if (!place)
	{
		++texturesLeftOut_;
		if (!capLogged_)
		{
			capLogged_ = true;
			log_.Push(std::format("renderer: Minecraft's texture {} ({} x {}) is left out, past the entity textures' 64 MB: what uses it isn't drawn",
			                      aId, aWidth, aHeight));
		}
		return;
	}
	textureReady_.erase(aId);  // placed again: drawn once this upload is done
	pendingTextures_.push_back({ aId, *place, std::vector<uint8_t>(aRgba.begin(), aRgba.end()) });
}

void Renderer::ClearEntityTextures()
{
	std::lock_guard lock(mutex_);
	textures_.Clear();
	pendingTextures_.clear();
	textureReady_.clear();
	capLogged_ = false;
	if (entityTextures_)
	{
		// Lists on their way may still read it, so the next texture starts a fresh buffer, and this one waits for them.
		Retire(entityTextures_);
		entityTextures_.Reset();
		entityTexels_ = 0;
	}
}

void Renderer::SetEntities(entities::EntityFrame aFrame)
{
	std::lock_guard lock(mutex_);
	pendingEntities_ = std::move(aFrame);
}

uint32_t Renderer::GlowLight() const
{
	std::lock_guard lock(mutex_);
	return glowLight_;
}
```
```cpp
	D3D12_ROOT_PARAMETER parameters[5]{};
	parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;  // b0: the frame
	parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	parameters[1].Constants = { 1, 0, 4 };  // b1: the section
	for (UINT i = 2; i < 5; ++i)            // t0 vertices, t1 atlas, t2 depth copy
```
```cpp
	D3D12_ROOT_PARAMETER parameters[6]{};
	parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;  // b0: the frame
	parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	parameters[1].Constants = { 1, 0, 8 };  // b1: a section's, the HUD's, or an entity draw's (spike 7)
	for (UINT i = 2; i < 6; ++i)            // t0 vertices, t1 atlas, t2 depth copy, t3 entity textures (spike 7)
```
```cpp
	const D3D12_ROOT_SIGNATURE_DESC rootDesc{ 5, parameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE };
```
```cpp
	const D3D12_ROOT_SIGNATURE_DESC rootDesc{ 6, parameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE };
```
```cpp
	timings_ = gpu::MakeBuffer(device_, kSlots * 2 * sizeof(uint64_t), D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST);
	if (FAILED(device_->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap_))) ||
	    FAILED(device_->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsvHeap_))) ||
	    FAILED(device_->CreateQueryHeap(&queryDesc, IID_PPV_ARGS(&queries_))) || !constants_ || !timings_ ||
	    FAILED(constants_->Map(0, nullptr, reinterpret_cast<void**>(&constantsMapped_))) ||
	    FAILED(timings_->Map(0, nullptr, reinterpret_cast<void**>(&timingsMapped_))) ||
```
```cpp
	timings_ = gpu::MakeBuffer(device_, kSlots * 2 * sizeof(uint64_t), D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST);
	entityTimings_ = gpu::MakeBuffer(device_, kSlots * 2 * sizeof(uint64_t), D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST);
	if (FAILED(device_->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap_))) ||
	    FAILED(device_->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsvHeap_))) ||
	    FAILED(device_->CreateQueryHeap(&queryDesc, IID_PPV_ARGS(&queries_))) ||
	    FAILED(device_->CreateQueryHeap(&queryDesc, IID_PPV_ARGS(&entityQueries_))) || !constants_ || !timings_ || !entityTimings_ ||
	    FAILED(constants_->Map(0, nullptr, reinterpret_cast<void**>(&constantsMapped_))) ||
	    FAILED(timings_->Map(0, nullptr, reinterpret_cast<void**>(&timingsMapped_))) ||
	    FAILED(entityTimings_->Map(0, nullptr, reinterpret_cast<void**>(&entityTimingsMapped_))) ||
```
```cpp
	timings_->SetName(L"BlockRender timings");
```
```cpp
	timings_->SetName(L"BlockRender timings");
	entityTimings_->SetName(L"BlockRender entity timings");
```
```cpp
bool Renderer::EnsureDepthTarget(uint32_t aWidth, uint32_t aHeight)
```
```cpp
bool Renderer::EnsureEntityPipelines(bool aReversed)
{
	if (entityPipeline_ && entityReversed_ == aReversed && entityMotionWrites_ == gbufferMotion_)
	{
		return true;
	}
	if (entityPipelinesFailed_ || !shaders_)
	{
		return false;
	}
	// Minecraft draws its entities two-sided (entity_cutout culls nothing) and with a greater-or-equal depth test, so a
	// later triangle on the same surface wins: a banner's patterns over its cloth, a villager's clothes over its body.
	const D3D12_COMPARISON_FUNC depthFunc = aReversed ? D3D12_COMPARISON_FUNC_GREATER_EQUAL : D3D12_COMPARISON_FUNC_LESS_EQUAL;
	// The models: into the G-buffer as the blocks are, their stencil class too.
	D3D12_GRAPHICS_PIPELINE_STATE_DESC models{};
	models.pRootSignature = rootSignature_.Get();
	models.VS = { shaders_->entityVertex.data(), shaders_->entityVertex.size() };
	models.PS = { shaders_->entity.data(), shaders_->entity.size() };
	models.BlendState.IndependentBlendEnable = TRUE;  // so the motion vectors alone can be left alone
	for (UINT i = 0; i < 4; ++i)
	{
		models.BlendState.RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		models.RTVFormats[i] = kGbufferFormats[i];
	}
	if (!gbufferMotion_)
	{
		models.BlendState.RenderTarget[3].RenderTargetWriteMask = 0;
	}
	models.SampleMask = UINT_MAX;
	models.RasterizerState = BlockRasterizer(0);
	models.DepthStencilState.DepthEnable = TRUE;
	models.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	models.DepthStencilState.DepthFunc = depthFunc;
	models.DepthStencilState.StencilEnable = TRUE;
	models.DepthStencilState.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
	models.DepthStencilState.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
	const D3D12_DEPTH_STENCILOP_DESC replace{ D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_REPLACE,
		                                      D3D12_COMPARISON_FUNC_ALWAYS };
	models.DepthStencilState.FrontFace = replace;
	models.DepthStencilState.BackFace = replace;
	models.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	models.NumRenderTargets = 4;
	models.DSVFormat = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
	models.SampleDesc.Count = 1;
	// Their depth, into the tagged depth: the same tests, depth only.
	D3D12_GRAPHICS_PIPELINE_STATE_DESC depth{};
	depth.pRootSignature = rootSignature_.Get();
	depth.VS = models.VS;
	depth.PS = { shaders_->entityDepth.data(), shaders_->entityDepth.size() };
	depth.SampleMask = UINT_MAX;
	depth.RasterizerState = BlockRasterizer(0);
	depth.DepthStencilState.DepthEnable = TRUE;
	depth.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	depth.DepthStencilState.DepthFunc = depthFunc;
	depth.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	depth.NumRenderTargets = 0;
	depth.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	depth.SampleDesc.Count = 1;
	// The cracks: the base colour's RGB alone, blended DEST_COLOR, SRC_COLOR as Minecraft's crumbling, twice the crack's
	// colour times what's there; depth tested, not written; the stencil left alone.
	D3D12_GRAPHICS_PIPELINE_STATE_DESC cracks{};
	cracks.pRootSignature = rootSignature_.Get();
	cracks.VS = models.VS;
	cracks.PS = { shaders_->crack.data(), shaders_->crack.size() };
	D3D12_RENDER_TARGET_BLEND_DESC& blend = cracks.BlendState.RenderTarget[0];
	blend.BlendEnable = TRUE;
	blend.SrcBlend = D3D12_BLEND_DEST_COLOR;
	blend.DestBlend = D3D12_BLEND_SRC_COLOR;
	blend.BlendOp = D3D12_BLEND_OP_ADD;
	blend.SrcBlendAlpha = D3D12_BLEND_ONE;
	blend.DestBlendAlpha = D3D12_BLEND_ZERO;
	blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	blend.LogicOp = D3D12_LOGIC_OP_NOOP;
	blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_RED | D3D12_COLOR_WRITE_ENABLE_GREEN | D3D12_COLOR_WRITE_ENABLE_BLUE;
	cracks.SampleMask = UINT_MAX;
	cracks.RasterizerState = BlockRasterizer(0);
	cracks.DepthStencilState.DepthEnable = TRUE;
	cracks.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	cracks.DepthStencilState.DepthFunc = depthFunc;
	cracks.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	cracks.NumRenderTargets = 1;
	cracks.RTVFormats[0] = kGbufferFormats[0];
	cracks.DSVFormat = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
	cracks.SampleDesc.Count = 1;
	gpu::ComPtr<ID3D12PipelineState> modelPipeline;
	gpu::ComPtr<ID3D12PipelineState> depthPipeline;
	gpu::ComPtr<ID3D12PipelineState> crackPipeline;
	if (FAILED(device_->CreateGraphicsPipelineState(&models, IID_PPV_ARGS(&modelPipeline))) ||
	    FAILED(device_->CreateGraphicsPipelineState(&depth, IID_PPV_ARGS(&depthPipeline))) ||
	    FAILED(device_->CreateGraphicsPipelineState(&cracks, IID_PPV_ARGS(&crackPipeline))))
	{
		entityPipelinesFailed_ = true;
		log_.Push("renderer: couldn't create the pipelines for Minecraft's entities, so they aren't drawn; the blocks still are");
		return false;
	}
	modelPipeline->SetName(L"BlockRender entities pipeline");
	depthPipeline->SetName(L"BlockRender entities' depth pipeline");
	crackPipeline->SetName(L"BlockRender cracks pipeline");
	entityPipeline_ = modelPipeline;
	entityDepthPipeline_ = depthPipeline;
	crackPipeline_ = crackPipeline;
	entityReversed_ = aReversed;
	entityMotionWrites_ = gbufferMotion_;
	return true;
}

bool Renderer::EnsureDepthTarget(uint32_t aWidth, uint32_t aHeight)
```
```cpp
		stats_.gpuMsWorst = std::max(stats_.gpuMsWorst, ms);
	}
}
```
```cpp
		stats_.gpuMsWorst = std::max(stats_.gpuMsWorst, ms);
	}
	if (std::exchange(slotEntityTimed_[aSlot], false))
	{
		const uint64_t entityBegin = entityTimingsMapped_[aSlot * 2];
		const uint64_t entityEnd = entityTimingsMapped_[aSlot * 2 + 1];
		if (entityEnd > entityBegin && timestampFrequency_ > 0)
		{
			entityGpuMsSum_ += double(entityEnd - entityBegin) * 1000.0 / double(timestampFrequency_);
			++entityGpuSamples_;
		}
	}
}
```
```cpp
	Upload();
```
```cpp
	Upload();
	CopyEntities();
```
```cpp
	if ((pending_.Size() == 0 && !atlasDue) || completed < uploadFence_)
```
```cpp
	if ((pending_.Size() == 0 && !atlasDue && pendingTextures_.empty()) || completed < uploadFence_)
```
```cpp
		}
	}
	original.close(uploadList_.Get());
```
```cpp
		}
	}
	UploadEntityTextures(ready, staging, failed);
	original.close(uploadList_.Get());
```
```cpp
		log_.Push("renderer: couldn't create some upload buffers; those sections aren't drawn");
	}
```
```cpp
		log_.Push("renderer: couldn't create some upload buffers; the sections or entity textures they held aren't drawn");
	}
}

void Renderer::UploadEntityTextures(uint64_t aReady, std::vector<gpu::ComPtr<ID3D12Resource>>& aStaging, bool& aFailed)
{
	if (pendingTextures_.empty())
	{
		return;
	}
	// The one buffer, grown first if the textures don't fit: the old texels copied over on the GPU, and every texture in
	// it drawn again once the copy is done.
	const uint64_t capacity = textures_.Capacity(entityTexels_);
	if (capacity > entityTexels_)
	{
		gpu::ComPtr<ID3D12Resource> grown = gpu::MakeBuffer(device_, capacity * 4, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON);
		if (!grown)
		{
			aFailed = true;
			return;  // they wait for the next upload
		}
		grown->SetName(L"BlockRender entity textures");
		if (entityTextures_)
		{
			// Only the texels uploaded before, which the waiting textures' places follow: copies in one list aren't
			// ordered, so a copy over their places could land after theirs.
			const uint64_t uploaded = std::min<uint64_t>(pendingTextures_.front().place.start, entityTexels_);
			if (uploaded > 0)
			{
				uploadList_->CopyBufferRegion(grown.Get(), 0, entityTextures_.Get(), 0, uploaded * 4);
			}
			// This upload reads the old buffer, so it's kept until the upload is done, not only the lists before it, or
			// until the list of ours on its way to the GPU is, which comes later.
			retired_.Retire(std::move(entityTextures_), aReady);
			for (auto& [id, ready] : textureReady_)
			{
				ready = aReady;
			}
		}
		entityTextures_ = grown;
		entityTexels_ = capacity;
	}
	for (const PendingTexture& texture : pendingTextures_)
	{
		const uint64_t bytes = texture.rgba.size();
		auto           source = gpu::MakeBuffer(device_, bytes, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
		void*          mapped = nullptr;
		if (!source || FAILED(source->Map(0, nullptr, &mapped)))
		{
			aFailed = true;
			continue;  // never ready, so what uses it is skipped
		}
		std::memcpy(mapped, texture.rgba.data(), bytes);
		source->Unmap(0, nullptr);
		uploadList_->CopyBufferRegion(entityTextures_.Get(), uint64_t(texture.place.start) * 4, source.Get(), 0, bytes);
		aStaging.push_back(source);
		textureReady_[texture.id] = aReady;
	}
	pendingTextures_.clear();
}

void Renderer::CopyEntities()
{
	if (!pendingEntities_)
	{
		return;
	}
	if (!fence_ || mode_.load() != Mode::Gbuffer || !entitiesOn_)
	{
		pendingEntities_.reset();  // drawn only into the G-buffer
		return;
	}
	const std::optional<uint32_t> index = snapshotRing_.Writable(fence_->GetCompletedValue());
	if (!index)
	{
		++stats_.snapshotWaits;  // every snapshot is held: the next frame's geometry replaces this one's
		return;
	}
	const entities::EntityFrame& frame = *pendingEntities_;
	SnapshotGpu&                 snapshot = snapshots_[*index];
	const uint64_t               bytes = std::min<uint64_t>(frame.vertices.size() * sizeof(Vertex), kSnapshotMaxBytes);
	if (!snapshot.buffer || snapshot.bytes < bytes)
	{
		// No list of ours reads this one (Writable), so it can go at once.
		if (snapshot.mapped)
		{
			snapshot.buffer->Unmap(0, nullptr);
		}
		snapshot = {};
		const uint64_t size = std::max<uint64_t>(std::bit_ceil(bytes), 64ull << 10);
		snapshot.buffer = gpu::MakeBuffer(device_, size, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
		void* mapped = nullptr;
		if (!snapshot.buffer || FAILED(snapshot.buffer->Map(0, nullptr, &mapped)))
		{
			snapshot = {};
			pendingEntities_.reset();
			if (!snapshotLogged_)
			{
				snapshotLogged_ = true;
				log_.Push("renderer: couldn't create a snapshot buffer for Minecraft's entities, so they're skipped until one can be");
			}
			return;
		}
		snapshot.buffer->SetName(L"BlockRender entity snapshot");
		snapshot.mapped = static_cast<uint8_t*>(mapped);
		snapshot.bytes = size;
	}
	if (bytes > 0)
	{
		std::memcpy(snapshot.mapped, frame.vertices.data(), size_t(bytes));
	}
	// A frame over 8 MB keeps its draws in order while they fit; the rest are counted.
	const uint64_t fit = bytes / sizeof(Vertex);
	snapshot.origin = frame.origin;
	snapshot.draws.clear();
	for (const entities::EntityDraw& draw : frame.draws)
	{
		if (uint64_t(draw.first) + draw.count <= fit)
		{
			snapshot.draws.push_back(draw);
		}
		else
		{
			++stats_.snapshotCut;
		}
	}
	snapshotRing_.Written(*index);
	pendingEntities_.reset();
```
```cpp
	}
	stats_ = {};
	gpuMsSum_ = frameMsSum_ = updateMsSum_ = hudCopyMsSum_ = hudGpuMsSum_ = 0.0;
	gpuSamples_ = frameSamples_ = updateSamples_ = hudCopySamples_ = hudGpuSamples_ = 0;
```
```cpp
	}
	stats.entitiesOn = entitiesOn_;
	stats.entityTextures = textures_.Count();
	stats.entityTextureMb = double(textures_.Used()) * 4.0 / 1048576.0;
	stats.entityTexturesLeftOut = texturesLeftOut_;
	stats.entityGpuMsAverage = entityGpuSamples_ ? entityGpuMsSum_ / double(entityGpuSamples_) : -1.0;
	stats_ = {};
	gpuMsSum_ = frameMsSum_ = updateMsSum_ = hudCopyMsSum_ = hudGpuMsSum_ = entityGpuMsSum_ = 0.0;
	gpuSamples_ = frameSamples_ = updateSamples_ = hudCopySamples_ = hudGpuSamples_ = entityGpuSamples_ = 0;
```
```cpp
	constants.material[2] = materialB_;
```
```cpp
	constants.material[2] = materialB_;
	constants.entity[0] = glow_;
	constants.entity[1] = float(glowLight_);
```
```cpp
		d3d::Original().drawInstanced(aList, gpuSection.vertices, 1, 0, 0);
```
```cpp
		d3d::Original().drawInstanced(aList, gpuSection.vertices, 1, 0, 0);
	}
}

void Renderer::DrawEntities(ID3D12GraphicsCommandList* aList, const CameraFrame& aCamera, uint64_t aCompleted, const SnapshotGpu& aSnapshot,
                            entities::EntityDraw::Kind aKind, bool aCount)
{
	// The snapshot's positions are relative to its origin, a whole block: camera-relative, as a section's corner is.
	const Vec3d     corner = WorldFromMc(aSnapshot.origin);
	EntityConstants constants{};
	constants.offset[0] = float(corner.x - aCamera.origin.x);
	constants.offset[1] = float(corner.y - aCamera.origin.y);
	constants.offset[2] = float(corner.z - aCamera.origin.z);
	aList->SetGraphicsRootShaderResourceView(2, aSnapshot.buffer->GetGPUVirtualAddress());
	aList->SetGraphicsRootShaderResourceView(5, (entityTextures_ ? entityTextures_ : atlasBuffer_)->GetGPUVirtualAddress());
	for (const entities::EntityDraw& draw : aSnapshot.draws)
	{
		if (draw.kind != aKind)
		{
			continue;
		}
		constants.first = draw.first;
		constants.textureStart = 0;  // Minecraft's combined atlas
		constants.textureWidth = 0;
		constants.textureHeight = 0;
		if (draw.texture != 0)
		{
			const EntityTextures::Place* place = textures_.Find(draw.texture);
			const auto                   ready = textureReady_.find(draw.texture);
			if (!place || !entityTextures_ || ready == textureReady_.end() || ready->second > aCompleted)
			{
				stats_.entityDrawsSkipped += aCount ? 1 : 0;  // not on the GPU yet, or left out at the cap
				continue;
			}
			constants.textureStart = place->start;
			constants.textureWidth = place->width;
			constants.textureHeight = place->height;
		}
		aList->SetGraphicsRoot32BitConstants(1, 8, &constants, 0);
		d3d::Original().drawInstanced(aList, draw.count, 1, 0, 0);
```
```cpp
	list->SetGraphicsRootShaderResourceView(3, atlasBuffer_->GetGPUVirtualAddress());
	list->SetGraphicsRootShaderResourceView(4, atlasBuffer_->GetGPUVirtualAddress());  // t2: only step 2's shader reads it
	DrawSections(list, *camera, completed);
	const std::vector<D3D12_RESOURCE_BARRIER> back = Barriers(MovesBack(into));
```
```cpp
	list->SetGraphicsRootShaderResourceView(3, atlasBuffer_->GetGPUVirtualAddress());
	list->SetGraphicsRootShaderResourceView(4, atlasBuffer_->GetGPUVirtualAddress());  // t2: only step 2's shader reads it
	list->SetGraphicsRootShaderResourceView(5, atlasBuffer_->GetGPUVirtualAddress());  // t3: only the entities' read it
	DrawSections(list, *camera, completed);
	const std::vector<D3D12_RESOURCE_BARRIER> back = Barriers(MovesBack(into));
```
```cpp
	list->SetGraphicsRootShaderResourceView(4, atlasBuffer_->GetGPUVirtualAddress());  // t2: only step 2's shader reads it
	DrawSections(list, *camera, completed);
```
```cpp
	list->SetGraphicsRootShaderResourceView(4, atlasBuffer_->GetGPUVirtualAddress());  // t2: only step 2's shader reads it
	list->SetGraphicsRootShaderResourceView(5, atlasBuffer_->GetGPUVirtualAddress());  // t3: until the entities' own
	DrawSections(list, *camera, completed);
	// Spike 7: Minecraft's entities after the blocks, from the newest snapshot, if it has any: its models, then their
	// cracks over the base colour alone. The snapshot is held until the fence after this list.
	const std::optional<uint32_t> newest = snapshotRing_.Newest();
	const std::optional<uint32_t> snapshot = entitiesOn_ && newest && !snapshots_[*newest].draws.empty() && EnsureEntityPipelines(camera->reversedDepth)
	                                             ? snapshotRing_.TakeNewest()
	                                             : std::nullopt;
	if (snapshot)
	{
		const SnapshotGpu& shot = snapshots_[*snapshot];
		list->EndQuery(entityQueries_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, slot * 2);
		list->SetPipelineState(entityPipeline_.Get());
		DrawEntities(list, *camera, completed, shot, entities::EntityDraw::Kind::Model, true);
		original.omSetRenderTargets(list, 1, rtvs, FALSE, &gbufferDsv);
		list->SetPipelineState(crackPipeline_.Get());
		DrawEntities(list, *camera, completed, shot, entities::EntityDraw::Kind::Crack, true);
		list->EndQuery(entityQueries_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, slot * 2 + 1);
		list->ResolveQueryData(entityQueries_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, slot * 2, 2, entityTimings_.Get(), slot * 2 * sizeof(uint64_t));
	}
```
```cpp
		DrawSections(list, *camera, completed);
```
```cpp
		DrawSections(list, *camera, completed);
		if (snapshot)
		{
			list->SetPipelineState(entityDepthPipeline_.Get());
			DrawEntities(list, *camera, completed, snapshots_[*snapshot], entities::EntityDraw::Kind::Model, false);
		}
```
```cpp
	}
	list->EndQuery(queries_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, slot * 2 + 1);
	list->ResolveQueryData(queries_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, slot * 2, 2, timings_.Get(), slot * 2 * sizeof(uint64_t));
	if (FAILED(original.close(list)))
	{
		Fail("couldn't close a command list");
		return nullptr;
	}
	++recordings_;
	if (gbufferDepth_ && !intoTaggedDepth)
```
```cpp
	}
	list->EndQuery(queries_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, slot * 2 + 1);
	list->ResolveQueryData(queries_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, slot * 2, 2, timings_.Get(), slot * 2 * sizeof(uint64_t));
	if (FAILED(original.close(list)))
	{
		if (snapshot)
		{
			snapshotRing_.Abandon(*snapshot);
		}
		Fail("couldn't close a command list");
		return nullptr;
	}
	++recordings_;
	if (gbufferDepth_ && !intoTaggedDepth)
```
```cpp
	}
	t_insertedSlot = slot;
```
```cpp
	}
	slotSnapshot_[slot] = snapshot;
	slotEntityTimed_[slot] = snapshot.has_value();
	stats_.entityFrames += snapshot ? 1 : 0;
	t_insertedSlot = slot;
```
```cpp
	retired_.ListSubmitted(slotFence_[slot]);
```
```cpp
	retired_.ListSubmitted(slotFence_[slot]);
	if (const std::optional<uint32_t> snapshot = std::exchange(slotSnapshot_[slot], std::nullopt))
	{
		snapshotRing_.Submitted(*snapshot, slotFence_[slot]);
	}
```
```cpp
	list->SetGraphicsRootShaderResourceView(4, (submitted_.depth ? depthCopy_ : atlasBuffer_)->GetGPUVirtualAddress());
```
```cpp
	list->SetGraphicsRootShaderResourceView(4, (submitted_.depth ? depthCopy_ : atlasBuffer_)->GetGPUVirtualAddress());
	list->SetGraphicsRootShaderResourceView(5, atlasBuffer_->GetGPUVirtualAddress());  // t3: only the entities' read it
```
```cpp
	for (UINT parameter = 2; parameter < 5; ++parameter)  // t0 to t2: the image, which HudPS reads as t1
```
```cpp
	for (UINT parameter = 2; parameter < 6; ++parameter)  // t0 to t3: the image, which HudPS reads as t1
```
```cpp
	watch_.reset();
	retired_.Clear();
```
```cpp
	watch_.reset();
	for (SnapshotGpu& snapshot : snapshots_)
	{
		if (snapshot.mapped)
		{
			snapshot.buffer->Unmap(0, nullptr);
		}
		snapshot = {};
	}
	snapshotRing_ = Snapshots(kSnapshots);
	slotSnapshot_ = {};
	pendingEntities_.reset();
	pendingTextures_.clear();
	textureReady_.clear();
	textures_.Clear();
	entityTextures_.Reset();
	entityTexels_ = 0;
	entityPipeline_.Reset();
	entityDepthPipeline_.Reset();
	crackPipeline_.Reset();
	if (entityTimingsMapped_)
	{
		entityTimings_->Unmap(0, nullptr);
		entityTimingsMapped_ = nullptr;
	}
	entityTimings_.Reset();
	entityQueries_.Reset();
	retired_.Clear();
```

- [ ] **Step 3: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `489 test(s), 0 failure(s)`.

- [ ] **Step 4: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "minecraft-entities: the renderer draws Minecraft's entities into the G-buffer: their textures in one buffer, a snapshot a frame, their models, cracks and depth" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 14: BlockRender 0.20.0 hands Minecraft's entities to the renderer

The plugin's side (the spec's "The frame", "Requests and settings" and "Measurements and the status"):
- **The sink** takes the link's textures (into the renderer, and their sizes kept for the request), scenes and tables
  (into `Entities`). At Minecraft's clear it drops the textures, the scene and the sizes, and logs `Minecraft cleared
  its sections and textures: it sends everything again`; Unlink and the `clear` request keep them, since SkyCraft sends
  each texture once.
- **Each frame,** after the link's update: `Entities` learns whether Minecraft is in its world, and in `gbuffer` mode
  the renderer gets the frame's geometry (`SetEntities`), with Minecraft's eye for its origin when there's no scene.
- **The `entities` request** writes the newest scene batch by batch, the textures and the world entities into the log
  (`Entities::Lines`).
- **The status's `entities` line,** after `blasts`: on or off, scenes and MB a second, the newest scene's batches and
  vertices, those glowing from `glowlight` and those translucent, the textures and their MB, those left out, the frames
  drawn and draws skipped, the GPU time, the world entities by kind (`WorldWords`, moved from `mclink` into
  `link/WorldTable`), and the nearest mob from Minecraft's player's feet; snapshot waits and cuts when there are any.
  The overlay shows it from Task 15.
- **The version** goes to 0.20.0.

**Files:**
- Modify: `tests/TestEntities.cpp`, `tests/TestRequests.cpp`, `tests/TestStatus.cpp`, `tests/TestWorldTable.cpp`,
  `src/link/WorldTable.h`, `src/link/WorldTable.cpp`, `tools/mclink/Main.cpp`, `src/entities/Entities.h`,
  `src/entities/Entities.cpp`, `src/Requests.h`, `src/Requests.cpp`, `src/Status.h`, `src/Status.cpp`, `src/Plugin.cpp`,
  `CMakeLists.txt`

- [ ] **Step 1: Edit `tests/TestEntities.cpp` to add the failing test of the request's lines**

```cpp
TEST(EntitiesFindTheNearestMob)
```
```cpp
TEST(EntitiesDescribeTheNewestScene)
{
	Entities entities;
	CHECK_STR(entities.Lines(15, {}).front(), "no scene: nothing of Minecraft's is there");
	std::vector<Vertex> vertices = Vertices(9);
	vertices[3].light = 15 | (15u << 8);
	entities.Scene(Scene(vertices, { { 0, 0, 3, 0 }, { 5, 3, 6, 1 } }, kOrigin));
	link::WorldTable table;
	table.entities = { Record(proto::kWeShadow, 885.5f, 6.12f, 2007.5f) };
	entities.World(table);
	const std::vector<std::string> lines = entities.Lines(15, { { 5, { 64, 64 } } });
	CHECK_EQ(lines.size(), size_t(4));
	if (lines.size() == 4)
	{
		CHECK_STR(lines[0], "the newest scene, from 880 7 2007: 2 batches, 9 vertices (1 glowing, 6 translucent)");
		CHECK_STR(lines[1], "  batch 0: the atlas, 3 vertices from 0, 0 glowing");
		CHECK_STR(lines[2], "  batch 1: texture 5 (64 x 64), 6 vertices from 3, translucent, 1 glowing");
		CHECK_STR(lines[3], "world: 1 shadow");
	}
}

TEST(EntitiesFindTheNearestMob)
```

- [ ] **Step 2: Edit `tests/TestRequests.cpp` to add the failing test of the request**

```cpp
TEST(RequestsForBlasts)
```
```cpp
TEST(RequestsForTheEntities)
{
	std::string error;
	const auto  entities = cpmc::ParseRequest("entities", error);
	CHECK(entities && entities->kind == cpmc::Request::Kind::Entities);
	CHECK(!cpmc::ParseRequest("entities now", error));
	CHECK_STR(error, "entities takes nothing after it");
	for (const char* setting : { "set entities 0", "set glow 0.5", "set glowlight 14" })
	{
		const auto set = cpmc::ParseRequest(setting, error);
		CHECK(set && set->kind == cpmc::Request::Kind::Set);
	}
}

TEST(RequestsForBlasts)
```

- [ ] **Step 3: Edit `tests/TestStatus.cpp` to add the failing test of the line**

```cpp
	CHECK(Has(status, "0 unreadable; the bush rule leaves out sight-only shapes: 12 so far"));
}
```
```cpp
	CHECK(Has(status, "0 unreadable; the bush rule leaves out sight-only shapes: 12 so far"));
}

TEST(StatusWritesTheEntities)
{
	cpmc::StatusReport report;
	cpmc::LinkReport   link;
	report.link = link;
	std::string status = cpmc::FormatStatus(report);
	CHECK(Has(status, "entities=on: 0 scenes a second (0.00 MB/s); the newest 0 batches, 0 vertices, 0 glowing from block light 15, 0 "
	                  "translucent; 0 textures (0.00 MB), 0 left out; drawn in 0 frames, 0 draws skipped, GPU not measured; world: nothing"));
	CHECK(status.find("blasts=") < status.find("entities=") && status.find("entities=") < status.find("ring="));
	link.scenesPerSecond = 60.0;
	link.sceneMbPerSecond = 7.25;
	link.sceneBatches = 12;
	link.sceneVertices = 4152;
	link.sceneGlowing = 720;
	link.sceneTranslucent = 78;
	link.glowLight = 14;
	link.entityTextures = 14;
	link.entityTextureMb = 3.64;
	link.entityFrames = 59;
	link.entityDrawsSkipped = 2;
	link.entityGpuMsAverage = 0.05;
	link.worldEntities = "1 arrow, 1 block, 1 item, 4 shadows";
	link.nearestMob = 5.04;
	link.snapshotWaits = 1;
	link.snapshotCut = 3;
	report.link = link;
	CHECK(Has(cpmc::FormatStatus(report),
	          "entities=on: 60 scenes a second (7.25 MB/s); the newest 12 batches, 4152 vertices, 720 glowing from block light 14, 78 "
	          "translucent; 14 textures (3.64 MB), 0 left out; drawn in 59 frames, 2 draws skipped, GPU 0.050 ms average; world: 1 arrow, "
	          "1 block, 1 item, 4 shadows; the nearest mob 5.0 m away; 1 frames waited for a snapshot; 3 draws past a snapshot's 8 MB"));
	link.entitiesOn = false;
	report.link = link;
	CHECK(cpmc::FormatStatus(report).find("\nentities=off: 60 scenes a second") != std::string::npos);
}
```

- [ ] **Step 4: Edit `tests/TestWorldTable.cpp` to add the failing test of the table in words**

```cpp
TEST(WorldTableNamesTheKinds)
```
```cpp
TEST(WorldTableInWords)
{
	WorldTable table;
	CHECK_STR(WorldWords(table), "nothing");
	table.entities = { Record(proto::kWeShadow, 1), Record(proto::kWeItem, 2), Record(proto::kWeShadow, 3), Record(proto::kWeArrow, 4) };
	CHECK_STR(WorldWords(table), "1 arrow, 1 item, 2 shadows");
	table.outline = std::array<float, 6>{};
	CHECK_STR(WorldWords(table), "1 arrow, 1 item, 2 shadows; an outline");
	table.entities.clear();
	CHECK_STR(WorldWords(table), "an outline");
}

TEST(WorldTableNamesTheKinds)
```

- [ ] **Step 5: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `'WorldWords': identifier not found`.

- [ ] **Step 6: Edit `src/link/WorldTable.h` to declare `WorldWords`**

```cpp
#include <optional>
```
```cpp
#include <optional>
#include <string>
```
```cpp
const char* WorldKindName(uint32_t aKind);
```
```cpp
const char* WorldKindName(uint32_t aKind);

// The table's records counted by kind, the kinds in alphabetical order, and its outline: "1 arrow, 2 items; an
// outline", or "nothing".
std::string WorldWords(const WorldTable& aTable);
```

- [ ] **Step 7: Edit `src/link/WorldTable.cpp` to add it**

```cpp
#include <cmath>
```
```cpp
#include <cmath>
#include <format>
#include <map>
```
```cpp
	default: return "other";
	}
}
}
```
```cpp
	default: return "other";
	}
}

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
}
```

- [ ] **Step 8: Edit `tools/mclink/Main.cpp` to drop its own**

```cpp
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
```
```cpp
```

- [ ] **Step 9: Edit `src/entities/Entities.h` to declare the request's lines**

```cpp
#include <cstdint>
#include <optional>
```
```cpp
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>
```
```cpp
	std::optional<double> NearestMob(const Vec3d& aFeet) const;
```
```cpp
	std::optional<double> NearestMob(const Vec3d& aFeet) const;
	// The entities request's lines (the spec's "Requests and settings"): the newest scene, then each of its batches with
	// its texture (and its size, if aSizes has it, by id), vertices and those glowing, then the world entities by kind.
	std::vector<std::string> Lines(uint32_t aGlowLight, const std::map<uint32_t, std::pair<uint32_t, uint32_t>>& aSizes) const;
```

- [ ] **Step 10: Edit `src/entities/Entities.cpp` to add them**

```cpp
#include <cmath>
```
```cpp
#include <cmath>
#include <format>
```
```cpp
	return nearest;
}
}
```
```cpp
	return nearest;
}

std::vector<std::string> Entities::Lines(uint32_t aGlowLight, const std::map<uint32_t, std::pair<uint32_t, uint32_t>>& aSizes) const
{
	std::vector<std::string> lines;
	const SceneCounts        counts = Counts(aGlowLight);
	lines.push_back(origin_ ? std::format("the newest scene, from {:.0f} {:.0f} {:.0f}: {} batches, {} vertices ({} glowing, {} translucent)",
	                                      origin_->x, origin_->y, origin_->z, counts.batches, counts.vertices, counts.glowing, counts.translucent)
	                        : std::string("no scene: nothing of Minecraft's is there"));
	for (size_t b = 0; b < batches_.size(); ++b)
	{
		const link::SceneBatch& batch = batches_[b];
		const auto              size = aSizes.find(batch.texture);
		const std::string       texture = batch.texture == 0       ? std::string("the atlas")
		                                  : size != aSizes.end() ? std::format("texture {} ({} x {})", batch.texture, size->second.first,
		                                                                       size->second.second)
		                                                         : std::format("texture {}", batch.texture);
		uint32_t glowing = 0;
		for (uint64_t i = batch.first; i < uint64_t(batch.first) + batch.count && i < vertices_.size(); ++i)
		{
			glowing += (vertices_[i].light & 0xFF) >= aGlowLight ? 1 : 0;
		}
		lines.push_back(std::format("  batch {}: {}, {} vertices from {}{}, {} glowing", b, texture, batch.count, batch.first,
		                            (batch.flags & 1) ? ", translucent" : "", glowing));
	}
	lines.push_back("world: " + link::WorldWords(world_));
	return lines;
}
}
```

- [ ] **Step 11: Edit `src/Requests.h` to add the request**

```cpp
//                               with its radius, over 0 and up to 16)
```
```cpp
//                               with its radius, over 0 and up to 16)
//   entities                   (spike 7: the newest scene batch by batch, the textures and the world entities, into the
//                               log)
```
```cpp
		Explode,
```
```cpp
		Explode,
		Entities,
```

- [ ] **Step 12: Edit `src/Requests.cpp` to parse it**

```cpp
	    command == "punch")
```
```cpp
	    command == "punch" || command == "entities")
```
```cpp
		const Request::Kind kind = command == "log"      ? Request::Kind::Log
		                           : command == "clear"  ? Request::Kind::Clear
		                           : command == "floor"  ? Request::Kind::Floor
		                           : command == "shapes" ? Request::Kind::Shapes
		                           : command == "npcs"   ? Request::Kind::Npcs
		                           : command == "punch"  ? Request::Kind::Punch
		                                                 : Request::Kind::Reload;
```
```cpp
		const Request::Kind kind = command == "log"        ? Request::Kind::Log
		                           : command == "clear"    ? Request::Kind::Clear
		                           : command == "floor"    ? Request::Kind::Floor
		                           : command == "shapes"   ? Request::Kind::Shapes
		                           : command == "npcs"     ? Request::Kind::Npcs
		                           : command == "punch"    ? Request::Kind::Punch
		                           : command == "entities" ? Request::Kind::Entities
		                                                   : Request::Kind::Reload;
```

- [ ] **Step 13: Edit `src/Status.h` to add the line's figures**

```cpp
	std::string lastBlast;           // its centre, reach, NPCs within it and its own hits; after its push, knocked down and dead
```
```cpp
	std::string lastBlast;           // its centre, reach, NPCs within it and its own hits; after its push, knocked down and dead
	// Minecraft's entities (spike 7)
	bool                  entitiesOn = true;        // set entities
	double                scenesPerSecond = 0.0;    // in the last second
	double                sceneMbPerSecond = 0.0;
	size_t                sceneBatches = 0;         // the newest scene's
	uint32_t              sceneVertices = 0;
	uint32_t              sceneGlowing = 0;         // of those, with block light of at least glowLight
	uint32_t              sceneTranslucent = 0;     // in translucent batches
	uint32_t              glowLight = 15;           // set glowlight
	size_t                entityTextures = 0;       // placed: uploaded, or waiting to be
	double                entityTextureMb = 0.0;
	uint64_t              entityTexturesLeftOut = 0;  // at the cap, since the start
	uint64_t              entityFrames = 0;         // G-buffer lists that drew them, in the last second
	uint64_t              entityDrawsSkipped = 0;   // their texture not on the GPU yet, or left out; in the last second
	uint64_t              snapshotWaits = 0;        // frames whose geometry waited for a free snapshot, in the last second
	uint64_t              snapshotCut = 0;          // draws past a snapshot's 8 MB, in the last second
	double                entityGpuMsAverage = -1.0;  // their draws, on the GPU; below 0: not measured
	std::string           worldEntities = "nothing";  // the world-entity table's records by kind, and its outline
	std::optional<double> nearestMob;               // the nearest player's or mob's feet from Minecraft's player, metres
```
```cpp
// drive, v, positions, ground, solids, bodies, npcs, combat, blasts, ring, atlas, hud, latency, burst and processes); then
// one structure line per structure:
```
```cpp
// drive, v, positions, ground, solids, bodies, npcs, combat, blasts, entities, ring, atlas, hud, latency, burst and
// processes); then one structure line per structure:
```

- [ ] **Step 14: Edit `src/Status.cpp` to write the line**

```cpp
std::string FormatLink(const LinkReport& aLink)
```
```cpp
// Minecraft's entities (spike 7): the scenes coming, the newest, the textures, what's drawn, and the world entities.
std::string FormatEntities(const LinkReport& aLink)
{
	std::string out = std::format("entities={}: {:.0f} scenes a second ({:.2f} MB/s); the newest {} batches, {} vertices, {} glowing from "
	                              "block light {}, {} translucent; {} textures ({:.2f} MB), {} left out; drawn in {} frames, {} draws "
	                              "skipped, GPU {}; world: {}",
	                              aLink.entitiesOn ? "on" : "off", aLink.scenesPerSecond, aLink.sceneMbPerSecond, aLink.sceneBatches,
	                              aLink.sceneVertices, aLink.sceneGlowing, aLink.glowLight, aLink.sceneTranslucent, aLink.entityTextures,
	                              aLink.entityTextureMb, aLink.entityTexturesLeftOut, aLink.entityFrames, aLink.entityDrawsSkipped,
	                              MsAverage(aLink.entityGpuMsAverage), OneLine(aLink.worldEntities));
	if (aLink.nearestMob)
	{
		out += std::format("; the nearest mob {:.1f} m away", *aLink.nearestMob);
	}
	if (aLink.snapshotWaits > 0)
	{
		out += std::format("; {} frames waited for a snapshot", aLink.snapshotWaits);
	}
	if (aLink.snapshotCut > 0)
	{
		out += std::format("; {} draws past a snapshot's 8 MB", aLink.snapshotCut);
	}
	return out + "\n";
}

std::string FormatLink(const LinkReport& aLink)
```
```cpp
	out += FormatBlasts(aLink);
```
```cpp
	out += FormatBlasts(aLink);
	out += FormatEntities(aLink);
```

- [ ] **Step 15: Edit `src/Plugin.cpp` for the sink, each frame's entities, the request and the line**

```cpp
constexpr const char* kVersion = "0.19.1";
```
```cpp
constexpr const char* kVersion = "0.20.0";
```
```cpp
// Spike 6's Minecraft blasts (its spec's "Part 3"), and V's health share for Minecraft's player (its follow-up). Main
```
```cpp
// Spike 7: Minecraft's entities (its spec's "The frame"): the newest scene and world-entity table, made into each frame's
// geometry for the renderer, and the textures' sizes, for the entities request. Main thread only.
cpmc::entities::Entities                          g_entities;
std::map<uint32_t, std::pair<uint32_t, uint32_t>> g_entityTextureSizes;  // by id: width and height

// Spike 6's Minecraft blasts (its spec's "Part 3"), and V's health share for Minecraft's player (its follow-up). Main
```
```cpp
	uint64_t moduleCalls = 0;
```
```cpp
	uint64_t moduleCalls = 0;
	// Spike 7's entities
	uint64_t scenesAtStart = 0;
	uint64_t sceneBytesAtStart = 0;
```
```cpp
		::Log("Minecraft cleared its sections: it sends everything again");
	}
```
```cpp
		g_renderer->ClearEntityTextures();
		g_entities.Clear();
		g_entityTextureSizes.clear();
		::Log("Minecraft cleared its sections and textures: it sends everything again");
	}

	// Spike 7: Minecraft's entity textures, its scenes of entities and particles, and its world-entity table.
	void Texture(const cpmc::link::TextureImage& aTexture) override
	{
		g_entityTextureSizes[aTexture.id] = { aTexture.width, aTexture.height };
		g_renderer->AddEntityTexture(aTexture.id, aTexture.width, aTexture.height, aTexture.rgba);
	}

	void Scene(const cpmc::link::SceneMessage& aScene) override { g_entities.Scene(aScene); }
	void WorldEntities(const cpmc::link::WorldTable& aTable) override { g_entities.World(aTable); }
```
```cpp
// The command in aFolder's blockrender.request, if there is one: in the plugin's folder, where the agent writes it, or
```
```cpp
// entities (spike 7): the newest scene batch by batch, the textures and the world entities, into the log.
void EntitiesRequest()
{
	for (const std::string& line : g_entities.Lines(g_renderer->GlowLight(), g_entityTextureSizes))
	{
		Log("entities: {}", line);
	}
	std::string textures;
	for (const auto& [id, size] : g_entityTextureSizes)
	{
		textures += std::format("{}{} ({} x {})", textures.empty() ? "" : ", ", id, size.first, size.second);
	}
	Log("entities: {} textures: {}", g_entityTextureSizes.size(), textures.empty() ? "none" : textures);
	Report("entities: the newest scene, the textures and the world entities, in the log");
}

// The command in aFolder's blockrender.request, if there is one: in the plugin's folder, where the agent writes it, or
```
```cpp
	case cpmc::Request::Kind::Explode: ExplodeRequest(*request); break;
```
```cpp
	case cpmc::Request::Kind::Explode: ExplodeRequest(*request); break;
	case cpmc::Request::Kind::Entities: EntitiesRequest(); break;
```
```cpp
	report.lastBlast = g_lastBlast;
```
```cpp
	report.lastBlast = g_lastBlast;
	// Spike 7's entities, Minecraft's player's feet standing for V's
	const uint32_t                              glowLight = g_renderer->GlowLight();
	const cpmc::entities::Entities::SceneCounts counts = g_entities.Counts(glowLight);
	const double                                entitySeconds = std::max(aNow - g_second.start, 0.001);
	report.entitiesOn = aStats.entitiesOn;
	report.scenesPerSecond = double(g_entities.Scenes() - g_second.scenesAtStart) / entitySeconds;
	report.sceneMbPerSecond = double(g_entities.SceneBytes() - g_second.sceneBytesAtStart) / 1048576.0 / entitySeconds;
	report.sceneBatches = counts.batches;
	report.sceneVertices = counts.vertices;
	report.sceneGlowing = counts.glowing;
	report.sceneTranslucent = counts.translucent;
	report.glowLight = glowLight;
	report.entityTextures = aStats.entityTextures;
	report.entityTextureMb = aStats.entityTextureMb;
	report.entityTexturesLeftOut = aStats.entityTexturesLeftOut;
	report.entityFrames = aStats.entityFrames;
	report.entityDrawsSkipped = aStats.entityDrawsSkipped;
	report.snapshotWaits = aStats.snapshotWaits;
	report.snapshotCut = aStats.snapshotCut;
	report.entityGpuMsAverage = aStats.entityGpuMsAverage;
	report.worldEntities = cpmc::link::WorldWords(g_entities.Table());
	report.nearestMob = g_link ? g_entities.NearestMob(g_link->State().minecraftFeet) : std::nullopt;
```
```cpp
	g_second.start = aNow;
```
```cpp
	g_second.start = aNow;
	g_second.scenesAtStart = g_entities.Scenes();
	g_second.sceneBytesAtStart = g_entities.SceneBytes();
```
```cpp
	SendHurts(seconds);
```
```cpp
	SendHurts(seconds);
	if (g_link)
	{
		// Spike 7: this frame's entities, which the renderer's Update copies into a snapshot, drawn only into the G-buffer.
		g_entities.InWorld(g_link->State().inWorld);
		if (g_renderer->ModeName() == "gbuffer")
		{
			g_renderer->SetEntities(g_entities.Frame(g_link->State().minecraftEye));
		}
	}
```

- [ ] **Step 16: Edit `CMakeLists.txt` for 0.20.0**

```cmake
project(BlockRender VERSION 0.19.1 LANGUAGES C CXX)
```
```cmake
project(BlockRender VERSION 0.20.0 LANGUAGES C CXX)
```

- [ ] **Step 17: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `493 test(s), 0 failure(s)`.

- [ ] **Step 18: Check that BlockRender imports nothing from PhysX**

```bash
DUMPBIN=$(ls "/c/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/"*/bin/Hostx64/x64/dumpbin.exe | head -1)
"$DUMPBIN" -imports -nologo /d/Cyberpunk-Minecraft/spikes/block-render/build/RelWithDebInfo/BlockRender.dll | grep -i "\.dll$"
```
Expected: `KERNEL32.dll` and the other DLLs it imports, none of them PhysX's.

- [ ] **Step 19: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "minecraft-entities: BlockRender 0.20.0 hands Minecraft's entities to the renderer, with the entities request and status line" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 15: The overlay's Spawn rows, Clear, and Entities line

What run A spawns, from the overlay (the spec's "The overlay's Spawn row"), each button typing its commands into
Minecraft's chat as `mc` requests, as the Fun rows do. Each thing goes ahead of Minecraft's player, level (`/execute
rotated ~ 0 positioned ^ ^ ^N`, as the named sets do), tagged `sp7`, and each block placed gets a marker tagged `sp7b`,
so Clear removes them all and spares the NPCs' stand-ins. Each kind has its own distance, so none lands on another:
- **Mobs, 5 m ahead:** a pig, a villager with a profession and a level, a spider and a blaze with their AI off, and a
  slime of the smallest size, which hops on its own with no path, where a pig won't walk on the city's ground (step 0),
  and which doesn't split when it dies. Each has an empty loot table, as in step 0.
- **More, ahead:** a herd of 10 pigs 11 and 13 m ahead (10 requests, about 10 s); a falling sand from 12 blocks up, 3
  m ahead, which vanishes wherever it lands (`CancelDrop`) and drops no item if it falls out of the world
  (`DropItem`); the block entities 8 m ahead, 3 m apart: a chest, a bed (its foot, and its head to the north, each with
  its marker), a sign, a banner with two patterns, and a torch, for its flame; and a dropped diamond and grass block, 3
  m ahead.
- **Clear:** the placed blocks set back to air by their markers, `strict`, so no block update lets one half of the bed
  break the other with a drop; then everything tagged killed. A TNT's blast breaks the placed blocks and drops them,
  so run A clears them before any TNT.

`tnt-ahead` stands its TNT on obsidian, not stone: a TNT blast drops every block it breaks, and TNT can't break
obsidian. The obsidian gets a marker, so Clear removes it too; the marker goes `~ ~-1 ~` from the level position, as
the test's rule that "ahead" is level asks. The overlay's `Entities: on/off` button switches `set entities`, and an
`Entities:` line shows the status's scenes a second, frames drawn, textures and the nearest mob.

The falling sand's block is given as Minecraft 26.3 reads it, a block id (`BlockState:"minecraft:sand"`), here and in
step 0's script: Task 7 found 26.3 refusing step 0's `{Name:"minecraft:sand"}`, the sand coming only from the falling
block's default.

**Files:**
- Modify: `tests/TestCommands.cpp`, `src/link/Commands.cpp`, `tools/mclink/entities.txt`,
  `tools/blockrender_hud/init.lua`

- [ ] **Step 1: Edit `tests/TestCommands.cpp` to check `tnt-ahead`'s obsidian and marker**

```cpp
	CHECK_STR(NamedCommands("tnt-ahead")->at(1), "/execute rotated ~ 0 positioned ^ ^ ^4 run setblock ~ ~-1 ~ stone");  // TNT needs a block under it
```
```cpp
	CHECK_EQ(NamedCommands("tnt-ahead")->size(), size_t(4));
	// TNT needs a block under it: obsidian, which its blast can't break, so nothing drops; a marker lets Clear remove it.
	CHECK_STR(NamedCommands("tnt-ahead")->at(1), "/execute rotated ~ 0 positioned ^ ^ ^4 run setblock ~ ~-1 ~ obsidian");
	CHECK_STR(NamedCommands("tnt-ahead")->at(2), "/execute rotated ~ 0 positioned ^ ^ ^4 run summon marker ~ ~-1 ~ {Tags:[\"sp7b\"]}");
```

- [ ] **Step 2: Build and run the tests; the new one must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | grep -E "^FAIL|failure"
```
Expected: `FAIL CommandsNamedSetsAreValidCommands` and `493 test(s), 3 failure(s)`, its three new checks: the
harness counts failed checks, not tests.

- [ ] **Step 3: Edit `src/link/Commands.cpp` to stand the TNT on obsidian, with a marker**

```cpp
		// On a stone block: only the player collides with the floor's triangles, so a TNT in the air falls into the void.
		return std::vector<std::string>{ "/gamemode creative", "/execute rotated ~ 0 positioned ^ ^ ^4 run setblock ~ ~-1 ~ stone",
```
```cpp
		// On a block: only the player collides with the floor's triangles, so a TNT in the air falls into the void. Obsidian,
		// since a TNT blast drops every block it breaks, and TNT can't break obsidian; with a marker tagged sp7b, so the
		// overlay's Clear removes it after (spike 7).
		return std::vector<std::string>{ "/gamemode creative", "/execute rotated ~ 0 positioned ^ ^ ^4 run setblock ~ ~-1 ~ obsidian",
			                             "/execute rotated ~ 0 positioned ^ ^ ^4 run summon marker ~ ~-1 ~ {Tags:[\"sp7b\"]}",
```

- [ ] **Step 4: Edit `tools/mclink/entities.txt` to give the falling sand's block as 26.3 reads it**

```text
type /execute rotated ~ 0 positioned ^-4 ^ ^5 run summon minecraft:falling_block ~ ~12 ~ {BlockState:{Name:"minecraft:sand"},DropItem:0b,Tags:["sp7"]}
```
```text
type /execute rotated ~ 0 positioned ^-4 ^ ^5 run summon minecraft:falling_block ~ ~12 ~ {BlockState:"minecraft:sand",DropItem:0b,Tags:["sp7"]}
```

- [ ] **Step 5: Build and run the tests; they must pass**

The tests parse step 0's script too.

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `493 test(s), 0 failure(s)`.

- [ ] **Step 6: Edit `tools/blockrender_hud/init.lua` to add the Spawn rows, Clear, and the Entities button and line**

```lua
-- combat), and a line each for the last request's reply and the requests waiting to be sent, build mode (F8) with its
-- drive and whether V is hidden, the bodies, the NPCs, combat, Minecraft's blasts, and V. Three folded sections hold the
-- rest: "More controls" (the drawing mode, Minecraft's HUD, commands typed into Minecraft, the drive log, the ground and
-- its shapes, the NPCs' list, V's punch, V hidden or seen in build mode, and a blast ahead of V), "Fun" (Minecraft's game
-- mode, items for the hotbar and the hand, armor, and a heal, typed into Minecraft's chat) and "Full status" (every status
-- line, and the frame time).
```
```lua
-- combat, Minecraft's entities), and a line each for the last request's reply and the requests waiting to be sent, build
-- mode (F8) with its drive and whether V is hidden, the bodies, the NPCs, combat, Minecraft's blasts, its entities, and
-- V. Three folded sections hold the rest: "More controls" (the drawing mode, Minecraft's HUD, commands typed into
-- Minecraft, the drive log, the ground and its shapes, the NPCs' list, V's punch, V hidden or seen in build mode, and a
-- blast ahead of V), "Fun" (Minecraft's game mode, items for the hotbar and the hand, armor, a heal, and things to spawn
-- ahead and clear again, typed into Minecraft's chat) and "Full status" (every status line, and the frame time).
```
```lua
                         -- those knocked down and dead after its push
```
```lua
                         -- those knocked down and dead after its push
local entities = nil     -- the entities line (spike 7): on or off, scenes a second, frames drawn, textures, the nearest mob
local entitiesOn = nil   -- whether Minecraft's entities are drawn: "on" or "off"
```
```lua
local fun = {
```
```lua
-- Spike 7's Spawn rows: Minecraft's things ahead of its player, level, each tagged sp7, and each block placed with a
-- marker tagged sp7b, so Clear removes them all and spares the NPCs' stand-ins. Each kind has its own distance, so none
-- lands on another: the dropped items and the sand 3 blocks ahead, the mobs 5, the block entities 8 and the herd 11 and
-- 13. The mobs have no loot and the sand vanishes where it lands, so nothing is left behind. A TNT's blast breaks the
-- placed blocks and drops them, so Clear comes before any TNT.
local function ahead(where)
    return "/execute rotated ~ 0 positioned " .. where .. " run "
end

local function mob(where, kind, extra, at)
    return ahead(where) .. "summon minecraft:" .. kind .. " " .. (at or "~ ~ ~")
        .. ' {Tags:["sp7"],PersistenceRequired:1b,DeathLootTable:"minecraft:empty"' .. (extra or "") .. "}"
end

local herd = {}  -- ten pigs, two rows of five
for _, row in ipairs({ "^11", "^13" }) do
    for _, across in ipairs({ "^-4", "^-2", "^", "^2", "^4" }) do
        table.insert(herd, mob(across .. " ^ " .. row, "pig"))
    end
end

-- Side by side, 3 blocks apart, each block with its marker, and a torch for its flame. A bed is two blocks: its head goes
-- north of its foot, which the gap keeps off its neighbours.
local blockEntities = {}
for _, placed in ipairs({ { "^-6", "~ ~ ~", "chest" }, { "^-3", "~ ~ ~", "red_bed[facing=north,part=foot]" },
    { "^-3", "~ ~ ~-1", "red_bed[facing=north,part=head]" }, { "^", "~ ~ ~", "oak_sign" },
    { "^3", "~ ~ ~", 'white_banner{patterns:[{pattern:"minecraft:stripe_top",color:"red"},{pattern:"minecraft:creeper",color:"black"}]}' },
    { "^6", "~ ~ ~", "torch" } }) do
    table.insert(blockEntities, ahead(placed[1] .. " ^ ^8") .. "setblock " .. placed[2] .. " minecraft:" .. placed[3])
    table.insert(blockEntities, ahead(placed[1] .. " ^ ^8") .. "summon minecraft:marker " .. placed[2] .. ' {Tags:["sp7b"]}')
end

local fun = {
```
```lua
        { label = "Heal", commands = { "/effect give @s minecraft:instant_health 1 3" } },
```
```lua
        { label = "Heal", commands = { "/effect give @s minecraft:instant_health 1 3" } },
    } },
    -- A slime hops on its own with no path, which a pig won't do on the city's ground; Size 0 is the smallest, which
    -- doesn't split when it dies.
    { label = "Mobs, 5 m ahead:", buttons = {
        { label = "Pig", commands = { mob("^ ^ ^5", "pig") } },
        { label = "Villager", commands = { mob("^-2 ^ ^5", "villager",
            ',VillagerData:{type:"minecraft:plains",profession:"minecraft:farmer",level:2}') } },
        { label = "Spider", commands = { mob("^2 ^ ^5", "spider", ",NoAI:1b") } },
        { label = "Blaze", commands = { mob("^4 ^ ^5", "blaze", ",NoAI:1b", "~ ~1 ~") } },
        { label = "Slime", commands = { mob("^-4 ^ ^5", "slime", ",Size:0") } },
    } },
    -- The sand drops from 12 blocks up and vanishes where it lands (CancelDrop), even on a block, and drops no item if it
    -- falls out of the world (DropItem).
    { label = "More, ahead:", buttons = {
        { label = "10 pigs", commands = herd },
        { label = "Falling sand", commands = { ahead("^ ^ ^3") .. "summon minecraft:falling_block ~ ~12 ~ "
            .. '{BlockState:"minecraft:sand",DropItem:0b,CancelDrop:1b,Tags:["sp7"]}' } },
        { label = "Block entities", commands = blockEntities },
        { label = "Dropped items", commands = {
            ahead("^-1 ^ ^3") .. 'summon minecraft:item ~ ~1 ~ {Item:{id:"minecraft:diamond",count:1},PickupDelay:32767,Tags:["sp7"]}',
            ahead("^1 ^ ^3") .. 'summon minecraft:item ~ ~1 ~ {Item:{id:"minecraft:grass_block",count:1},PickupDelay:32767,Tags:["sp7"]}',
        } },
    } },
    -- Strict: no block updates, so neither half of the bed breaks the other with a drop.
    { label = "Spawned:", buttons = {
        { label = "Clear", commands = { "/execute at @e[tag=sp7b] run setblock ~ ~ ~ minecraft:air strict",
            "/kill @e[tag=sp7]", "/kill @e[tag=sp7b]" } },
```
```lua
    blasts = nil
```
```lua
    blasts = nil
    entities = nil
    entitiesOn = nil
```
```lua
                .. (down and (", " .. down .. " knocked down, " .. dead .. " dead") or "")
```
```lua
                .. (down and (", " .. down .. " knocked down, " .. dead .. " dead") or "")
        end
        local entityState = line:match("^entities=(%a+)")
        if entityState ~= nil then
            entitiesOn = entityState
            local scenes = line:match("^entities=%a+: (%d+ scenes a second)")
            local drawn = line:match("; drawn in (%d+ frames)")
            local textures = line:match("; (%d+ textures) %(")
            local nearest = line:match("; the nearest mob ([%d.]+ m) away")
            entities = entityState .. (scenes and (", " .. scenes) or "") .. (drawn and (", drawn in " .. drawn) or "")
                .. (textures and (", " .. textures) or "") .. (nearest and (", the nearest mob " .. nearest .. " away") or "")
```
```lua
        end
        if status == nil then
```
```lua
        end
        if entitiesOn ~= nil then
            ImGui.SameLine()
            if ImGui.Button("Entities: " .. entitiesOn .. "###entities") then
                request("set entities " .. (entitiesOn == "on" and 0 or 1))
            end
        end
        if status == nil then
```
```lua
            ImGui.Text("Blasts: " .. blasts)
```
```lua
            ImGui.Text("Blasts: " .. blasts)
        end
        if entities ~= nil then
            ImGui.Text("Entities: " .. entities)
```

- [ ] **Step 7: Check the overlay's syntax**

```bash
luac -p /d/Cyberpunk-Minecraft/spikes/block-render/tools/blockrender_hud/init.lua && echo "syntax ok"
```
Expected: `syntax ok`.

- [ ] **Step 8: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "minecraft-entities: the overlay's Spawn rows, Clear and entities line, tnt-ahead's TNT on obsidian with a marker, and the falling sand's block as 26.3 reads it" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 16: In the game: run A

Run A of the spec's "The steps", with the user, on BlockRender 0.20.0, linked to SkyCraft's Minecraft in the
`CyberCraft` instance with CyberCraft Tweaks 1.4.0 (Task 7). Minecraft's player in creative, from the Fun row; combat off
(`set combat 0`, the default), so Minecraft's blasts stay in Minecraft and its death can't kill V. DRED stays on.

**Before the user looks,** name what isn't drawn yet, so it isn't taken for a fault:
- translucent particles (the potion's swirls, a firework's sparks, campfire smoke): step B's;
- the block outline in build mode: step B's;
- signs' words and name tags: SkyCraft sends no text;
- shadows under mobs: SkyCraft's contact shadows wait for this run's check 6;
- a slime's outer layer, drawn solid rather than see-through: SkyCraft marks every entity layer solid.

**The order matters:** a TNT's blast breaks the placed blocks and drops them, so Clear comes before any TNT, and
`tnt-cube` goes last, since it destroys what's near it. Arrows and a trident shot or thrown in creative vanish by
themselves after a minute; anything else run A makes is tagged and cleared.

If the game freezes, read the logs and the newest folder in `%LOCALAPPDATA%\REDEngine\ReportQueue`, and force-close it
only when the user asks. If a check fails, stop and find the cause with superpowers:systematic-debugging before going
on. Fixes become new tasks.

These commands come back, as in spike 6:
- **Send a request,** atomically: the plugin runs it within a second. Replace `entities` with the command, and send the
  next only once the status's `request=` line shows the last.
  ```bash
  P=/e/Cyberpunk2077/red4ext/plugins/BlockRender; printf '%s\n' "entities" > "$P/request.tmp" && mv "$P/request.tmp" "$P/blockrender.request"
  ```
- **The log's newest lines:**
  ```bash
  L=$(ls -t /e/Cyberpunk2077/red4ext/logs | grep -i blockrender | head -1); tail -30 "/e/Cyberpunk2077/red4ext/logs/$L"
  ```
- **The status:**
  ```bash
  cat /e/Cyberpunk2077/bin/x64/plugins/cyber_engine_tweaks/mods/blockrender_hud/status.txt
  ```
- **Minecraft's log, for the chat's replies and the textures SkyCraft sends:**
  ```bash
  grep -E "\[CHAT\]|SkyCraft: (sent texture|texture)" "/c/Users/BLOXY/AppData/Roaming/ModrinthApp/profiles/CyberCraft/logs/latest.log" | tail -10
  ```

**Files:**
- Modify: `docs/spikes/2026-10-06-minecraft-entities-results.md`

- [ ] **Step 1: Deploy (the game must not be running)**

```bash
tasklist | grep -i -c "cyberpunk2077.exe"
```
Expected: `0`. If it's `1`, ask the user to quit the game first, because a running game locks the DLL.
```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
P=/e/Cyberpunk2077/red4ext/plugins/BlockRender
H=/e/Cyberpunk2077/bin/x64/plugins/cyber_engine_tweaks/mods/blockrender_hud
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default -DCP2077_DIR=E:/Cyberpunk2077 > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C|error MSB"; \
  "$CMAKE" --preset default -DCP2077_DIR= > /dev/null; \
  cmp build/RelWithDebInfo/BlockRender.dll "$P/BlockRender.dll" && cmp shaders/blocks.hlsl "$P/shaders/blocks.hlsl" \
  && cmp tools/blockrender_hud/init.lua "$H/init.lua" && echo installed
```
Expected: `installed`. The second configure clears `CP2077_DIR` again.

- [ ] **Step 2: In the game: BlockRender 0.20.0 loads**

Ask the user to start the `CyberCraft` instance, then the game, and to wait at the game's main menu. Then:
```bash
L=$(ls -t /e/Cyberpunk2077/red4ext/logs | grep -i blockrender | head -1); grep -E "loaded|ERROR|error" "/e/Cyberpunk2077/red4ext/logs/$L" | head -5
```
Expected: `BlockRender 0.20.0 loaded (game 2.31…), …; dred on, debug layer off; …`, and no error lines.

- [ ] **Step 3: In the game: load, link, creative**

Ask the user to load a save on an open pavement by day, with at least 15 m clear ahead of V, and to click Link. Then
ask them to click Creative in the overlay's Fun row. Expected:
- in the log, `link on: …; drawing is gbuffer`. If drawing was already `predlss` or `present`, Link leaves it: ask the
  user to click the overlay's `G-buffer` mode button;
- in Minecraft's log, `[CHAT] Set own game mode to Creative Mode`;
- in the status, `mode=gbuffer`, `combat=off`, and `entities=on: 0 scenes a second (0.00 MB/s); the newest 0 batches,
  …; drawn in 0 frames, …; world: nothing` (nothing spawned yet; the textures count those SkyCraft sent for the ragdoll,
  Minecraft's player's skin and cape); in the overlay, the `Entities: on` button and `Entities:` line.

Then name what isn't drawn yet ("Before the user looks", above).

- [ ] **Step 4: In the game: things in place (check 1)**

Ask the user to click the Mobs row's Pig, Villager, Spider, Blaze and Slime, one at a time, then to look at them, walk
around them and turn. Expected:
- in the status, about `60 scenes a second`, `drawn in` about as many frames as the game's frame rate, `0 draws
  skipped` but for a frame or two after each new texture, and `the nearest mob 5.0 m away` or so (the pig's), until V
  moves;
- on screen: each stands with its feet on the ground, neither floating nor sunk, the blaze a block up with its AI off;
  each animates (the pig and the villager turn their heads, the blaze's rods turn, the slime hops); each is lit like the
  blocks by day; and each stays where it is while V walks and turns.

Send `entities`. Expected, in the log: `entities: the newest scene, from …: N batches, …`, a line per batch, the
villager's in Minecraft's draw order (body, biome, profession, level, by their sizes and Minecraft's names for the ids),
`entities: world: 5 shadows` or so, and `entities: N textures: …`. Then ask the user to look up a little, 3 m ahead, and
click More's Falling sand: a sand block falls from 12 m up and vanishes where it lands.

- [ ] **Step 5: In the game: layers and smearing (checks 7 and 8)**

Ask the user to look closely at the villager and the spider. Expected: the villager's farmer clothes over its body, its
iron level badge on its belt over those; the spider's red eyes over its head. Then send `set entities 0`: every one of
Minecraft's entities goes, and the blocks stay; then `set entities 1`.

Ask the user to watch the slime hop and the blaze's rods turn, standing still, then turning V slowly and quickly. Note
any trail, ghost or doubled image behind them; trails decide step B's `dlsshint 2`.

- [ ] **Step 6: In the game: block entities (check 4)**

Ask the user to click More's Block entities, which places them 8 m ahead (12 requests, about 12 s). Expected: a chest; a
whole red bed; an oak sign, its face blank; a white banner with a red stripe across its top and a black creeper face,
each pattern over the cloth; a torch, with its flame if it's drawn. In Minecraft's log, a `[CHAT]` reply to each, and
none an error. The chest and the banner are step A's; the bed, the sign and the torch are block models in 26.3, drawn
with the blocks (Task 7's check).

- [ ] **Step 7: In the game: world entities and cracks (check 5)**

Ask the user to click More's Dropped items: a diamond icon and a grass block, 3 m ahead, each turning and bobbing, the
grass block's top green. The status's world: `1 block, 1 item, …`.

Then ask the user to click the Weapons row's Bow, press F8 for build mode, and shoot a few arrows into the ground and a
wall (hold the right button to draw, release to shoot); then the Trident, and throw it. Expected: each arrow and the
trident stuck where it hit Night City, in the status's world line; they vanish after a minute.

Then ask the user to click the Mode row's Survival, then Blocks in hand's Stone bricks, place one block on the ground
(the right button), press a number key for a hotbar slot with nothing in it (or anything but a pickaxe), and mine the
block with the left button held. Expected: cracks grow over it for about 7 s, then it breaks, dropping nothing, since
stone bricks need a pickaxe to drop. Then ask the user to click Creative, and press F8 to leave build mode.

- [ ] **Step 8: In the game: contact darkening (check 6)**

Ask the user to look at the ground under the pig, by day, with a screenshot. Note whether the game's ambient occlusion
darkens it: if not, SkyCraft's contact shadows wait for a later step.

- [ ] **Step 9: In the game: glow, at night (check 2)**

Ask the user to use the game's Skip Time to the night, and look at the mobs. Expected: the blaze glows; the pig, the
villager, the spider and the slime don't. Send `set glow 0.5`, then `set glow 1`, asking the user what changes each
time, then `set glow 0.9`. Send `set glowlight 14`, and ask the user to look at the torch: its flame, if it's drawn, and
anything right beside a block of light 15, glows now. Send `set glowlight 15`. Then send `set glow 2` and `set
glowlight 16`. Expected: refused, `glow is from 0 to 1: …` and `glowlight is a whole number from 1 to 15: …`.

Then ask the user to click Clear (the Spawned row). Expected: in Minecraft's log, `Changed the block at …` for each of
the six placed blocks, `Killed N entities` for the mobs and any dropped item still there (an item vanishes after 5
minutes), and `Killed 6 entities` for the markers; within about 7 s, once the death puffs and the blaze's smoke are
gone, the status's `the newest 0 batches` and `world: nothing`, but for arrows not yet gone; on screen, nothing left.

- [ ] **Step 10: In the game: the explosion (check 3)**

Still at night, ask the user to click `tnt-ahead` (More controls' Type row). Expected: a TNT on an obsidian block 4 m
ahead flashes white, glowing, for 4 s, then explodes: puffs, glowing, and smoke, coming and going as in Minecraft. V
takes nothing (combat off). Then ask the user to click Clear: the obsidian goes.

- [ ] **Step 11: In the game: cost, then the big blast (check 9)**

Ask the user to click the Mobs row's five again and More's 10 pigs, wait 15 s, and read the status three times, a few
seconds apart. Note the entities' GPU time, the frame time (`frame=`), the scene's batches, vertices and MB a second,
and any snapshot waits. Then, last, ask the user to click `tnt-cube` and watch its blast (125 TNT, 6 m ahead), and read
the status twice during it. Note the same, the draws skipped, any draws past a snapshot's 8 MB, and the frame time's
dip. Then ask the user to click Clear.

- [ ] **Step 12: In the game: no crash or freeze (check 10), and unlinking**

Ask the user to click Unlink. Expected: Minecraft's entities go with its blocks, and the game ran without a crash or a
freeze throughout.

- [ ] **Step 13: Ask the user to quit the game and stop Minecraft**

- [ ] **Step 14: Read the log once more**

```bash
L=$(ls -t /e/Cyberpunk2077/red4ext/logs | grep -i blockrender | head -1); grep -E "entities:|renderer:|Minecraft cleared|link on|set " "/e/Cyberpunk2077/red4ext/logs/$L" | head -200
```
Everything the steps above logged, for the results.

- [ ] **Step 15: Record run A's results in `docs/spikes/2026-10-06-minecraft-entities-results.md`**

A "Run A" section: the versions; each check above with its outcome and numbers: where things stand and how they look;
the layers; smearing, still and turning; the block entities; the world entities and cracks; contact darkening; the glow
at each setting; the explosion; the costs, with the herd and during `tnt-cube`; and anything that surprised, and what it
means for Part 3. Put the spike's answer so far at the top, for step A.

- [ ] **Step 16: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add docs/spikes/2026-10-06-minecraft-entities-results.md docs/superpowers/plans/2026-10-06-minecraft-entities.md \
  && git commit -q -m "minecraft-entities: run A's results" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 17: Close Part 2, and plan Part 3

- [ ] **Step 1: Add "How Part 2 went" to this plan**

At the end, after "Found while executing Part 2": what ran as written, what changed, and what step C's `mclink` run and
run A found.

- [ ] **Step 2: Plan Part 3**

Use superpowers:writing-plans to add Part 3's tasks, from Task 18 on, to this plan: step B (the forward list: the
translucent particles, the outline in build mode and DLSS's bias hint), with what run A asks for, such as the glow
fallback or step C's emissive flag. Verify them as Parts 1 and 2 were. Changes to the spec found while planning go in
its "Changes while planning Part 3".

- [ ] **Step 3: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add docs/superpowers \
  && git commit -q -m "minecraft-entities plan: Part 2 closed, Part 3 planned" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

## Found while planning Part 2

The spec's "Changes while planning Part 2" lists what planning settled in the design. In the plan itself:
- **The texture buffer's growth,** found by the review of Task 13 before this plan was written: the growth copied the
  old buffer's whole size, over where the waiting textures go, and copies in one list aren't ordered, so a new texture
  could have come out as stale texels; and it retired the old buffer to the fence before the upload, which itself reads
  it, so the GPU could have read a freed buffer. The growth now copies only the textures uploaded before, and keeps
  the old buffer until the upload is done.
- **The version is in two places,** as for every earlier one: `CMakeLists.txt`'s `project` and `Plugin.cpp`'s
  `kVersion`.
- **The Spawn rows, played through against Minecraft 26.3 and SkyCraft:**
  - every entity but Minecraft's player stands on SkyCraft's blocky copy of the city's ground
    (`BlockCollisionsMixin` and `SkyCollision.shapeAt`), so the mobs and items stay up, as step 0 saw;
  - `/setblock` places one block, so a bed is two commands, its foot and its head, each with its marker; and since a
    bed's half breaks with a drop when the other goes (only the head drops the bed), Clear sets the blocks to air
    `strict`, which makes no block updates (26.3's `SetBlockCommand`: flags 2 | 816);
  - a falling sand with `CancelDrop` vanishes wherever it lands, even on a placed block, where `DropItem` alone would
    have let it land as one;
  - each kind has its own distance (3, 5, 8, 11 and 13 m), and the block entities stand 3 m apart, so the bed's head,
    a block north of its foot, can't land on a neighbour;
  - a slime of `Size:0` (the smallest, size 1) doesn't split when it dies, and hops with no path (26.3's
    `AbstractCubeMob` goals: a random direction every 2 to 5 s);
  - with NBT given, `/summon` doesn't run `finalizeSpawn`, so the villager keeps its data and the slime its size.
- **`tnt-ahead`'s marker** goes `~ ~-1 ~` from `^ ^ ^4`, not at `^ ^-1 ^4`: the same spot, written as the named sets'
  test asks, each command starting `/execute rotated ~ 0 positioned ^ ^ ^`.
- **The settings' ranges are the renderer's** (`Renderer::Set`), as every drawing setting's are, in the plugin rather
  than `render_core`, so no unit test reaches them: run A sends two out of range. The `entities` request and the
  settings' parsing are tested.
- **The mutations:** 62, all caught. A default in a header (`LinkReport::entitiesOn`) is compiled into the test that
  makes the report, so under `%TEMP%`, where MSBuild doesn't track headers, its mutation touches that test.
- **The replay** caught one expectation: the test harness counts failed checks, not tests, so Task 15's failing test
  shows `3 failure(s)`, its three new checks.

## Found while executing Part 2

- **Task 7 passed** (the results' "Step C's draw order, with `mclink`"): every scene's batches in Minecraft's draw
  order, and nothing else changed from step 0.
- **26.3 reads a falling block's `BlockState` as a block id,** or a map keyed `id` (`BlockState.CODEC`), not as
  `{Name:…}`: Minecraft's log showed `Failed to decode value '{Name:"minecraft:sand"}' from field 'BlockState'`, the
  sand coming only from the falling block's default. Task 15 now gives the id, in the overlay and in step 0's script
  (a new step), and its commit says so.
- **The Spawn rows were typed into Minecraft with `mclink`,** with the user's leave, after Task 7's run and with the
  sand fixed: every command was answered without an error, and Clear left nothing (the results' "The overlay's Spawn
  rows, with `mclink`").
- **Beds, signs and torches are block models in 26.3,** in SkyCraft's section mesh, so they're drawn with the blocks;
  only the chest and the banner of Task 15's block entities are step A's. Task 16's check 4 says so.
