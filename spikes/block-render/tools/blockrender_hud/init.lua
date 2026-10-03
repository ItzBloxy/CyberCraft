-- BlockRender HUD: what BlockRender (spikes 3 and 4 of D:\Cyberpunk-Minecraft) is doing, from the status.txt it writes
-- into this folder once a second, plus V's position, heading and Minecraft block, and the nearest structure, and the
-- frame time as CET sees it, which works without BlockRender too and goes into frames.txt here as well. Its buttons
-- switch BlockRender's drawing mode, link it to Minecraft, type commands into Minecraft, and cycle when Minecraft's
-- HUD shows, through blockrender.request in this folder. A line under them shows build mode, which F8 toggles, and
-- Minecraft's hotbar slot. Delete this folder to remove it.
local status = nil       -- status.txt's lines, except the structures
local structures = {}    -- { name, min = { x, y, z }, max = { x, y, z } }, in world coordinates
local hudSetting = nil   -- the hud line's setting: "never", "in build mode" or "always"
local build = nil        -- the build line's first two parts: on, or off and why; and the slot
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

-- Sends BlockRender a command. It reads blockrender.request in this folder once a second, once the line is whole.
local function request(command)
    local file = io.open("blockrender.request", "w")
    if file ~= nil then
        file:write(command, "\n")
        file:close()
    end
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
    for line in file:lines() do
        local name, a, b, c, d, e, f = line:match("^structure=(%S+) (%S+) (%S+) (%S+) (%S+) (%S+) (%S+)$")
        hudSetting = line:match("^hud=([^,]+),") or hudSetting
        local mode, slot = line:match("^build=([^;]*); ([^;]*);")
        if mode ~= nil then
            build = mode .. ", " .. slot
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

registerForEvent("onUpdate", function(delta)
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
    local text = string.format(
        "V: x %.2f  y %.2f  z %.2f, heading %d (0 north, 90 east)\nMinecraft block at V's feet: %d %d %d",
        p.x, p.y, p.z, math.floor(heading + 0.5), math.floor(p.x), math.floor(p.z), math.floor(-p.y))
    local nearest, nearestName = nil, nil
    for _, box in ipairs(structures) do
        local distance = distanceToBox(p, box)
        if nearest == nil or distance < nearest then
            nearest, nearestName = distance, box.name
        end
    end
    if nearest ~= nil then
        text = text .. string.format("\nnearest structure: %s, %.2f m away", nearestName, nearest)
    end
    player = text
end)

registerForEvent("onDraw", function()
    ImGui.SetNextWindowPos(30, 420, ImGuiCond.FirstUseEver)
    if ImGui.Begin("BlockRender", ImGuiWindowFlags.AlwaysAutoResize) then
        if ImGui.SetWindowFontScale then
            ImGui.SetWindowFontScale(fontScale)
        end
        ImGui.Text("Draw:")
        for _, mode in ipairs(modes) do
            ImGui.SameLine()
            if ImGui.Button(mode.label) then
                request("mode " .. mode.name)
            end
        end
        ImGui.Text("Minecraft:")
        ImGui.SameLine()
        if ImGui.Button("Link") then
            request("link on")
        end
        ImGui.SameLine()
        if ImGui.Button("Unlink") then
            request("link off")
        end
        local nextHud = hudNext[hudSetting]
        if nextHud ~= nil then
            -- "###hud" keeps the button's ID the same while its label changes.
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
        if build ~= nil then
            ImGui.Text("Build mode (F8): " .. build)
        end
        ImGui.Separator()
        if status == nil then
            ImGui.Text("no status.txt yet: is BlockRender loaded?")
        else
            for _, line in ipairs(status) do
                for _, part in ipairs(displayLines(line)) do
                    ImGui.Text(part)
                end
            end
        end
        if frameLine ~= nil then
            ImGui.Text(frameLine)
        end
        if player ~= nil then
            ImGui.Separator()
            ImGui.Text(player)
        end
    end
    ImGui.End()
end)
