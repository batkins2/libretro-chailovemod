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
#include "../graphicsreadback.h"
#include "FenceSync.h"
#include "../common/math.h"

namespace love
{
namespace gfx
{
namespace opengl
{

class GraphicsReadback final : public love::gfx::GraphicsReadback
{
public:

	GraphicsReadback(love::gfx::graphics *gfx, ReadbackMethod method, love::gfx::Buffer *buffer, size_t offset, size_t size, data::ByteData *dest, size_t destoffset);
	GraphicsReadback(love::gfx::graphics *gfx, ReadbackMethod method, love::gfx::Texture *texture, int slice, int mipmap, const Rect &rect, imagemod::ImageData *dest, int destx, int desty);
	virtual ~GraphicsReadback();

	void wait() override;
	void update() override;

private:

	FenceSync sync;

	StrongRef<love::gfx::Buffer> stagingBuffer;

}; // GraphicsReadback

} // opengl
} // graphics
} // love
