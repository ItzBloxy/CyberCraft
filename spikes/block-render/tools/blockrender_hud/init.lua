-- BlockRender HUD: what BlockRender (spikes 3 and 4 of D:\Cyberpunk-Minecraft) is doing, from the status.txt it writes
-- into this folder once a second, plus V's position, heading and Minecraft block, and the nearest structure, and the
-- frame time as CET sees it, which works without BlockRender too and goes into frames.txt here as well. It asks
-- BlockRender for things through blockrender.request in this folder.
-- Kept small: one row of buttons (link, Minecraft's blocks as bodies, step 4's test box, the turn, NPCs in Minecraft,
-- combat, Minecraft's entities), and a line each for the last request's reply and the requests waiting to be sent,
-- build mode (F8) with its drive and whether V is hidden, the bodies, the NPCs, combat, Minecraft's blasts, its
-- entities, and V. It tells BlockRender when CET's overlay opens and closes, which pauses build mode while it's open.
-- Three folded sections hold the rest: "More controls" (the drawing mode, Minecraft's HUD, commands typed into
-- Minecraft, the drive log, the ground and its shapes, the NPCs' list, V's punch, V hidden or seen in build mode, and a
-- blast ahead of V), "Fun" (Minecraft's game mode, items for the hotbar and the hand, armor, a heal, and things to
-- spawn ahead and clear again, typed into Minecraft's chat) and "Full status" (every status line, and the frame time).
-- Delete this folder to remove it.
local status = nil       -- status.txt's lines, except the structures
local structures = {}    -- { name, min = { x, y, z }, max = { x, y, z } }, in world coordinates
local hudSetting = nil   -- the hud line's setting: "never", "in build mode" or "always"
local build = nil        -- the build line's first two parts: on, or off and why; and the slot
local drive = nil        -- the drive line's state, V's camera from Minecraft's eye, and V's health
local driveState = nil   -- the drive line's state alone
local turn = nil         -- the drive line's turn setting: "on" or "off"
local ground = nil       -- the ground line's setting, "floor" or "city", and how much of the city is known
local groundName = nil   -- "floor" or "city"
local bodies = nil       -- the bodies line: on or off, and how many, or why they wait; and the test box
local blocksOn = nil     -- whether bodies are on: "on" or "off"
local boxOn = false      -- whether the test box stands
local npcs = nil         -- the npcs line: on or off, how many are listed and the nearest, or why none are
local npcsOn = nil       -- whether NPCs go into SkyCraft's actor table: "on" or "off"
local combat = nil       -- the combat line: on or off; V's weapon drawn or not; hits queued and landed; V's hurts;
                         -- Immortal; Minecraft's death
local combatOn = nil     -- whether Minecraft's hits land as V's and V's hurts go to Minecraft: "on" or "off"
local blasts = nil       -- the blasts line, with combat on: how many, and the last's NPCs within it, its own hits, and
                         -- those knocked down and dead after its push
local entities = nil     -- the entities line (spike 7): on or off, scenes a second, frames drawn and with the forward
                         -- list (step B), textures, the nearest mob
local entitiesOn = nil   -- whether Minecraft's entities are drawn: "on" or "off"
local vLook = nil        -- the v line: V hidden in build mode, or seen and why
local hideV = nil        -- whether V's meshes are hidden in build mode: "on" or "off"
local reply = nil        -- the request line: what BlockRender made of the last request, cut short
local player = nil       -- the lines about V
local sinceRead = 1.0
local frames = { sum = 0.0, count = 0, worst = 0.0 }  -- this second's frame times, from onUpdate
local frameLine = nil                                  -- the last whole second's
local fontScale = 0.8    -- the window's text, against CET's own size

local atan2 = math.atan2 or function(y, x) return math.atan(y, x) end

-- The drawing modes the buttons switch between, as blockrender.request names them.
local modes = {
    { name = "off", label = "Off" },
    { name = "predlss", label = "Before DLSS" },
    { name = "gbuffer", label = "G-buffer" },
}

-- The named sets the buttons type into Minecraft's chat, as `mc <name>`.
local sets = { "creative", "tnt-kit", "tnt-ahead", "wall", "tnt-cube", "field", "wipe" }

-- The HUD button's cycle: after each `hud` setting, as the status names it, the one a click asks for.
local hudNext = { ["never"] = 1, ["in build mode"] = 2, ["always"] = 0 }

-- The Fun header's rows (spike 6's "The overlay's Fun controls"): each button's commands are typed into Minecraft's chat,
-- one `mc` request each, in order. "In hand" is the hotbar slot Minecraft's player has selected, and "Your slots" those
-- the user keeps these in: both replace what is there, since without Minecraft's inventory screen an item given past the
-- hotbar is out of reach. Arrows are given, as a bow or a crossbow takes them from anywhere.
local hand = "/item replace entity @s weapon.mainhand with minecraft:"

local function wear(head, chest, legs, feet)
    return {
        "/item replace entity @s armor.head with minecraft:" .. head,
        "/item replace entity @s armor.chest with minecraft:" .. chest,
        "/item replace entity @s armor.legs with minecraft:" .. legs,
        "/item replace entity @s armor.feet with minecraft:" .. feet,
    }
end

-- Spike 7's Spawn rows: Minecraft's things ahead of its player, level, each tagged sp7, and each block placed with a
-- marker tagged sp7b, so Clear removes them all and spares the NPCs' stand-ins. Each kind has its own place, so none
-- lands on another: the dropped items and the sand 3 blocks ahead, the potion and the campfire 3 ahead to either side,
-- the mobs 5, the firework 6, the block entities 8 and the herd 11 and 13. The mobs have no loot and the sand vanishes
-- where it lands, so nothing is left behind. A TNT's blast breaks the placed blocks and drops them, so Clear comes
-- before any TNT.
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

-- Side by side, 3 blocks apart, and a torch for its flame. Each block's marker goes first, tagged with its kind, then the
-- block goes where its markers are: the overlay sends a command a second, each from where Minecraft's player is then, so
-- a block placed from the player would part from its marker if V moved in between, and Clear would miss it (spike 7's
-- run A). A bed is two blocks: its head's marker goes north of its foot's, which the gap keeps off its neighbours.
local blockEntities = {}
for _, placed in ipairs({ { "^-6", "chest", "chest" }, { "^-3", "bed", "red_bed[facing=north,part=foot]" },
    { "^", "sign", "oak_sign" },
    { "^3", "banner", 'white_banner{patterns:[{pattern:"minecraft:stripe_top",color:"red"},{pattern:"minecraft:creeper",color:"black"}]}' },
    { "^6", "torch", "torch" } }) do
    local tag = "sp7" .. placed[2]
    table.insert(blockEntities, ahead(placed[1] .. " ^ ^8") .. 'summon minecraft:marker ~ ~ ~ {Tags:["sp7b","' .. tag .. '"]}')
    table.insert(blockEntities, "/execute at @e[tag=" .. tag .. "] run setblock ~ ~ ~ minecraft:" .. placed[3])
    if placed[2] == "bed" then
        table.insert(blockEntities, '/execute at @e[tag=sp7bed] run summon minecraft:marker ~ ~ ~-1 {Tags:["sp7b","sp7bedhead"]}')
        table.insert(blockEntities, "/execute at @e[tag=sp7bedhead] run setblock ~ ~ ~ minecraft:red_bed[facing=north,part=head]")
    end
end

local fun = {
    { label = "Mode:", buttons = {
        { label = "Creative", commands = { "/gamemode creative" } },
        { label = "Survival", commands = { "/gamemode survival" } },
    } },
    { label = "Your slots:", buttons = {
        { label = "Golden apples (6)", commands = { "/item replace entity @s hotbar.5 with minecraft:golden_apple 64" } },
        { label = "Golden carrots (7)", commands = { "/item replace entity @s hotbar.6 with minecraft:golden_carrot 64" } },
        { label = "TNT (8)", commands = { "/item replace entity @s hotbar.7 with minecraft:tnt 64" } },
        { label = "Flint and steel (9)", commands = { "/item replace entity @s hotbar.8 with minecraft:flint_and_steel" } },
    } },
    { label = "Blocks in hand:", buttons = {
        { label = "Stone bricks", commands = { hand .. "stone_bricks 64" } },
        { label = "Oak planks", commands = { hand .. "oak_planks 64" } },
        { label = "Bricks", commands = { hand .. "bricks 64" } },
        { label = "White concrete", commands = { hand .. "white_concrete 64" } },
        { label = "Glass", commands = { hand .. "glass 64" } },
    } },
    { label = "Tools in hand:", buttons = {
        { label = "Pickaxe", commands = { hand .. "diamond_pickaxe" } },
        { label = "Axe", commands = { hand .. "diamond_axe" } },
        { label = "Shovel", commands = { hand .. "diamond_shovel" } },
    } },
    { label = "Weapons in hand:", buttons = {
        { label = "Sword", commands = { hand .. "diamond_sword" } },
        { label = "Mace", commands = { hand .. "mace" } },
        { label = "Trident", commands = { hand .. "trident" } },
        { label = "Crossbow", commands = { hand .. "crossbow", "/give @s minecraft:arrow 64" } },
        { label = "Bow", commands = { hand .. "bow", "/give @s minecraft:arrow 64" } },
    } },
    { label = "Armor:", buttons = {
        { label = "Diamond armor", commands = wear("diamond_helmet", "diamond_chestplate", "diamond_leggings", "diamond_boots") },
        { label = "No armor", commands = wear("air", "air", "air", "air") },
    } },
    { label = "Health:", buttons = {
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
    -- Step B's translucent particles: a firework bursting overhead, 6 blocks ahead (look up for it); a splash potion
    -- that breaks on the ground 3 ahead and 3 to the left (^ is left first); and a lit campfire 3 ahead and 3 to the
    -- right, whose smoke rises until Clear, placed at its marker as the block entities are.
    { label = "Particles, ahead:", buttons = {
        { label = "Firework", commands = { ahead("^ ^ ^6") .. "summon minecraft:firework_rocket ~ ~ ~ "
            .. '{LifeTime:20,Tags:["sp7"],FireworksItem:{id:"minecraft:firework_rocket",count:1,components:'
            .. '{"minecraft:fireworks":{explosions:[{shape:"large_ball",colors:[I;16711680]}]}}}}' } },
        { label = "Splash potion", commands = { ahead("^3 ^ ^3") .. "summon minecraft:splash_potion ~ ~2 ~ "
            .. '{Tags:["sp7"],Item:{id:"minecraft:splash_potion",count:1,components:'
            .. '{"minecraft:potion_contents":{potion:"minecraft:regeneration"}}}}' } },
        { label = "Campfire", commands = {
            ahead("^-3 ^ ^3") .. 'summon minecraft:marker ~ ~ ~ {Tags:["sp7b","sp7campfire"]}',
            "/execute at @e[tag=sp7campfire] run setblock ~ ~ ~ minecraft:campfire",
        } },
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
    } },
}

-- Requests not yet sent, oldest first. BlockRender reads blockrender.request in this folder once a second, once its line
-- is whole, and deletes it: the next is written only once it's gone, so no click overwrites one not yet read.
local queued = {}

-- Asks BlockRender for something: queued, and written by sendQueued.
local function request(command)
    table.insert(queued, command)
end

-- Writes the oldest queued request, once BlockRender has read the last.
local function sendQueued()
    if #queued == 0 then
        return
    end
    local unread = io.open("blockrender.request", "r")
    if unread ~= nil then
        unread:close()
        return
    end
    local file = io.open("blockrender.request", "w")
    if file ~= nil then
        file:write(table.remove(queued, 1), "\n")
        file:close()
    end
end

-- A line cut to aLength characters, with "..." where it was cut.
local function shortened(line, aLength)
    if #line <= aLength then
        return line
    end
    return line:sub(1, aLength - 3) .. "..."
end

-- A status line as the window shows it: a long line splits at each "; ", with the later parts indented.
local function displayLines(line)
    local out = {}
    for part in string.gmatch(line .. "; ", "(.-); ") do
        table.insert(out, (#out == 0 and "" or "    ") .. part)
    end
    return out
end

local function readStatus()
    local file = io.open("status.txt", "r")
    if file == nil then
        status = nil
        structures = {}
        return
    end
    status = {}
    structures = {}
    hudSetting = nil
    build = nil
    drive = nil
    driveState = nil
    turn = nil
    ground = nil
    groundName = nil
    bodies = nil
    blocksOn = nil
    boxOn = false
    npcs = nil
    npcsOn = nil
    combat = nil
    combatOn = nil
    blasts = nil
    entities = nil
    entitiesOn = nil
    vLook = nil
    hideV = nil
    reply = nil
    for line in file:lines() do
        local said = line:match("^request=(.*)$")
        if said ~= nil and said ~= "" then
            reply = shortened(said, 90)
        end
        local name, a, b, c, d, e, f = line:match("^structure=(%S+) (%S+) (%S+) (%S+) (%S+) (%S+) (%S+)$")
        hudSetting = line:match("^hud=([^,]+),") or hudSetting
        local mode, slot = line:match("^build=([^;]*); ([^;]*);")
        if mode ~= nil then
            build = mode .. ", " .. slot
        end
        local state = line:match("^drive=([^;]*)")
        if state ~= nil then
            local eye = line:match("V's camera from Minecraft's eye ([%d.]+ m)")
            local health = line:match("V's health ([^;]*)")
            drive = state .. (eye and (", V's camera " .. eye .. " from Minecraft's eye") or "")
                .. (health and (", V's health " .. health) or "")
            driveState = state
            turn = line:match("; turn (%a+),")
        end
        local look = line:match("^v=(.*)$")
        if look ~= nil then
            vLook = look
            hideV = look:match("^shown: set hidev is 0") and "off" or "on"
        end
        local setting = line:match("^ground=(%a+)")
        if setting ~= nil then
            groundName = setting
            local known = line:match("(%d+ regions known, %d+ batches to read)")
            local fallback = line:match("; it fell back from the city: (.*)$")
            ground = setting .. (known and (", " .. known) or "") .. (fallback and (", fell back: " .. fallback) or "")
        end
        local state = line:match("^bodies=(%a+)")
        if state ~= nil then
            blocksOn = state
            boxOn = line:match("the test box at world") ~= nil
            local counts = line:match("^bodies=%a+: (%d+ sections with bodies, %d+ boxes)")
            local waiting = line:match("; waiting: (.*)$")
            bodies = state .. (counts and (", " .. counts) or "") .. (waiting and (", waiting: " .. waiting) or "")
                .. (boxOn and ", the test box standing" or "")
        end
        local npcState = line:match("^npcs=(%a+)")
        if npcState ~= nil then
            npcsOn = npcState
            local listed = line:match("^npcs=on: (%d+ listed of %-?%d+ in the world)")
            local nearest, distance = line:match(", the nearest (.-) at ([%d.]+ m);")
            local why = line:match("; none listed: (.*)$")
            npcs = npcState .. (listed and (", " .. listed) or "")
                .. (nearest and (", the nearest " .. nearest .. " at " .. distance) or "")
                .. (why and (", none listed: " .. why) or "")
        end
        local combatState = line:match("^combat=(%a+)")
        if combatState ~= nil then
            combatOn = combatState
            local drawn = line:match("^combat=on: V's punch [^;]*, (not drawn)") or line:match("^combat=on: V's punch [^;]*, (drawn)")
            local hits = line:match("; hits (%d+ queued, %d+ landed)")
            local hurts = line:match("; V's hurts: (%d+ taken, %d+ sent)")
            local immortal = line:match("; Immortal (%a+);")
            local dead = line:match("; Minecraft's player (dead for [%d.]+ s)")
            combat = combatState .. (drawn and (", V's weapon " .. drawn) or "") .. (hits and (", hits " .. hits) or "")
                .. (hurts and (", V's hurts " .. hurts) or "") .. (immortal and (", Immortal " .. immortal) or "")
                .. (dead and (", Minecraft's player " .. dead) or "")
        end
        local blastCount = line:match("^blasts=on: (%d+) taken")
        if blastCount ~= nil then
            local within = line:match("; the last [^;]*: (%d+) NPCs within it")
            local own = line:match("(%d+) hits of its own")
            local down, dead = line:match("after its push (%d+) knocked down, (%d+) dead")
            blasts = blastCount .. " taken" .. (within and ("; the last: " .. within .. " NPCs within it") or "")
                .. (own and (", " .. own .. " hits of its own") or "")
                .. (down and (", " .. down .. " knocked down, " .. dead .. " dead") or "")
        end
        local entityState = line:match("^entities=(%a+)")
        if entityState ~= nil then
            entitiesOn = entityState
            local scenes = line:match("^entities=%a+: (%d+ scenes a second)")
            local drawn = line:match("; drawn in (%d+ frames)")
            local forward = line:match("; the forward list in (%d+ frames)")
            local textures = line:match("; (%d+ textures) %(")
            local nearest = line:match("; the nearest mob ([%d.]+ m) away")
            entities = entityState .. (scenes and (", " .. scenes) or "") .. (drawn and (", drawn in " .. drawn) or "")
                .. (forward and (", the forward list in " .. forward) or "")
                .. (textures and (", " .. textures) or "") .. (nearest and (", the nearest mob " .. nearest .. " away") or "")
        end
        if name ~= nil then
            table.insert(structures, {
                name = name,
                min = { tonumber(a), tonumber(b), tonumber(c) },
                max = { tonumber(d), tonumber(e), tonumber(f) },
            })
        else
            table.insert(status, line)
        end
    end
    file:close()
end

-- How far a point is from a box, 0 inside it.
local function distanceToBox(p, box)
    local function gap(value, low, high)
        if value < low then return low - value end
        if value > high then return value - high end
        return 0
    end
    local dx = gap(p.x, box.min[1], box.max[1])
    local dy = gap(p.y, box.min[2], box.max[2])
    local dz = gap(p.z, box.min[3], box.max[3])
    return math.sqrt(dx * dx + dy * dy + dz * dz)
end

-- CET's overlay opening and closing, told to BlockRender, which pauses build mode while it's open. Shift+F6 pauses it at
-- once; this keeps it right if the overlay was opened or closed some other way. First in the queue, so it doesn't wait
-- behind the Fun rows' commands, and in place of any still waiting, since only the latest is true.
local function tellOverlay(state)
    for i = #queued, 1, -1 do
        if queued[i]:match("^overlay ") then
            table.remove(queued, i)
        end
    end
    table.insert(queued, 1, "overlay " .. state)
end

registerForEvent("onOverlayOpen", function()
    tellOverlay("open")
end)

registerForEvent("onOverlayClose", function()
    tellOverlay("closed")
end)

registerForEvent("onUpdate", function(delta)
    sendQueued()
    frames.sum = frames.sum + delta
    frames.count = frames.count + 1
    frames.worst = math.max(frames.worst, delta)
    sinceRead = sinceRead + delta
    if sinceRead >= 1.0 then
        sinceRead = 0.0
        readStatus()
        local average = frames.sum / frames.count
        frameLine = string.format("CET's frame time: %.1f ms average (%.1f fps), %.1f ms worst", average * 1000.0,
            1.0 / average, frames.worst * 1000.0)
        frames = { sum = 0.0, count = 0, worst = 0.0 }
        local out = io.open("frames.txt", "w")
        if out ~= nil then
            out:write(frameLine, "\n")
            out:close()
        end
    end
    local v = Game.GetPlayer()
    if v == nil then
        player = nil
        return
    end
    local p = v:GetWorldPosition()
    local f = v:GetWorldForward()
    local heading = math.deg(atan2(f.x, f.y)) % 360  -- 0 = north (+y), 90 = east (+x)
    -- Minecraft's axes: x is world x, y is world z (up), z is world -y (south).
    local text = string.format("V: x %.2f  y %.2f  z %.2f, heading %d (0 north, 90 east)\nMinecraft block %d %d %d",
        p.x, p.y, p.z, math.floor(heading + 0.5), math.floor(p.x), math.floor(p.z), math.floor(-p.y))
    local nearest, nearestName = nil, nil
    for _, box in ipairs(structures) do
        local distance = distanceToBox(p, box)
        if nearest == nil or distance < nearest then
            nearest, nearestName = distance, box.name
        end
    end
    if nearest ~= nil then
        text = text .. string.format("; nearest structure: %s, %.2f m away", nearestName, nearest)
    end
    player = text
end)

registerForEvent("onDraw", function()
    ImGui.SetNextWindowPos(30, 420, ImGuiCond.FirstUseEver)
    if ImGui.Begin("BlockRender", ImGuiWindowFlags.AlwaysAutoResize) then
        if ImGui.SetWindowFontScale then
            ImGui.SetWindowFontScale(fontScale)
        end
        -- The buttons the in-game steps use. "###id" keeps a button's ID the same while its label changes.
        if ImGui.Button("Link") then
            request("link on")
        end
        ImGui.SameLine()
        if ImGui.Button("Unlink") then
            request("link off")
        end
        if blocksOn ~= nil then
            ImGui.SameLine()
            if ImGui.Button("Blocks: " .. blocksOn .. "###blocks") then
                request("set blocks " .. (blocksOn == "on" and 0 or 1))
            end
            ImGui.SameLine()
            if ImGui.Button((boxOn and "Box: on" or "Box: off") .. "###box") then
                request("box " .. (boxOn and "off" or "on"))
            end
        end
        if turn ~= nil then
            ImGui.SameLine()
            if ImGui.Button("Turn: " .. turn .. "###turn") then
                request("set turn " .. (turn == "on" and 0 or 1))
            end
        end
        if npcsOn ~= nil then
            ImGui.SameLine()
            if ImGui.Button("NPCs: " .. npcsOn .. "###npcs") then
                request("set npcs " .. (npcsOn == "on" and 0 or 1))
            end
        end
        if combatOn ~= nil then
            ImGui.SameLine()
            if ImGui.Button("Combat: " .. combatOn .. "###combat") then
                request("set combat " .. (combatOn == "on" and 0 or 1))
            end
        end
        if entitiesOn ~= nil then
            ImGui.SameLine()
            if ImGui.Button("Entities: " .. entitiesOn .. "###entities") then
                request("set entities " .. (entitiesOn == "on" and 0 or 1))
            end
        end
        if status == nil then
            ImGui.Text("no status.txt yet: is BlockRender loaded?")
        end
        -- Both always there, so the rows below don't move as requests come and go, and a click lands where it was aimed.
        ImGui.Text("Last request: " .. (reply or "none yet"))
        ImGui.Text(#queued > 0 and string.format("Requests waiting to be sent: %d, about one a second", #queued)
            or "Requests waiting to be sent: none")
        if build ~= nil then
            ImGui.Text("Build mode (F8): " .. build .. (driveState and ("; drive " .. driveState) or "")
                .. (vLook and ("; V " .. vLook) or ""))
        end
        if bodies ~= nil then
            ImGui.Text("Bodies: " .. bodies)
        end
        if npcs ~= nil then
            ImGui.Text("NPCs: " .. npcs)
        end
        if combat ~= nil then
            ImGui.Text("Combat: " .. combat)
        end
        if blasts ~= nil then
            ImGui.Text("Blasts: " .. blasts)
        end
        if entities ~= nil then
            ImGui.Text("Entities: " .. entities)
        end
        if player ~= nil then
            ImGui.Text(player)
        end

        if ImGui.CollapsingHeader("More controls") then
            ImGui.Text("Draw:")
            for _, mode in ipairs(modes) do
                ImGui.SameLine()
                if ImGui.Button(mode.label) then
                    request("mode " .. mode.name)
                end
            end
            local nextHud = hudNext[hudSetting]
            if nextHud ~= nil then
                ImGui.SameLine()
                if ImGui.Button("HUD: " .. hudSetting .. "###hud") then
                    request("set hud " .. nextHud)
                end
            end
            ImGui.Text("Type:")
            for _, set in ipairs(sets) do
                ImGui.SameLine()
                if ImGui.Button(set) then
                    request("mc " .. set)
                end
            end
            if drive ~= nil then
                ImGui.Text("Drive: " .. drive)
                ImGui.SameLine()
                if ImGui.Button("Drive log 10 s") then
                    request("drivelog 10")
                end
            end
            if ground ~= nil then
                ImGui.Text("Ground: " .. ground)
                ImGui.SameLine()
                if ImGui.Button((groundName == "city" and "Use the floor" or "Use the city") .. "###ground") then
                    request("set ground " .. (groundName == "city" and "floor" or "city"))
                end
                ImGui.SameLine()
                if ImGui.Button("Shapes") then
                    request("shapes")
                end
            end
            if npcsOn ~= nil then
                ImGui.Text("NPCs:")
                ImGui.SameLine()
                if ImGui.Button("List them in the log") then
                    request("npcs")
                end
            end
            if combatOn ~= nil then
                ImGui.Text("Combat:")
                ImGui.SameLine()
                if ImGui.Button("Punch in the log") then
                    request("punch")
                end
            end
            if blasts ~= nil then
                ImGui.Text("Blasts:")
                ImGui.SameLine()
                if ImGui.Button("TNT 5 m ahead") then
                    request("explode 5 4")
                end
            end
            if hideV ~= nil then
                ImGui.Text("V in build mode:")
                ImGui.SameLine()
                if ImGui.Button((hideV == "on" and "Hidden" or "Seen") .. "###hidev") then
                    request("set hidev " .. (hideV == "on" and 0 or 1))
                end
            end
        end

        if ImGui.CollapsingHeader("Fun") then
            for _, row in ipairs(fun) do
                ImGui.Text(row.label)
                for _, button in ipairs(row.buttons) do
                    ImGui.SameLine()
                    if ImGui.Button(button.label) then
                        for _, command in ipairs(button.commands) do
                            request("mc " .. command)
                        end
                    end
                end
            end
        end

        if ImGui.CollapsingHeader("Full status") then
            if status ~= nil then
                for _, line in ipairs(status) do
                    for _, part in ipairs(displayLines(line)) do
                        ImGui.Text(part)
                    end
                end
            end
            if frameLine ~= nil then
                ImGui.Text(frameLine)
            end
        end
    end
    ImGui.End()
end)
