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

#ifndef LOVE_AUDIO_NULL_SOURCE_H
#define LOVE_AUDIO_NULL_SOURCE_H

// LOVE
#include "../../common/Object.h"
#include "../Source.h"
#include "../Filter.h"

namespace love
{
namespace audiomod
{
namespace null
{

class Source : public love::audiomod::Source
{
public:
	Source();
	virtual ~Source();

	virtual love::audiomod::Source *clone();
	virtual bool play();
	virtual void stop();
	virtual void pause();
	virtual bool isPlaying() const;
	virtual bool isFinished() const;
	virtual bool update();
	virtual void setPitch(float pitch);
	virtual float getPitch() const;
	virtual void setVolume(float volume);
	virtual float getVolume() const;
	virtual void seek(double offset, Unit unit);
	virtual double tell(Unit unit);
	virtual double getDuration(Unit unit);
	virtual void setPosition(float *v);
	virtual void getPosition(float *v) const;
	virtual void setVelocity(float *v);
	virtual void getVelocity(float *v) const;
	virtual void setDirection(float *v);
	virtual void getDirection(float *v) const;
	virtual void setCone(float innerAngle, float outerAngle, float outerVolume, float outerHighGain);
	virtual void getCone(float &innerAngle, float &outerAngle, float &outerVolume, float &outerHighGain) const;
	virtual void setRelative(bool enable);
	virtual bool isRelative() const;
	void setLooping(bool looping);
	bool isLooping() const;
	virtual void setMinVolume(float volume);
	virtual float getMinVolume() const;
	virtual void setMaxVolume(float volume);
	virtual float getMaxVolume() const;
	virtual void setReferenceDistance(float distance);
	virtual float getReferenceDistance() const;
	virtual void setRolloffFactor(float factor);
	virtual float getRolloffFactor() const;
	virtual void setMaxDistance(float distance);
	virtual float getMaxDistance() const;
	virtual void setAirAbsorptionFactor(float factor);
	virtual float getAirAbsorptionFactor() const;
	virtual int getChannelCount() const;

	virtual int getFreeBufferCount() const;
	virtual bool queue(void *data, size_t length, int dataSampleRate, int dataBitDepth, int dataChannels);

	virtual bool setFilter(const std::map<Filter::Parameter, float> &params);
	virtual bool setFilter();
	virtual bool getFilter(std::map<Filter::Parameter, float> &params);

	virtual bool setEffect(const char *effect);
	virtual bool setEffect(const char *effect, const std::map<Filter::Parameter, float> &params);
	virtual bool unsetEffect(const char *effect);
	virtual bool getEffect(const char *effect, std::map<Filter::Parameter, float> &params);
	virtual bool getActiveEffects(std::vector<std::string> &list) const;

private:

	float pitch;
	float volume;
	float coneInnerAngle;
	float coneOuterAngle;
	float coneOuterVolume;
	float coneOuterHighGain;
	bool relative;
	bool looping;
	float minVolume;
	float maxVolume;
	float referenceDistance;
	float rolloffFactor;
	float maxDistance;
	float absorptionFactor;

}; // Source

} // null
} // audio
} // love

#endif // LOVE_AUDIO_NULL_SOURCE_H
