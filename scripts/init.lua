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
		SEEEE#EE
		WWWWWWWN
		........
	]]

	local circuit_p2 = [[
		........
		WWWWWWWS
		NEEEE#EE
	]]

	conf:loadSchematic(schematic)
	conf:loadCircuitP1(circuit_p1)
	conf:loadCircuitP2(circuit_p2)
end

function assignTextures(res)
	res:assignTileTexture('.', './textures/tile1.png')
	res:assignTileTexture('#', './textures/tile2.png')
end

function randomEvent(state, data, event)
	print("Random event from lua!")

	local newData = {
		skibidi = "yes"
	}

	event:send("notRandom", newData, false)
end

function notRandomEvent(state, data, event)
	print("Not random event in lua!")

	print("skibidi?!?!?! " .. data.skibidi)
end