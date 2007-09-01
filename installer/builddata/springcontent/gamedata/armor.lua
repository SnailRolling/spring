--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
--
--  file:    armor.lua
--  brief:   armor.txt lua parser
--  author:  Dave Rodgers
--
--  Copyright (C) 2007.
--  Licensed under the terms of the GNU GPL, v2 or later.
--
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------


local TDF = VFS.Include('gamedata/parse_tdf.lua')
TDF.AllowDuplicates(true)
TDF.SetKeyFilter(string.lower)

local armor, err = TDF.Parse('armor.txt')
if (armor == nil) then
  error('Error parsing armor.txt: ' .. err)
end


--------------------------------------------------------------------------------
--------------------------------------------------------------------------------

return armor

--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
