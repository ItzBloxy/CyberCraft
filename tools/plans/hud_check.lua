-- Runs the overlay's init.lua (arg[1]) against status.txt in the current folder, with CET's functions stubbed, and
-- prints the lines and buttons it shows.
local handlers = {}
function registerForEvent(name, fn) handlers[name] = fn end
Game = { GetPlayer = function() return nil end }
ImGuiCond = { FirstUseEver = 0 }
ImGuiWindowFlags = { AlwaysAutoResize = 0 }
local shown = {}
ImGui = {
    SetNextWindowPos = function() end,
    Begin = function() return true end,
    End = function() end,
    SetWindowFontScale = function() end,
    SameLine = function() end,
    Text = function(t) table.insert(shown, "text: " .. t) end,
    Button = function(label) table.insert(shown, "button: " .. label) return false end,
    CollapsingHeader = function(name) return name == "More controls" end,
}
dofile(arg[1])
handlers.onUpdate(1.0)
handlers.onDraw()
for _, line in ipairs(shown) do print(line) end
