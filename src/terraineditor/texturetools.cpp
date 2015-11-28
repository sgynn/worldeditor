#include "texturetools.h"
#include "editabletexture.h"

using namespace base;

inline ubyte saturate(int v) { return v<0? 0: v>255? 255: v; }

void TextureTool::paint(const Brush& b, int flags) {
	if(!texture) return;

	Rect r = getRect(b);
	texture->clampRect(r);
	int channel = flags&3;
	bool add = flags&4;

	// Loop pixels
	float w;
	ubyte pixel[4];
	Point e = r.position() + r.size();
	for(int x=r.x; x<e.x; ++x) for(int y=r.y; y<e.y; ++y) {
		w = getValue(b, x, y);
		if(w==0) continue;
		texture->getPixel(x, y, pixel);
		ubyte* buf = buffer->value(x, y);

		if(add) {
			ubyte old = pixel[channel] - *buf;
			ubyte max = saturate(old + w*255);
			if(pixel[channel] < max) {
				pixel[channel] = max;
				*buf = max - old;
			}
		} else {
			ubyte old = pixel[channel] + *buf;
			ubyte min = saturate(old - w*255);
			if(pixel[channel] > min) {
				pixel[channel] = min;
				*buf = old - min;
			}
		}
		texture->setPixel(x, y, pixel);
	}
}


void ColourTool::paint(const Brush& b, int flags) {
	if(!texture) return;

	Rect r = getRect(b);
	texture->clampRect(r);
	const ubyte c[3] = { (ubyte)((flags>>16)&0xff), (ubyte)((flags>>8)&0xff), (ubyte)(flags&0xff) };

	// Loop pixels
	float w;
	ubyte pixel[4];
	Point e = r.position() + r.size();
	for(int x=r.x; x<e.x; ++x) for(int y=r.y; y<e.y; ++y) {
		w = getValue(b, x, y);
		if(w==0) continue;

		texture->getPixel(x, y, pixel);
		float* buf = buffer->value(x, y);
		float v = w - *buf;
		*buf = w;

		pixel[0] = pixel[0] + v * (c[0] - pixel[0]);
		pixel[1] = pixel[1] + v * (c[1] - pixel[1]);
		pixel[2] = pixel[2] + v * (c[2] - pixel[2]);

		texture->setPixel(x, y, pixel);
	}
}




void MaterialTool::paint(const Brush& b, int material) {
	if(!weightMap || !indexMap) return;

	Rect r = getRect(b);
	weightMap->clampRect(r);
	int channels = indexMap->getChannels();

	float w;
	int exist, remain, current;
	ubyte t;
	ubyte* index = 0;
	ubyte* weight = 0;
	Point e = r.position() + r.size();
	for(int x=r.x; x<e.x; ++x) for(int y=r.y; y<e.y; ++y) {
		w = getValue(b, x, y);
		if(w==0) continue;
		weightMap->getPixel(x, y, weight);
		if(w*255 <= weight[channels-1]) continue; // No change

		indexMap->getPixel(x, y, index);
		ubyte& buffer = *this->buffer->value(x, y);

		// get existing weight for this material
		for(exist=0; exist<4; ++exist)
			if(index[exist]==material || weight[exist]==0) break;

		// Calculate value using paintBuffer
		if(buffer == 0) buffer = exist<4? weight[exist]: 0;
		w = exist<channels? fmax(weight[exist], buffer + w*255): w*255;
		if(exist == 4) {
			if(weight[exist]<w) --exist;
			else continue;
		}

		// Set value
		weight[exist] = w<255? w: 255;
		index[exist] = material;

		// Normalise
		remain = 255 - weight[exist];
		current = weight[0] + weight[1] + weight[2] + weight[3] - weight[exist];
		if(current) for(int i=0; i<4; ++i) if(i!=exist) weight[i] = weight[i] * remain / current;

		// Fix rounding errors
		remain = weight[0] + weight[1] + weight[2] + weight[3] - 255;
		if(remain) weight[0] -= remain;
		
		// Sort channels by weight (bubble selected up)
		while(exist>0) {
			if(w > weight[exist-1]) {
				t = index[exist]; index[exist] = index[exist-1]; index[exist-1] = t;
				t = weight[exist]; weight[exist] = weight[exist-1]; weight[exist-1] = t;
				--exist;
			} else break;
		}

		// Write pixels
		weightMap->setPixel(x, y, weight);
		indexMap->setPixel(x, y, index);
	}
}

