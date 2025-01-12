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

function strip_filename(path) {
	return path.substring(0, path.lastIndexOf("/"));
}

function get_obj_type(o) {
	if (o.tile.id == 2) {
		return 0; // OBJ_PLAYER_INIT_POS
	} else if (o.tile.id == 3) {
		return 3; // OBJ_RING
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
		
		var file = new BinaryFile(fileName, BinaryFile.WriteOnly);
		
		write_u8(file, "T".charCodeAt(0));
		write_u8(file, "M".charCodeAt(0));
		write_u8(file, "A".charCodeAt(0));
		write_u8(file, "P".charCodeAt(0));
		
		var version = 2;
		write_u32(file, version);
		
		write_int(file, map.width);
		
		write_int(file, map.height);
		
		var layer_a = find_tile_layer(map, "Ground");
		write_layer(file, map, layer_a);
		
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
			write_u32(file, flags);
			
			var x = o.x + o.width / 2;
			write_float32(file, x);
			
			var y = o.y + o.height / 2;
			write_float32(file, y);
			
			// Extras
			if (o.tile.id == 2) {
				// OBJ_PLAYER_INIT_POS
			} else if (o.tile.id == 3) {
				// OBJ_RING
			} else {
				throw new Error("Object type not serialized.");
			}
		}
		
		file.commit();
	},
}

tiled.registerMapFormat("Sonic VHS", customMapFormat);
