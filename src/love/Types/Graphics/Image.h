#ifndef SRC_LOVE_TYPES_GRAPHICS_IMAGE_H_
#define SRC_LOVE_TYPES_GRAPHICS_IMAGE_H_

#include <SDL2/SDL.h>
#include <string>

namespace love {
namespace Types {
namespace Graphics {

/**
 * Drawable image type.
 *
 * @see love.graphics.newImage
 */
class Image {
	public:
	SDL_Surface* surface;
	SDL_Texture* texture;
	Image(SDL_RWops* rw);
	Image(const std::string& filename);
	~Image();
	bool loaded();
	bool loadFromRW(SDL_RWops* rw);
	bool destroy();

	/**
	 * Gets the width of the Texture.
	 *
	 * @see getHeight
	 */
	int getWidth();

	/**
	 * Gets the height of the Texture.
	 *
	 * @see getWidth
	 */
	int getHeight();
};

}  // namespace Graphics
}  // namespace Types
}  // namespace love

#endif  // SRC_LOVE_TYPES_GRAPHICS_IMAGE_H_
