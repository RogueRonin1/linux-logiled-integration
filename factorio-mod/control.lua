-- Verification probe: confirms helpers.write_file works from a native Linux
-- build, and measures how cheap a decimated tick handler is.
local EVERY = 6  -- ~10 Hz at 60 UPS

local function snapshot()
  local p = game.get_player(1)
  local f = p and p.force or game.forces.player
  local s = p and p.surface
  local health = 0
  if p and p.character then health = p.character.get_health_ratio() or 0 end
  local alerts = 0
  if p then
    for _, bysurface in pairs(p.get_alerts{}) do
      for _, list in pairs(bysurface) do alerts = alerts + #list end
    end
  end
  return string.format(
    '{"tick":%d,"daytime":%.3f,"research":"%s","progress":%.4f,"health":%.3f,"alerts":%d}\n',
    game.tick, s and s.daytime or 0,
    f.current_research and f.current_research.name or "",
    f.research_progress or 0, health, alerts)
end

script.on_event(defines.events.on_tick, function(e)
  if e.tick % EVERY ~= 0 then return end
  helpers.write_file("g915/state.json", snapshot(), false)
  helpers.write_file("g915/log.jsonl", snapshot(), true)
end)

script.on_init(function()
  helpers.write_file("g915/boot.txt", "on_init fired at tick " .. game.tick .. "\n", false)
end)
