require "core.error"

local Overlay = require "bindings.overlay"
local ConsoleOverlay = require "dev.console_overlay"
local CheatOverlay = require "dev.cheat_command_overlay"
local GameCommands = require "dev.commands.game_commands"

GameCommands.registerGameCommands()

Overlay.addOverlay("console", ConsoleOverlay)
Overlay.addOverlay("cheatTable", CheatOverlay)

--[[
    Here's a little example of how to create a simple Overlay to render
    a window with a text and register it to the system

local myOverlay = {
    init = function ()
        print("Init Function")
    end,

    ---@param ctx NuklearContext
    update = function (ctx)
        Overlay.nk.window_begin(ctx, "HELLO WORLD", { x = 200, y = 200, w = 300, h = 100 },
            { "scalable", "movable", "title" })
        Overlay.nk.layout_row_dynamic(ctx, 0, 1)
        Overlay.nk.label_colored(ctx, "HELLO THERE!!!", { 255, 255, 0, 255 })
        Overlay.nk.window_end(ctx)
    end,

    close = function ()
        print("Close Function")
    end
}

Overlay.addOverlay("MyOverlay", myOverlay)
]]
