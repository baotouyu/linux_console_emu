/*
 * SDL 1.2 video driver for Linux DRM dumb buffers.
 * FBCON is registered first, so /dev/fb0 is used when it can be opened.
 * This driver runs only when framebuffer is absent and /dev/dri/cardN opens.
 */
#include "SDL_config.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <drm/drm.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_mode.h>

#include "SDL_video.h"
#include "SDL_mouse.h"
#include "../SDL_sysvideo.h"
#include "../SDL_pixels_c.h"

#undef _THIS
#define _THIS SDL_VideoDevice *this

#define DRM_MODE_CONNECTED 1

struct SDL_PrivateVideoData {
	int fd;
	int master;
	uint32_t crtc_id;
	uint32_t connector_id;
	uint32_t fb_id;
	uint32_t handle;
	uint32_t dumb_pitch;
	uint64_t dumb_size;
	int screen_w;
	int screen_h;
	uint8_t *map;
	struct drm_mode_modeinfo mode;
	uint8_t *buffer;
	int width;
	int height;
	int pitch;
	int bpp;
};

static int DRM_Available(void);
static SDL_VideoDevice *DRM_CreateDevice(int devindex);
static void DRM_DeleteDevice(SDL_VideoDevice *device);
static int DRM_VideoInit(_THIS, SDL_PixelFormat *vformat);
static SDL_Rect **DRM_ListModes(_THIS, SDL_PixelFormat *format, Uint32 flags);
static SDL_Surface *DRM_SetVideoMode(_THIS, SDL_Surface *current, int width, int height, int bpp, Uint32 flags);
static int DRM_SetColors(_THIS, int firstcolor, int ncolors, SDL_Color *colors);
static void DRM_UpdateRects(_THIS, int numrects, SDL_Rect *rects);
static int DRM_FlipHWSurface(_THIS, SDL_Surface *surface);
static void DRM_VideoQuit(_THIS);
static int DRM_AllocHWSurface(_THIS, SDL_Surface *surface);
static void DRM_FreeHWSurface(_THIS, SDL_Surface *surface);
static int DRM_LockHWSurface(_THIS, SDL_Surface *surface);
static void DRM_UnlockHWSurface(_THIS, SDL_Surface *surface);
static void DRM_InitOSKeymap(_THIS);
static void DRM_PumpEvents(_THIS);

static void present(_THIS);

static int drm_open(void)
{
	const char *path;
	int fd;

	path = SDL_getenv("SDL_DRMDEV");
	if (path == NULL || path[0] == '\0') {
		path = "/dev/dri/card0";
	}
	fd = open(path, O_RDWR | O_CLOEXEC);
	return fd;
}

static int DRM_Available(void)
{
	int fd = drm_open();

	if (fd < 0) {
		return 0;
	}
	close(fd);
	return 1;
}

static void DRM_DeleteDevice(SDL_VideoDevice *device)
{
	if (device == NULL) {
		return;
	}
	SDL_free(device->hidden);
	SDL_free(device);
}

static SDL_VideoDevice *DRM_CreateDevice(int devindex)
{
	SDL_VideoDevice *this;

	(void)devindex;
	this = (SDL_VideoDevice *)SDL_malloc(sizeof(*this));
	if (this == NULL) {
		SDL_OutOfMemory();
		return NULL;
	}
	SDL_memset(this, 0, sizeof(*this));
	this->hidden = (struct SDL_PrivateVideoData *)SDL_malloc(sizeof(*this->hidden));
	if (this->hidden == NULL) {
		SDL_free(this);
		SDL_OutOfMemory();
		return NULL;
	}
	SDL_memset(this->hidden, 0, sizeof(*this->hidden));
	this->hidden->fd = -1;

	this->VideoInit = DRM_VideoInit;
	this->ListModes = DRM_ListModes;
	this->SetVideoMode = DRM_SetVideoMode;
	this->SetColors = DRM_SetColors;
	this->UpdateRects = DRM_UpdateRects;
	this->VideoQuit = DRM_VideoQuit;
	this->AllocHWSurface = DRM_AllocHWSurface;
	this->LockHWSurface = DRM_LockHWSurface;
	this->UnlockHWSurface = DRM_UnlockHWSurface;
	this->FlipHWSurface = DRM_FlipHWSurface;
	this->FreeHWSurface = DRM_FreeHWSurface;
	this->InitOSKeymap = DRM_InitOSKeymap;
	this->PumpEvents = DRM_PumpEvents;
	this->free = DRM_DeleteDevice;
	return this;
}

VideoBootStrap DRM_bootstrap = {
	"drm", "Linux DRM dumb buffer",
	DRM_Available, DRM_CreateDevice
};

static int pick_crtc(uint32_t possible, uint32_t *crtcs, uint32_t count, uint32_t current)
{
	uint32_t i;

	if (current != 0) {
		for (i = 0; i < count; i++) {
			if (crtcs[i] == current) {
				return (int)current;
			}
		}
	}
	for (i = 0; i < count; i++) {
		if (possible & (1u << i)) {
			return (int)crtcs[i];
		}
	}
	return -1;
}

static int DRM_VideoInit(_THIS, SDL_PixelFormat *vformat)
{
	struct drm_mode_card_res res;
	struct drm_mode_get_connector conn;
	struct drm_mode_get_encoder enc;
	struct drm_mode_create_dumb dumb;
	struct drm_mode_map_dumb map;
	struct drm_mode_fb_cmd2 fb;
	struct drm_mode_crtc crtc;
	struct drm_mode_modeinfo *modes = NULL;
	uint32_t *connectors = NULL;
	uint32_t *crtcs = NULL;
	uint32_t *encoders = NULL;
	uint32_t connector_id = 0;
	uint32_t encoder_id = 0;
	int crtc_id = -1;
	int chosen = -1;
	uint32_t i, m;
	int fd;

	fd = drm_open();
	if (fd < 0) {
		SDL_SetError("Unable to open /dev/dri/card0: %s", strerror(errno));
		return -1;
	}
	this->hidden->fd = fd;

	if (ioctl(fd, DRM_IOCTL_SET_MASTER, 0) < 0) {
		SDL_SetError("Unable to become DRM master: %s", strerror(errno));
		DRM_VideoQuit(this);
		return -1;
	}
	this->hidden->master = 1;

	SDL_memset(&res, 0, sizeof(res));
	if (ioctl(fd, DRM_IOCTL_MODE_GETRESOURCES, &res) < 0) {
		SDL_SetError("DRM_IOCTL_MODE_GETRESOURCES failed: %s", strerror(errno));
		DRM_VideoQuit(this);
		return -1;
	}
	connectors = SDL_calloc(res.count_connectors ? res.count_connectors : 1, sizeof(*connectors));
	crtcs = SDL_calloc(res.count_crtcs ? res.count_crtcs : 1, sizeof(*crtcs));
	if (connectors == NULL || crtcs == NULL) {
		SDL_free(connectors);
		SDL_free(crtcs);
		SDL_OutOfMemory();
		DRM_VideoQuit(this);
		return -1;
	}
	res.connector_id_ptr = (uintptr_t)connectors;
	res.crtc_id_ptr = (uintptr_t)crtcs;
	if (ioctl(fd, DRM_IOCTL_MODE_GETRESOURCES, &res) < 0) {
		SDL_free(connectors);
		SDL_free(crtcs);
		SDL_SetError("DRM_IOCTL_MODE_GETRESOURCES failed: %s", strerror(errno));
		DRM_VideoQuit(this);
		return -1;
	}

	for (i = 0; i < res.count_connectors; i++) {
		SDL_memset(&conn, 0, sizeof(conn));
		conn.connector_id = connectors[i];
		if (ioctl(fd, DRM_IOCTL_MODE_GETCONNECTOR, &conn) < 0) {
			continue;
		}
		if (conn.connection != DRM_MODE_CONNECTED || conn.count_modes == 0) {
			continue;
		}
		modes = SDL_calloc(conn.count_modes, sizeof(*modes));
		encoders = SDL_calloc(conn.count_encoders ? conn.count_encoders : 1, sizeof(*encoders));
		if (modes == NULL || encoders == NULL) {
			SDL_free(modes);
			SDL_free(encoders);
			SDL_free(connectors);
			SDL_free(crtcs);
			SDL_OutOfMemory();
			DRM_VideoQuit(this);
			return -1;
		}
		SDL_memset(&conn, 0, sizeof(conn));
		conn.connector_id = connectors[i];
		conn.count_modes = 0;
		/* refill counts */
		if (ioctl(fd, DRM_IOCTL_MODE_GETCONNECTOR, &conn) < 0) {
			break;
		}
		conn.modes_ptr = (uintptr_t)modes;
		conn.encoders_ptr = (uintptr_t)encoders;
		if (ioctl(fd, DRM_IOCTL_MODE_GETCONNECTOR, &conn) < 0) {
			break;
		}
		connector_id = connectors[i];
		encoder_id = conn.encoder_id;
		if (encoder_id == 0 && conn.count_encoders > 0) {
			encoder_id = encoders[0];
		}
		for (m = 0; m < conn.count_modes; m++) {
			if (modes[m].type & DRM_MODE_TYPE_PREFERRED) {
				chosen = (int)m;
				break;
			}
		}
		if (chosen < 0) {
			chosen = 0;
		}
		break;
	}

	if (connector_id == 0 || chosen < 0) {
		SDL_free(modes);
		SDL_free(encoders);
		SDL_free(connectors);
		SDL_free(crtcs);
		SDL_SetError("No connected DRM display");
		DRM_VideoQuit(this);
		return -1;
	}

	SDL_memset(&enc, 0, sizeof(enc));
	enc.encoder_id = encoder_id;
	if (ioctl(fd, DRM_IOCTL_MODE_GETENCODER, &enc) < 0) {
		SDL_free(modes);
		SDL_free(encoders);
		SDL_free(connectors);
		SDL_free(crtcs);
		SDL_SetError("DRM_IOCTL_MODE_GETENCODER failed: %s", strerror(errno));
		DRM_VideoQuit(this);
		return -1;
	}
	crtc_id = pick_crtc(enc.possible_crtcs, crtcs, res.count_crtcs, enc.crtc_id);
	SDL_free(encoders);
	SDL_free(connectors);
	SDL_free(crtcs);
	if (crtc_id < 0) {
		SDL_free(modes);
		SDL_SetError("No DRM CRTC for the connected display");
		DRM_VideoQuit(this);
		return -1;
	}

	this->hidden->mode = modes[chosen];
	this->hidden->screen_w = modes[chosen].hdisplay;
	this->hidden->screen_h = modes[chosen].vdisplay;
	this->hidden->crtc_id = (uint32_t)crtc_id;
	this->hidden->connector_id = connector_id;
	SDL_free(modes);

	SDL_memset(&dumb, 0, sizeof(dumb));
	dumb.width = (uint32_t)this->hidden->screen_w;
	dumb.height = (uint32_t)this->hidden->screen_h;
	dumb.bpp = 32;
	if (ioctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &dumb) < 0) {
		SDL_SetError("DRM_IOCTL_MODE_CREATE_DUMB failed: %s", strerror(errno));
		DRM_VideoQuit(this);
		return -1;
	}
	this->hidden->handle = dumb.handle;
	this->hidden->dumb_pitch = dumb.pitch;
	this->hidden->dumb_size = dumb.size;

	SDL_memset(&fb, 0, sizeof(fb));
	fb.width = dumb.width;
	fb.height = dumb.height;
	fb.pixel_format = DRM_FORMAT_XRGB8888;
	fb.handles[0] = dumb.handle;
	fb.pitches[0] = dumb.pitch;
	if (ioctl(fd, DRM_IOCTL_MODE_ADDFB2, &fb) < 0) {
		SDL_SetError("DRM_IOCTL_MODE_ADDFB2 failed: %s", strerror(errno));
		DRM_VideoQuit(this);
		return -1;
	}
	this->hidden->fb_id = fb.fb_id;

	SDL_memset(&map, 0, sizeof(map));
	map.handle = dumb.handle;
	if (ioctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &map) < 0) {
		SDL_SetError("DRM_IOCTL_MODE_MAP_DUMB failed: %s", strerror(errno));
		DRM_VideoQuit(this);
		return -1;
	}
	this->hidden->map = mmap(NULL, dumb.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, (off_t)map.offset);
	if (this->hidden->map == MAP_FAILED) {
		this->hidden->map = NULL;
		SDL_SetError("mmap DRM buffer failed: %s", strerror(errno));
		DRM_VideoQuit(this);
		return -1;
	}
	SDL_memset(this->hidden->map, 0, dumb.size);

	SDL_memset(&crtc, 0, sizeof(crtc));
	crtc.crtc_id = this->hidden->crtc_id;
	crtc.fb_id = this->hidden->fb_id;
	crtc.set_connectors_ptr = (uintptr_t)&this->hidden->connector_id;
	crtc.count_connectors = 1;
	crtc.mode = this->hidden->mode;
	crtc.mode_valid = 1;
	if (ioctl(fd, DRM_IOCTL_MODE_SETCRTC, &crtc) < 0) {
		SDL_SetError("DRM_IOCTL_MODE_SETCRTC failed: %s", strerror(errno));
		DRM_VideoQuit(this);
		return -1;
	}

	vformat->BitsPerPixel = 16;
	vformat->BytesPerPixel = 2;
	fprintf(stderr, "sdl drm %dx%d\n", this->hidden->screen_w, this->hidden->screen_h);
	return 0;
}

static SDL_Rect **DRM_ListModes(_THIS, SDL_PixelFormat *format, Uint32 flags)
{
	(void)this;
	(void)format;
	(void)flags;
	return (SDL_Rect **)-1;
}

static void pixel_rgb(const uint8_t *src, int bpp, int *r, int *g, int *b)
{
	if (bpp == 16) {
		uint16_t p;
		SDL_memcpy(&p, src, 2);
		*r = ((p >> 11) & 31) * 255 / 31;
		*g = ((p >> 5) & 63) * 255 / 63;
		*b = (p & 31) * 255 / 31;
		return;
	}
	*b = src[0];
	*g = src[1];
	*r = src[2];
}

static void present(_THIS)
{
	int x, y;
	int sw = this->hidden->width;
	int sh = this->hidden->height;
	int dw = this->hidden->screen_w;
	int dh = this->hidden->screen_h;
	int bpp = this->hidden->bpp;
	int sp = this->hidden->pitch;
	int dp = (int)this->hidden->dumb_pitch;

	if (this->hidden->map == NULL || this->hidden->buffer == NULL || sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0) {
		return;
	}
	for (y = 0; y < dh; y++) {
		int sy = y * sh / dh;
		uint8_t *dst = this->hidden->map + (size_t)y * (size_t)dp;
		const uint8_t *row = this->hidden->buffer + (size_t)sy * (size_t)sp;
		for (x = 0; x < dw; x++) {
			int sx = x * sw / dw;
			int r, g, b;
			uint32_t pixel;
			pixel_rgb(row + sx * (bpp / 8), bpp, &r, &g, &b);
			pixel = ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
			SDL_memcpy(dst + x * 4, &pixel, 4);
		}
	}
}

static SDL_Surface *DRM_SetVideoMode(_THIS, SDL_Surface *current, int width, int height, int bpp, Uint32 flags)
{
	Uint32 rmask, gmask, bmask, amask;

	if (bpp != 16 && bpp != 32) {
		SDL_SetError("DRM backend supports 16 or 32 bits per pixel");
		return NULL;
	}
	if (width <= 0 || height <= 0) {
		SDL_SetError("Invalid video mode");
		return NULL;
	}

	SDL_free(this->hidden->buffer);
	this->hidden->buffer = SDL_malloc((size_t)width * (size_t)height * (bpp / 8));
	if (this->hidden->buffer == NULL) {
		SDL_OutOfMemory();
		return NULL;
	}
	SDL_memset(this->hidden->buffer, 0, (size_t)width * (size_t)height * (bpp / 8));

	if (bpp == 16) {
		rmask = 0xF800;
		gmask = 0x07E0;
		bmask = 0x001F;
		amask = 0;
	} else {
		rmask = 0x00FF0000;
		gmask = 0x0000FF00;
		bmask = 0x000000FF;
		amask = 0;
	}
	if (!SDL_ReallocFormat(current, bpp, rmask, gmask, bmask, amask)) {
		SDL_free(this->hidden->buffer);
		this->hidden->buffer = NULL;
		SDL_SetError("Couldn't allocate pixel format");
		return NULL;
	}

	current->flags = flags & SDL_FULLSCREEN;
	this->hidden->width = current->w = width;
	this->hidden->height = current->h = height;
	this->hidden->bpp = bpp;
	this->hidden->pitch = current->pitch = width * (bpp / 8);
	current->pixels = this->hidden->buffer;
	present(this);
	return current;
}

static int DRM_SetColors(_THIS, int firstcolor, int ncolors, SDL_Color *colors)
{
	(void)this;
	(void)firstcolor;
	(void)ncolors;
	(void)colors;
	return 1;
}

static void DRM_UpdateRects(_THIS, int numrects, SDL_Rect *rects)
{
	(void)numrects;
	(void)rects;
	present(this);
}

static int DRM_FlipHWSurface(_THIS, SDL_Surface *surface)
{
	(void)surface;
	present(this);
	return 0;
}

static void DRM_VideoQuit(_THIS)
{
	int fd = this->hidden->fd;
	uint32_t fb_id = this->hidden->fb_id;
	struct drm_mode_destroy_dumb dumb;

	if (this->hidden->map != NULL && this->hidden->dumb_size > 0) {
		munmap(this->hidden->map, this->hidden->dumb_size);
		this->hidden->map = NULL;
	}
	if (fd >= 0 && fb_id != 0) {
		ioctl(fd, DRM_IOCTL_MODE_RMFB, &fb_id);
		this->hidden->fb_id = 0;
	}
	if (fd >= 0 && this->hidden->handle != 0) {
		SDL_memset(&dumb, 0, sizeof(dumb));
		dumb.handle = this->hidden->handle;
		ioctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dumb);
		this->hidden->handle = 0;
	}
	SDL_free(this->hidden->buffer);
	this->hidden->buffer = NULL;
	if (fd >= 0 && this->hidden->master) {
		ioctl(fd, DRM_IOCTL_DROP_MASTER, 0);
		this->hidden->master = 0;
	}
	if (fd >= 0) {
		close(fd);
		this->hidden->fd = -1;
	}
}

static int DRM_AllocHWSurface(_THIS, SDL_Surface *surface)
{
	(void)this;
	(void)surface;
	return -1;
}

static void DRM_FreeHWSurface(_THIS, SDL_Surface *surface)
{
	(void)this;
	(void)surface;
}

static int DRM_LockHWSurface(_THIS, SDL_Surface *surface)
{
	(void)this;
	(void)surface;
	return 0;
}

static void DRM_UnlockHWSurface(_THIS, SDL_Surface *surface)
{
	(void)this;
	(void)surface;
}

static void DRM_InitOSKeymap(_THIS)
{
	(void)this;
}

static void DRM_PumpEvents(_THIS)
{
	(void)this;
}
