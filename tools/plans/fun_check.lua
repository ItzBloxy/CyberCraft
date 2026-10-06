-- Runs the overlay's init.lua (arg[1]) in the current folder with CET's functions stubbed, against status.txt here.
--   lua fun_check.lua <init.lua> show              prints the lines and buttons shown, More controls and Fun open
--   lua fun_check.lua <init.lua> click <label>...  clicks each button in turn, one a frame, then plays BlockRender:
--                                                  each frame it reads blockrender.request and deletes it, printing it
--   lua fun_check.lua <init.lua> hold <label>...   clicks them, then runs 5 frames with nobody reading the file, prints
--                                                  the file and the waiting line, then reads one a frame as above
local handlers = {}
function registerForEvent(name, fn) handlers[name] = fn end
Game = { GetPlayer = function() return nil end }
ImGuiCond = { FirstUseEver = 0 }
ImGuiWindowFlags = { AlwaysAutoResize = 0 }
local click = nil
local shown = {}
ImGui = {
    SetNextWindowPos = function() end,
    Begin = function() return true end,
    End = function() end,
    SetWindowFontScale = function() end,
    SameLine = function() end,
    Text = function(t) table.insert(shown, "text: " .. t) end,
    Button = function(label)
        table.insert(shown, "button: " .. label)
        return label == click
    end,
    CollapsingHeader = function(name) return name == "More controls" or name == "Fun" end,
}

local function take()
    local file = io.open("blockrender.request", "r")
    if file == nil then
        return nil
    end
    local text = file:read("a")
    file:close()
    os.remove("blockrender.request")
    return text
end

dofile(arg[1])
os.remove("blockrender.request")
handlers.onUpdate(1.0)
shown = {}
handlers.onDraw()
local mode = arg[2]
if mode == "show" then
    for _, line in ipairs(shown) do print(line) end
    return
end
for i = 3, #arg do
    click = arg[i]
    shown = {}
    handlers.onDraw()
    local found = false
    for _, line in ipairs(shown) do
        if line == "button: " .. click then found = true end
    end
    if not found then print("no button: " .. click) end
    click = nil
end
if mode == "hold" then
    for _ = 1, 5 do handlers.onUpdate(0.016) end
    local file = io.open("blockrender.request", "r")
    io.write("unread after 5 frames: ", file and file:read("a") or "none\n")
    if file then file:close() end
    shown = {}
    handlers.onDraw()
    for _, line in ipairs(shown) do
        if line:match("^text: Requests waiting") then print(line) end
    end
end
for _ = 1, 200 do
    handlers.onUpdate(0.016)
    local text = take()
    if text ~= nil then io.write("request: ", text) end
end
shown = {}
handlers.onDraw()
for _, line in ipairs(shown) do
    if line:match("^text: Requests waiting") then print("still: " .. line) end
end
