#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include <SDL.h>
#ifdef WIN32
#undef main /* avoid SDL's nasty SDLmain hack */
#endif
#include <SDL_opengl.h>
#include <bass.h>
#include <stdio.h>
#include <stdarg.h>
#include <math.h>

#if defined(__APPLE__) && defined(__MACH__)
#include <GLKit/GLKMatrix4.h>
#define gluPerspective(f, a, zn, zf) glMultMatrixf(GLKMatrix4MakePerspective((f) * M_PI / 180, a, zn, zf).m)
#define gluLookAt(ex, ey, ez, cx, cy, cz, ux, uy, uz) glMultMatrixf(GLKMatrix4MakeLookAt(ex, ey, ez, cx, cy, cz, ux, uy, uz).m)
#endif

#include "../lib/sync.h"

static const float bpm = 150.0f; /* beats per minute */
static const int rpb = 8; /* rows per beat */
static const double row_rate = (double(bpm) / 60) * rpb;

static double bass_get_row(HSTREAM h)
{
	QWORD pos = BASS_ChannelGetPosition(h, BASS_POS_BYTE);
	double time = BASS_ChannelBytes2Seconds(h, pos);
	return time * row_rate;
}

#ifndef SYNC_PLAYER

static void bass_pause(void *d, int flag)
{
	HSTREAM h = *((HSTREAM *)d);
	if (flag)
		BASS_ChannelPause(h);
	else
		BASS_ChannelPlay(h, false);
}

static void bass_set_row(void *d, int row)
{
	HSTREAM h = *((HSTREAM *)d);
	QWORD pos = BASS_ChannelSeconds2Bytes(h, row / row_rate);
	BASS_ChannelSetPosition(h, pos, BASS_POS_BYTE);
}

static int bass_is_playing(void *d)
{
	HSTREAM h = *((HSTREAM *)d);
	return BASS_ChannelIsActive(h) == BASS_ACTIVE_PLAYING;
}

static struct sync_cb bass_cb = {
	bass_pause,
	bass_set_row,
	bass_is_playing
};

#endif /* !defined(SYNC_PLAYER) */

static void die(const char *fmt, ...)
{
	char temp[4096];
	va_list va;
	va_start(va, fmt);
	vsnprintf(temp, sizeof(temp), fmt, va);
	va_end(va);

#if !defined(_WIN32) || defined(_CONSOLE)
	fprintf(stderr, "*** error: %s\n", temp);
#else
	MessageBox(NULL, temp, NULL, MB_OK | MB_ICONERROR);
#endif

	exit(EXIT_FAILURE);
}

static const unsigned int width  = 800;
static const unsigned int height = 600;

void setup_sdl()
{
	if (SDL_Init(SDL_INIT_VIDEO))
		die("%s", SDL_GetError());

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
	SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, 1);

	if (!SDL_SetVideoMode(width, height, 32, SDL_OPENGL))
		die("%s", SDL_GetError());
}

void draw_cube()
{
	glBegin(GL_QUADS);

	// Front Face
	glColor3ub(255, 0, 0);
	glVertex3f(-1.0f, -1.0f,  1.0f);
	glVertex3f( 1.0f, -1.0f,  1.0f);
	glVertex3f( 1.0f,  1.0f,  1.0f);
	glVertex3f(-1.0f,  1.0f,  1.0f);

	// Back Face
	glColor3ub(0, 255, 0);
	glVertex3f(-1.0f, -1.0f, -1.0f);
	glVertex3f(-1.0f,  1.0f, -1.0f);
	glVertex3f( 1.0f,  1.0f, -1.0f);
	glVertex3f( 1.0f, -1.0f, -1.0f);

	// Top Face
	glColor3ub(0, 0, 255);
	glVertex3f(-1.0f,  1.0f, -1.0f);
	glVertex3f(-1.0f,  1.0f,  1.0f);
	glVertex3f( 1.0f,  1.0f,  1.0f);
	glVertex3f( 1.0f,  1.0f, -1.0f);

	// Bottom Face
	glColor3ub(255, 255, 0);
	glVertex3f(-1.0f, -1.0f, -1.0f);
	glVertex3f( 1.0f, -1.0f, -1.0f);
	glVertex3f( 1.0f, -1.0f,  1.0f);
	glVertex3f(-1.0f, -1.0f,  1.0f);

	// Right face
	glColor3ub(255, 0, 255);
	glVertex3f( 1.0f, -1.0f, -1.0f);
	glVertex3f( 1.0f,  1.0f, -1.0f);
	glVertex3f( 1.0f,  1.0f,  1.0f);
	glVertex3f( 1.0f, -1.0f,  1.0f);

	// Left Face
	glColor3ub(255, 255, 255);
	glVertex3f(-1.0f, -1.0f, -1.0f);
	glVertex3f(-1.0f, -1.0f,  1.0f);
	glVertex3f(-1.0f,  1.0f,  1.0f);
	glVertex3f(-1.0f,  1.0f, -1.0f);

	glEnd();
}

int main(int argc, char *argv[])
{
	HSTREAM stream;

	const struct sync_track *clear_r, *clear_g, *clear_b;
	const struct sync_track *cam_rot, *cam_dist;

	setup_sdl();

	/* init BASS */
	if (!BASS_Init(-1, 44100, 0, 0, 0))
		die("failed to init bass");
	stream = BASS_StreamCreateFile(false, "tune.ogg", 0, 0,
	    BASS_STREAM_PRESCAN);
	if (!stream)
		die("failed to open tune");

	sync_device *rocket = sync_create_device("sync");
	if (!rocket)
		die("out of memory?");

#ifndef SYNC_PLAYER
	if (sync_connect(rocket, "localhost", SYNC_DEFAULT_PORT))
		die("failed to connect to host");
#endif

	/* get tracks */
	clear_r = sync_get_track(rocket, "clear.r");
	clear_g = sync_get_track(rocket, "clear.g");
	clear_b = sync_get_track(rocket, "clear.b");
	cam_rot = sync_get_track(rocket, "cam.rot"),
	cam_dist = sync_get_track(rocket, "cam.dist");

	/* let's roll! */
	BASS_Start();
	BASS_ChannelPlay(stream, false);

	bool done = false;
	while (!done) {
		double row = bass_get_row(stream);
#ifndef SYNC_PLAYER
		if (sync_update(rocket, (int)floor(row), &bass_cb, (void *)&stream))
			sync_connect(rocket, "localhost", SYNC_DEFAULT_PORT);
#endif

		/* draw */

		glClearColor(float(sync_get_val(clear_r, row)),
		             float(sync_get_val(clear_g, row)),
		             float(sync_get_val(clear_b, row)), 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		float rot = float(sync_get_val(cam_rot, row));
		float dist = float(sync_get_val(cam_dist, row));

		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		gluPerspective(60.0f, 4.0f / 3, 0.1f, 100.0f);

		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glPushMatrix();
		gluLookAt(sin(rot) * dist, 0, cos(rot) * dist,
		          0, 0, 0,
		          0, 1, 0);

		glEnable(GL_DEPTH_TEST);
		draw_cube();

		glPopMatrix();
		SDL_GL_SwapBuffers();

		BASS_Update(0); /* decrease the chance of missing vsync */

		SDL_Event e;
		while (SDL_PollEvent(&e)) {
			if (e.type == SDL_QUIT ||
			    (e.type == SDL_KEYDOWN &&
			    e.key.keysym.sym == SDLK_ESCAPE))
				done = true;
		}
	}

#ifndef SYNC_PLAYER
	sync_save_tracks(rocket);
#endif
	sync_destroy_device(rocket);

	BASS_StreamFree(stream);
	BASS_Free();
	SDL_Quit();

	return 0;
}
