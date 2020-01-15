#include "editor.h"
#include "texturetools.h"
#include "editabletexture.h"

using namespace base;

inline ubyte saturate(int v) { return v<0? 0: v>255? 255: v; }

void TextureTool::paint(BrushData& data, const Brush& brush, int flags) {
	int channel = flags&3;
	bool add = flags&4;
	if(channel >= data.getChannels()) return;

	// Loop pixels
	float weight, *pixel;
	const Point& o = data.getOffset();
	const Point& e = data.getSize();
	for(int x=0; x<e.x; ++x) for(int y=0; y<e.y; ++y) {
		weight = brush.getWeight( data.getWorldPosition(x,y) );
		if(weight==0) continue;
		pixel = data.getValue(x,y);
		ubyte* buf = buffer->value(x+o.x, y+o.y);

		if(add) {
			ubyte old = pixel[channel] - *buf;
			ubyte max = saturate(old + weight*255);
			if(pixel[channel] < max) {
				pixel[channel] = max;
				*buf = max - old;
			}
		} else {
			ubyte old = pixel[channel] + *buf;
			ubyte min = saturate(old - weight*255);
			if(pixel[channel] > min) {
				pixel[channel] = min;
				*buf = old - min;
			}
		}
	}
}


void ColourTool::paint(BrushData& data, const Brush& brush, int flags) {
	if(data.getChannels()<3) return;
	const ubyte c[3] = { (ubyte)((flags>>16)&0xff), (ubyte)((flags>>8)&0xff), (ubyte)(flags&0xff) };

	// Loop pixels
	float weight, *pixel;
	const Point& o = data.getOffset();
	const Point& e = data.getSize();
	for(int x=0; x<e.x; ++x) for(int y=0; y<e.y; ++y) {
		weight = brush.getWeight( data.getWorldPosition(x,y) );
		if(weight==0) continue;

		pixel = data.getValue(x,y);
		ColourToolBuffer& buf = *buffer->value(x+o.x, y+o.y);
		if(!buf.set) {
			buf.o[0] = pixel[0];
			buf.o[1] = pixel[1];
			buf.o[2] = pixel[2];
			buf.set = true;
		}
		
		for(int i=0; i<3; ++i) {
			if(buf.w[i] < weight) buf.w[i] = weight;
			pixel[i] = buf.o[i] + buf.w[i] * (c[i] - buf.o[i]);
		}
	}
}

void IndexTool::paint(BrushData& data, const Brush& brush, int flags) {
	float index = flags;
	float weight;
	const Point& e = data.getSize();
	for(int x=0; x<e.x; ++x) for(int y=0; y<e.y; ++y) {
		weight = brush.getWeight( data.getWorldPosition(x,y) );
		if(weight==0) continue;
		// TODO: Dissolve - need 2d perlin noise?
		*data.getValue(x, y) = index;
	}
}



void IndexWeightTool::paint(BrushData& data, const Brush& brush, int flags) {
	int channels = data.getChannels() / 2;
	float material = flags;
	// Data contains indices then weights

	float weight, t;
	int exist, remain, current;
	float* indices;
	float* weights;
	const Point& e = data.getSize();
	for(int x=0; x<e.x; ++x) for(int y=0; y<e.y; ++y) {
		weight = brush.getWeight( data.getWorldPosition(x,y) );
		if(weight==0) continue;

		indices = data.getValue(x, y);
		weights = indices + channels;
		if(weight*255 <= weights[channels-1]) continue; // No change
		weights[channels] = 0;

		ubyte& buffer = *this->buffer->value(x, y);

		// get existing weight for this material
		for(exist=0; exist<channels; ++exist)
			if(indices[exist]==material || weights[exist]==0) break;

		// Calculate value using paintBuffer
		if(buffer == 0) buffer = weights[exist]? weights[exist]: 1;
		weight = fmax(weights[exist], buffer + weight*255);
		if(exist==channels && (--exist,true) && weights[exist] < weights[exist+1]) continue;

		// Set value
		weights[exist] = weight<255? weight: 255;
		indices[exist] = material;

		// Normalise
		remain = 255 - weights[exist];
		current = -weights[exist];
		for(int i=0; i<channels; ++i) current += weights[i];
		if(current) for(int i=0; i<channels; ++i) if(i!=exist) weights[i] = weights[i] * remain / current;

		// Fix rounding errors
		remain = weights[0] + weights[1] + weights[2] + weights[3] - 255;
		if(remain) weights[0] -= remain;
		
		// Sort channels by weight (bubble selected up)
		while(exist>0) {
			if(weight > weights[exist-1]) {
				t = indices[exist]; indices[exist] = indices[exist-1]; indices[exist-1] = t;
				t = weights[exist]; weights[exist] = weights[exist-1]; weights[exist-1] = t;
				--exist;
			} else break;
		}
	}
}

