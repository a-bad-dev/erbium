// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2017 nerzhul, Loic Blot <loic.blot@unix-experience.fr>

#pragma once

#include <vector>
#include <memory>
#include <string>
#include <cmath>
#include "client/inputhandler.h"
#include "debug.h"
#include "config.h"
#include "client/shader.h"
#include "client/render/core.h"
// include the shadow mapper classes too
#include "client/shadows/dynamicshadowsrender.h"
#include <IVideoDriver.h>

#if !IS_CLIENT_BUILD
#error Do not include in server builds
#endif

struct VideoDriverInfo {
	std::string name;
	std::string friendly_name;
};

class ITextureSource;
class Client;
class Hud;

class RenderingCore;

// Instead of a mechanism to disable fog we just set it to be really far away
#define FOG_RANGE_ALL (100000 * BS)

/* Helpers */

struct FpsControl {
	FpsControl() : last_time(0), busy_time(0), sleep_time(0) {}

	void reset();

	void limit(IrrlichtDevice *device, f32 *dtime);

	u32 getBusyMs() const { return busy_time / 1000; }

	// all values in microseconds (us)
	u64 last_time, busy_time, sleep_time;
};

// Populates fogColor, fogDistance, fogShadingParameter with values from Irrlicht
class FogShaderUniformSetterFactory : public IShaderUniformSetterFactory
{
public:
	FogShaderUniformSetterFactory() {};
	virtual IShaderUniformSetter *create(const std::string &name);
};

/* Rendering engine class */

class RenderingEngine
{
public:
	RenderingEngine(MyEventReceiver *eventReceiver);
	~RenderingEngine();

	void setResizable(bool resize);

	video::IVideoDriver *getVideoDriver() { return driver; }

	static const VideoDriverInfo &getVideoDriverInfo(video::E_DRIVER_TYPE type);
	static float getDisplayDensity();

	bool setupTopLevelWindow();
	bool setWindowIcon();
	void cleanupMeshCache();

	void removeMesh(const scene::IMesh* mesh);

	/**
	 * This takes 3d_mode into account - side-by-side will return a
	 * halved horizontal size.
	 *
	 * @return "window" size
	 */
	static v2u32 getWindowSize()
	{
		sanity_check(s_singleton);
		return s_singleton->_getWindowSize();
	}

	io::IFileSystem *get_filesystem()
	{
		return m_device->getFileSystem();
	}

	static video::IVideoDriver *get_video_driver()
	{
		sanity_check(s_singleton && s_singleton->m_device);
		return s_singleton->m_device->getVideoDriver();
	}

	scene::ISceneManager *get_scene_manager()
	{
		return m_device->getSceneManager();
	}

	static IrrlichtDevice *get_raw_device()
	{
		sanity_check(s_singleton && s_singleton->m_device);
		return s_singleton->m_device;
	}

	gui::IGUIEnvironment *get_gui_env()
	{
		return m_device->getGUIEnvironment();
	}

	// If "indef_pos" is given, the value of "percent" is ignored and an indefinite
	// progress bar is drawn.
	void draw_load_screen(const std::wstring &text,
			gui::IGUIEnvironment *guienv, ITextureSource *tsrc,
			float dtime = 0, int percent = 0, float *indef_pos = nullptr);

	void draw_scene(video::SColor skycolor, bool show_hud,
			bool draw_wield_tool, bool draw_crosshair);

	void initialize(Client *client, Hud *hud);
	void finalize();

	bool run()
	{
		return m_device->run();
	}

	void setMenuSkyColor(video::SColor &color);
	void setMenuCloudsColor(video::SColor &color);
	void setMenuStarsEnabled(bool enabled);
	const video::SColor getMenuSkyColor();
	const video::SColor getMenuCloudsColor();
	bool getMenuStarsEnabled();

	void generateMenuStars()
	{
		auto size = getWindowSize();
		float scale_x = (1.0f / size.X);
		float scale_y = (1.0f / size.Y);
		m_menu_stars.clear();

		for (int i = 0; i < 96; i++) {
			float x = rand() % size.X * scale_x;
			float y = rand() % size.Y * scale_y;
			float star_size = (rand() % 8 + 4) * 0.0004f;
			m_menu_stars.push_back(core::vector3d<f32>(x, y, star_size));
		}
	}

	void drawMenuStars(video::IVideoDriver *driver, float dtime)
	{
		static float star_time = 0;
		star_time += dtime;
		if (star_time > M_PI * 2)
			star_time = 0.0f;

		if (!m_menu_stars_enabled)
			return;

		if (driver == nullptr)
			return;

		if (m_menu_stars.empty())
			generateMenuStars();

		auto window_size = getWindowSize();
		for (const auto& star : m_menu_stars)
		{
			// Reconstruct the original star size and position
			s32 x = (s32)(star.X * window_size.X);
			s32 y = (s32)(star.Y * window_size.Y);
			s32 size = (s32)(star.Z * window_size.Y);
			if (size < 1) size = 1;

			float seed = (x * 12.9898f) + (y * 78.233f);
			float brightness = 128 * (std::sin(star_time + seed) / 2 + 0.5f);

			driver->draw2DRectangle(video::SColor(brightness + 64, 255, 255, 255),
									core::rect<s32>(x, y, x + size, y + size));
		}
	}

	// FIXME: this is still global when it shouldn't be
	static ShadowRenderer *get_shadow_renderer()
	{
		if (s_singleton && s_singleton->core)
			return s_singleton->core->get_shadow_renderer();
		return nullptr;
	}
	static std::vector<video::E_DRIVER_TYPE> getSupportedVideoDrivers();

	static void autosaveScreensizeAndCo(
			const core::dimension2d<u32> initial_screen_size,
			const bool initial_window_maximized);

	static PointerType getLastPointerType()
	{
		sanity_check(s_singleton && s_singleton->m_receiver);
		return s_singleton->m_receiver->getLastPointerType();
	}

private:
	video::SColor m_menu_sky_color = video::SColor(255, 140, 186, 250);
	video::SColor m_menu_clouds_color = video::SColor(255, 240, 240, 255);
	static void settingChangedCallback(const std::string &name, void *data);
	v2u32 _getWindowSize() const;

	std::unique_ptr<RenderingCore> core;
	IrrlichtDevice *m_device = nullptr;
	video::IVideoDriver *driver;
	MyEventReceiver *m_receiver = nullptr;
	static RenderingEngine *s_singleton;
	std::vector<core::vector3d<f32>> m_menu_stars;
	bool m_menu_stars_enabled = true;
};
