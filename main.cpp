//
//  main.cpp
//  png2gb
//
//  Created by Lucas Duroj on 15/06/14.
//  Copyright (c) 2014 Lucas Duroj. All rights reserved.
//

#define STB_IMAGE_IMPLEMENTATION
//#include <iostream>
#include <stb_image.h>
#include <string>
#include <list>

const char* VERSION = "0.2b";

struct RawPixel {
	unsigned char gray;
	unsigned char alpha;
};

struct GBPixel {
	unsigned char index;
};

unsigned char pallet[] = {0, 1, 2, 3};
bool invertPixels = false;
bool printDefines = true;

using namespace std;

string HEADER = "//Exported by png2gb \n\n\n";
string START_ARRAY = "const unsigned char %s_data[] = {\n";
string END_ARRAY = "};\n";

int decodeImageFile(const string& imageFilePath, string& dataString, string& defineString, const int offset, int& out_size, const bool lastImage) {
	char BUFFER[1024];
	
	size_t lastSlash = imageFilePath.find_last_of('/');
	size_t lastDot = imageFilePath.find_last_of('.');
	
	string imgName = imageFilePath.substr(lastSlash+1, lastDot-(lastSlash+1));
	string fileName = imageFilePath.substr(lastSlash+1, imageFilePath.length()-(lastSlash+1));
	
	FILE* imgFile = fopen(imageFilePath.c_str(), "r");
	if(imgFile == NULL) {
		printf("Error: Can't read file: %s\n", imageFilePath.c_str());
		return 1;
	}
	
	int width;
	int height;
	int comp;
	
	RawPixel* pixels = (RawPixel*)stbi_load_from_file(imgFile, &width, &height, &comp, STBI_grey_alpha);
	int pixelCount = width*height;
	
	if(width%8 != 0 || height%8 != 0) {
		printf("Error: Image size has to be multiple of 8 image size is: %s %i x %i\n", imageFilePath.c_str(), width, height);
		return 2;
	}
	
	GBPixel* gbPixels = (GBPixel*)malloc(sizeof(GBPixel)*pixelCount);
	
	int tilesWidth = width/8;
	int tilesHeight = height/8;
	
	for (int i = 0; i < pixelCount; ++i) {
		if(pixels[i].alpha == 0) {
			gbPixels[i].index = 0;
		} else {
			gbPixels[i].index = pallet[3-(pixels[i].gray/64)];
			if(invertPixels)
				gbPixels[i].index = 3-gbPixels[i].index;
		}
	}
	
	
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
				
				unsigned char hBits = 0;
				unsigned char lBits = 0;
				
				const int pixelRowI = tilePixelI + y*width;
				
				for (int x = 0; x < 8; ++x) {
					lBits = lBits << 1;
					hBits = hBits << 1;
					
					unsigned char p = gbPixels[pixelRowI+x].index;
					if(p == 1 || p == 3)
						lBits += 1;
					if(p == 2 || p == 3)
						hBits += 1;
				}
				
				sprintf(BUFFER, "0x%02x,0x%02x", lBits, hBits);
				dataString.append(BUFFER);
				
				if(!(pixelRowI+8 == pixelCount && lastImage))
					dataString.append(",");
			}
			dataString.append("\n");
		}
	}
	
	free(gbPixels);
	
	
	
	return 0;
}

void printHelp() {
	printf("./png2bg [files... , options] \n");
	printf("\n");
	printf("-o        | --output    output path\n");
	printf("-p <iiii> | --pallet    ex: '-p 3210' would invert the pixles data\n");
	printf("-i        | --invert    invertes the final pixel data\n");
	printf("-d y/n    | --defines   turn on or off printing of defines\n");
	printf("-h        | --help      print this message\n");
}

int main(int argc, const char * argv[]) {
	char BUFFER[1024];
	
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
		} else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "-defines") == 0) {
			if(i+1 >= argc)
				continue;
			skipNext = true;
			printDefines = strcmp(argv[i+1], "y") == 0;
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
	sprintf(BUFFER, START_ARRAY.c_str(), fileName.c_str());
	dataString.append(BUFFER);
	
	int i = 0;
	int data_offset = 0;
	for (list<string>::const_iterator iter = inputPaths->begin(); iter != inputPaths->end(); ++iter) {
		int res;
		int errCode = decodeImageFile(iter->c_str(), dataString, defineString, data_offset, res, ++i == inputPaths->size());
		if(errCode == 0) {
			data_offset += res;
		} else {
			return errCode;
		}
	}
	
	dataString.append(END_ARRAY);
	
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

