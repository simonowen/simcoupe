/*   SDLMain.m - main entry point for our Cocoa-ized SDL app
       Initial Version: Darrell Walisser <dwaliss1@purdue.edu>
       Non-NIB-Code & other changes: Max Horn <max@quendi.de>

 Customised for SimCoupe by Simon Owen <simon@simonowen.com>
*/

#import <Cocoa/Cocoa.h>

/* Portions of CPS.h */
typedef struct CPSProcessSerNum
{
    UInt32 lo;
    UInt32 hi;
} CPSProcessSerNum;

extern OSErr CPSGetCurrentProcess( CPSProcessSerNum *psn);
extern OSErr CPSEnableForegroundOperation( CPSProcessSerNum *psn, UInt32 _arg2, UInt32 _arg3, UInt32 _arg4, UInt32 _arg5);
extern OSErr CPSSetFrontProcess( CPSProcessSerNum *psn);

static int gArgc;
static char **gArgv;
static BOOL gFinderLaunch;
static BOOL gFinishedLaunching;

@interface SDLMain : NSObject<NSApplicationDelegate>
- (NSApplicationTerminateReply) applicationShouldTerminate:(NSApplication *)sender;
- (void) setupWorkingDirectory:(BOOL)shouldChdir;
- (BOOL) application:(NSApplication *)theApplication openFile:(NSString *)filename;
- (void) applicationDidFinishLaunching: (NSNotification *) note;
@end
