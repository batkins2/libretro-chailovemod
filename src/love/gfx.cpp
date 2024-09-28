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

// LOVE
#include "gfx.h"
#include "buffer.h"
#include "math/MathModule.h"
#include "data/DataModule.h"
#include "polyline.h"
#include "font/FontMod.h"
#include "window/Window.h"
#include "spritebatch.h"
#include "particlesystem.h"
#include "fontmod.h"
#include "video.h"
#include "textbatch.h"
#include "common/deprecation.h"
#include "common/config.h"

// C++
#include <algorithm>
#include <stdlib.h>

namespace love
{
namespace gfx
{

static bool gammaCorrect = false;
static bool debugMode = false;
static bool debugModeQueried = false;

void setGammaCorrect(bool gammacorrect)
{
	gammaCorrect = gammacorrect;
}

bool isGammaCorrect()
{
	return gammaCorrect;
}

void gammaCorrectColor(Colorf &c)
{
	if (isGammaCorrect())
	{
		c.r = math::gammaToLinear(c.r);
		c.g = math::gammaToLinear(c.g);
		c.b = math::gammaToLinear(c.b);
	}
}

Colorf gammaCorrectColor(const Colorf &c)
{
	Colorf r = c;
	gammaCorrectColor(r);
	return r;
}

void unGammaCorrectColor(Colorf &c)
{
	if (isGammaCorrect())
	{
		c.r = math::linearToGamma(c.r);
		c.g = math::linearToGamma(c.g);
		c.b = math::linearToGamma(c.b);
	}
}

Colorf unGammaCorrectColor(const Colorf &c)
{
	Colorf r = c;
	unGammaCorrectColor(r);
	return r;
}

bool isDebugEnabled()
{
	if (!debugModeQueried)
	{
		const char *debugenv = getenv("LOVE_GRAPHICS_DEBUG");
		debugMode = debugenv != nullptr && debugenv[0] != '0';
		debugModeQueried = true;
	}

	return debugMode;
}

love::Type type("graphics", &Module::type);

namespace opengl { extern love::gfx::graphics *createInstance(); }
#ifdef LOVE_GRAPHICS_METAL
namespace metal { extern love::gfx::graphics *createInstance(); }
#endif
// #ifdef LOVE_GRAPHICS_VULKAN
// namespace vulkan { extern love::gfx::graphics* createInstance(); }
// #endif

static const Renderer rendererOrder[] = {
	RENDERER_METAL,
	RENDERER_OPENGL,
	RENDERER_VULKAN,
};

static std::vector<Renderer> defaultRenderers =
{
	RENDERER_METAL,
	RENDERER_OPENGL,
	RENDERER_VULKAN,
};

static std::vector<Renderer> _renderers = defaultRenderers;

const std::vector<Renderer> &getDefaultRenderers()
{
	return defaultRenderers;
}

const std::vector<Renderer> &getRenderers()
{
	return _renderers;
}

void setRenderers(const std::vector<Renderer> &renderers)
{
	_renderers = renderers;
}

graphics *createInstance()
{
	graphics *instance = Module::getInstance<graphics>(Module::M_GRAPHICS);

	if (instance != nullptr)
		instance->retain();
	else
	{
		for (auto r : rendererOrder)
		{

			if (std::find(_renderers.begin(), _renderers.end(), r) == _renderers.end())
				continue;

// #ifdef LOVE_GRAPHICS_VULKAN
// 			if (r == RENDERER_VULKAN)
// 				instance = vulkan::createInstance();
// #endif
			if (r == RENDERER_OPENGL)
				instance = opengl::createInstance();
#ifdef LOVE_GRAPHICS_METAL
			if (r == RENDERER_METAL)
				instance = metal::createInstance();
#endif
			if (instance != nullptr)
				break;
		}
	}

	return instance;
}

graphics::DisplayState::DisplayState()
{
	defaultSamplerState.mipmapFilter = SamplerState::MIPMAP_FILTER_LINEAR;
}

graphics::graphics(const char *name)
	: Module(M_GRAPHICS, name)
	, width(0)
	, height(0)
	, pixelWidth(0)
	, pixelHeight(0)
	, backbufferHasStencil(false)
	, backbufferHasDepth(false)
	, created(false)
	, active(true)
	, batchedDrawState()
	, deviceProjectionMatrix()
	, renderTargetSwitchCount(0)
	, drawCalls(0)
	, drawCallsBatched(0)
	, quadIndexBuffer(nullptr)
	, fanIndexBuffer(nullptr)
	, capabilities()
	, defaultTextures()
	, defaultTexelBuffers()
	, defaultStorageBuffer(nullptr)
	, cachedShaderStages()
{
	transformStack.reserve(16);
	transformStack.push_back(Matrix4());

	pixelScaleStack.reserve(16);
	pixelScaleStack.push_back(1);

	states.reserve(10);
	states.push_back(DisplayState());

	if (!Shader::initialize())
		throw love::Exception("Shader support failed to initialize!");
}

graphics::~graphics()
{
	if (quadIndexBuffer != nullptr)
		quadIndexBuffer->release();
	if (fanIndexBuffer != nullptr)
		fanIndexBuffer->release();

	releaseDefaultResources();

	// Clean up standard shaders before the active shader. If we do it after,
	// the active shader may try to activate a standard shader when deactivating
	// itself, which will cause problems since it calls Graphics methods in the
	// Graphics destructor.
	for (int i = 0; i < Shader::STANDARD_MAX_ENUM; i++)
	{
		if (Shader::standardShaders[i])
		{
			Shader::standardShaders[i]->release();
			Shader::standardShaders[i] = nullptr;
		}
	}

	states.clear();

	defaultFont.set(nullptr);

	if (batchedDrawState.vb[0])
		batchedDrawState.vb[0]->release();
	if (batchedDrawState.vb[1])
		batchedDrawState.vb[1]->release();
	if (batchedDrawState.indexBuffer)
		batchedDrawState.indexBuffer->release();

	for (int i = 0; i < (int) SHADERSTAGE_MAX_ENUM; i++)
		cachedShaderStages[i].clear();

	pendingReadbacks.clear();
	clearTemporaryResources();

	Shader::deinitialize();
}

void graphics::createQuadIndexBuffer()
{
	if (quadIndexBuffer != nullptr)
		return;

	size_t size = sizeof(uint16) * getIndexCount(TRIANGLEINDEX_QUADS, LOVE_UINT16_MAX);

	Buffer::Settings settings(BUFFERUSAGEFLAG_INDEX, BUFFERDATAUSAGE_STATIC);
	quadIndexBuffer = newBuffer(settings, DATAFORMAT_UINT16, nullptr, size, 0);

	{
		Buffer::Mapper map(*quadIndexBuffer);
		fillIndices(TRIANGLEINDEX_QUADS, 0, LOVE_UINT16_MAX, (uint16 *) map.data);
	}

	quadIndexBuffer->setImmutable(true);
}

void graphics::createFanIndexBuffer()
{
	if (fanIndexBuffer != nullptr)
		return;

	size_t size = sizeof(uint16) * getIndexCount(TRIANGLEINDEX_FAN, LOVE_UINT16_MAX);

	Buffer::Settings settings(BUFFERUSAGEFLAG_INDEX, BUFFERDATAUSAGE_STATIC);
	fanIndexBuffer = newBuffer(settings, DATAFORMAT_UINT16, nullptr, size, 0);

	Buffer::Mapper map(*fanIndexBuffer);
	fillIndices(TRIANGLEINDEX_FAN, 0, LOVE_UINT16_MAX, (uint16 *) map.data);

	fanIndexBuffer->setImmutable(true);
}

Quad *graphics::newQuad(Quad::Viewport v, double sw, double sh)
{
	return new Quad(v, sw, sh);
}

FontMod *graphics::newFont(love::font::Rasterizer *data)
{
	return new FontMod(data, states.back().defaultSamplerState);
}

FontMod *graphics::newDefaultFont(int size, const font::TrueTypeRasterizer::Settings &settings)
{
	auto fontmodule = Module::getInstance<font::FontMod>(M_FONT);
	if (!fontmodule)
		throw love::Exception("Font module has not been loaded.");

	StrongRef<font::Rasterizer> r(fontmodule->newTrueTypeRasterizer(size, settings), Acquire::NORETAIN);
	return newFont(r.get());
}

Video *graphics::newVideo(love::video::VideoStream *stream, float dpiscale)
{
	return new Video(this, stream, dpiscale);
}

love::gfx::SpriteBatch *graphics::newSpriteBatch(Texture *texture, int size, BufferDataUsage usage)
{
	return new SpriteBatch(this, texture, size, usage);
}

love::gfx::ParticleSystem *graphics::newParticleSystem(Texture *texture, int size)
{
	return new ParticleSystem(texture, size);
}

ShaderStage *graphics::newShaderStage(ShaderStageType stage, const std::string &source, const Shader::CompileOptions &options, const Shader::SourceInfo &info, bool cache)
{
	ShaderStage *s = nullptr;
	std::string cachekey;

	// Never cache if there are custom defines set... because hashing would get
	// more complicated/expensive, and there shouldn't be a lot of duplicate
	// shader stages with custom defines anyway.
	if (!options.defines.empty())
		cache = false;

	if (cache && !source.empty())
	{
		datamod::HashFunction::Value hashvalue;
		datamod::hash(datamod::HashFunction::FUNCTION_SHA1, source.c_str(), source.size(), hashvalue);

		cachekey = std::string(hashvalue.data, hashvalue.size);

		auto it = cachedShaderStages[stage].find(cachekey);
		if (it != cachedShaderStages[stage].end())
		{
			s = it->second;
			s->retain();
		}
	}

	if (s == nullptr)
	{
		bool glsles = usesGLSLES();
		std::string glsl = Shader::createShaderStageCode(this, stage, source, options, info, glsles, true);
		s = newShaderStageInternal(stage, cachekey, glsl, glsles);
		if (cache && !cachekey.empty())
			cachedShaderStages[stage][cachekey] = s;
	}

	return s;
}

Shader *graphics::newShader(const std::vector<std::string> &stagessource, const Shader::CompileOptions &options)
{
	StrongRef<ShaderStage> stages[SHADERSTAGE_MAX_ENUM] = {};

	bool validstages[SHADERSTAGE_MAX_ENUM] = {};
	validstages[SHADERSTAGE_VERTEX] = true;
	validstages[SHADERSTAGE_PIXEL] = true;

	for (const std::string &source : stagessource)
	{
		Shader::SourceInfo info = Shader::getSourceInfo(source);
		bool isanystage = false;

		for (int i = 0; i < SHADERSTAGE_MAX_ENUM; i++)
		{
			if (!validstages[i])
				continue;

			if (info.stages[i] != Shader::ENTRYPOINT_NONE)
			{
				isanystage = true;
				stages[i].set(newShaderStage((ShaderStageType) i, source, options, info, true), Acquire::NORETAIN);
			}
		}

		if (!isanystage)
			throw love::Exception("Could not parse shader code (missing shader entry point function such as 'position' or 'effect')");
	}

	for (int i = 0; i < SHADERSTAGE_MAX_ENUM; i++)
	{
		auto stype = (ShaderStageType) i;
		if (validstages[i] && stages[i].get() == nullptr)
		{
			const std::string &source = Shader::getDefaultCode(Shader::STANDARD_DEFAULT, stype);
			Shader::SourceInfo info = Shader::getSourceInfo(source);
			Shader::CompileOptions opts;
			stages[i].set(newShaderStage(stype, source, opts, info, true), Acquire::NORETAIN);
		}

	}

	return newShaderInternal(stages, options);
}

Shader *graphics::newComputeShader(const std::string &source, const Shader::CompileOptions &options)
{
	Shader::SourceInfo info = Shader::getSourceInfo(source);

	if (info.stages[SHADERSTAGE_COMPUTE] == Shader::ENTRYPOINT_NONE)
		throw love::Exception("Could not parse compute shader code (missing 'computemain' function?)");

	StrongRef<ShaderStage> stages[SHADERSTAGE_MAX_ENUM];

	// Don't bother caching compute shader intermediate source, since there
	// shouldn't be much reuse.
	stages[SHADERSTAGE_COMPUTE].set(newShaderStage(SHADERSTAGE_COMPUTE, source, options, info, false));

	return newShaderInternal(stages, options);
}

Buffer *graphics::newBuffer(const Buffer::Settings &settings, DataFormat format, const void *data, size_t size, size_t arraylength)
{
	std::vector<Buffer::DataDeclaration> dataformat = {{"", format, 0}};
	return newBuffer(settings, dataformat, data, size, arraylength);
}

Mesh *graphics::newMesh(const std::vector<Buffer::DataDeclaration> &vertexformat, int vertexcount, PrimitiveType drawmode, BufferDataUsage usage)
{
	return new Mesh(this, vertexformat, vertexcount, drawmode, usage);
}

Mesh *graphics::newMesh(const std::vector<Buffer::DataDeclaration> &vertexformat, const void *data, size_t datasize, PrimitiveType drawmode, BufferDataUsage usage)
{
	return new Mesh(this, vertexformat, data, datasize, drawmode, usage);
}

Mesh *graphics::newMesh(const std::vector<Mesh::BufferAttribute> &attributes, PrimitiveType drawmode)
{
	return new Mesh(attributes, drawmode);
}

love::gfx::TextBatch *graphics::newTextBatch(gfx::FontMod *font, const std::vector<love::font::ColoredString> &text)
{
	return new TextBatch(font, text);
}

love::datamod::ByteData *graphics::readbackBuffer(Buffer *buffer, size_t offset, size_t size, datamod::ByteData *dest, size_t destoffset)
{
	StrongRef<GraphicsReadback> readback;
	readback.set(newReadbackInternal(READBACK_IMMEDIATE, buffer, offset, size, dest, destoffset), Acquire::NORETAIN);

	auto data = readback->getBufferData();
	if (data == nullptr)
		throw love::Exception("love.graphics.readbackBuffer failed.");

	data->retain();
	return data;
}

GraphicsReadback *graphics::readbackBufferAsync(Buffer *buffer, size_t offset, size_t size, datamod::ByteData *dest, size_t destoffset)
{
	auto readback = newReadbackInternal(READBACK_ASYNC, buffer, offset, size, dest, destoffset);
	pendingReadbacks.push_back(readback);
	return readback;
}

image::ImageData *graphics::readbackTexture(Texture *texture, int slice, int mipmap, const Rect &rect, image::ImageData *dest, int destx, int desty)
{
	StrongRef<GraphicsReadback> readback;
	readback.set(newReadbackInternal(READBACK_IMMEDIATE, texture, slice, mipmap, rect, dest, destx, desty), Acquire::NORETAIN);

	auto imagedata = readback->getImageData();
	if (imagedata == nullptr)
		throw love::Exception("love.graphics.readbackTexture failed.");

	imagedata->retain();
	return imagedata;
}

GraphicsReadback *graphics::readbackTextureAsync(Texture *texture, int slice, int mipmap, const Rect &rect, image::ImageData *dest, int destx, int desty)
{
	auto readback = newReadbackInternal(READBACK_ASYNC, texture, slice, mipmap, rect, dest, destx, desty);
	pendingReadbacks.push_back(readback);
	return readback;
}

void graphics::cleanupCachedShaderStage(ShaderStageType type, const std::string &hashkey)
{
	cachedShaderStages[type].erase(hashkey);
}

bool graphics::validateShader(bool gles, const std::vector<std::string> &stagessource, const Shader::CompileOptions &options, std::string &err)
{
	StrongRef<ShaderStage> stages[SHADERSTAGE_MAX_ENUM] = {};

	bool validstages[SHADERSTAGE_MAX_ENUM] = {};
	validstages[SHADERSTAGE_VERTEX] = true;
	validstages[SHADERSTAGE_PIXEL] = true;
	validstages[SHADERSTAGE_COMPUTE] = true;

	// Don't use cached shader stages, since the gles flag may not match the
	// current renderer.
	for (const std::string &source : stagessource)
	{
		Shader::SourceInfo info = Shader::getSourceInfo(source);
		bool isanystage = false;

		for (int i = 0; i < SHADERSTAGE_MAX_ENUM; i++)
		{
			auto stype = (ShaderStageType) i;

			if (!validstages[i])
				continue;

			if (info.stages[i] != Shader::ENTRYPOINT_NONE)
			{
				isanystage = true;
				std::string glsl = Shader::createShaderStageCode(this, stype, source, options, info, gles, false);
				stages[i].set(new ShaderStageForValidation(this, stype, glsl, gles), Acquire::NORETAIN);
			}
		}

		if (!isanystage)
		{
			err = "Could not parse shader code (missing 'position' or 'effect' function?)";
			return false;
		}
	}

	return Shader::validate(stages, err);
}

Texture *graphics::getDefaultTexture(TextureType type, DataBaseType dataType, bool depthSample)
{
	uint32 depthsampleindex = depthSample ? 1 : 0;
	Texture *tex = defaultTextures[type][dataType][depthsampleindex];
	if (tex != nullptr)
		return tex;

	Texture::Settings settings;
	settings.type = type;
	settings.readable.set(true);

	switch (dataType)
	{
	case DATA_BASETYPE_INT:
		settings.format = PIXELFORMAT_RGBA8_INT;
		break;
	case DATA_BASETYPE_UINT:
		settings.format = PIXELFORMAT_RGBA8_UINT;
		break;
	case DATA_BASETYPE_FLOAT:
	default:
		settings.format = PIXELFORMAT_RGBA8_UNORM;
		break;
	}

	if (depthSample)
	{
		settings.renderTarget = true;

		if (isPixelFormatSupported(PIXELFORMAT_DEPTH16_UNORM, PIXELFORMATUSAGE_SAMPLE))
			settings.format = PIXELFORMAT_DEPTH16_UNORM;
		else if (isPixelFormatSupported(PIXELFORMAT_DEPTH24_UNORM, PIXELFORMATUSAGE_SAMPLE))
			settings.format = PIXELFORMAT_DEPTH24_UNORM;
		else if (isPixelFormatSupported(PIXELFORMAT_DEPTH32_FLOAT, PIXELFORMATUSAGE_SAMPLE))
			settings.format = PIXELFORMAT_DEPTH32_FLOAT;
		else // TODO?
			settings.format = PIXELFORMAT_DEPTH24_UNORM;
	}

	std::string name = "default_";

	const char *tname = "unknown";
	Texture::getConstant(type, tname);
	name += tname;

	const char *formatname = "unknown";
	love::getConstant(settings.format, formatname);
	name += std::string("_") + formatname;

	settings.debugName = name;

	tex = newTexture(settings);

	SamplerState s;
	s.minFilter = s.magFilter = SamplerState::FILTER_NEAREST;
	s.wrapU = s.wrapV = s.wrapW = SamplerState::WRAP_CLAMP;

	if (depthSample)
		s.depthSampleMode.set(COMPARE_ALWAYS);

	tex->setSamplerState(s);

	if (!depthSample)
	{
		uint8 pixel[] = {255, 255, 255, 255};
		if (isPixelFormatInteger(settings.format))
			pixel[0] = pixel[1] = pixel[2] = pixel[3] = 1;

		for (int slice = 0; slice < (type == TEXTURE_CUBE ? 6 : 1); slice++)
			tex->replacePixels(pixel, sizeof(pixel), slice, 0, {0, 0, 1, 1}, false);
	}

	defaultTextures[type][dataType][depthsampleindex] = tex;

	return tex;
}

Buffer *graphics::getDefaultTexelBuffer(DataBaseType dataType)
{
	Buffer *buffer = defaultTexelBuffers[dataType];
	if (buffer != nullptr)
		return buffer;

	Buffer::Settings settings(BUFFERUSAGEFLAG_TEXEL, BUFFERDATAUSAGE_STATIC);
	settings.zeroInitialize = true;
	settings.debugName = "default_texelbuffer_";

	DataFormat format = DATAFORMAT_FLOAT;
	switch (dataType)
	{
	case DATA_BASETYPE_FLOAT:
	default:
		format = DATAFORMAT_FLOAT;
		settings.debugName += "float";
		break;
	case DATA_BASETYPE_INT:
		format = DATAFORMAT_INT32;
		settings.debugName += "int";
		break;
	case DATA_BASETYPE_UINT:
		format = DATAFORMAT_UINT32;
		settings.debugName += "uint";
		break;
	}

	buffer = newBuffer(settings, format, nullptr, sizeof(float), 1);

	defaultTexelBuffers[dataType] = buffer;

	return buffer;
}

Buffer *graphics::getDefaultStorageBuffer()
{
	if (defaultStorageBuffer != nullptr)
		return defaultStorageBuffer;

	Buffer::Settings settings(BUFFERUSAGEFLAG_SHADER_STORAGE, BUFFERDATAUSAGE_STATIC);
	settings.zeroInitialize = true;
	settings.debugName = "default_storagebuffer";

	defaultStorageBuffer = newBuffer(settings, DATAFORMAT_FLOAT, nullptr, Buffer::SHADER_STORAGE_BUFFER_MAX_STRIDE, 0);

	return defaultStorageBuffer;
}

void graphics::releaseDefaultResources()
{
	for (int type = 0; type < TEXTURE_MAX_ENUM; type++)
	{
		for (int dataType = 0; dataType < DATA_BASETYPE_MAX_ENUM; dataType++)
		{
			for (int depthsample = 0; depthsample < 2; depthsample++)
			{
				if (defaultTextures[type][dataType][depthsample])
					defaultTextures[type][dataType][depthsample]->release();
				defaultTextures[type][dataType][depthsample] = nullptr;
			}
		}
	}

	for (int dataType = 0; dataType < DATA_BASETYPE_MAX_ENUM; dataType++)
	{
		if (defaultTexelBuffers[dataType])
			defaultTexelBuffers[dataType]->release();
		defaultTexelBuffers[dataType] = nullptr;
	}

	if (defaultStorageBuffer)
		defaultStorageBuffer->release();
	defaultStorageBuffer = nullptr;
}

Texture *graphics::getTextureOrDefaultForActiveShader(Texture *tex)
{
	if (tex != nullptr)
		return tex;

	Shader *shader = Shader::current;

	if (shader != nullptr)
	{
		auto texinfo = shader->getMainTextureInfo();
		if (texinfo != nullptr && texinfo->textureType != TEXTURE_MAX_ENUM)
			return getDefaultTexture(texinfo->textureType, texinfo->dataBaseType, texinfo->isDepthSampler);
	}

	return getDefaultTexture(TEXTURE_2D, DATA_BASETYPE_FLOAT, false);
}

void graphics::validateStencilState(const StencilState &s) const
{
	if (s.action != STENCIL_KEEP)
	{
		const auto &rts = states.back().renderTargets;
		love::gfx::Texture *dstexture = rts.depthStencil.texture.get();

		if (!isRenderTargetActive() && !backbufferHasStencil)
			throw love::Exception("The window must have stenciling enabled to draw to the main screen's stencil buffer.");
		else if (isRenderTargetActive() && (rts.temporaryRTFlags & TEMPORARY_RT_STENCIL) == 0 && (dstexture == nullptr || !isPixelFormatStencil(dstexture->getPixelFormat())))
			throw love::Exception("Drawing to the stencil buffer with a Canvas active requires either stencil=true or a custom stencil-type Canvas to be used, in setCanvas.");
	}
}

void graphics::validateDepthState(bool depthwrite) const
{
	if (depthwrite)
	{
		const auto &rts = states.back().renderTargets;
		love::gfx::Texture *dstexture = rts.depthStencil.texture.get();

		if (!isRenderTargetActive() && !backbufferHasDepth)
			throw love::Exception("The window must have depth enabled to draw to the main screen's depth buffer.");
		else if (isRenderTargetActive() && (rts.temporaryRTFlags & TEMPORARY_RT_DEPTH) == 0 && (dstexture == nullptr || !isPixelFormatDepth(dstexture->getPixelFormat())))
			throw love::Exception("Drawing to the depth buffer with a Canvas active requires either depth=true or a custom depth-type Canvas to be used, in setCanvas.");
	}
}

int graphics::getWidth() const
{
	return width;
}

int graphics::getHeight() const
{
	return height;
}

int graphics::getPixelWidth() const
{
	return pixelWidth;
}

int graphics::getPixelHeight() const
{
	return pixelHeight;
}

double graphics::getCurrentDPIScale() const
{
	const auto &rt = states.back().renderTargets.getFirstTarget();
	if (rt.texture.get())
		return rt.texture->getDPIScale();

	return getScreenDPIScale();
}

double graphics::getScreenDPIScale() const
{
	return (double) getPixelHeight() / (double) getHeight();
}

bool graphics::isCreated() const
{
	return created;
}

bool graphics::isActive() const
{
	// The graphics module is only completely 'active' if there's a window, a
	// context, and the active variable is set.
	auto window = getInstance<love::window::Window>(M_WINDOW);
	return active && isCreated() && window != nullptr && window->isOpen();
}

void graphics::reset()
{
	DisplayState s;
	restoreState(s);
	origin();
}

void graphics::backbufferChanged(int width, int height, int pixelwidth, int pixelheight)
{
	backbufferChanged(width, height, pixelwidth, pixelheight, backbufferHasStencil, backbufferHasDepth, getRequestedBackbufferMSAA());
}

/**
 * State functions.
 **/

void graphics::restoreState(const DisplayState &s)
{
	setColor(s.color);
	setBackgroundColor(s.backgroundColor);

	setBlendState(s.blend);

	setLineWidth(s.lineWidth);
	setLineStyle(s.lineStyle);
	setLineJoin(s.lineJoin);

	setPointSize(s.pointSize);

	if (s.scissor)
		setScissor(s.scissorRect);
	else
		setScissor();

	setMeshCullMode(s.meshCullMode);
	setFrontFaceWinding(s.winding);

	setFont(s.font.get());
	setShader(s.shader.get());
	setRenderTargets(s.renderTargets);

	setStencilState(s.stencil);
	setDepthMode(s.depthTest, s.depthWrite);

	setColorMask(s.colorMask);
	setWireframe(s.wireframe);

	setDefaultSamplerState(s.defaultSamplerState);

	if (s.useCustomProjection)
		updateDeviceProjection(s.customProjection);
	else
		resetProjection();
}

void graphics::restoreStateChecked(const DisplayState &s)
{
	const DisplayState &cur = states.back();

	if (s.color != cur.color)
		setColor(s.color);

	setBackgroundColor(s.backgroundColor);

	if (!(s.blend == cur.blend))
		setBlendState(s.blend);

	// These are just simple assignments.
	setLineWidth(s.lineWidth);
	setLineStyle(s.lineStyle);
	setLineJoin(s.lineJoin);

	if (s.pointSize != cur.pointSize)
		setPointSize(s.pointSize);

	if (s.scissor != cur.scissor || (s.scissor && !(s.scissorRect == cur.scissorRect)))
	{
		if (s.scissor)
			setScissor(s.scissorRect);
		else
			setScissor();
	}

	setMeshCullMode(s.meshCullMode);

	if (s.winding != cur.winding)
		setFrontFaceWinding(s.winding);

	setFont(s.font.get());
	setShader(s.shader.get());

	const auto &sRTs = s.renderTargets;
	const auto &curRTs = cur.renderTargets;

	bool rtschanged = sRTs.colors.size() != curRTs.colors.size();
	if (!rtschanged)
	{
		for (size_t i = 0; i < sRTs.colors.size() && i < curRTs.colors.size(); i++)
		{
			if (sRTs.colors[i] != curRTs.colors[i])
			{
				rtschanged = true;
				break;
			}
		}

		if (!rtschanged && sRTs.depthStencil != curRTs.depthStencil)
			rtschanged = true;

		if (sRTs.temporaryRTFlags != curRTs.temporaryRTFlags)
			rtschanged = true;
	}

	if (rtschanged)
		setRenderTargets(s.renderTargets);

	if (!(s.stencil == cur.stencil))
		setStencilState(s.stencil);

	if (s.depthTest != cur.depthTest || s.depthWrite != cur.depthWrite)
		setDepthMode(s.depthTest, s.depthWrite);

	if (s.colorMask != cur.colorMask)
		setColorMask(s.colorMask);

	if (s.wireframe != cur.wireframe)
		setWireframe(s.wireframe);

	setDefaultSamplerState(s.defaultSamplerState);

	if (s.useCustomProjection)
		setProjection(s.customProjection);
	else if (cur.useCustomProjection)
		resetProjection();
}

Colorf graphics::getColor() const
{
	return states.back().color;
}

void graphics::setBackgroundColor(Colorf c)
{
	states.back().backgroundColor = c;
}

Colorf graphics::getBackgroundColor() const
{
	return states.back().backgroundColor;
}

void graphics::checkSetDefaultFont()
{
	// We don't create or set the default Font if an existing font is in use.
	if (states.back().font.get() != nullptr)
		return;

	// Create a new default font if we don't have one yet.
	if (!defaultFont.get())
	{
		font::TrueTypeRasterizer::Settings settings;
		defaultFont.set(newDefaultFont(13, settings), Acquire::NORETAIN);
	}

	states.back().font.set(defaultFont.get());
}

void graphics::setFont(love::gfx::FontMod *font)
{
	// We don't need to set a default font here if null is passed in, since we
	// only care about the default font in getFont and print.
	DisplayState &state = states.back();
	state.font.set(font);
}

love::gfx::FontMod *graphics::getFont()
{
	checkSetDefaultFont();
	return states.back().font.get();
}

void graphics::setShader(love::gfx::Shader *shader)
{
	if (shader == nullptr)
		return setShader();

	shader->attach();
	states.back().shader.set(shader);
}

void graphics::setShader()
{
	Shader::attachDefault(Shader::STANDARD_DEFAULT);
	states.back().shader.set(nullptr);
}

love::gfx::Shader *graphics::getShader() const
{
	return states.back().shader.get();
}

void graphics::setRenderTarget(RenderTarget rt, uint32 temporaryRTFlags)
{
	if (rt.texture == nullptr)
		return setRenderTarget();

	RenderTargets rts;
	rts.colors.push_back(rt);
	rts.temporaryRTFlags = temporaryRTFlags;

	setRenderTargets(rts);
}

void graphics::setRenderTargets(const RenderTargetsStrongRef &rts)
{
	RenderTargets targets;
	targets.colors.reserve(rts.colors.size());

	for (const auto &rt : rts.colors)
		targets.colors.emplace_back(rt.texture.get(), rt.slice, rt.mipmap);

	targets.depthStencil = RenderTarget(rts.depthStencil.texture, rts.depthStencil.slice, rts.depthStencil.mipmap);
	targets.temporaryRTFlags = rts.temporaryRTFlags;

	return setRenderTargets(targets);
}

void graphics::setRenderTargets(const RenderTargets &rts)
{
	DisplayState &state = states.back();
	int rtcount = (int) rts.colors.size();

	RenderTarget firsttarget = rts.getFirstTarget();
	Texture *firsttex = firsttarget.texture;

	if (firsttex == nullptr)
		return setRenderTarget();

	const auto &prevRTsRef = state.renderTargets;

	if (rtcount == (int) prevRTsRef.colors.size())
	{
		bool modified = false;

		for (int i = 0; i < rtcount; i++)
		{
			if (rts.colors[i] != prevRTsRef.colors[i])
			{
				modified = true;
				break;
			}
		}

		if (!modified && rts.depthStencil != prevRTsRef.depthStencil)
			modified = true;

		if (rts.temporaryRTFlags != prevRTsRef.temporaryRTFlags)
			modified = true;

		if (!modified)
			return;
	}

	const RenderTargetsStrongRef prevRTs = prevRTsRef;

	if (rtcount > capabilities.limits[LIMIT_RENDER_TARGETS])
		throw love::Exception("This system can't simultaneously render to %d textures.", rtcount);

	bool hasSRGBtexture = false;
	int pixelw = firsttex->getPixelWidth(firsttarget.mipmap);
	int pixelh = firsttex->getPixelHeight(firsttarget.mipmap);
	int reqmsaa = firsttex->getRequestedMSAA();

	for (int i = 0; i < rtcount; i++)
	{
		Texture *c = rts.colors[i].texture;
		PixelFormat format = c->getPixelFormat();
		int mip = rts.colors[i].mipmap;
		int slice = rts.colors[i].slice;

		if (!c->isRenderTarget())
			throw love::Exception("Texture must be created as a canvas to be used in setCanvas.");

		if (mip < 0 || mip >= c->getMipmapCount())
			throw love::Exception("Invalid mipmap level %d.", mip + 1);

		if (!c->isValidSlice(slice, mip))
			throw love::Exception("Invalid slice index: %d.", slice + 1);

		if (c->getPixelWidth(mip) != pixelw || c->getPixelHeight(mip) != pixelh)
			throw love::Exception("All textures must have the same pixel dimensions.");

		if (c->getRequestedMSAA() != reqmsaa)
			throw love::Exception("All textures must have the same MSAA value.");

		if (isPixelFormatDepthStencil(format))
			throw love::Exception("Depth/stencil format textures must be used with the 'depthstencil' field of the table passed into setCanvas.");

		if (isPixelFormatSRGB(format))
			hasSRGBtexture = true;
	}

	if (rts.depthStencil.texture != nullptr)
	{
		Texture *c = rts.depthStencil.texture;
		int mip = rts.depthStencil.mipmap;
		int slice = rts.depthStencil.slice;

		if (!c->isRenderTarget())
			throw love::Exception("Texture must be created as a canvas to be used in setCanvas.");

		if (!isPixelFormatDepthStencil(c->getPixelFormat()))
			throw love::Exception("Only depth/stencil format textures can be used with the 'depthstencil' field of the table passed into setCanvas.");

		if (c->getPixelWidth(mip) != pixelw || c->getPixelHeight(mip) != pixelh)
			throw love::Exception("All Textures must have the same pixel dimensions.");

		if (c->getRequestedMSAA() != firsttex->getRequestedMSAA())
			throw love::Exception("All Textures must have the same MSAA value.");

		if (mip < 0 || mip >= c->getMipmapCount())
			throw love::Exception("Invalid mipmap level %d.", mip + 1);

		if (!c->isValidSlice(slice, mip))
			throw love::Exception("Invalid slice index: %d.", slice + 1);
	}

	flushBatchedDraws();

	if (rts.depthStencil.texture == nullptr && rts.temporaryRTFlags != 0)
	{
		bool wantsdepth   = (rts.temporaryRTFlags & TEMPORARY_RT_DEPTH) != 0;
		bool wantsstencil = (rts.temporaryRTFlags & TEMPORARY_RT_STENCIL) != 0;

		PixelFormat dsformat = PIXELFORMAT_STENCIL8;
		if (wantsdepth && wantsstencil)
			dsformat = PIXELFORMAT_DEPTH24_UNORM_STENCIL8;
		else if (wantsdepth && isPixelFormatSupported(PIXELFORMAT_DEPTH24_UNORM, PIXELFORMATUSAGEFLAGS_RENDERTARGET))
			dsformat = PIXELFORMAT_DEPTH24_UNORM;
		else if (wantsdepth)
			dsformat = PIXELFORMAT_DEPTH16_UNORM;
		else if (wantsstencil)
			dsformat = PIXELFORMAT_STENCIL8;

		// We want setRenderTargetsInternal to have a pointer to the temporary RT,
		// but we don't want to directly store it in the main graphics state.
		RenderTargets realRTs = rts;

		realRTs.depthStencil.texture = getTemporaryTexture(dsformat, pixelw, pixelh, reqmsaa);
		realRTs.depthStencil.slice = 0;

		// TODO: fix this to call release at the right time.
		// This only works here because nothing else calls getTemporaryTexture.
		releaseTemporaryTexture(realRTs.depthStencil.texture);

		setRenderTargetsInternal(realRTs, pixelw, pixelh, hasSRGBtexture);
	}
	else
		setRenderTargetsInternal(rts, pixelw, pixelh, hasSRGBtexture);

	RenderTargetsStrongRef refs;
	refs.colors.reserve(rts.colors.size());

	for (auto c : rts.colors)
		refs.colors.emplace_back(c.texture, c.slice, c.mipmap);

	refs.depthStencil = RenderTargetStrongRef(rts.depthStencil.texture, rts.depthStencil.slice);
	refs.temporaryRTFlags = rts.temporaryRTFlags;

	std::swap(state.renderTargets, refs);

	renderTargetSwitchCount++;

	resetProjection();

	// generateMipmaps can't be used for depth/stencil textures.
	for (const auto &rt : prevRTs.colors)
	{
		if (rt.texture && rt.texture->getMipmapsMode() == Texture::MIPMAPS_AUTO && rt.mipmap == 0)
			rt.texture->generateMipmaps();
	}

	// Clear/reset the temporary depth/stencil buffers.
	// TODO: make this deferred somehow to avoid double clearing if the user
	// also calls love.graphics.clear after setCanvas.
	if (rts.depthStencil.texture == nullptr && rts.temporaryRTFlags != 0)
	{
		OptionalColorD clearcolor;
		OptionalInt clearstencil(0);
		OptionalDouble cleardepth(1.0);
		clear(clearcolor, clearstencil, cleardepth);
	}
}

void graphics::setRenderTarget()
{
	DisplayState &state = states.back();

	if (state.renderTargets.colors.empty() && state.renderTargets.depthStencil.texture == nullptr)
		return;

	const RenderTargetsStrongRef prevRTs = state.renderTargets;

	flushBatchedDraws();
	setRenderTargetsInternal(RenderTargets(), pixelWidth, pixelHeight, isGammaCorrect());

	state.renderTargets = RenderTargetsStrongRef();
	renderTargetSwitchCount++;

	resetProjection();

	// generateMipmaps can't be used for depth/stencil textures.
	for (const auto& rt : prevRTs.colors)
	{
		if (rt.texture && rt.texture->getMipmapsMode() == Texture::MIPMAPS_AUTO && rt.mipmap == 0)
			rt.texture->generateMipmaps();
	}
}

graphics::RenderTargets graphics::getRenderTargets() const
{
	const auto &curRTs = states.back().renderTargets;

	RenderTargets rts;
	rts.colors.reserve(curRTs.colors.size());

	for (const auto &rt : curRTs.colors)
		rts.colors.emplace_back(rt.texture.get(), rt.slice, rt.mipmap);

	rts.depthStencil = RenderTarget(curRTs.depthStencil.texture, curRTs.depthStencil.slice, curRTs.depthStencil.mipmap);
	rts.temporaryRTFlags = curRTs.temporaryRTFlags;

	return rts;
}

bool graphics::isRenderTargetActive() const
{
	const auto &rts = states.back().renderTargets;
	return !rts.colors.empty() || rts.depthStencil.texture != nullptr;
}

bool graphics::isRenderTargetActive(Texture *texture) const
{
	Texture *roottexture = texture->getRootViewInfo().texture;
	const auto &rts = states.back().renderTargets;

	for (const auto &rt : rts.colors)
	{
		if (rt.texture.get() && rt.texture->getRootViewInfo().texture == roottexture)
			return true;
	}

	if (rts.depthStencil.texture.get() && rts.depthStencil.texture->getRootViewInfo().texture == roottexture)
		return true;

	return false;
}

bool graphics::isRenderTargetActive(Texture *texture, int slice) const
{
	const auto &rootinfo = texture->getRootViewInfo();
	slice += rootinfo.startLayer;

	const auto &rts = states.back().renderTargets;

	for (const auto &rt : rts.colors)
	{
		if (rt.texture.get())
		{
			const auto &info = rt.texture->getRootViewInfo();
			if (rootinfo.texture == info.texture && rt.slice + info.startLayer == slice)
				return true;
		}
	}

	if (rts.depthStencil.texture.get())
	{
		const auto &info = rts.depthStencil.texture->getRootViewInfo();
		if (rootinfo.texture == info.texture && rts.depthStencil.slice + info.startLayer == slice)
			return true;
	}

	return false;
}

Texture *graphics::getTemporaryTexture(PixelFormat format, int w, int h, int samples)
{
	Texture *texture = nullptr;

	for (TemporaryTexture &temp : temporaryTextures)
	{
		if (temp.framesSinceUse < 0)
			continue;

		Texture *c = temp.texture;
		if (c->getPixelFormat() == format && c->getPixelWidth() == w
			&& c->getPixelHeight() == h && c->getRequestedMSAA() == samples)
		{
			texture = c;
			temp.framesSinceUse = -1;
			break;
		}
	}

	if (texture == nullptr)
	{
		Texture::Settings settings;
		settings.renderTarget = true;
		settings.format = format;
		settings.width = w;
		settings.height = h;
		settings.msaa = samples;

		texture = newTexture(settings);

		temporaryTextures.emplace_back(texture);
	}

	return texture;
}

void graphics::releaseTemporaryTexture(Texture *texture)
{
	for (TemporaryTexture &temp : temporaryTextures)
	{
		if (temp.texture == texture)
		{
			temp.framesSinceUse = 0;
			break;
		}
	}
}

Buffer *graphics::getTemporaryBuffer(size_t size, DataFormat format, uint32 usageflags, BufferDataUsage datausage)
{
	Buffer *buffer = nullptr;

	for (TemporaryBuffer &temp : temporaryBuffers)
	{
		if (temp.framesSinceUse < 0)
			continue;

		Buffer *b = temp.buffer;

		if (temp.size == size && b->getDataMember(0).decl.format == format
			&& b->getUsageFlags() == usageflags && b->getDataUsage() == datausage)
		{
			buffer = b;
			temp.framesSinceUse = -1;
			break;
		}
	}

	if (buffer == nullptr)
	{
		Buffer::Settings settings(usageflags, datausage);
		buffer = newBuffer(settings, format, nullptr, size, 0);

		temporaryBuffers.emplace_back(buffer, size);
	}

	return buffer;
}

void graphics::releaseTemporaryBuffer(Buffer *buffer)
{
	for (TemporaryBuffer &temp : temporaryBuffers)
	{
		if (temp.buffer == buffer)
		{
			temp.framesSinceUse = 0;
			break;
		}
	}
}

void graphics::updateTemporaryResources()
{
	for (int i = (int) temporaryTextures.size() - 1; i >= 0; i--)
	{
		auto &t = temporaryTextures[i];
		if (t.framesSinceUse >= MAX_TEMPORARY_RESOURCE_UNUSED_FRAMES)
		{
			t.texture->release();
			t = temporaryTextures.back();
			temporaryTextures.pop_back();
		}
		else if (t.framesSinceUse >= 0)
			t.framesSinceUse++;
	}

	for (int i = (int) temporaryBuffers.size() - 1; i >= 0; i--)
	{
		auto &t = temporaryBuffers[i];
		if (t.framesSinceUse >= MAX_TEMPORARY_RESOURCE_UNUSED_FRAMES)
		{
			t.buffer->release();
			t = temporaryBuffers.back();
			temporaryBuffers.pop_back();
		}
		else if (t.framesSinceUse >= 0)
			t.framesSinceUse++;
	}
}

void graphics::clearTemporaryResources()
{
	for (auto temp :temporaryBuffers)
		temp.buffer->release();

	for (auto temp : temporaryTextures)
		temp.texture->release();

	temporaryBuffers.clear();
	temporaryTextures.clear();
}

void graphics::updatePendingReadbacks()
{
	for (int i = (int)pendingReadbacks.size() - 1; i >= 0; i--)
	{
		pendingReadbacks[i]->update();
		if (pendingReadbacks[i]->isComplete())
		{
			pendingReadbacks[i] = pendingReadbacks.back();
			pendingReadbacks.pop_back();
		}
	}
}

void graphics::intersectScissor(const Rect &rect)
{
	Rect currect = states.back().scissorRect;

	if (!states.back().scissor)
	{
		currect.x = 0;
		currect.y = 0;
		currect.w = std::numeric_limits<int>::max();
		currect.h = std::numeric_limits<int>::max();
	}

	int x1 = std::max(currect.x, rect.x);
	int y1 = std::max(currect.y, rect.y);

	int x2 = std::min(currect.x + currect.w, rect.x + rect.w);
	int y2 = std::min(currect.y + currect.h, rect.y + rect.h);

	Rect newrect = {x1, y1, std::max(0, x2 - x1), std::max(0, y2 - y1)};
	setScissor(newrect);
}

bool graphics::getScissor(Rect &rect) const
{
	const DisplayState &state = states.back();
	rect = state.scissorRect;
	return state.scissor;
}

void graphics::setStencilMode(StencilMode mode, int value)
{
	setStencilState(computeStencilState(mode, value));
	if (mode == STENCIL_MODE_DRAW)
		setColorMask({ false, false, false, false });
	else
		setColorMask({ true, true, true, true });
}

void graphics::setStencilMode()
{
	setStencilState(computeStencilState(STENCIL_MODE_OFF, 0));
	setColorMask({ true, true, true, true });
}

StencilMode graphics::getStencilMode(int &value) const
{
	const DisplayState& state = states.back();
	StencilMode mode = computeStencilMode(state.stencil);
	value = state.stencil.value;
	return mode;
}

void graphics::setStencilState()
{
	StencilState s;
	setStencilState(s);
}

const StencilState &graphics::getStencilState() const
{
	const DisplayState &state = states.back();
	return state.stencil;
}

void graphics::setDepthMode()
{
	setDepthMode(COMPARE_ALWAYS, false);
}

void graphics::getDepthMode(CompareMode &compare, bool &write) const
{
	const DisplayState &state = states.back();
	compare = state.depthTest;
	write = state.depthWrite;
}

void graphics::setMeshCullMode(CullMode cull)
{
	// Handled inside the draw() graphics API implementations.
	states.back().meshCullMode = cull;
}

CullMode graphics::getMeshCullMode() const
{
	return states.back().meshCullMode;
}

Winding graphics::getFrontFaceWinding() const
{
	return states.back().winding;
}

ColorChannelMask graphics::getColorMask() const
{
	return states.back().colorMask;
}

void graphics::setBlendMode(BlendMode mode, BlendAlpha alphamode)
{
	if (alphamode == BLENDALPHA_MULTIPLY && !isAlphaMultiplyBlendSupported(mode))
	{
		const char *modestr = "unknown";
		love::gfx::getConstant(mode, modestr);
		throw love::Exception("The '%s' blend mode must be used with premultiplied alpha.", modestr);
	}

	setBlendState(computeBlendState(mode, alphamode));
}

BlendMode graphics::getBlendMode(BlendAlpha &alphamode) const
{
	return computeBlendMode(states.back().blend, alphamode);
}

const BlendState &graphics::getBlendState() const
{
	return states.back().blend;
}

void graphics::setDefaultSamplerState(const SamplerState &s)
{
	states.back().defaultSamplerState = s;
}

const SamplerState &graphics::getDefaultSamplerState() const
{
	return states.back().defaultSamplerState;
}

void graphics::setLineWidth(float width)
{
	states.back().lineWidth = width;
}

void graphics::setLineStyle(graphics::LineStyle style)
{
	states.back().lineStyle = style;
}

void graphics::setLineJoin(graphics::LineJoin join)
{
	states.back().lineJoin = join;
}

float graphics::getLineWidth() const
{
	return states.back().lineWidth;
}

graphics::LineStyle graphics::getLineStyle() const
{
	return states.back().lineStyle;
}

graphics::LineJoin graphics::getLineJoin() const
{
	return states.back().lineJoin;
}

float graphics::getPointSize() const
{
	return states.back().pointSize;
}

bool graphics::isWireframe() const
{
	return states.back().wireframe;
}

void graphics::captureScreenshot(const ScreenshotInfo &info)
{
	pendingScreenshotCallbacks.push_back(info);
}

void graphics::copyBuffer(Buffer *source, Buffer *dest, size_t sourceoffset, size_t destoffset, size_t size)
{
	Range sourcerange(sourceoffset, size);
	Range destrange(destoffset, size);

	if (dest->getDataUsage() == BUFFERDATAUSAGE_STREAM)
		throw love::Exception("Buffers created with 'stream' data usage cannot be used as a copy destination.");

	if (source->getDataUsage() == BUFFERDATAUSAGE_READBACK)
		throw love::Exception("Buffers created with 'readback' data usage cannot be used as a copy source.");

	if (sourcerange.getMax() >= source->getSize())
		throw love::Exception("Buffer copy source offset and size doesn't fit within the source Buffer's size.");

	if (destrange.getMax() >= dest->getSize())
		throw love::Exception("Buffer copy destination offset and size doesn't fit within the destination buffer's size.");

	if (source == dest && sourcerange.intersects(destrange))
		throw love::Exception("Copying a portion of a buffer to the same buffer requires non-overlapping source and destination offsets.");

	if (dest->isImmutable())
		throw love::Exception("Cannot copy to an immutable buffer.");

	if (sourceoffset % 4 != 0 || destoffset % 4 != 0 || size % 4 != 0)
		throw love::Exception("Buffer copy source offset, destination offset, and size parameters must be multiples of 4 bytes.");

	source->copyTo(dest, sourceoffset, destoffset, size);
}

void graphics::copyTextureToBuffer(Texture *source, Buffer *dest, int slice, int mipmap, const Rect &rect, size_t destoffset, int destwidth)
{
	if (!capabilities.features[FEATURE_COPY_TEXTURE_TO_BUFFER])
	{
		if (!source->isRenderTarget())
			throw love::Exception("Copying a non-render target Texture to a Buffer is not supported on this system.");
	}

	PixelFormat format = source->getPixelFormat();

	if (isPixelFormatDepthStencil(format))
		throw love::Exception("Copying a depth/stencil Texture to a Buffer is not supported.");

	if (!source->isReadable())
		throw love::Exception("copyTextureToBuffer can only be called on readable Textures.");

	if (dest->getDataUsage() == BUFFERDATAUSAGE_STREAM)
		throw love::Exception("Buffers created with 'stream' data usage cannot be used as a copy destination.");

	if (dest->isImmutable())
		throw love::Exception("Cannot copy to an immutable buffer.");

	if (isRenderTargetActive(source))
		throw love::Exception("copyTextureToBuffer cannot be called while the Texture is an active render target.");

	if (mipmap < 0 || mipmap >= source->getMipmapCount())
		throw love::Exception("Invalid texture mipmap index %d.", mipmap + 1);

	TextureType textype = source->getTextureType();
	if (slice < 0 || (textype == TEXTURE_CUBE && slice >= 6)
		|| (textype == TEXTURE_VOLUME && slice >= source->getDepth(mipmap))
		|| (textype == TEXTURE_2D_ARRAY && slice >= source->getLayerCount()))
	{
		throw love::Exception("Invalid texture slice index %d.", slice + 1);
	}

	int mipw = source->getPixelWidth(mipmap);
	int miph = source->getPixelHeight(mipmap);

	if (rect.x < 0 || rect.y < 0 || rect.w <= 0 || rect.h <= 0
		|| (rect.x + rect.w) > mipw || (rect.y + rect.h) > miph)
	{
		throw love::Exception("Invalid rectangle dimensions (x=%d, y=%d, w=%d, h=%d) for %dx%d texture.", rect.x, rect.y, rect.w, rect.h, mipw, miph);
	}

	if (destwidth <= 0)
		destwidth = rect.w;

	size_t size = 0;

	if (isPixelFormatCompressed(format))
	{
		if (destwidth != rect.w) // OpenGL limitation...
			throw love::Exception("Copying a compressed texture to a buffer cannot use a custom destination width.");

		const PixelFormatInfo &info = getPixelFormatInfo(format);
		int bw = (int) info.blockWidth;
		int bh = (int) info.blockHeight;
		if (rect.x % bw != 0 || rect.y % bh != 0 ||
			((rect.w % bw != 0 || rect.h % bh != 0) && rect.x + rect.w != source->getPixelWidth(mipmap)))
		{
			const char *name = nullptr;
			love::getConstant(format, name);
			throw love::Exception("Compressed texture format %s only supports copying a sub-rectangle with offset and dimensions that are a multiple of %d x %d.", name, bw, bh);
		}

		// Note: this will need to change if destwidth == rect.w restriction
		// is removed.
		size = getPixelFormatSliceSize(format, destwidth, rect.h);
	}
	else
	{
		// Not the cleanest, but should work since uncompressed formats always
		// have 1x1 blocks.
		int pixels = (rect.h - 1) * destwidth + rect.w;
		size = getPixelFormatUncompressedRowSize(format, pixels);
	}

	Range destrange(destoffset, size);

	if (destoffset % 4 != 0 || size % 4 != 0)
		throw love::Exception("Buffer copy destination offset and computed byte size must be multiples of 4 bytes.");

	if (destrange.getMax() >= dest->getSize())
		throw love::Exception("Buffer copy destination offset and width/height doesn't fit within the destination Buffer.");

	source->copyToBuffer(dest, slice, mipmap, rect, destoffset, destwidth, size);
}

void graphics::copyBufferToTexture(Buffer *source, Texture *dest, size_t sourceoffset, int sourcewidth, int slice, int mipmap, const Rect &rect)
{
	if (source->getDataUsage() == BUFFERDATAUSAGE_READBACK)
		throw love::Exception("Buffers created with 'readback' data usage cannot be used as a copy source.");

	PixelFormat format = dest->getPixelFormat();

	if (isPixelFormatDepthStencil(format))
		throw love::Exception("Copying a Buffer to a depth/stencil Texture is not supported.");

	if (!dest->isReadable())
		throw love::Exception("copyBufferToTexture can only be called on readable Textures.");

	if (isRenderTargetActive(dest))
		throw love::Exception("copyBufferToTexture cannot be called while the Texture is an active render target.");

	if (mipmap < 0 || mipmap >= dest->getMipmapCount())
		throw love::Exception("Invalid texture mipmap index %d.", mipmap + 1);

	TextureType textype = dest->getTextureType();
	if (slice < 0 || (textype == TEXTURE_CUBE && slice >= 6)
		|| (textype == TEXTURE_VOLUME && slice >= dest->getDepth(mipmap))
		|| (textype == TEXTURE_2D_ARRAY && slice >= dest->getLayerCount()))
	{
		throw love::Exception("Invalid texture slice index %d.", slice + 1);
	}

	int mipw = dest->getPixelWidth(mipmap);
	int miph = dest->getPixelHeight(mipmap);

	if (rect.x < 0 || rect.y < 0 || rect.w <= 0 || rect.h <= 0
		|| (rect.x + rect.w) > mipw || (rect.y + rect.h) > miph)
	{
		throw love::Exception("Invalid rectangle dimensions (x=%d, y=%d, w=%d, h=%d) for %dx%d texture.", rect.x, rect.y, rect.w, rect.h, mipw, miph);
	}

	if (sourcewidth <= 0)
		sourcewidth = rect.w;

	size_t size = 0;

	if (isPixelFormatCompressed(format))
	{
		if (sourcewidth != rect.w) // OpenGL limitation...
			throw love::Exception("Copying a buffer to a compressed texture cannot use a custom source width.");

		const PixelFormatInfo &info = getPixelFormatInfo(format);
		int bw = (int) info.blockWidth;
		int bh = (int) info.blockHeight;
		if (rect.x % bw != 0 || rect.y % bh != 0 ||
			((rect.w % bw != 0 || rect.h % bh != 0) && rect.x + rect.w != dest->getPixelWidth(mipmap)))
		{
			const char *name = nullptr;
			love::getConstant(format, name);
			throw love::Exception("Compressed texture format %s only supports copying a sub-rectangle with offset and dimensions that are a multiple of %d x %d.", name, bw, bh);
		}

		// Note: this will need to change if sourcewidth == rect.w restriction
		// is removed.
		size = getPixelFormatSliceSize(format, sourcewidth, rect.h);
	}
	else
	{
		// Not the cleanest, but should work since uncompressed formats always
		// have 1x1 blocks.
		int pixels = (rect.h - 1) * sourcewidth + rect.w;
		size = getPixelFormatUncompressedRowSize(format, pixels);
	}

	Range sourcerange(sourceoffset, size);

	if (sourceoffset % 4 != 0 || size % 4 != 0)
		throw love::Exception("Buffer copy source offset and computed byte size must be multiples of 4 bytes.");

	if (sourcerange.getMax() >= source->getSize())
		throw love::Exception("Buffer copy source offset and width/height doesn't fit within the source Buffer.");

	dest->copyFromBuffer(source, sourceoffset, sourcewidth, size, slice, mipmap, rect);
}

static const char *getIndirectArgsTypeName(graphics::IndirectArgsType argstype)
{
	switch (argstype)
	{
		case graphics::INDIRECT_ARGS_DISPATCH: return "Compute shader threadgroup argument data";
		case graphics::INDIRECT_ARGS_DRAW_VERTICES: return "Draw vertices argument data";
		case graphics::INDIRECT_ARGS_DRAW_INDICES: return "Draw indices argument data";
	}

	return "(Unknown argument data)";
}

void graphics::validateIndirectArgsBuffer(IndirectArgsType argstype, Buffer *indirectargs, int argsindex)
{
	if (!capabilities.features[FEATURE_INDIRECT_DRAW])
		throw love::Exception("Indirect draws and compute dispatches are not supported on this system.");

	if ((indirectargs->getUsageFlags() & BUFFERUSAGEFLAG_INDIRECT_ARGUMENTS) == 0)
		throw love::Exception("The given Buffer must be created with the indirectarguments usage flag set, to be used for indirect arguments.");

	if (argsindex < 0)
		throw love::Exception("The given indirect argument index cannot be negative.");

	size_t argelements = 0;
	if (argstype == INDIRECT_ARGS_DISPATCH)
		argelements = 3;
	else if (argstype == INDIRECT_ARGS_DRAW_VERTICES)
		argelements = 4;
	else if (argstype == INDIRECT_ARGS_DRAW_INDICES)
		argelements = 5;

	size_t totalmembers = indirectargs->getArrayLength() * indirectargs->getDataMembers().size();

	if (totalmembers % argelements != 0)
		throw love::Exception("%s requires the given indirect argument Buffer to have a multiple of %ld int or uint values.", getIndirectArgsTypeName(argstype), argelements);

	size_t argsoffset = argsindex * indirectargs->getArrayStride();

	if (indirectargs->getSize() < argsoffset + sizeof(uint32) * argelements)
		throw love::Exception("The given index into the indirect argument Buffer does not fit within the Buffer's size.");
}

void graphics::dispatchThreadgroups(Shader *shader, int x, int y, int z)
{
	if (!shader->hasStage(SHADERSTAGE_COMPUTE))
		throw love::Exception("Only compute shaders can have threads dispatched.");

	if (x <= 0 || y <= 0 || z <= 0)
		throw love::Exception("Threadgroup dispatch size must be positive.");

	if (x > capabilities.limits[LIMIT_THREADGROUPS_X]
		|| y > capabilities.limits[LIMIT_THREADGROUPS_Y]
		|| z > capabilities.limits[LIMIT_THREADGROUPS_Z])
	{
		throw love::Exception("Too many threadgroups dispatched.");
	}

	flushBatchedDraws();

	auto prevshader = Shader::current;
	shader->attach();

	bool success = dispatch(shader, x, y, z);

	if (prevshader != nullptr)
		prevshader->attach();

	if (!success)
		throw love::Exception("Compute shader must have resources bound to all writable texture and buffer variables.");
}

void graphics::dispatchIndirect(Shader *shader, Buffer *indirectargs, int argsindex)
{
	if (!shader->hasStage(SHADERSTAGE_COMPUTE))
		throw love::Exception("Only compute shaders can have threads dispatched.");

	validateIndirectArgsBuffer(INDIRECT_ARGS_DISPATCH, indirectargs, argsindex);

	flushBatchedDraws();

	auto prevshader = Shader::current;
	shader->attach();

	bool success = dispatch(shader, indirectargs, argsindex * indirectargs->getArrayStride());

	if (prevshader != nullptr)
		prevshader->attach();

	if (!success)
		throw love::Exception("Compute shader must have resources bound to all writable texture and buffer variables.");
}

graphics::BatchedVertexData graphics::requestBatchedDraw(const BatchedDrawCommand &cmd)
{
	BatchedDrawState &state = batchedDrawState;

	bool shouldflush = false;
	bool shouldresize = false;

	if (cmd.primitiveMode != state.primitiveMode
		|| cmd.formats[0] != state.formats[0] || cmd.formats[1] != state.formats[1]
		|| ((cmd.indexMode != TRIANGLEINDEX_NONE) != (state.indexCount > 0))
		|| cmd.texture != state.texture
		|| cmd.standardShaderType != state.standardShaderType)
	{
		shouldflush = true;
	}

	int totalvertices = state.vertexCount + cmd.vertexCount;

	// We only support uint16 index buffers for now.
	if (totalvertices > LOVE_UINT16_MAX && cmd.indexMode != TRIANGLEINDEX_NONE)
		shouldflush = true;

	int reqIndexCount = getIndexCount(cmd.indexMode, cmd.vertexCount);
	size_t reqIndexSize = reqIndexCount * sizeof(uint16);

	size_t newdatasizes[2] = {0, 0};
	size_t buffersizes[3] = {0, 0, 0};

	for (int i = 0; i < 2; i++)
	{
		if (cmd.formats[i] == CommonFormat::NONE)
			continue;

		size_t stride = getFormatStride(cmd.formats[i]);
		size_t datasize = stride * totalvertices;

		if (state.vbMap[i].data != nullptr && datasize > state.vbMap[i].size)
			shouldflush = true;

		if (datasize > state.vb[i]->getUsableSize())
		{
			buffersizes[i] = std::max(datasize, state.vb[i]->getSize() * 2);
			shouldresize = true;
		}

		newdatasizes[i] = stride * cmd.vertexCount;
	}

	if (cmd.indexMode != TRIANGLEINDEX_NONE)
	{
		size_t datasize = (state.indexCount + reqIndexCount) * sizeof(uint16);

		if (state.indexBufferMap.data != nullptr && datasize > state.indexBufferMap.size)
			shouldflush = true;

		if (datasize > state.indexBuffer->getUsableSize())
		{
			buffersizes[2] = std::max(datasize, state.indexBuffer->getSize() * 2);
			shouldresize = true;
		}
	}

	if (shouldflush || shouldresize)
	{
		flushBatchedDraws();

		state.primitiveMode = cmd.primitiveMode;
		state.formats[0] = cmd.formats[0];
		state.formats[1] = cmd.formats[1];
		state.texture = cmd.texture;
		state.standardShaderType = cmd.standardShaderType;
	}

	if (state.vertexCount == 0)
	{
		if (Shader::isDefaultActive())
			Shader::attachDefault(state.standardShaderType);

		if (Shader::current != nullptr)
			Shader::current->validateDrawState(cmd.primitiveMode, cmd.texture);
	}

	if (shouldresize)
	{
		for (int i = 0; i < 2; i++)
		{
			if (state.vb[i]->getSize() < buffersizes[i])
			{
				state.vb[i]->release();
				state.vb[i] = newStreamBuffer(BUFFERUSAGE_VERTEX, buffersizes[i]);
			}
		}

		if (state.indexBuffer->getSize() < buffersizes[2])
		{
			state.indexBuffer->release();
			state.indexBuffer = newStreamBuffer(BUFFERUSAGE_INDEX, buffersizes[2]);
		}
	}

	if (cmd.indexMode != TRIANGLEINDEX_NONE)
	{
		if (state.indexBufferMap.data == nullptr)
			state.indexBufferMap = state.indexBuffer->map(reqIndexSize);

		uint16 *indices = (uint16 *) state.indexBufferMap.data;
		fillIndices(cmd.indexMode, state.vertexCount, cmd.vertexCount, indices);

		state.indexBufferMap.data += reqIndexSize;
	}

	BatchedVertexData d;

	for (int i = 0; i < 2; i++)
	{
		if (newdatasizes[i] > 0)
		{
			if (state.vbMap[i].data == nullptr)
				state.vbMap[i] = state.vb[i]->map(newdatasizes[i]);

			d.stream[i] = state.vbMap[i].data;

			state.vbMap[i].data += newdatasizes[i];
		}
	}

	if (state.vertexCount > 0)
		drawCallsBatched++;

	state.vertexCount += cmd.vertexCount;
	state.indexCount  += reqIndexCount;

	return d;
}

void graphics::flushBatchedDraws()
{
	auto &sbstate = batchedDrawState;

	if ((sbstate.vertexCount == 0 && sbstate.indexCount == 0) || sbstate.flushing)
		return;

	VertexAttributes attributes;
	BufferBindings buffers;

	size_t usedsizes[3] = {0, 0, 0};

	for (int i = 0; i < 2; i++)
	{
		if (sbstate.formats[i] == CommonFormat::NONE)
			continue;

		attributes.setCommonFormat(sbstate.formats[i], (uint8) i);

		usedsizes[i] = getFormatStride(sbstate.formats[i]) * sbstate.vertexCount;

		size_t offset = sbstate.vb[i]->unmap(usedsizes[i]);
		buffers.set(i, sbstate.vb[i], offset);
		sbstate.vbMap[i] = StreamBuffer::MapInfo();
	}

	if (attributes.enableBits == 0)
		return;

	sbstate.flushing = true;

	Colorf nc = getColor();
	if (attributes.isEnabled(ATTRIB_COLOR))
		setColor(Colorf(1.0f, 1.0f, 1.0f, 1.0f));

	pushIdentityTransform();

	if (sbstate.indexCount > 0)
	{
		usedsizes[2] = sizeof(uint16) * sbstate.indexCount;

		DrawIndexedCommand cmd(&attributes, &buffers, sbstate.indexBuffer);
		cmd.primitiveType = sbstate.primitiveMode;
		cmd.indexCount = sbstate.indexCount;
		cmd.indexType = INDEX_UINT16;
		cmd.indexBufferOffset = sbstate.indexBuffer->unmap(usedsizes[2]);
		cmd.texture = getTextureOrDefaultForActiveShader(sbstate.texture);
		draw(cmd);

		sbstate.indexBufferMap = StreamBuffer::MapInfo();
	}
	else
	{
		DrawCommand cmd(&attributes, &buffers);
		cmd.primitiveType = sbstate.primitiveMode;
		cmd.vertexStart = 0;
		cmd.vertexCount = sbstate.vertexCount;
		cmd.texture = getTextureOrDefaultForActiveShader(sbstate.texture);
		draw(cmd);
	}

	for (int i = 0; i < 2; i++)
	{
		if (usedsizes[i] > 0)
			sbstate.vb[i]->markUsed(usedsizes[i]);
	}

	if (usedsizes[2] > 0)
		sbstate.indexBuffer->markUsed(usedsizes[2]);

	popTransform();

	if (attributes.isEnabled(ATTRIB_COLOR))
		setColor(nc);

	sbstate.vertexCount = 0;
	sbstate.indexCount = 0;
	sbstate.flushing = false;
}

void graphics::flushBatchedDrawsGlobal()
{
	graphics *instance = getInstance<graphics>(M_GRAPHICS);
	if (instance != nullptr)
		instance->flushBatchedDraws();
}

/**
 * Drawing
 **/

void graphics::draw(Drawable *drawable, const Matrix4 &m)
{
	drawable->draw(this, m);
}

void graphics::draw(Texture *texture, Quad *quad, const Matrix4 &m)
{
	texture->draw(this, quad, m);
}

void graphics::drawLayer(Texture *texture, int layer, const Matrix4 &m)
{
	texture->drawLayer(this, layer, m);
}

void graphics::drawLayer(Texture *texture, int layer, Quad *quad, const Matrix4 &m)
{
	texture->drawLayer(this, layer, quad, m);
}

void graphics::drawInstanced(Mesh *mesh, const Matrix4 &m, int instancecount)
{
	mesh->drawInstanced(this, m, instancecount);
}

void graphics::drawIndirect(Mesh *mesh, const Matrix4 &m, Buffer *indirectargs, int argsindex)
{
	mesh->drawIndirect(this, m, indirectargs, argsindex);
}

void graphics::drawFromShader(PrimitiveType primtype, int vertexcount, int instancecount, Texture *maintexture)
{
	if (primtype == PRIMITIVE_TRIANGLE_FAN && vertexcount > LOVE_UINT16_MAX)
		throw love::Exception("drawFromShader cannot draw more than %d vertices when the 'fan' draw mode is used.", LOVE_UINT16_MAX);

	// Emulated triangle fan via an index buffer.
	if (primtype == PRIMITIVE_TRIANGLE_FAN && getFanIndexBuffer())
	{
		int indexcount = getIndexCount(TRIANGLEINDEX_FAN, vertexcount);
		drawFromShader(getFanIndexBuffer(), indexcount, instancecount, 0, maintexture);
		return;
	}

	flushBatchedDraws();

	if (Shader::isDefaultActive() || !Shader::current)
		throw love::Exception("drawFromShader can only be used with a custom shader.");

	if (vertexcount < 0 || instancecount < 0)
		throw love::Exception("drawFromShader vertex and instance count parameters must not be negative.");

	Shader::current->validateDrawState(primtype, maintexture);

	VertexAttributes attributes;
	BufferBindings buffers;

	DrawCommand cmd(&attributes, &buffers);

	cmd.primitiveType = primtype;
	cmd.vertexCount = vertexcount;
	cmd.instanceCount = std::max(1, instancecount);
	cmd.texture = getTextureOrDefaultForActiveShader(maintexture);

	draw(cmd);
}

void graphics::drawFromShader(Buffer *indexbuffer, int indexcount, int instancecount, int startindex, Texture *maintexture)
{
	flushBatchedDraws();

	if (!(indexbuffer->getUsageFlags() & BUFFERUSAGEFLAG_INDEX))
		throw love::Exception("The buffer passed to drawFromShader must be an index buffer.");

	if (startindex < 0)
		throw love::Exception("drawFromShader startindex parameter must not be negative.");

	if (indexcount < 0 || instancecount < 0)
		throw love::Exception("drawFromShader index and instance count parameters must not be negative.");

	if ((size_t)(startindex + indexcount) > indexbuffer->getArrayLength() * indexbuffer->getDataMembers().size())
		throw love::Exception("drawFromShader startindex and index count parameters do not fit in the given index buffer.");

	if (Shader::isDefaultActive() || !Shader::current)
		throw love::Exception("drawFromShader can only be used with a custom shader.");

	Shader::current->validateDrawState(PRIMITIVE_TRIANGLES, maintexture);

	VertexAttributes attributes;
	BufferBindings buffers;

	DrawIndexedCommand cmd(&attributes, &buffers, indexbuffer);

	cmd.primitiveType = PRIMITIVE_TRIANGLES;
	cmd.indexCount = indexcount;
	cmd.instanceCount = std::max(1, instancecount);

	cmd.indexType = getIndexDataType(indexbuffer->getDataMember(0).decl.format);
	cmd.indexBufferOffset = startindex * getIndexDataSize(cmd.indexType);

	cmd.texture = getTextureOrDefaultForActiveShader(maintexture);

	draw(cmd);
}

void graphics::drawFromShaderIndirect(PrimitiveType primtype, Buffer *indirectargs, int argsindex, Texture *maintexture)
{
	flushBatchedDraws();

	if (primtype == PRIMITIVE_TRIANGLE_FAN)
		throw love::Exception("The fan draw mode is not supported in indirect draws.");

	if (Shader::isDefaultActive() || !Shader::current)
		throw love::Exception("drawFromShaderIndirect can only be used with a custom shader.");

	validateIndirectArgsBuffer(INDIRECT_ARGS_DRAW_VERTICES, indirectargs, argsindex);

	Shader::current->validateDrawState(primtype, maintexture);

	VertexAttributes attributes;
	BufferBindings buffers;

	DrawCommand cmd(&attributes, &buffers);

	cmd.primitiveType = primtype;
	cmd.indirectBuffer = indirectargs;
	cmd.indirectBufferOffset = argsindex * indirectargs->getArrayStride();
	cmd.texture = getTextureOrDefaultForActiveShader(maintexture);

	draw(cmd);
}

void graphics::drawFromShaderIndirect(Buffer *indexbuffer, Buffer *indirectargs, int argsindex, Texture *maintexture)
{
	flushBatchedDraws();

	if (!(indexbuffer->getUsageFlags() & BUFFERUSAGEFLAG_INDEX))
		throw love::Exception("The buffer passed to the indexed variant of drawFromShaderIndirect must be an index buffer.");

	if (Shader::isDefaultActive() || !Shader::current)
		throw love::Exception("drawFromShaderIndirect can only be used with a custom shader.");

	validateIndirectArgsBuffer(INDIRECT_ARGS_DRAW_INDICES, indirectargs, argsindex);

	Shader::current->validateDrawState(PRIMITIVE_TRIANGLES, maintexture);

	VertexAttributes attributes;
	BufferBindings buffers;

	DrawIndexedCommand cmd(&attributes, &buffers, indexbuffer);

	cmd.primitiveType = PRIMITIVE_TRIANGLES;
	cmd.indexType = getIndexDataType(indexbuffer->getDataMember(0).decl.format);
	cmd.indirectBuffer = indirectargs;
	cmd.indexBufferOffset = argsindex * indirectargs->getArrayStride();
	cmd.texture = getTextureOrDefaultForActiveShader(maintexture);

	draw(cmd);
}

void graphics::print(const std::vector<love::font::ColoredString> &str, const Matrix4 &m)
{
	checkSetDefaultFont();

	if (states.back().font.get() != nullptr)
		print(str, states.back().font.get(), m);
}

void graphics::print(const std::vector<love::font::ColoredString> &str, FontMod *font, const Matrix4 &m)
{
	font->print(this, str, m, states.back().color);
}

void graphics::printf(const std::vector<love::font::ColoredString> &str, float wrap, FontMod::AlignMode align, const Matrix4 &m)
{
	checkSetDefaultFont();

	if (states.back().font.get() != nullptr)
		printf(str, states.back().font.get(), wrap, align, m);
}

void graphics::printf(const std::vector<love::font::ColoredString> &str, FontMod *font, float wrap, FontMod::AlignMode align, const Matrix4 &m)
{
	font->printf(this, str, wrap, align, m, states.back().color);
}

/**
 * Primitives (points, shapes, lines).
 **/

void graphics::points(const Vector2 *positions, const Colorf *colors, size_t numpoints)
{
	const Matrix4 &t = getTransform();
	bool is2D = t.isAffine2DTransform();

	BatchedDrawCommand cmd;
	cmd.primitiveMode = PRIMITIVE_POINTS;
	cmd.formats[0] = getSinglePositionFormat(is2D);
	cmd.formats[1] = CommonFormat::RGBAub;
	cmd.vertexCount = (int) numpoints;
	cmd.standardShaderType = Shader::STANDARD_POINTS;

	BatchedVertexData data = requestBatchedDraw(cmd);

	if (is2D)
		t.transformXY((Vector2 *) data.stream[0], positions, cmd.vertexCount);
	else
		t.transformXY0((Vector3 *) data.stream[0], positions, cmd.vertexCount);

	Color32 *colordata = (Color32 *) data.stream[1];

	if (colors)
	{
		Colorf nc = getColor();
		gammaCorrectColor(nc);

		if (isGammaCorrect())
		{
			for (int i = 0; i < cmd.vertexCount; i++)
			{
				Colorf ci = colors[i];
				gammaCorrectColor(ci);
				ci *= nc;
				unGammaCorrectColor(ci);
				colordata[i] = toColor32(ci);
			}
		}
		else
		{
			for (int i = 0; i < cmd.vertexCount; i++)
				colordata[i] = toColor32(nc * colors[i]);
		}
	}
	else
	{
		Color32 c = toColor32(getColor());

		for (int i = 0; i < cmd.vertexCount; i++)
			colordata[i] = c;
	}
}

int graphics::calculateEllipsePoints(float rx, float ry) const
{
	int points = (int) sqrtf(((rx + ry) / 2.0f) * 20.0f * (float) pixelScaleStack.back());
	return std::max(points, 8);
}

void graphics::polyline(const Vector2 *vertices, size_t count)
{
	float halfwidth = getLineWidth() * 0.5f;
	LineJoin linejoin = getLineJoin();
	LineStyle linestyle = getLineStyle();

	float pixelsize = 1.0f / std::max((float) pixelScaleStack.back(), 0.000001f);

	if (linejoin == LINE_JOIN_NONE)
	{
		NoneJoinPolyline line;
		line.render(vertices, count, halfwidth, pixelsize, linestyle == LINE_SMOOTH);
		line.draw(this);
	}
	else if (linejoin == LINE_JOIN_BEVEL)
	{
		BevelJoinPolyline line;
		line.render(vertices, count, halfwidth, pixelsize, linestyle == LINE_SMOOTH);
		line.draw(this);
	}
	else if (linejoin == LINE_JOIN_MITER)
	{
		MiterJoinPolyline line;
		line.render(vertices, count, halfwidth, pixelsize, linestyle == LINE_SMOOTH);
		line.draw(this);
	}
}

void graphics::rectangle(DrawMode mode, float x, float y, float w, float h)
{
	Vector2 coords[] = {Vector2(x,y), Vector2(x,y+h), Vector2(x+w,y+h), Vector2(x+w,y), Vector2(x,y)};
	polygon(mode, coords, 5);
}

void graphics::rectangle(DrawMode mode, float x, float y, float w, float h, float rx, float ry, int points)
{
	if (rx <= 0 || ry <= 0)
	{
		rectangle(mode, x, y, w, h);
		return;
	}

	// Radius values that are more than half the rectangle's size aren't handled
	// correctly (for now)...
	if (w >= 0.02f)
		rx = std::min(rx, w / 2.0f - 0.01f);
	if (h >= 0.02f)
		ry = std::min(ry, h / 2.0f - 0.01f);

	points = std::max(points / 4, 1);

	const float half_pi = static_cast<float>(LOVE_M_PI / 2);
	float angle_shift = half_pi / ((float) points + 1.0f);

	int num_coords = (points + 2) * 4;
	Vector2 *coords = getScratchBuffer<Vector2>(num_coords + 1);
	float phi = .0f;

	for (int i = 0; i <= points + 2; ++i, phi += angle_shift)
	{
		coords[i].x = x + rx * (1 - cosf(phi));
		coords[i].y = y + ry * (1 - sinf(phi));
	}

	phi = half_pi;

	for (int i = points + 2; i <= 2 * (points + 2); ++i, phi += angle_shift)
	{
		coords[i].x = x + w - rx * (1 + cosf(phi));
		coords[i].y = y +     ry * (1 - sinf(phi));
	}

	phi = 2 * half_pi;

	for (int i = 2 * (points + 2); i <= 3 * (points + 2); ++i, phi += angle_shift)
	{
		coords[i].x = x + w - rx * (1 + cosf(phi));
		coords[i].y = y + h - ry * (1 + sinf(phi));
	}

	phi = 3 * half_pi;

	for (int i = 3 * (points + 2); i <= 4 * (points + 2); ++i, phi += angle_shift)
	{
		coords[i].x = x +     rx * (1 - cosf(phi));
		coords[i].y = y + h - ry * (1 + sinf(phi));
	}

	coords[num_coords] = coords[0];

	polygon(mode, coords, num_coords + 1);
}

void graphics::rectangle(DrawMode mode, float x, float y, float w, float h, float rx, float ry)
{
	int points = calculateEllipsePoints(std::min(rx, std::abs(w/2)), std::min(ry, std::abs(h/2)));
	rectangle(mode, x, y, w, h, rx, ry, points);
}

void graphics::circle(DrawMode mode, float x, float y, float radius, int points)
{
	ellipse(mode, x, y, radius, radius, points);
}

void graphics::circle(DrawMode mode, float x, float y, float radius)
{
	ellipse(mode, x, y, radius, radius);
}

void graphics::ellipse(DrawMode mode, float x, float y, float a, float b, int points)
{
	float two_pi = (float) (LOVE_M_PI * 2);
	if (points <= 0) points = 1;
	float angle_shift = (two_pi / points);
	float phi = .0f;

	// 1 extra point at the end for a closed loop, and 1 extra point at the
	// start in filled mode for the vertex in the center of the ellipse.
	int extrapoints = 1 + (mode == DRAW_FILL ? 1 : 0);

	Vector2 *polygoncoords = getScratchBuffer<Vector2>(points + extrapoints);
	Vector2 *coords = polygoncoords;

	if (mode == DRAW_FILL)
	{
		coords[0].x = x;
		coords[0].y = y;
		coords++;
	}

	for (int i = 0; i < points; ++i, phi += angle_shift)
	{
		coords[i].x = x + a * cosf(phi);
		coords[i].y = y + b * sinf(phi);
	}

	coords[points] = coords[0];

	// Last argument to polygon(): don't skip the last vertex in fill mode.
	polygon(mode, polygoncoords, points + extrapoints, false);
}

void graphics::ellipse(DrawMode mode, float x, float y, float a, float b)
{
	ellipse(mode, x, y, a, b, calculateEllipsePoints(a, b));
}

void graphics::arc(DrawMode drawmode, ArcMode arcmode, float x, float y, float radius, float angle1, float angle2, int points)
{
	// Nothing to display with no points or equal angles. (Or is there with line mode?)
	if (points <= 0 || angle1 == angle2)
		return;

	// Oh, you want to draw a circle?
	if (fabs(angle1 - angle2) >= 2.0f * (float) LOVE_M_PI)
	{
		circle(drawmode, x, y, radius, points);
		return;
	}

	float angle_shift = (angle2 - angle1) / points;
	// Bail on precision issues.
	if (angle_shift == 0.0)
		return;

	// Prevent the connecting line from being drawn if a closed line arc has a
	// small angle. Avoids some visual issues when connected lines are at sharp
	// angles, due to the miter line join drawing code.
	if (drawmode == DRAW_LINE && arcmode == ARC_CLOSED && fabsf(angle1 - angle2) < LOVE_TORAD(4))
		arcmode = ARC_OPEN;

	// Quick fix for the last part of a filled open arc not being drawn (because
	// polygon(DRAW_FILL, ...) doesn't work without a closed loop of vertices.)
	if (drawmode == DRAW_FILL && arcmode == ARC_OPEN)
		arcmode = ARC_CLOSED;

	float phi = angle1;

	Vector2 *coords = nullptr;
	int num_coords = 0;

	const auto createPoints = [&](Vector2 *coordinates)
	{
		for (int i = 0; i <= points; ++i, phi += angle_shift)
		{
			coordinates[i].x = x + radius * cosf(phi);
			coordinates[i].y = y + radius * sinf(phi);
		}
	};

	if (arcmode == ARC_PIE)
	{
		num_coords = points + 3;
		coords = getScratchBuffer<Vector2>(num_coords);

		coords[0] = coords[num_coords - 1] = Vector2(x, y);

		createPoints(coords + 1);
	}
	else if (arcmode == ARC_OPEN)
	{
		num_coords = points + 1;
		coords = getScratchBuffer<Vector2>(num_coords);

		createPoints(coords);
	}
	else // ARC_CLOSED
	{
		num_coords = points + 2;
		coords = getScratchBuffer<Vector2>(num_coords);

		createPoints(coords);

		// Connect the ends of the arc.
		coords[num_coords - 1] = coords[0];
	}

	polygon(drawmode, coords, num_coords);
}

void graphics::arc(DrawMode drawmode, ArcMode arcmode, float x, float y, float radius, float angle1, float angle2)
{
	float points = (float) calculateEllipsePoints(radius, radius);

	// The amount of points is based on the fraction of the circle created by the arc.
	float angle = fabsf(angle1 - angle2);
	if (angle < 2.0f * (float) LOVE_M_PI)
		points *= angle / (2.0f * (float) LOVE_M_PI);

	arc(drawmode, arcmode, x, y, radius, angle1, angle2, (int) (points + 0.5f));
}

void graphics::polygon(DrawMode mode, const Vector2 *coords, size_t count, bool skipLastFilledVertex)
{
	// coords is an array of a closed loop of vertices, i.e.
	// coords[count-1] == coords[0]
	if (mode == DRAW_LINE)
	{
		polyline(coords, count);
	}
	else
	{
		const Matrix4 &t = getTransform();
		bool is2D = t.isAffine2DTransform();

		BatchedDrawCommand cmd;
		cmd.formats[0] = getSinglePositionFormat(is2D);
		cmd.formats[1] = CommonFormat::STf_RGBAub;
		cmd.indexMode = TRIANGLEINDEX_FAN;
		cmd.vertexCount = (int)count - (skipLastFilledVertex ? 1 : 0);

		BatchedVertexData data = requestBatchedDraw(cmd);

		// Compute texture coordinates.
		constexpr float inf = std::numeric_limits<float>::infinity();
		Vector2 mincoord(inf, inf);
		Vector2 maxcoord(-inf, -inf);

		for (int i = 0; i < cmd.vertexCount; i++)
		{
			Vector2 v = coords[i];
			mincoord.x = std::min(mincoord.x, v.x);
			mincoord.y = std::min(mincoord.y, v.y);
			maxcoord.x = std::max(maxcoord.x, v.x);
			maxcoord.y = std::max(maxcoord.y, v.y);
		}

		Vector2 invsize(1.0f / (maxcoord.x - mincoord.x), 1.0f / (maxcoord.y - mincoord.y));
		Vector2 start(mincoord.x * invsize.x, mincoord.y * invsize.y);

		Color32 c = toColor32(getColor());
		STf_RGBAub *attributes = (STf_RGBAub *) data.stream[1];
		for (int i = 0; i < cmd.vertexCount; i++)
		{
			attributes[i].s = coords[i].x * invsize.x - start.x;
			attributes[i].t = coords[i].y * invsize.y - start.y;
			attributes[i].color = c;
		}

		if (is2D)
			t.transformXY((Vector2*)data.stream[0], coords, cmd.vertexCount);
		else
			t.transformXY0((Vector3*)data.stream[0], coords, cmd.vertexCount);
	}
}

const graphics::Capabilities &graphics::getCapabilities() const
{
	return capabilities;
}

PixelFormat graphics::getSizedFormat(PixelFormat format) const
{
	switch (format)
	{
	case PIXELFORMAT_NORMAL:
		if (isGammaCorrect())
			return PIXELFORMAT_RGBA8_sRGB;
		else
			return PIXELFORMAT_RGBA8_UNORM;
	case PIXELFORMAT_HDR:
		return PIXELFORMAT_RGBA16_FLOAT;
	default:
		return format;
	}
}

graphics::Stats graphics::getStats() const
{
	Stats stats;

	getAPIStats(stats.shaderSwitches);

	stats.drawCalls = drawCalls;
	if (batchedDrawState.vertexCount > 0)
		stats.drawCalls++;

	stats.renderTargetSwitches = renderTargetSwitchCount;
	stats.drawCallsBatched = drawCallsBatched;
	stats.textures = Texture::textureCount;
	stats.fonts = FontMod::fontCount;
	stats.buffers = Buffer::bufferCount;
	stats.textureMemory = Texture::totalGraphicsMemory;
	stats.bufferMemory = Buffer::totalGraphicsMemory;

	return stats;
}

size_t graphics::getStackDepth() const
{
	return stackTypeStack.size();
}

void graphics::push(StackType type)
{
	if (stackTypeStack.size() == MAX_USER_STACK_DEPTH)
		throw Exception("Maximum stack depth reached (more pushes than pops?)");

	pushTransform();

	pixelScaleStack.push_back(pixelScaleStack.back());

	if (type == STACK_ALL)
		states.push_back(states.back());

	stackTypeStack.push_back(type);
}

void graphics::pop()
{
	if (stackTypeStack.size() < 1)
		throw Exception("Minimum stack depth reached (more pops than pushes?)");

	popTransform();
	pixelScaleStack.pop_back();

	if (stackTypeStack.back() == STACK_ALL)
	{
		DisplayState &newstate = states[states.size() - 2];

		restoreStateChecked(newstate);

		// The last two states in the stack should be equal now.
		states.pop_back();
	}

	stackTypeStack.pop_back();
}

/**
 * Transform and stack functions.
 **/

const Matrix4 &graphics::getTransform() const
{
	return transformStack.back();
}

const Matrix4 &graphics::getDeviceProjection() const
{
	return deviceProjectionMatrix;
}

void graphics::pushTransform()
{
	transformStack.push_back(transformStack.back());
}

void graphics::pushIdentityTransform()
{
	transformStack.push_back(Matrix4());
}

void graphics::popTransform()
{
	transformStack.pop_back();
}

void graphics::rotate(float r)
{
	transformStack.back().rotate(r);
}

void graphics::scale(float x, float y)
{
	transformStack.back().scale(x, y);
	pixelScaleStack.back() *= (fabs(x) + fabs(y)) / 2.0;
}

void graphics::translate(float x, float y)
{
	transformStack.back().translate(x, y);
}

void graphics::shear(float kx, float ky)
{
	transformStack.back().shear(kx, ky);
}

void graphics::origin()
{
	transformStack.back().setIdentity();
	pixelScaleStack.back() = 1;
}

void graphics::applyTransform(const Matrix4 &m)
{
	Matrix4 &current = transformStack.back();
	current *= m;

	float sx, sy;
	current.getApproximateScale(sx, sy);
	pixelScaleStack.back() = (sx + sy) / 2.0;
}

void graphics::replaceTransform(const Matrix4 &m)
{
	transformStack.back() = m;

	float sx, sy;
	m.getApproximateScale(sx, sy);
	pixelScaleStack.back() = (sx + sy) / 2.0;
}

Vector2 graphics::transformPoint(Vector2 point)
{
	Vector2 p;
	transformStack.back().transformXY(&p, &point, 1);
	return p;
}

Vector2 graphics::inverseTransformPoint(Vector2 point)
{
	Vector2 p;
	// TODO: We should probably cache the inverse transform so we don't have to
	// re-calculate it every time this is called.
	transformStack.back().inverse().transformXY(&p, &point, 1);
	return p;
}

void graphics::setProjection(const Matrix4 &m)
{
	flushBatchedDraws();

	auto &state = states.back();

	state.useCustomProjection = true;
	state.customProjection = m;

	updateDeviceProjection(m);
}

void graphics::resetProjection()
{
	flushBatchedDraws();

	auto &state = states.back();
	int w = getWidth();
	int h = getHeight();

	const auto &rt = state.renderTargets.getFirstTarget();
	if (rt.texture.get())
	{
		w = rt.texture->getWidth(rt.mipmap);
		h = rt.texture->getHeight(rt.mipmap);
	}

	state.useCustomProjection = false;

	// NDC is y-up. The ortho() parameter names assume that as well. We want
	// a y-down projection, so we set bottom to h and top to 0.
	updateDeviceProjection(Matrix4::ortho(0.0f, w, h, 0.0f, -10.0f, 10.0f));
}

void graphics::updateDeviceProjection(const Matrix4 &projection)
{
	deviceProjectionMatrix = projection;
}

STRINGMAP_CLASS_BEGIN(graphics, graphics::DrawMode, graphics::DRAW_MAX_ENUM, drawMode)
{
	{ "line", graphics::DRAW_LINE },
	{ "fill", graphics::DRAW_FILL },
}
STRINGMAP_CLASS_END(graphics, graphics::DrawMode, graphics::DRAW_MAX_ENUM, drawMode)

STRINGMAP_CLASS_BEGIN(graphics, graphics::ArcMode, graphics::ARC_MAX_ENUM, arcMode)
{
	{ "open",   graphics::ARC_OPEN   },
	{ "closed", graphics::ARC_CLOSED },
	{ "pie",    graphics::ARC_PIE    },
}
STRINGMAP_CLASS_END(graphics, graphics::ArcMode, graphics::ARC_MAX_ENUM, arcMode)

STRINGMAP_CLASS_BEGIN(graphics, graphics::LineStyle, graphics::LINE_MAX_ENUM, lineStyle)
{
	{ "smooth", graphics::LINE_SMOOTH },
	{ "rough",  graphics::LINE_ROUGH  }
}
STRINGMAP_CLASS_END(graphics, graphics::LineStyle, graphics::LINE_MAX_ENUM, lineStyle)

STRINGMAP_CLASS_BEGIN(graphics, graphics::LineJoin, graphics::LINE_JOIN_MAX_ENUM, lineJoin)
{
	{ "none",  graphics::LINE_JOIN_NONE  },
	{ "miter", graphics::LINE_JOIN_MITER },
	{ "bevel", graphics::LINE_JOIN_BEVEL }
}
STRINGMAP_CLASS_END(graphics, graphics::LineJoin, graphics::LINE_JOIN_MAX_ENUM, lineJoin)

STRINGMAP_CLASS_BEGIN(graphics, graphics::Feature, graphics::FEATURE_MAX_ENUM, feature)
{
	{ "multirendertargetformats", graphics::FEATURE_MULTI_RENDER_TARGET_FORMATS },
	{ "clampzero",                graphics::FEATURE_CLAMP_ZERO           },
	{ "clampone",                 graphics::FEATURE_CLAMP_ONE            },
	{ "lighten",                  graphics::FEATURE_LIGHTEN              },
	{ "fullnpot",                 graphics::FEATURE_FULL_NPOT            },
	{ "pixelshaderhighp",         graphics::FEATURE_PIXEL_SHADER_HIGHP   },
	{ "shaderderivatives",        graphics::FEATURE_SHADER_DERIVATIVES   },
	{ "glsl3",                    graphics::FEATURE_GLSL3                },
	{ "glsl4",                    graphics::FEATURE_GLSL4                },
	{ "instancing",               graphics::FEATURE_INSTANCING           },
	{ "texelbuffer",              graphics::FEATURE_TEXEL_BUFFER         },
	{ "copytexturetobuffer",      graphics::FEATURE_COPY_TEXTURE_TO_BUFFER },
	{ "indirectdraw",             graphics::FEATURE_INDIRECT_DRAW        },
}
STRINGMAP_CLASS_END(graphics, graphics::Feature, graphics::FEATURE_MAX_ENUM, feature)

STRINGMAP_CLASS_BEGIN(graphics, graphics::SystemLimit, graphics::LIMIT_MAX_ENUM, systemLimit)
{
	{ "pointsize",               graphics::LIMIT_POINT_SIZE                 },
	{ "texturesize",             graphics::LIMIT_TEXTURE_SIZE               },
	{ "texturelayers",           graphics::LIMIT_TEXTURE_LAYERS             },
	{ "volumetexturesize",       graphics::LIMIT_VOLUME_TEXTURE_SIZE        },
	{ "cubetexturesize",         graphics::LIMIT_CUBE_TEXTURE_SIZE          },
	{ "texelbuffersize",         graphics::LIMIT_TEXEL_BUFFER_SIZE          },
	{ "shaderstoragebuffersize", graphics::LIMIT_SHADER_STORAGE_BUFFER_SIZE },
	{ "threadgroupsx",           graphics::LIMIT_THREADGROUPS_X             },
	{ "threadgroupsy",           graphics::LIMIT_THREADGROUPS_Y             },
	{ "threadgroupsz",           graphics::LIMIT_THREADGROUPS_Z             },
	{ "rendertargets",           graphics::LIMIT_RENDER_TARGETS             },
	{ "texturemsaa",             graphics::LIMIT_TEXTURE_MSAA               },
	{ "anisotropy",              graphics::LIMIT_ANISOTROPY                 },
}
STRINGMAP_CLASS_END(graphics, graphics::SystemLimit, graphics::LIMIT_MAX_ENUM, systemLimit)

STRINGMAP_CLASS_BEGIN(graphics, graphics::StackType, graphics::STACK_MAX_ENUM, stackType)
{
	{ "all",       graphics::STACK_ALL       },
	{ "transform", graphics::STACK_TRANSFORM },
}
STRINGMAP_CLASS_END(graphics, graphics::StackType, graphics::STACK_MAX_ENUM, stackType)

STRINGMAP_BEGIN(Renderer, RENDERER_MAX_ENUM, renderer)
{
	{ "opengl", RENDERER_OPENGL },
	// { "vulkan", RENDERER_VULKAN },
	{ "metal",  RENDERER_METAL  },
}
STRINGMAP_END(Renderer, RENDERER_MAX_ENUM, renderer)

} // gfx
} // love
