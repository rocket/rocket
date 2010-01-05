#include "../sync/sync.h"
namespace sync {
	class Timer {
	public:
		virtual void pause() = 0;
		virtual void play() = 0;
		virtual void setRow(float) = 0;
		virtual bool isPlaying() = 0;
	};

	static void xpause(void *ptr, int flag)
	{
		Timer *timer = (Timer *)ptr;
		if (flag)
			timer->pause();
		else
			timer->play();
	}

	static void xset_row(void *ptr, int row)
	{
		Timer *timer = (Timer *)ptr;
		timer->setRow(float(row));
	}

	static int xis_playing(void *ptr)
	{
		Timer *timer = (Timer *)ptr;
		return timer->isPlaying();
	}

	class Track {
	public:
		float getValue(double row)
		{
			return sync_get_val(track, row);
		}
		sync_track *track;
	};

	class Device {
	public:
		Track getTrack(const char *name)
		{
			Track track;
			track.track = sync_get_track(device, name);
			return track;
		}

		bool update(double row)
		{
			sync_update(device, row);
			return true;
		}

		sync_device *device;
		sync_cb cb;
	};

	inline Device *createDevice(const char *prefix, Timer &timer)
	{
		Device *device = new Device;
		device->device = sync_create_device(prefix);
		if (!device->device) {
			delete device;
			return NULL;
		}

		device->cb.is_playing = xis_playing;
		device->cb.pause = xpause;
		device->cb.set_row = xset_row;
		sync_set_callbacks(device->device, &device->cb, (void *)&timer);
		return device;
	}
}
