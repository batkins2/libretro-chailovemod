#ifndef SRC_LOVE_EVENT_H_
#define SRC_LOVE_EVENT_H_

#include <SDL2/SDL.h>

namespace love {
/**
 * Manages events, like keypresses.
 */
class event {
	public:
	/**
	 * Exits the ChaiLove application.
	 */
	void quit();
	void update();
	void pause();
	void renderlock();
	bool isrenderlocked();

	bool m_shouldclose = false;
	bool m_pauserendering = false;
	bool m_renderlocked = false;
};

}  // namespace love

#endif  // SRC_LOVE_EVENT_H_
