//
//  main.cpp
//  png2gb
//
//  Created by Lucas Duroj on 15/06/14.
//  Copyright (c) 2014 Lucas Duroj. All rights reserved.
//

#define STB_IMAGE_IMPLEMENTATION

#include <stb_image.h>
#include <string>
#include <list>

const char* VERSION = "0.5b";

struct RawPixel {
	unsigned char gray;
	unsigned char alpha;
};

struct GBPixel {
	unsigned char index;
};

//Setting
unsigned char pallet[] = {0, 1, 2, 3};
bool invertPixels = false;
bool printDefines = true;
bool mapMode = false;

using namespace std;

string HEADER = "//Exported by png2gb \n\n\n";
string START_ARRAY = "const unsigned char %s_data[] = {\n";
string START_TILESET = "const unsigned char %s_tileset[] = {\n";
string START_MAP = "const unsigned char %s_map[] = {\n";
string END_ARRAY = "};\n";

char BUFFER[1024];

int decodeImageFile(const string& imageFilePath, int& width, int& height, GBPixel*& gbPixels) {
	
	FILE* imgFile = fopen(imageFilePath.c_str(), "r");
	if(imgFile == NULL) {
		printf("Error: Can't read file: %s\n", imageFilePath.c_str());
		return 1;
	}
	
	int comp;
	
	RawPixel* pixels = (RawPixel*)stbi_load_from_file(imgFile, &width, &height, &comp, STBI_grey_alpha);
	int pixelCount = width*height;
	
	if(width%8 != 0 || height%8 != 0) {
		printf("Error: Image size has to be multiple of 8 image size is: %s %i x %i\n", imageFilePath.c_str(), width, height);
		return 2;
	}
	
	gbPixels = (GBPixel*)malloc(sizeof(GBPixel)*pixelCount);
	
	for (int i = 0; i < pixelCount; ++i) {
		if(pixels[i].alpha == 0) {
			gbPixels[i].index = 0;
		} else {
			gbPixels[i].index = pallet[3-(pixels[i].gray/64)];
			if(invertPixels)
				gbPixels[i].index = 3-gbPixels[i].index;
		}
	}
	
	free(pixels);
	
	return 0;
}


string writePixelTileRow(const GBPixel* gbPixels) {
	unsigned char hBits = 0;
	unsigned char lBits = 0;
	
	for (int x = 0; x < 8; ++x) {
		lBits = lBits << 1;
		hBits = hBits << 1;
		
		unsigned char p = gbPixels[x].index;
		if(p == 1 || p == 3)
			lBits += 1;
		if(p == 2 || p == 3)
			hBits += 1;
	}
	
	sprintf(BUFFER, "0x%02x,0x%02x", lBits, hBits);
	return string(BUFFER);
}

void writeSpriteData(const string& imageFilePath, GBPixel* gbPixels, const int width, const int height,  string& dataString, string& defineString, const int offset, int& out_size, const bool lastImage) {
	
	size_t lastSlash = imageFilePath.find_last_of('/');
	size_t lastDot = imageFilePath.find_last_of('.');
	
	string imgName = imageFilePath.substr(lastSlash+1, lastDot-(lastSlash+1));
	string fileName = imageFilePath.substr(lastSlash+1, imageFilePath.length()-(lastSlash+1));
	
	const int pixelCount = width*height;
	
	int tilesWidth = width/8;
	int tilesHeight = height/8;
	
	int imageSize = out_size = pixelCount/64;
	sprintf(BUFFER, "#define %s_offset %i\n", imgName.c_str(), offset);
	defineString.append(BUFFER);
	sprintf(BUFFER, "#define %s_size %i\n\n", imgName.c_str(), imageSize);
	defineString.append(BUFFER);
	
	sprintf(BUFFER, "\n\t//  %s %i x %i tiles \n\n", fileName.c_str(), tilesWidth, tilesHeight);
	dataString.append(BUFFER);
	
	for (int tx = 0; tx < tilesWidth; ++tx) {
		for (int ty = 0; ty < tilesHeight; ++ty) {
			
			const int tilePixelI = ((ty*width)*8) + (tx * 8);
			
			dataString.append("\t");
			
			for (int y = 0; y < 8; ++y) {
				
				const int pixelRowI = tilePixelI + y*width;
				dataString.append(writePixelTileRow(&gbPixels[pixelRowI]));
				
				if(!(pixelRowI+8 == pixelCount && lastImage))
					dataString.append(",");
			}
			dataString.append("\n");
		}
	}
}

class GBTile {
public:
	GBTile() {
		gbPixels = new GBPixel[8*8];
	}
	~GBTile() {
		delete this->gbPixels;
	}
	GBPixel* gbPixels;
	
};

void writeMapData(const string& imageFilePath, GBPixel* gbPixels, const int width, const int height, string& dataString, string& defineString) {
	int tilesWidth = width/8;
	int tilesHeight = height/8;
	
	const int tileCount = tilesWidth*tilesHeight;
	unsigned char* mapData = (unsigned char*)malloc(sizeof(unsigned char)*tileCount);
	list<GBTile*> tiles;
	
	int map_i = 0;
	for (int ty = 0; ty < tilesHeight; ++ty) {
		for (int tx = 0; tx < tilesWidth; ++tx) {
			const int tilePixelI = ((ty*width)*8) + (tx * 8);
			
			GBTile* newTile = new GBTile();
			
			for (int y = 0; y < 8; ++y) {
				const int pixelRowI = tilePixelI + y*width;
				memcpy(&(newTile->gbPixels[y*8]), &(gbPixels[pixelRowI]), 8);
			}
			
			int tile_i = 0;
			bool foundCopy = false;
			for (list<GBTile*>::const_iterator tile = tiles.begin(); tile != tiles.end(); ++tile) {
				if (memcmp(newTile->gbPixels, (*tile)->gbPixels, 8*8) == 0) {
					foundCopy = true;
					break;
				}
				++tile_i;
			}
			
			if(!foundCopy) {
				tiles.push_back(newTile);
				mapData[map_i] = tile_i;
			} else {
				mapData[map_i] = tile_i;
				free(newTile);
			}
			printf("%i -> %lu\n", tile_i, tiles.size());
			
			++map_i;
		}
	}
	
	printf("tiles: %lu, map: %i", tiles.size(), tilesWidth*tilesHeight);
	
	size_t lastSlash = imageFilePath.find_last_of('/');
	size_t lastDot = imageFilePath.find_last_of('.');
	
	string imgName = imageFilePath.substr(lastSlash+1, lastDot-(lastSlash+1));
	string fileName = imageFilePath.substr(lastSlash+1, imageFilePath.length()-(lastSlash+1));
	
	
	sprintf(BUFFER, "#define %s_tileset_size %i\n\n", imgName.c_str(), (int)tiles.size());
	defineString.append(BUFFER);
	
	sprintf(BUFFER, "#define %s_map_width %i\n\n", imgName.c_str(), tilesWidth);
	defineString.append(BUFFER);
	
	sprintf(BUFFER, "#define %s_map_height %i\n\n", imgName.c_str(), tilesHeight);
	defineString.append(BUFFER);
	
	sprintf(BUFFER, START_TILESET.c_str(), imgName.c_str());
	dataString.append(BUFFER);
	
	int i = 1;
	for (list<GBTile*>::const_iterator tile = tiles.begin(); tile != tiles.end(); ++tile) {
		dataString.append("\t");
		
		for (int y = 0; y < 8; ++y) {
			
			const int pixelRowI = y*8;
			dataString.append(writePixelTileRow(&((*tile)->gbPixels[pixelRowI])));
			
			if(!(i == tiles.size() && y == 7))
				dataString.append(",");
		}
		++i;
		dataString.append("\n");
	}
	
	dataString.append(END_ARRAY);
	
	dataString.append("\n");
	dataString.append("\n");
	
	sprintf(BUFFER, START_MAP.c_str(), imgName.c_str());
	dataString.append(BUFFER);
	
	dataString.append("\t");
	for (i = 0; i < tileCount; ++i) {
		if(i%tilesWidth == 0 && i != 0)
			dataString.append("\n\t");
		
		sprintf(BUFFER, "0x%02x", mapData[i]);
		dataString.append(BUFFER);
		
		if(i+1 != tileCount)
			dataString.append(",");
		
	}
	dataString.append("\n");
	dataString.append(END_ARRAY);
	
	
	free(mapData);
}




void printHelp() {
	printf("./png2bg [files... , options] \n");
	printf("\n");
	printf("-o        | --output    output path\n");
	printf("-p <iiii> | --pallet    ex: '-p 3210' would invert the pixles data\n");
	printf("-i        | --invert    invertes the final pixel data\n");
	printf("-d y/n    | --defines   turn on or off printing of defines\n");
	printf("-h        | --help      print this message\n");
	printf("-m        | --map       maps the first image to a tileset\n");
}

int main(int argc, const char * argv[]) {
	string outputPath = "";
	list<string>* inputPaths = new list<string>();
	
	bool skipNext = false;
	for (int i = 1; i < argc; ++i) {
		
		if(skipNext) {
			skipNext = false;
			continue;
		}
		
		if(strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output") == 0) {
			if(i+1 >= argc)
				continue;
			outputPath = argv[i+1];
			skipNext = true;
		} else if(strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--pallet") == 0) {
			if(i+1 >= argc)
				continue;
			skipNext = true;
			string palletString = argv[i+1];
			if(palletString.length() < 4)
				continue;
			
			pallet[0] = atoi(palletString.substr(0, 1).c_str());
			pallet[1] = atoi(palletString.substr(1, 1).c_str());
			pallet[2] = atoi(palletString.substr(2, 1).c_str());
			pallet[3] = atoi(palletString.substr(3, 1).c_str());
			
		} else if(strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--invert") == 0) {
			invertPixels = true;
		} else if(strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
			printf("png2gb v%s", VERSION);
			return 0;
		} else if(strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
			printHelp();
			return 0;
		} else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--defines") == 0) {
			if(i+1 >= argc)
				continue;
			skipNext = true;
			printDefines = strcmp(argv[i+1], "y") == 0;
		} else if (strcmp(argv[i], "-m") == 0 || strcmp(argv[i], "--map") == 0) {
			mapMode = true;
		} else {
			inputPaths->push_back(argv[i]);
		}
	}
	
	if(inputPaths->size() == 0) {
		printf("Error: there is no input files \n");
		printHelp();
		return 1;
	}
	
	string filePath = inputPaths->front();
	
	size_t lastDot = filePath.find_last_of('.');
	size_t lastSlash = filePath.find_last_of('/');
	
	string fileName = filePath.substr(lastSlash+1, lastDot-(lastSlash+1));
	string fileDir = filePath.substr(0, lastSlash+1);
	
	if(outputPath.compare("") == 0) {
		outputPath = fileDir;
		outputPath.append(fileName);
		outputPath.append(".c");
	}
	
	string fileString = "";
	fileString.append(HEADER);
	
	string defineString = "";
	string dataString = "";
	
	if(!mapMode) { //Normal mode
		sprintf(BUFFER, START_ARRAY.c_str(), fileName.c_str());
		dataString.append(BUFFER);
		
		int i = 0;
		int data_offset = 0;
		for (list<string>::const_iterator iter = inputPaths->begin(); iter != inputPaths->end(); ++iter) {
			int res;
			
			GBPixel* gbPixels;
			int width;
			int height;
			
			int errCode = decodeImageFile(iter->c_str(), width, height, gbPixels);
			if(errCode == 0) {
				data_offset += res;
			} else {
				return errCode;
			}
			writeSpriteData(iter->c_str(), gbPixels, width, height, dataString, defineString, data_offset, res, ++i == inputPaths->size());
			
			free(gbPixels);
		}
		
		dataString.append(END_ARRAY);
		
	} else {
		
		GBPixel* gbPixels;
		int width;
		int height;
		
		int errCode = decodeImageFile(inputPaths->front().c_str(), width, height, gbPixels);
		if(errCode != 0)
			return errCode;
		writeMapData(inputPaths->front(), gbPixels, width, height, dataString, defineString);
		
		
		free(gbPixels);
	}
	
	if(printDefines)
		fileString.append(defineString);
	fileString.append(dataString);
	
	FILE* cFile = fopen(outputPath.c_str(), "w");
	if(cFile == NULL) {
		printf("Error: Can't write to file: %s\n", outputPath.c_str());
		return 3;
	}
		
	fwrite(fileString.c_str(), sizeof(char), fileString.length(), cFile);
	
    return 0;
}

