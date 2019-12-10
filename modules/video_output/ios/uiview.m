/*****************************************************************************
 * uiview.m: iOS UIView vout window provider
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

/**
 * @file uiview.m
 * @brief UIView implementation as a vout_window provider
 *
 * This UIView window provider mostly handle resizing constraints from upper
 * views and provides event forwarding to VLC. It is usable for any kind of
 * subview and in particular can be used to implement a CAEAGLLayer in a
 * vlc_gl_t provider as well as a CAMetalLayer.
 *
 * It also handles the installation of the View inside the host application
 * and makes sure no duplicated views are done with the host view.
 */

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
#import "opengl/vout_helper.h"

@interface VLCVideoUIView : UIView {
    vlc_mutex_t _mutex;

    BOOL _appActive;

    UIView *_viewContainer;
    UITapGestureRecognizer *_tapRecognizer;

    /* Written from MT, read locked from vout */
    CGSize _viewSize;
    CGFloat _scaleFactor;
}

struct vout_window_sys {
    VLCCALayerVideoView *view;
    UIView *parent;
};

/*****************************************************************************
 * Our UIView object
 *****************************************************************************/
@implementation VLCCALayerVideoView

// 
+ (void)getNewView:(NSArray *)value
{
    id *ret = [[value objectAtIndex:0] pointerValue];
    *ret = [[self alloc] initWithFrame:CGRectMake(0.,0.,320.,240.)];
}

- (id)initWithFrame:(CGRect)frame
{
    _appActive = ([UIApplication sharedApplication].applicationState == UIApplicationStateActive);
    if (unlikely(!_appActive))
        return nil;

    self = [super initWithFrame:frame];
    if (!self)
        return nil;

    vlc_mutex_init(&_mutex);
    vlc_cond_init(&_gl_attached_wait);

    /* The window is controlled by the host application through the UIView
     * sizing mechanisms. */
    self.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;

    if (![self fetchViewContainer])
    {
        [self release];
        return nil;
    }

    return self;
}

- (BOOL)fetchViewContainer
{
    @try {
        /* get the object we will draw into */
        UIView *viewContainer = var_InheritAddress (_voutDisplay, "drawable-nsobject");
        if (unlikely(viewContainer == nil)) {
            msg_Err(_voutDisplay, "provided view container is nil");
            return NO;
        }

        if (unlikely(![viewContainer respondsToSelector:@selector(isKindOfClass:)])) {
            msg_Err(_voutDisplay, "void pointer not an ObjC object");
            return NO;
        }

        [viewContainer retain];

        if (![viewContainer isKindOfClass:[UIView class]]) {
            msg_Err(_voutDisplay, "passed ObjC object not of class UIView");
            return NO;
        }

        /* This will be released in Close(), on
         * main thread, after we are done using it. */
        _viewContainer = viewContainer;

        self.frame = viewContainer.bounds;
        [self reshape];

        [_viewContainer addSubview:self];

        /* add tap gesture recognizer for DVD menus and stuff */
        _tapRecognizer = [[UITapGestureRecognizer alloc] initWithTarget:self
                                                                 action:@selector(tapRecognized:)];
        if (_viewContainer.window
         && _viewContainer.window.rootViewController
         && _viewContainer.window.rootViewController.view)
            [_viewContainer.superview addGestureRecognizer:_tapRecognizer];
        _tapRecognizer.cancelsTouchesInView = NO;
        return YES;
    } @catch (NSException *exception) {
        msg_Err(_voutDisplay, "Handling the view container failed due to an Obj-C exception (%s, %s", [exception.name UTF8String], [exception.reason UTF8String]);
        vout_display_sys_t *sys = _voutDisplay->sys;
        if (_tapRecognizer)
            [_tapRecognizer release];
        return NO;
    }
}

- (void)cleanAndReleaseFromMainThread
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];

    [_tapRecognizer.view removeGestureRecognizer:_tapRecognizer];
    [_tapRecognizer release];

    [self removeFromSuperview];
    [_viewContainer release];

    [self release];
}

- (void)cleanAndRelease:(BOOL)flushed
{
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
}

- (void)layoutSubviews
{
    [self reshape];
}

- (void)reshape
{
    assert([NSThread isMainThread]);
    assert(_wnd);

    vlc_mutex_lock(&_mutex);
    _viewSize = [self bounds].size;
    _scaleFactor = self.contentScaleFactor;

    vout_window_ReportSize(_wnd,
            _viewSize.width * _scaleFactor,
            _viewSize.height * _scaleFactor);

    vlc_mutex_unlock(&_mutex);
}

- (void)tapRecognized:(UITapGestureRecognizer *)tapRecognizer
{
    vlc_mutex_lock(&_mutex);

    UIGestureRecognizerState state = [tapRecognizer state];
    CGPoint touchPoint = [tapRecognizer locationInView:self];
    CGFloat scaleFactor = self.contentScaleFactor;

    vout_window_ReportMouseMoved(_wnd,
            (int)touchPoint.x * scaleFactor, (int)touchPoint.y * scaleFactor);
    vout_window_ReportMousePressed(_wnd, MOUSE_BUTTON_LEFT);
    vout_window_ReportMouseReleased(_wnd, MOUSE_BUTTON_LEFT);

    vlc_mutex_unlock(&_mutex);
}


- (void)updateConstraints
{
    [super updateConstraints];
    [self reshape];
}

- (BOOL)isOpaque
{
    return YES;
}

- (BOOL)acceptsFirstResponder
{
    return YES;
}

static int Enable(vout_window_t *wnd)
{
    VLCVideoUIView *sys = wnd->sys;

    return VLC_SUCCESS;
}

static void Disable(vout_window_t *wnd)
{
    VLCVideoUIView *sys = wnd->sys;
}

static void Close(vout_window_t *wnd)
{
    VLCVideoUIView *sys = wnd->sys;

    free(sys);
}

static int Open(vout_window_t *wnd)
{
    struct vout_window_sys *sys = malloc(sizeof *sys);
    if (sys == NULL)
        return VLC_EGENERIC;

    var_Create(vlc_object_parent(vd), "ios-eaglcontext", VLC_VAR_ADDRESS);

    @autoreleasepool {
        /* setup the actual OpenGL ES view */

        [VLCVideoUIView performSelectorOnMainThread:@selector(getNewView:)
                                         withObject:[NSArray arrayWithObjects:
                                                    [NSValue valueWithPointer:&sys->glESView],
                                                    [NSValue valueWithPointer:wnd], nil]
                                      waitUntilDone:YES];
        if (!sys->glESView) {
            msg_Err(vd, "Creating OpenGL ES 2 view failed");
            var_Destroy(vlc_object_parent(vd), "ios-eaglcontext");
            goto error;
        }
    }


    wnd->sys = sys;

    wnd->enable  = Enable;
    wnd->disable = Disable;
    wnd->resize  = Resize;
    wnd->destroy = Close;

    wnd->handle.nsobject = view;
    wnd->type = VOUT_WINDOW_TYPE_NSOBJECT;

    return VLC_SUCCESS;

error:
    free(sys);

    return VLC_EGENERIC;
}


@end
