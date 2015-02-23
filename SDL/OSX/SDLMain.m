/*   SDLMain.m - main entry point for our Cocoa-ized SDL app
 Initial Version: Darrell Walisser <dwaliss1@purdue.edu>
 Non-NIB-Code & other changes: Max Horn <max@quendi.de>

 Customised for SimCoupe by Simon Owen <simon.owen@simcoupe.org>
 */

#import <SDL2/SDL.h>
#import "SDLMain.h"
#include "../UI.h"      // For UE_* user event definitions

extern int SimCoupe_main (int argc_, char* argv_[]);


static NSString *getApplicationName(void)
{
    NSDictionary *dict;
    NSString *appName = 0;

    /* Determine the application name */
    dict = (NSDictionary *)CFBundleGetInfoDictionary(CFBundleGetMainBundle());
    if (dict)
        appName = [dict objectForKey: @"CFBundleName"];

    if (![appName length])
        appName = [[NSProcessInfo processInfo] processName];

    return appName;
}

/* Our NSApplication delegate. */
@implementation SDLMain

static void setupApplicationMenus ()
{
    NSString *appName;
    NSString *title;
    NSMenu *menu;
    NSMenu *subMenu;
    NSMenuItem *item;

    if (NSApp == nil) {
        return;
    }

    /* Create the main menu bar */
    [NSApp setMainMenu:[[NSMenu alloc] init]];

    /* Create the application menu */
    appName = getApplicationName();
    menu = [[NSMenu alloc] initWithTitle:@""];

    title = [@"About " stringByAppendingString:appName];
    [menu addItemWithTitle:title action:@selector(orderFrontStandardAboutPanel:) keyEquivalent:@""];
    [menu addItem:[NSMenuItem separatorItem]];
    [menu addItemWithTitle:@"Preferences…" action:@selector(appPreferences:) keyEquivalent:@","];
    [menu addItem:[NSMenuItem separatorItem]];

    subMenu = [[NSMenu alloc] initWithTitle:@""];
    item = [menu addItemWithTitle:@"Services" action:nil keyEquivalent:@""];
    [item setSubmenu:subMenu];
    [NSApp setServicesMenu:subMenu];
    [subMenu release];

    [menu addItem:[NSMenuItem separatorItem]];

    title = [@"Hide " stringByAppendingString:appName];
    [menu addItemWithTitle:title action:@selector(hide:) keyEquivalent:@"h"];

    item = [menu addItemWithTitle:@"Hide Others" action:@selector(hideOtherApplications:) keyEquivalent:@"h"];
    [item setKeyEquivalentModifierMask:(NSAlternateKeyMask|NSCommandKeyMask)];

    [menu addItemWithTitle:@"Show All" action:@selector(unhideAllApplications:) keyEquivalent:@""];
    [menu addItem:[NSMenuItem separatorItem]];

    title = [@"Quit " stringByAppendingString:appName];
    [menu addItemWithTitle:title action:@selector(terminate:) keyEquivalent:@"q"];

    item = [[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
    [item setSubmenu:menu];
    [[NSApp mainMenu] addItem:item];
    [item release];

    [NSApp setAppleMenu:menu];
    [menu release];


    /* Create the File menu */
    menu = [[NSMenu alloc] initWithTitle:@"File"];

    [menu addItemWithTitle:@"New" action:@selector(appNew:) keyEquivalent:@"n"];
    [menu addItemWithTitle:@"Open" action:@selector(openDocument:) keyEquivalent:@"o"];

    item = [menu addItemWithTitle:@"Open Recent" action:nil keyEquivalent:@""];
    subMenu = [[NSMenu alloc] initWithTitle:@"Open Recent"];
    [subMenu performSelector:@selector(_setMenuName:) withObject:@"NSRecentDocumentsMenu"];
    [menu setSubmenu:subMenu forItem:item];
    [subMenu release];

    [menu addItem:[NSMenuItem separatorItem]];

    [menu addItemWithTitle:@"Save" action:nil keyEquivalent:@"s"];
    [menu addItemWithTitle:@"Save As" action:nil keyEquivalent:@""];

    [menu addItem:[NSMenuItem separatorItem]];
    
    item = [menu addItemWithTitle:@"Import Data" action:@selector(fileImportData:) keyEquivalent:@"i"];
    [item setKeyEquivalentModifierMask:(NSShiftKeyMask|NSCommandKeyMask)];
    item = [menu addItemWithTitle:@"Export Data" action:@selector(fileExportData:) keyEquivalent:@"e"];
    [item setKeyEquivalentModifierMask:(NSShiftKeyMask|NSCommandKeyMask)];
    
    item = [[NSMenuItem alloc] initWithTitle:@"File" action:nil keyEquivalent:@""];
    [item setSubmenu:menu];
    [[NSApp mainMenu] addItem:item];
    [item release];
    [menu release];


    /* Create the View menu */
    menu = [[NSMenu alloc] initWithTitle:@"View"];
    
    item = [menu addItemWithTitle:@"Fullscreen" action:@selector(viewFullscreen:) keyEquivalent:@"f"];
    [menu addItem:[NSMenuItem separatorItem]];
    item = [menu addItemWithTitle:@"Toggle Greyscale" action:@selector(viewGreyscale:) keyEquivalent:@"g"];
    item = [menu addItemWithTitle:@"5:4 Aspect Ratio" action:@selector(viewRatio54:) keyEquivalent:@"5"];
    item = [menu addItemWithTitle:@"TV Scanlines" action:@selector(viewScanlines:) keyEquivalent:@"l"];

    item = [[NSMenuItem alloc] initWithTitle:@"View" action:nil keyEquivalent:@""];
    [item setSubmenu:menu];
    [[NSApp mainMenu] addItem:item];
    [item release];
    [menu release];


    /* Create the System menu */
    menu = [[NSMenu alloc] initWithTitle:@"System"];
    
    item = [menu addItemWithTitle:@"Pause" action:@selector(systemPause:) keyEquivalent:@"p"];
    [menu addItem:[NSMenuItem separatorItem]];
    item = [menu addItemWithTitle:@"Generate NMI" action:@selector(systemNMI:) keyEquivalent:@"n"];
    [item setKeyEquivalentModifierMask:(NSShiftKeyMask|NSCommandKeyMask)];
    item = [menu addItemWithTitle:@"Reset" action:@selector(systemReset:) keyEquivalent:@"r"];
    item = [menu addItemWithTitle:@"Debugger" action:@selector(systemDebugger:) keyEquivalent:@"d"];
    [menu addItem:[NSMenuItem separatorItem]];
    item = [menu addItemWithTitle:@"Mute sound" action:@selector(systemMute:) keyEquivalent:@"m"];
    [item setKeyEquivalentModifierMask:(NSShiftKeyMask|NSCommandKeyMask)];

    item = [[NSMenuItem alloc] initWithTitle:@"System" action:nil keyEquivalent:@""];
    [item setSubmenu:menu];
    [[NSApp mainMenu] addItem:item];
    [item release];
    [menu release];


    /* Create the Window menu */
    menu = [[NSMenu alloc] initWithTitle:@"Window"];

    [menu addItemWithTitle:@"Minimize" action:@selector(performMiniaturize:) keyEquivalent:@"m"];
    [menu addItemWithTitle:@"Zoom" action:@selector(performZoom:) keyEquivalent:@""];

    item = [[NSMenuItem alloc] initWithTitle:@"Window" action:nil keyEquivalent:@""];
    [item setSubmenu:menu];
    [[NSApp mainMenu] addItem:item];
    [item release];

    [NSApp setWindowsMenu:menu];
    [menu release];


    /* Create the Help menu */
    menu = [[NSMenu alloc] initWithTitle:@"Help"];

    item = [menu addItemWithTitle:@"SimCoupe Help" action:@selector(helpHelp:) keyEquivalent:@""];
    item = [menu addItemWithTitle:@"View Changelog" action:@selector(helpChangeLog:) keyEquivalent:@""];
    [menu addItem:[NSMenuItem separatorItem]];
    item = [menu addItemWithTitle:@"SimCoupe Homepage" action:@selector(helpHomepage:) keyEquivalent:@""];

    item = [[NSMenuItem alloc] initWithTitle:@"Help" action:nil keyEquivalent:@""];
    [item setSubmenu:menu];
    [[NSApp mainMenu] addItem:item];
    [item release];
    [menu release];
}


/* Generate an SDL quite event for graceful app termination */
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender
{
    if (SDL_GetEventState(SDL_QUIT) == SDL_ENABLE) {
        SDL_Event event;
        event.type = SDL_QUIT;
        SDL_PushEvent(&event);
    }

    return NSTerminateCancel;
}

/* Pass dropped files through to SDL for processing, and ultimately on to SimCoupe */
- (BOOL)application:(NSApplication *)theApplication openFile:(NSString *)filename
{
    if (SDL_GetEventState(SDL_DROPFILE) == SDL_ENABLE) {
        SDL_Event event;
        event.type = SDL_DROPFILE;
        event.drop.file = SDL_strdup([filename UTF8String]);
        return (SDL_PushEvent(&event) > 0);
    }

    return NO;
}


/* Called when the internal event loop has just started running */
- (void) applicationDidFinishLaunching: (NSNotification *) note
{
    /* Hand off to main application code */
    int status = SimCoupe_main(gArgc, gArgv);

    /* We're done, thank you for playing */
    exit(status);
}

//////////////////////////////////////////////////////////////////////////////


// Internal function used to launch a file from the app bundle's resources folder.
-(void) openResourceFile:( NSString *) fileName
{
    NSString *myBundle = [[NSBundle mainBundle] bundlePath];
    NSString *fullpath = [NSString stringWithFormat:@"%@/Contents/Resources/%@",myBundle, fileName];
    [[NSWorkspace sharedWorkspace] openFile:fullpath ];
}


static void sendUserEventData (int event, void *data)
{
    SDL_Event e = {0};
    e.user.type = SDL_USEREVENT;
    e.user.code = event;
    e.user.data1 = data;
    SDL_PushEvent(&e);
}

static void sendUserEvent (int event)
{
    sendUserEventData(event, NULL);
}


- (IBAction)appPreferences:(id)sender { sendUserEvent(UE_OPTIONS); }
- (IBAction)systemPause:(id)sender { sendUserEvent(UE_PAUSE); }
- (IBAction)systemNMI:(id)sender { sendUserEvent(UE_NMIBUTTON); }
- (IBAction)systemReset:(id)sender { sendUserEvent(UE_RESETBUTTON); }
- (IBAction)systemDebugger:(id)sender { sendUserEvent(UE_DEBUGGER); }
- (IBAction)systemMute:(id)sender { sendUserEvent(UE_TOGGLEMUTE); }
- (IBAction)viewFullscreen:(id)sender { sendUserEvent(UE_TOGGLEFULLSCREEN); }
- (IBAction)viewFrameSync:(id)sender { sendUserEvent(UE_TOGGLESYNC); }
- (IBAction)viewGreyscale:(id)sender { sendUserEvent(UE_TOGGLEGREYSCALE); }
- (IBAction)viewScanlines:(id)sender { sendUserEvent(UE_TOGGLESCANLINES); }
- (IBAction)viewRatio54:(id)sender { sendUserEvent(UE_TOGGLE54); }
- (IBAction)fileImportData:(id)sender { sendUserEvent(UE_IMPORTDATA); }
- (IBAction)fileExportData:(id)sender { sendUserEvent(UE_EXPORTDATA); }


- (IBAction)openDocument:(id)sender
{
    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
    NSString *documentsDirectory = [paths objectAtIndex:0];
    NSArray *fileTypes = [NSArray arrayWithObjects:@"dsk", @"mgt", @"sad", @"sbt", nil];
    NSString *path = nil;

    NSOpenPanel *openPanel = [NSOpenPanel openPanel];
    [openPanel setDirectoryURL:[NSURL fileURLWithPath:documentsDirectory]];
    [openPanel setAllowedFileTypes:fileTypes];
    if ([openPanel runModal])
        path = [[[openPanel URLs] objectAtIndex:0] path];

    if (path == nil)
        return;

    sendUserEventData(UE_OPENFILE, strdup([path UTF8String]));

    [[NSDocumentController sharedDocumentController] noteNewRecentDocumentURL:[NSURL fileURLWithPath:path]];
}

// Help -> SimCoupe Help
- (IBAction)helpHelp:(id)sender
{
    [self openResourceFile:@"SimCoupe.rtf"];
}

// Help -> View ChangeLog
- (IBAction)helpChangeLog:(id)sender
{
    [self openResourceFile:@"ChangeLog.txt"];
}

// Help -> Homepage
- (IBAction)helpHomepage:(id)sender
{
    [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:@"http://simcoupe.org/"]];
}

@end


/* Replacement for NSApplicationMain */
static int CustomApplicationMain (int argc, char **argv)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    /* Ensure the application object is initialised */
    [NSApplication sharedApplication];

    CPSProcessSerNum PSN;
    /* Tell the dock about us */
    if (!CPSGetCurrentProcess(&PSN))
        if (!CPSEnableForegroundOperation(&PSN,0x03,0x3C,0x2C,0x1103))
            if (!CPSSetFrontProcess(&PSN))
                [NSApplication sharedApplication];

    /* Set up the menubar */
    setupApplicationMenus();

    /* Create SDLMain and make it the app delegate */
    SDLMain *sdlMain = [[SDLMain alloc] init];
    [NSApp setDelegate:sdlMain];

    /* Start the main event loop */
    [NSApp run];

    [sdlMain release];
    [pool release];

    return 0;
}


#undef main

/* Main entry point to executable - should *not* be SDL_main! */
int main (int argc, char **argv)
{
    /* Copy the arguments into a global variable */
    /* This is passed if we are launched by double-clicking */
    if ( argc >= 2 && strncmp (argv[1], "-psn", 4) == 0 ) {
        gArgv = (char **) malloc(sizeof (char *) * 2);
        gArgv[0] = argv[0];
        gArgv[1] = NULL;
        gArgc = 1;
        gFinderLaunch = YES;
    } else {
        int i;
        gArgc = argc;
        gArgv = (char **) malloc(sizeof (char *) * (argc+1));
        for (i = 0; i <= argc; i++)
            gArgv[i] = argv[i];
        gFinderLaunch = NO;
    }

    return CustomApplicationMain(argc, argv);
}
