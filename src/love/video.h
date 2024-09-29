/**
 * Copyright (c) 2006-2024 LOVE Development Team
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 **/

#pragma once

// LOVE
#include "common/math.h"
#include "drawable.h"
#include "texture.h"
#include "vertex.h"
#include "video/VideoStream.h"
#include "audio/Source.h"

namespace love
{
namespace gfx
{

class graphics;

class Video : public Drawable
{
public:

	static love::Type type;

	Video(graphics *gfx, love::video::VideoStream *stream, float dpiscale = 1.0f);
	virtual ~Video();

	// Drawable
	void draw(graphics *gfx, const Matrix4 &m) override;

	love::video::VideoStream *getStream();

	love::audiomod::Source *getSource();
	void setSource(love::audiomod::Source *source);

	int getWidth() const;
	int getHeight() const;

	int getPixelWidth() const;
	int getPixelHeight() const;

	void setSamplerState(const SamplerState &s);
	const SamplerState &getSamplerState() const;

private:

	void update();

	StrongRef<love::video::VideoStream> stream;

	int width;
	int height;

	SamplerState samplerState;

	Vertex vertices[4];

	StrongRef<Texture> textures[3];
	StrongRef<love::audiomod::Source> source;

}; // Video

} // graphics
} // love
