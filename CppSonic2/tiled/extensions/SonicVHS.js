function write_u8(binaryFile, value) {
	binaryFile.write(new Uint8Array([value]).buffer);
}

function write_u16(binaryFile, value) {
	binaryFile.write(new Uint16Array([value]).buffer);
}

function write_u32(binaryFile, value) {
	binaryFile.write(new Uint32Array([value]).buffer);
}

function write_int(binaryFile, value) {
	binaryFile.write(new Int32Array([value]).buffer);
}

function write_float32(binaryFile, value) {
	binaryFile.write(new Float32Array([value]).buffer);
}

function write_tile(binaryFile, index, hflip, vflip, top_solid, lrb_solid) {
	write_u16(binaryFile, index);
	
	var b = 0;
	if (hflip)      b |= 1 << 0;
	if (vflip)      b |= 1 << 1;
	if (top_solid)  b |= 1 << 2;
	if (lrb_solid)  b |= 1 << 3;
	
	write_u8(binaryFile, b);
	
	write_u8(binaryFile, 0); // Pad
}

function find_tile_layer(map, layerName) {
	var result = null;
	
	for (var i = 0; i < map.layerCount; i++) {
		var layer = map.layerAt(i);
		if (layer.isTileLayer && layer.name == layerName) {
			result = layer;
			break;
		}
	}
	
	if (result == null) {
		throw new Error("Layer \"" + layerName + "\" not found.");
	}
	
	return result;
}

function find_obj_layer(map) {
	var result = null;
	
	for (var i = 0; i < map.layerCount; i++) {
		var layer = map.layerAt(i);
		if (layer.isObjectLayer) {
			result = layer;
			break;
		}
	}
	
	if (result == null) {
		throw new Error("Object Layer not found.");
	}
	
	return result;
}

function write_layer(binaryFile, map, layer) {
	if (layer == null) {
		for (var y = 0; y < map.height; y++) {
			for (var x = 0; x < map.width; x++) {
				write_tile(binaryFile, 0, false, false, false, false);
			}
		}
	} else {
		if (map.width  != layer.width)  throw new Error("map.width  != layer.width");
		if (map.height != layer.height) throw new Error("map.height != layer.height");
		
		for (var y = 0; y < map.height; y++) {
			for (var x = 0; x < map.width; x++) {
				var cell = layer.cellAt(x, y);
				if (cell.tileId == -1) {
					write_tile(binaryFile, 0, false, false, false, false);
				} else {
					write_tile(binaryFile, cell.tileId, cell.flippedHorizontally, cell.flippedVertically, true, true);
				}
			}
		}
	}
}

function write_layer2(binaryFile, cells) {
	for (var y = 0; y < cells.length; y++) {
		for (var x = 0; x < cells[0].length; x++) {
			var cell = cells[y][x];
			if (cell.tileId == -1) {
				write_tile(binaryFile, 0, false, false, false, false);
			} else {
				write_tile(binaryFile, cell.tileId, cell.flippedHorizontally, cell.flippedVertically, cell.top_solid, cell.lrb_solid);
			}
		}
	}
}

function strip_filename(path) {
	return path.substring(0, path.lastIndexOf("/"));
}

const OBJ_PLAYER_INIT_POS =  0;
const OBJ_LAYER_SET       =  1;
const OBJ_LAYER_FLIP      =  2;
const OBJ_RING            =  3;
const OBJ_MONITOR         =  4;
const OBJ_SPRING          =  5;
const OBJ_MONITOR_BROKEN  =  6;
const OBJ_MONITOR_ICON    =  7;
const OBJ_SPIKE           =  8;
const OBJ_RING_DROPPED    =  9;
const OBJ_MOVING_PLATFORM = 10;

const DIR_RIGHT = 0;
const DIR_UP    = 1;
const DIR_LEFT  = 2;
const DIR_DOWN  = 3;

function get_obj_type(o) {
	if (o.tile.id == 2) {
		return OBJ_PLAYER_INIT_POS;
	} else if (o.tile.id == 3) {
		return OBJ_RING;
	} else if (o.tile.id >= 4 && o.tile.id <= 13) {
		return OBJ_MONITOR;
	} else if (o.tile.id == 16 || o.tile.id == 17) {
		return OBJ_SPRING;
	} else if (o.tile.id == 18) {
		return OBJ_LAYER_FLIP;
	} else if (o.tile.id == 19) {
		return OBJ_LAYER_SET;
	} else if (o.tile.id == 20) {
		return OBJ_SPIKE;
	} else if (o.tile.id == 21 || o.tile.id == 22) {
		return OBJ_MOVING_PLATFORM;
	}
	
	throw new Error("Unknown object type.");
}
var customMapFormat = {
	name: "Sonic VHS Binary Tilemap",
	extension: "bin",

	write: function(map, fileName) {
		// Tilemap
		
		tiled.log("Writing to " + fileName + "...");
		tiled.log("Stripped filename: " + strip_filename(fileName));
		
		if (!fileName.endsWith("Tilemap.bin")) {
			throw new Error("Filename must be Tilemap.bin");
		}
		
		var file = new BinaryFile(fileName, BinaryFile.WriteOnly);
		
		write_u8(file, "T".charCodeAt(0));
		write_u8(file, "M".charCodeAt(0));
		write_u8(file, "A".charCodeAt(0));
		write_u8(file, "P".charCodeAt(0));
		
		var version = 2;
		write_u32(file, version);
		
		write_int(file, map.width);
		
		write_int(file, map.height);
		
		{
			var layer = find_tile_layer(map, "Ground");
			
			var cells = [];
			for (var y = 0; y < layer.height; y++) {
				var row = [];
				for (var x = 0; x < layer.width; x++) {
					var cell = layer.cellAt(x, y);
					row.push({
						tileId: cell.tileId,
						flippedHorizontally: cell.flippedHorizontally,
						flippedVertically: cell.flippedVertically,
						top_solid: true,
						lrb_solid: true,
					});
				}
				cells.push(row);
			}
			
			layer = find_tile_layer(map, "BG");
			
			for (var y = 0; y < layer.height; y++) {
				for (var x = 0; x < layer.width; x++) {
					var cell = layer.cellAt(x, y);
					if (cell.tileId != -1) {
						cells[y][x] = {
							tileId: cell.tileId,
							flippedHorizontally: cell.flippedHorizontally,
							flippedVertically: cell.flippedVertically,
							top_solid: false,
							lrb_solid: false,
						};
					}
				}
			}
			
			layer = find_tile_layer(map, "Top Solid");
			
			for (var y = 0; y < layer.height; y++) {
				for (var x = 0; x < layer.width; x++) {
					var cell = layer.cellAt(x, y);
					if (cell.tileId != -1) {
						cells[y][x] = {
							tileId: cell.tileId,
							flippedHorizontally: cell.flippedHorizontally,
							flippedVertically: cell.flippedVertically,
							top_solid: true,
							lrb_solid: false,
						};
					}
				}
			}
			
			write_layer2(file, cells);
		}
		
		var layer_b = find_tile_layer(map, "Ground B");
		write_layer(file, map, layer_b);
		
		var layer_c = find_tile_layer(map, "Grass");
		write_layer(file, map, layer_c);
		
		file.commit();
		
		// Objects
		
		fileName = strip_filename(fileName) + "/Objects.bin";
		tiled.log("Writing to " + fileName + "...");
		
		var file = new BinaryFile(fileName, BinaryFile.WriteOnly);
		
		write_u8(file, "O".charCodeAt(0));
		write_u8(file, "B".charCodeAt(0));
		write_u8(file, "J".charCodeAt(0));
		write_u8(file, "T".charCodeAt(0));
		
		var version = 1;
		write_u32(file, version);
		
		var layer = find_obj_layer(map);
		
		write_u32(file, layer.objectCount);
		
		for (var i = 0; i < layer.objectCount; i++) {
			var o = layer.objects[i];
			
			write_int(file, get_obj_type(o));
			
			var flags = 0;
			if (o.tile.id == 18) {
				if (o.resolvedProperty("Grounded")) {
					flags |= 1 << 16;
				}
			}
			write_u32(file, flags);
			
			var x = o.x;
			var y = o.y;
			
			if (o.rotation == 0) {
				x += o.width  / 2;
				y -= o.height / 2;
			} else if (o.rotation == 90) {
				x += o.height / 2;
				y += o.width  / 2;
			} else if (o.rotation == 180) {
				x -= o.width  / 2;
				y += o.height / 2;
			} else if (o.rotation == 270) {
				x -= o.height / 2;
				y -= o.width  / 2;
			}
			
			write_float32(file, x);
			write_float32(file, y);
			
			var direction = 0;
			
			if (o.rotation == 0) {
				direction = DIR_UP;
			} else if (o.rotation == 90) {
				direction = DIR_RIGHT;
			} else if (o.rotation == 180) {
				direction = DIR_DOWN;
			} else if (o.rotation == 270) {
				direction = DIR_LEFT;
			}
			
			// Extras
			if (o.tile.id == 2) {
				// OBJ_PLAYER_INIT_POS
			} else if (o.tile.id == 3) {
				// OBJ_RING
			} else if (o.tile.id >= 4 && o.tile.id <= 13) {
				// OBJ_MONITOR
				var icon = o.tile.id - 4;
				write_int(file, icon);
			} else if (o.tile.id == 16 || o.tile.id == 17) {
				// OBJ_SPRING
				var color = o.tile.id - 16;
				write_int(file, color);
				
				write_int(file, direction);
			} else if (o.tile.id == 18) {
				var radius_x = o.width  / 2;
				var radius_y = o.height / 2;
				write_float32(file, radius_x);
				write_float32(file, radius_y);
			} else if (o.tile.id == 19) {
				var radius_x = o.width  / 2;
				var radius_y = o.height / 2;
				write_float32(file, radius_x);
				write_float32(file, radius_y);
				
				var _layer = o.resolvedProperty("Layer");
				write_int(file, _layer);
			} else if (o.tile.id == 20) {
				write_int(file, direction);
			} else if (o.tile.id == 21 || o.tile.id == 22) {
				var sprite_index = o.tile.id - 21;
				write_u32(file, sprite_index);
				
				var radius_x = 16;
				var radius_y = 16;
				if (o.tile.id == 21) {
					radius_x = 64 / 2;
					radius_y = 12 / 2;
				} else if (o.tile.id == 22) {
					radius_x = 128 / 2;
					radius_y = 30  / 2;
				}
				write_float32(file, radius_x);
				write_float32(file, radius_y);
				
				var xoffset = o.resolvedProperty("xoffset");
				var yoffset = o.resolvedProperty("yoffset");
				write_float32(file, xoffset);
				write_float32(file, yoffset);
				
				var time_multiplier = o.resolvedProperty("time_multiplier");
				write_float32(file, time_multiplier);
			} else {
				throw new Error("Object type not serialized.");
			}
		}
		
		file.commit();
	},
}

tiled.registerMapFormat("Sonic VHS", customMapFormat);
