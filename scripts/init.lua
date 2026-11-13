function initGameConfiguration(conf)
	conf.rows = 3
	conf.columns = 8
	conf.pebble = 5

	conf.p1_start_x = 4
	conf.p1_start_y = 0
	conf.p1_end_x = 5
	conf.p1_end_y = 0

	conf.p2_start_x = 4
	conf.p2_start_y = 2
	conf.p2_end_x = 5
	conf.p2_end_y = 2


	local schematic = [[
		.#.#.#.#
		.#.#.#.#
		.#.#.#.#
	]]

	local circuit_p1 = [[
		SWWWW#WW
		EEEEEEEN
		........
	]]

	local circuit_p2 = [[
		........
		EEEEEEES
		NWWWW#WW
	]]

	conf:loadSchematic(schematic)
	conf:loadCircuitP1(circuit_p1)
	conf:loadCircuitP2(circuit_p2)
end

function assignTextures(res)
	res:assignBoardTexture('./textures/board.png')
	res:setBoardTile(107.5, 660.5, 412.25, 412.25)
end

function isValidDirection(s)
	return s == "S" or s == "W" or s == "E" or s == "N"
end

function translatePositionByDirection(x, y, dir)
	if dir == "S" then
		y = y + 1
	end
	if dir == "N" then
		y = y - 1
	end
	if dir == "W" then
		x = x - 1
	end
	if dir == "E" then
		x = x + 1
	end
	return x, y
end

function tileClickedEvent(state, data, event)
	print("Tile clicked from lua: " .. data.tileX .. ", " .. data.tileY)
	local p1_pebbles, p2_pebbles = state:getTileState(data.tileX, data.tileY)
	print("Pebbles: " .. p1_pebbles .. " " .. p2_pebbles)
	local conf = state:getConfiguration();
	local p1_dir = conf:getDirectionP1(data.tileX, data.tileY)
	local p2_dir = conf:getDirectionP2(data.tileX, data.tileY)
	print("p1_dir " .. p1_dir)
	print("p2_dir " .. p2_dir)

	if isValidDirection(p1_dir) then
		local newX, newY = translatePositionByDirection(data.tileX, data.tileY, p1_dir)
		state:movePebbleP1(data.tileX, data.tileY, newX, newY)
	end

	if isValidDirection(p2_dir) then
		local newX, newY = translatePositionByDirection(data.tileX, data.tileY, p2_dir)
		state:movePebbleP2(data.tileX, data.tileY, newX, newY)
	end
end