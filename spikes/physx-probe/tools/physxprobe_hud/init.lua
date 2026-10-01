-- PhysXProbe HUD: V's position, heading and the nearest PhysX grid lines, for the origin-watch tests of
-- D:\Cyberpunk-Minecraft (spike 1). Read-only. Delete this folder to remove it.
local grid = 1024
local text = nil

local atan2 = math.atan2 or function(y, x) return math.atan(y, x) end
local names = { "N", "NE", "E", "SE", "S", "SW", "W", "NW" }

-- The grid lines on either side of a coordinate. They sit halfway between grid points: 512 + k * 1024.
local function lines(value)
    local below = math.floor((value - grid / 2) / grid) * grid + grid / 2
    return below, below + grid
end

registerForEvent("onUpdate", function()
    local player = Game.GetPlayer()
    if player == nil then
        text = nil
        return
    end
    local p = player:GetWorldPosition()
    local f = player:GetWorldForward()
    local heading = math.deg(atan2(f.x, f.y)) % 360  -- 0 = north (+y), 90 = east (+x)
    local compass = names[math.floor((heading + 22.5) / 45) % 8 + 1]
    local west, east = lines(p.x)
    local south, north = lines(p.y)
    text = string.format(
        "x %.1f   y %.1f   z %.1f\nfacing %s (%d deg)\nx line %d: %.0f m west | %d: %.0f m east\ny line %d: %.0f m south | %d: %.0f m north",
        p.x, p.y, p.z, compass, math.floor(heading + 0.5),
        west, p.x - west, east, east - p.x,
        south, p.y - south, north, north - p.y)
end)

registerForEvent("onDraw", function()
    if text == nil then
        return
    end
    ImGui.SetNextWindowPos(30, 260, ImGuiCond.FirstUseEver)
    if ImGui.Begin("PhysXProbe", ImGuiWindowFlags.AlwaysAutoResize) then
        ImGui.Text(text)
    end
    ImGui.End()
end)
