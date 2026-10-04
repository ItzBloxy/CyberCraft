-- BlockRender HUD: what BlockRender (spikes 3 and 4 of D:\Cyberpunk-Minecraft) is doing, from the status.txt it writes
-- into this folder once a second, plus V's position, heading and Minecraft block, and the nearest structure, and the
-- frame time as CET sees it, which works without BlockRender too and goes into frames.txt here as well. It asks
-- BlockRender for things through blockrender.request in this folder.
-- Kept small: one row of buttons (link, Minecraft's blocks as bodies, step 4's test box, the turn), and a line each for
-- the last request's reply, build mode (F8) and its drive, the bodies, and V. Two folded sections hold the rest: "More
-- controls" (the drawing mode, Minecraft's HUD, commands typed into Minecraft, the drive log, the ground and its
-- shapes) and "Full status" (every status line, and the frame time).
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

-- Sends BlockRender a command. It reads blockrender.request in this folder once a second, once the line is whole.
local function request(command)
    local file = io.open("blockrender.request", "w")
    if file ~= nil then
        file:write(command, "\n")
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
        if status == nil then
            ImGui.Text("no status.txt yet: is BlockRender loaded?")
        end
        if reply ~= nil then
            ImGui.Text("Last request: " .. reply)
        end
        if build ~= nil then
            ImGui.Text("Build mode (F8): " .. build .. (driveState and ("; drive " .. driveState) or ""))
        end
        if bodies ~= nil then
            ImGui.Text("Bodies: " .. bodies)
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
