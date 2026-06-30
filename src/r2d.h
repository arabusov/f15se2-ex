#ifndef R2D_H
#define R2D_H

/*
 * 2D overlay renderer seam (see docs/render-2d-overlay.md).
 *
 * The 2D overlay (HUD, sprites, text, menus, cockpit) is submitted to the *same*
 * renderer as the in-flight 3D path (r3d.h) — there is no separate 2D backend.
 *
 * Step 1 (this file's current scope) confines only the present/compose boundary
 * behind the renderer: the game still draws into the 320x200 paletted page
 * surfaces (gfx_impl.c), and this seam owns *how that virtual overlay page reaches
 * the window*. The software backend presents it through SDL_Renderer (letterbox);
 * the GL backend composites it as a flat textured quad over the GL 3D
 * (r3dgl_present). The active 2D backend always matches the active 3D backend:
 * the GL backend owns the window, so the two must agree.
 *
 * Later steps move the page model -> image/primitive submission + a hidden back
 * buffer and add virtual-vs-real resolution + oversized submission for widescreen
 * (docs/render-2d-overlay.md, Steps 2-7).
 */

struct SDL_Surface;

/*
 * Virtual-vs-real resolution (docs/render-2d-overlay.md, Step 2).
 *
 * 2D is authored in a virtual coordinate box (320x200 in flight, 640x350 for
 * the hi-res title). R2DMapping is the uniform-scale + centring placement of
 * that box onto the real window (letterbox/pillarbox bars on the unused axis).
 *
 * r2d_computeMapping is the SINGLE source of truth for the virtual->window
 * mapping: the software present, the GL 2D composite and the GL 3D viewport all
 * derive their placement from it, so the HUD, the 3D and the present quad stay
 * aligned and nothing stretches on non-1.6 displays.
 *
 * Submission contract: 2D is authored inside the virtual box, but a submission
 * MAY fall outside it (oversized, reserved for future widescreen 2D art) — out-
 * of-box pixels are clipped to the window, NOT to the virtual box. No stock call
 * site relies on this; the stock UI never leaves the box.
 */
typedef struct {
    int   virtW, virtH;  /* virtual (authoring) resolution */
    int   winW, winH;    /* real window framebuffer size in pixels */
    float scale;         /* window pixels per virtual pixel (uniform) */
    int   offX, offY;    /* top-left of the centred virtual box, window pixels */
} R2DMapping;

/* Fit a virtW x virtH virtual box into a winW x winH window preserving aspect
 * (uniform scale, centred). The one place the letterbox math lives. */
void r2d_computeMapping(int virtW, int virtH, int winW, int winH, R2DMapping *out);

/*
 * Image submission API (docs/render-2d-overlay.md, Step 3).
 *
 * An R2DImage is an owned 2D image (sprite sheet, decoded PIC, captured screen
 * region) the renderer can sample. Today the software realization is an INDEX8
 * SDL_Surface and drawing is a direct clipped blit (no projection); a later GPU
 * backend realizes images as textures and draws them as ortho quads. Callers
 * submit *images and rects*, never "textured quad" — the realization is the
 * backend's (docs/render-2d-overlay.md, "one renderer, 2D submission path").
 */
typedef struct R2DImage R2DImage;

/* Create a blank w x h image (cleared to index 0). NULL on failure. */
R2DImage *r2d_registerImage(int w, int h);

/* Create a w x h image holding a snapshot of the (x,y,w,h) region of `src`.
 * Used for the cockpit/popup save-restore (capture a screen region, draw it back
 * later) — the page->page copy of the page-model era. NULL on failure. */
R2DImage *r2d_captureImage(struct SDL_Surface *src, int x, int y, int w, int h);

/* The image's backing surface, for code that fills it directly (the PIC decoder
 * decodes into this). NULL if img is NULL. */
struct SDL_Surface *r2d_imageSurface(R2DImage *img);

/* Draw the (srcX,srcY,w,h) sub-rect of `img` into `dst` at (dstX,dstY). If
 * `key` >= 0, source pixels equal to it are skipped (transparency, conventionally
 * 0); if `key` < 0 the copy is opaque. Clipped to both surfaces. */
void r2d_drawImage(R2DImage *img, int srcX, int srcY, int w, int h,
                   struct SDL_Surface *dst, int dstX, int dstY, int key);

/* Release an image. Safe on NULL. */
void r2d_releaseImage(R2DImage *img);

/* The one clipped INDEX8 blit underlying every 2D image/page copy: copy a w x h
 * rect from src(srcX,srcY) to dst(dstX,dstY). key >= 0 skips matching source
 * pixels (sprite transparency); key < 0 is an opaque copy. Clipped to both
 * surfaces (negative offsets included). Operates directly on surfaces so the
 * page->page copies (copyRect/blitToCurrent) and the image draws share it. */
void r2d_blit(struct SDL_Surface *src, int srcX, int srcY,
              struct SDL_Surface *dst, int dstX, int dstY,
              int w, int h, int key);

/*
 * Native 2D vector layer (docs/render-2d-overlay.md, Step 4).
 *
 * The HUD/MFD vector primitives (lines, pitch-ladder, symbology) are *submitted*
 * to the renderer rather than rasterized into the 320x200 page. The software
 * backend realizes a submission by rasterizing into the page (the low-end/DOS
 * path, pixel-identical to before, via the registered callbacks below). The GL
 * backend records the submission in 320-space and replays it at the **native
 * window resolution** over the composited frame, with a line width relative to
 * the native resolution — a crisp vector HUD instead of an upscaled low-res
 * image. Same call sites, the realization is the backend's.
 */
typedef struct {
    short x1, y1, x2, y2; /* absolute 320-space, already clipped to the page */
    unsigned char color;  /* VGA palette index */
    unsigned char kind;   /* R2D_PRIM_LINE / R2D_PRIM_POINT */
} R2DVectorPrim;
#define R2D_PRIM_LINE  0
#define R2D_PRIM_POINT 1

/* Marks the start of a GL flight frame's 2D overlay (called from the 3D backend
 * once the main 3D view begins). Only between this and the present do 2D
 * submissions record for native replay; pure-2D screens (no 3D pass) rasterize
 * into the page as before. */
void r2d_vectorBeginFrame(void);

/* Whether 2D primitive submissions should be recorded for native replay (a GL
 * flight frame, unless disabled via F15_VECTOR2D=0) rather than rasterized into
 * the page. The gfx submission points branch on this. */
int r2d_vectorActive(void);

/* Submit a clipped 2D line / point in absolute 320-space with a palette colour
 * index. Records for native replay when r2d_vectorActive(), else hands off to
 * the registered software rasterizer. */
void r2d_submitLine(int x1, int y1, int x2, int y2, int color);
void r2d_submitPoint(int x, int y, int color);

/* Force the next line/point submissions to rasterize into the page (the software
 * path) instead of recording for the GL native-on-top replay. Set around an
 * in-flight MFD region (the radar scope) whose lines must compose *under* their
 * blip sprites in submission order — the native vector layer always draws last,
 * so its radar lines would otherwise land over the icons. No effect in software
 * (it already rasterizes). Bracket the region: set 1 before, 0 after. */
void r2d_setForceRaster(int on);

/* The software backend (gfx_impl.c) registers how it rasterizes a submitted
 * line/point into the page, so r2d need not own page state. */
void r2d_registerSoftwarePrims(void (*line)(int x1, int y1, int x2, int y2, int color),
                               void (*point)(int x, int y, int color));

/* The recorded vector primitives for the current frame, for the GL backend to
 * replay. Count via *count. */
const R2DVectorPrim *r2d_vectorPrims(int *count);

/* Called by the backend after replaying the layer at present: clears it so each
 * frame composes a fresh layer (a frame that submits no lines — e.g. after a
 * view change — then correctly shows none, with no stale carry-over). */
void r2d_vectorMarkPresented(void);

/* Present the virtual overlay page (its w/h are the virtual size) to the window
 * through the active backend. shakeOffset is the explosion screen-shake in
 * virtual pixels (0-3), applied as a horizontal present offset (Step 6 moves it
 * to a scene shake). */
void r2d_present(struct SDL_Surface *page, int shakeOffset);

/* Name of the active 2D backend ("opengl1" or "software"); follows the 3D
 * backend. */
const char *r2d_backendName(void);

/* The software path's SDL_Renderer present lives in gfx_impl.c (it owns the
 * renderer); gfx_impl registers it here at video init so r2d_present can dispatch
 * to it without the renderer leaking into this module. */
void r2d_registerSoftwarePresent(void (*present)(struct SDL_Surface *page, int shakeOffset));

#endif /* R2D_H */
