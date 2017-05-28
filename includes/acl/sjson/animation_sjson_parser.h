#pragma once

#include <string>
#include <vector>

namespace acl
{
	class AnimationSJSONParser : SJSONParser
	{
	protected:
		virtual void onKey(std::string key)
		{
			// TODO: record where we are, eg. clip, bones, or tracks
		}

		virtual void onValue(std::string key, Variant* v)
		{
			// TODO: assert the expected type and apply to the clip, track, etc
		}

		virtual void onValue(std::string key, std::vector<Variant*> a)
		{
			// TODO: assert the expected type and move to the track
		}

		virtual void onBeginObject()
		{
		}

		virtual void onEndObject()
		{
		}
	};
}