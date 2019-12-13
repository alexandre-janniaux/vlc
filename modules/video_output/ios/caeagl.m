/*****************************************************************************
 * caeagl.m: iOS OpenGL ES provider through CAEAGLLayer
 *****************************************************************************
 * Copyright (C) 2001-2017 VLC authors and VideoLAN
 * Copyright (C) 2019 Videolabs
 *
 * Authors: Pierre d'Herbemont <pdherbemont at videolan dot org>
 *          Felix Paul Kühne <fkuehne at videolan dot org>
 *          David Fuhrmann <david dot fuhrmann at googlemail dot com>
 *          Rémi Denis-Courmont
 *          Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
 *          Eric Petit <titer@m0k.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#import <UIKit/UIKit.h>
#import <OpenGLES/EAGL.h>
#import <OpenGLES/ES2/gl.h>
#import <OpenGLES/ES2/glext.h>
#import <QuartzCore/QuartzCore.h>
#import <dlfcn.h>

#ifdef HAVE_CONFIG_H
# import "config.h"
#endif

#import <vlc_common.h>
#import <vlc_plugin.h>
#import <vlc_vout_display.h>
#import <vlc_opengl.h>
#import <vlc_dialog.h>
#import "../opengl/vout_helper.h"


static void *OurGetProcAddress(vlc_gl_t *, const char *);

static int GLESMakeCurrent(vlc_gl_t *);
static void GLESSwap(vlc_gl_t *);
static void GLESReleaseCurrent(vlc_gl_t *);

@interface VLCOpenGLES2VideoView : UIView {
    vlc_gl_t *_gl;

    EAGLContext *_eaglContext;
    CAEAGLLayer *_layer;

    vlc_mutex_t _mutex;
    vlc_cond_t  _gl_attached_wait;
    BOOL        _gl_attached;

    BOOL _bufferNeedReset;
    BOOL _appActive;
    BOOL _eaglEnabled;

    UIView *_viewContainer;

    GLuint _renderBuffer;
    GLuint _frameBuffer;
}

- (id)initWithFrame:(CGRect)frame gl:(vlc_gl_t*)gl;
- (void)cleanAndRelease:(BOOL)flushed;
- (BOOL)makeCurrent;
- (void)releaseCurrent;
- (void)presentRenderbuffer;

@end

/*****************************************************************************
 * Our UIView object
 *****************************************************************************/
@implementation VLCOpenGLES2VideoView

+ (Class)layerClass
{
    return [CAEAGLLayer class];
}

+ (void)getNewView:(NSValue *)value
{

    VLCOpenGLES2VideoView *view = [self alloc];

    vlc_gl_t *gl     = [value pointerValue];
    gl->sys = [view initWithFrame:CGRectMake(0.,0.,320.,240.) gl:gl];
}

- (id)initWithFrame:(CGRect)frame gl:(vlc_gl_t*)gl
{
    _gl = gl;

    _appActive = ([UIApplication sharedApplication].applicationState == UIApplicationStateActive);
    if (unlikely(!_appActive))
        return nil;

    self = [super initWithFrame:frame];
    if (!self)
        return nil;

    _eaglEnabled = YES;
    _bufferNeedReset = YES;

    vlc_mutex_init(&_mutex);
    vlc_cond_init(&_gl_attached_wait);
    _gl_attached = YES;

    /* the following creates a new OpenGL ES context with the API version we
     * need if there is already an active context created by another OpenGL
     * provider we cache it and restore analog to the
     * makeCurrent/releaseCurrent pattern used through-out the class */
    EAGLContext *previousEaglContext = [EAGLContext currentContext];

    _eaglContext = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];

    if (unlikely(!_eaglContext)
     || unlikely(![EAGLContext setCurrentContext:_eaglContext]))
    {
        [_eaglContext release];
        [self release];
        return nil;
    }
    [self releaseCurrent];

    _layer = (CAEAGLLayer *)self.layer;
    _layer.drawableProperties = [NSDictionary dictionaryWithObject:kEAGLColorFormatRGBA8 forKey: kEAGLDrawablePropertyColorFormat];
    _layer.opaque = YES;

    self.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;

    if (![self bindToWindow: _gl->surface])
    {
        [_eaglContext release];
        [self release];
        return nil;
    }

    /* */
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(applicationStateChanged:)
                                                 name:UIApplicationWillResignActiveNotification
                                               object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(applicationStateChanged:)
                                                 name:UIApplicationDidBecomeActiveNotification
                                               object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(applicationStateChanged:)
                                                 name:UIApplicationDidEnterBackgroundNotification
                                               object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(applicationStateChanged:)
                                                 name:UIApplicationWillEnterForegroundNotification
                                               object:nil];

    return self;
}

- (BOOL)bindToWindow:(vout_window_t*)wnd
{
    @try {
        UIView *viewContainer = wnd->handle.nsobject;
        /* get the object we will draw into */
        if (unlikely(viewContainer == nil)) {
            msg_Err(_gl, "provided view container is nil");
            return NO;
        }

        if (unlikely(![viewContainer respondsToSelector:@selector(isKindOfClass:)])) {
            msg_Err(_gl, "void pointer not an ObjC object");
            return NO;
        }

        [viewContainer retain];

        if (![viewContainer isKindOfClass:[UIView class]]) {
            msg_Err(_gl, "passed ObjC object not of class UIView");
            return NO;
        }

        /* This will be released in Close(), on
         * main thread, after we are done using it. */
        _viewContainer = viewContainer;

        self.frame = viewContainer.bounds;

        [_viewContainer addSubview:self];

        return YES;
    } @catch (NSException *exception) {
        msg_Err(_gl, "Handling the view container failed due to an Obj-C exception (%s, %s", [exception.name UTF8String], [exception.reason UTF8String]);
        return NO;
    }
}

- (void)cleanAndReleaseFromMainThread
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];

    [self removeFromSuperview];
    [_viewContainer release];

    assert(!_gl_attached);
    [_eaglContext release];
    [self release];
}

- (void)cleanAndRelease:(BOOL)flushed
{
    // TODO: is needed ?
    vlc_mutex_lock(&_mutex);
    if (_eaglEnabled && !flushed)
        [self flushEAGLLocked];
    _eaglEnabled = NO;
    vlc_mutex_unlock(&_mutex);

    [self performSelectorOnMainThread:@selector(cleanAndReleaseFromMainThread)
                           withObject:nil
                        waitUntilDone:NO];
}

- (void)dealloc
{
    vlc_mutex_destroy(&_mutex);
    vlc_cond_destroy(&_gl_attached_wait);
    [super dealloc];
}

- (void)didMoveToWindow
{
    self.contentScaleFactor = self.window.screen.scale;

    vlc_mutex_lock(&_mutex);
    _bufferNeedReset = YES;
    vlc_mutex_unlock(&_mutex);
}

- (BOOL)doResetBuffers:(vlc_gl_t *)gl
{
    VLCOpenGLES2VideoView *view = gl->sys;

    if (_frameBuffer != 0)
    {
        /* clear frame buffer */
        glDeleteFramebuffers(1, &_frameBuffer);
        _frameBuffer = 0;
    }

    if (_renderBuffer != 0)
    {
        /* clear render buffer */
        glDeleteRenderbuffers(1, &_renderBuffer);
        _renderBuffer = 0;
    }

    glDisable(GL_DEPTH_TEST);

    glGenFramebuffers(1, &_frameBuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, _frameBuffer);

    glGenRenderbuffers(1, &_renderBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, _renderBuffer);

    [_eaglContext renderbufferStorage:GL_RENDERBUFFER fromDrawable:_layer];

    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, _renderBuffer);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        msg_Err(_gl, "Failed to make complete framebuffer object %x", glCheckFramebufferStatus(GL_FRAMEBUFFER));
        return NO;
    }
    return YES;
}

- (BOOL)makeCurrent
{
    vlc_mutex_lock(&_mutex);
    assert(!_gl_attached);

    VLCOpenGLES2VideoView *view = _gl->sys;

    if (unlikely(!_appActive))
    {
        vlc_mutex_unlock(&_mutex);
        return NO;
    }

    assert(_eaglEnabled);
    //*previousEaglContext = [EAGLContext currentContext];

    if (![EAGLContext setCurrentContext:_eaglContext])
    {
        vlc_mutex_unlock(&_mutex);
        return NO;
    }

    BOOL resetBuffers = NO;


    if (unlikely(_bufferNeedReset))
    {
        _bufferNeedReset = NO;
        resetBuffers = YES;
    }

    _gl_attached = YES;

    vlc_mutex_unlock(&_mutex);

    if (resetBuffers && ![self doResetBuffers:_gl])
    {
        [self releaseCurrent];
        return NO;
    }
    return YES;
}

- (void)releaseCurrent
{
    vlc_mutex_lock(&_mutex);
    assert(_gl_attached);
    _gl_attached = NO;
    [EAGLContext setCurrentContext:nil];
    vlc_mutex_unlock(&_mutex);
    vlc_cond_signal(&_gl_attached_wait);
}

- (void)presentRenderbuffer
{
    [_eaglContext presentRenderbuffer:GL_RENDERBUFFER];
}

- (void)layoutSubviews
{
    vlc_mutex_lock(&_mutex);
    _bufferNeedReset = YES;
    vlc_mutex_unlock(&_mutex);
}

- (void)flushEAGLLocked
{
    assert(_eaglEnabled);

    /* Ensure that all previously submitted commands are drained from the
     * command buffer and are executed by OpenGL ES before moving to the
     * background.*/
    EAGLContext *previousEaglContext = [EAGLContext currentContext];
    if ([EAGLContext setCurrentContext:_eaglContext])
        glFinish();
    [EAGLContext setCurrentContext:previousEaglContext];
}

- (void)applicationStateChanged:(NSNotification *)notification
{
    vlc_mutex_lock(&_mutex);

    if ([[notification name] isEqualToString:UIApplicationWillResignActiveNotification])
        _appActive = NO;
    else if ([[notification name] isEqualToString:UIApplicationDidEnterBackgroundNotification])
    {
        _appActive = NO;

        /* Wait for the vout to unlock the eagl context before releasing
         * it. */
        while (_gl_attached && _eaglEnabled)
            vlc_cond_wait(&_gl_attached_wait, &_mutex);

        /* _eaglEnabled can change during the vlc_cond_wait
         * as the mutex is unlocked during that, so this check
         * has to be done after the vlc_cond_wait! */
        if (_eaglEnabled) {
            [self flushEAGLLocked];
            _eaglEnabled = NO;
        }
    }
    else if ([[notification name] isEqualToString:UIApplicationWillEnterForegroundNotification])
        _eaglEnabled = YES;
    else
    {
        assert([[notification name] isEqualToString:UIApplicationDidBecomeActiveNotification]);
        _appActive = YES;
    }

    vlc_mutex_unlock(&_mutex);
}

- (void)updateConstraints
{
    [super updateConstraints];
}

- (BOOL)isOpaque
{
    return YES;
}

- (UIView *)hitTest:(CGPoint)point withEvent:(UIEvent *)event
{
    /* Disable events for this view, as the vout_window view will be the one
     * handling them. */
    return nil;
}

- (void)resize:(CGSize)size
{
    /* HOW TO RESIZE ? */
}

@end

/*****************************************************************************
 * vout opengl callbacks
 *****************************************************************************/
static void *GetSymbol(vlc_gl_t *gl, const char *name)
{
    VLC_UNUSED(gl);

    return dlsym(RTLD_DEFAULT, name);
}

static int MakeCurrent(vlc_gl_t *gl)
{
    VLCOpenGLES2VideoView *view = gl->sys;

    if (![view makeCurrent])
        return VLC_EGENERIC;
    return VLC_SUCCESS;
}

static void ReleaseCurrent(vlc_gl_t *gl)
{
    VLCOpenGLES2VideoView *view = gl->sys;
    [view releaseCurrent];
}

static void Swap(vlc_gl_t *gl)
{
    VLCOpenGLES2VideoView *view = gl->sys;
    [view presentRenderbuffer];
}

static void Resize(vlc_gl_t *gl, unsigned width, unsigned height)
{
    VLCOpenGLES2VideoView *view = gl->sys;
    [view resize:CGSizeMake(width, height)];
}

static void Close(vlc_gl_t *gl)
{
    VLCOpenGLES2VideoView *view = gl->sys;
    [view cleanAndRelease:YES];
}

static int Open(vlc_gl_t *gl, unsigned width, unsigned height)
{
    vout_window_t *wnd = gl->surface;
    if (wnd->type != VOUT_WINDOW_TYPE_NSOBJECT)
        return VLC_EGENERIC;

   @autoreleasepool {
        /* setup the actual OpenGL ES view */

        [VLCOpenGLES2VideoView performSelectorOnMainThread:@selector(getNewView:)
                                                withObject:[NSValue valueWithPointer:gl]
                                             waitUntilDone:YES];
        if (gl->sys == NULL)
        {
            msg_Err(gl, "Creating OpenGL ES 2 view failed");
            return VLC_EGENERIC;
        }
    }

    const vlc_fourcc_t *subpicture_chromas;

    gl->makeCurrent = MakeCurrent;
    gl->releaseCurrent = ReleaseCurrent;
    gl->resize = Resize;
    gl->swap = Swap;
    gl->getProcAddress = GetSymbol;
    gl->destroy = Close;

    return VLC_SUCCESS;
}

vlc_module_begin ()
    set_shortname (N_("CAEAGL"))
    set_description (N_("CAEAGL provider for OpenGL"))
    set_category (CAT_VIDEO)
    set_subcategory (SUBCAT_VIDEO_VOUT)
    set_capability ("opengl es2", 50)
    set_callback(Open)
    add_shortcut ("caeagl")
vlc_module_end ()

