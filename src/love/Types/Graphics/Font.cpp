#include "Font.h"
#include "SDL.h"
#include <string>
#include "SDL_ttf.h"
#include <SDL2_gfxPrimitives.h>
#include <SDL_fnt.h>
#include "../../../ChaiLove.h"
#include "../../../LibretroLog.h"
#include "Image.h"

namespace love {
namespace Types {
namespace Graphics {

Font::Font() {
	// Nothing.
}

Font::Font(const std::string& filename, int glyphWidth, int glyphHeight, const std::string& letters) {
	m_image = new Image(filename);
	if (m_image->loaded()) {
		TTY_Font* newFont = FNT_Create(m_image->surface, glyphWidth, glyphHeight, letters.c_str());
		if (newFont == NULL) {
			LibretroLog::log(RETRO_LOG_ERROR) << "[ChaiLove] Error creating FNT_Create()" << std::endl;
			return;
		}

		// ttyFont = newFont;
		ttyFontWidth = glyphWidth;
		ttyFontHeight = glyphHeight;
	}
}

Font::Font(const std::string& filename, int ptsize) {
	SDL_RWops* rwops = ChaiLove::getInstance()->filesystem.openRW(filename);
	if (rwops) {
		TTF_Font* newFont = TTF_OpenFontRW(rwops, 1, ptsize);
		if (!newFont) {
			const char* errorChar = TTF_GetError();
			std::string errString("");
			if (errorChar != NULL) {
				errString = errorChar;
			}
			LibretroLog::log(RETRO_LOG_ERROR) << "TTF_OpenFontRW: " << errString << std::endl;
			return;
		}

		ttfFont = newFont;
	}
}

Font::~Font() {
	destroy();
}

bool Font::loaded() {
	return ttfFont != NULL; // || ttyFont != NULL;
}

bool Font::destroy() {
	if (ttfFont != NULL) {
		TTF_CloseFont(ttfFont);
		ttfFont = NULL;
	}

	// if (ttyFont != NULL) {
	// 	FNT_Free(ttyFont);
	// 	ttyFont = NULL;
	// }

	if (m_image != NULL) {
		delete m_image;
		m_image = NULL;
	}

	return true;
}

int Font::getHeight(const std::string& text) {
	if (ttfFont != NULL) {
		return TTF_FontHeight(ttfFont);
	}
	// if (ttyFont != NULL) {
	// 	return FNT_GetTextHeight(ttyFont, text.c_str());
	// }

	return 12;
}

int Font::getHeight() {
	if (ttfFont != NULL) {
		return TTF_FontHeight(ttfFont);
	}
	// if (ttyFont != NULL) {
	// 	return ttyFontHeight;
	// }

	return 12;
}

int Font::getWidth(const std::string& text) {
	if (ttfFont != NULL) {
		int w;
		if (TTF_SizeText(ttfFont, text.c_str(), &w, NULL) == 0) {
			return w;
		}
	}

	// if (ttyFont != NULL) {
	// 	return FNT_GetTextWidth(ttyFont, text.c_str());
	// }

	return text.length() * 12;
}

void Font::print(const std::string& text, int x, int y, int r, int g, int b, int a) {
	SDL_Surface* screen = ChaiLove::getInstance()->screen;

	// Attempt to render the TTF Font.
	if (ttfFont != NULL) {
		SDL_Color color = {(Uint8)r, (Uint8)g, (Uint8)b};
		SDL_Surface* surface = TTF_RenderText_Blended(ttfFont, text.c_str(), color);
		if (!surface) {
			const char* errorChar = TTF_GetError();
			std::string errString("");
			if (errorChar != NULL) {
				errString = errorChar;
			}
			LibretroLog::log(RETRO_LOG_ERROR) << "[ChaiLove] Font::print - %s" << errString << std::endl;
			return;
		}

		SDL_Rect* dstrect = new SDL_Rect();
		dstrect->x = x;
		dstrect->y = y;
		SDL_BlitSurface(surface, NULL, screen, dstrect);

		// TODO(RobLoach): Allow re-use of text renderings.
		SDL_FreeSurface(surface);
		return;
	}

	// Use TTY to print the bitmap font?
	// if (ttyFont != NULL) {
	// 	FNT_Print(ttyFont, screen, x, y, FNT_ALIGN_LEFT, text.c_str());
	// 	return;
	// }

	// Fall back to SDL_gfx primitives.
	stringRGBA(ChaiLove::getInstance()->renderer, x, y, text.c_str(), r, g, b, a);
}

}  // namespace Graphics
}  // namespace Types
}  // namespace love
