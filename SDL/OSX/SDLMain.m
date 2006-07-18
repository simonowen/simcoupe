/*   SDLMain.m - main entry point for our Cocoa-ized SDL app
       Initial Version: Darrell Walisser <dwaliss1@purdue.edu>
       Non-NIB-Code & other changes: Max Horn <max@quendi.de>

    Feel free to customize this file to suit your needs
*/

#import "SDL.h"
#import "SDLMain.h"
#import <sys/param.h> /* for MAXPATHLEN */
#import <unistd.h>
#include "../UI.h"

static BOOL isFullyLaunched = NO;


/* A helper category for NSString */
@interface NSString (ReplaceSubString)
- (NSString *)stringByReplacingRange:(NSRange)aRange with:(NSString *)aString;
@end

@interface SDLApplication : NSApplication
@end

@implementation SDLApplication

// Invoked from the Quit menu item
- (void)terminate:(id)sender
{
    // Post an SDL_QUIT event
    SDL_Event event = {0};
    event.type = SDL_QUIT;
    SDL_PushEvent(&event);
}

- (void)sendEvent:(NSEvent *)anEvent
{
	if (NSKeyDown == [anEvent type] || NSKeyUp == [anEvent type])
	{
		int len = [[anEvent characters] length], i;

		for (i = 0; i < len; i++)
		{
			// Eat events for the function keys, as SDL processes them
			if ([[anEvent characters] characterAtIndex:i] >= NSF1FunctionKey &&
				[[anEvent characters] characterAtIndex:i] <= NSF12FunctionKey)
			return;
		}
		if ([anEvent modifierFlags] & NSCommandKeyMask)
			[super sendEvent: anEvent];
	}
	else
		[super sendEvent: anEvent];
}

@end


// The main class of the application, the application's delegate
@implementation SDLMain

// Fix menu to contain the real app name instead of "SDL App"
- (void)fixMenu:(NSMenu *)aMenu withAppName:(NSString *)appName
{
    NSRange aRange;
    NSEnumerator *enumerator;
    NSMenuItem *menuItem;

    aRange = [[aMenu title] rangeOfString:@"SDL App"];
    if (aRange.length != 0)
        [aMenu setTitle: [[aMenu title] stringByReplacingRange:aRange with:appName]];

    enumerator = [[aMenu itemArray] objectEnumerator];
    while ((menuItem = [enumerator nextObject]))
    {
        aRange = [[menuItem title] rangeOfString:@"SDL App"];
        if (aRange.length != 0)
            [menuItem setTitle: [[menuItem title] stringByReplacingRange:aRange with:appName]];
        if ([menuItem hasSubmenu])
            [self fixMenu:[menuItem submenu] withAppName:appName];
    }
    [ aMenu sizeToFit ];
}

// Called when the internal event loop has just started running
- (void) applicationDidFinishLaunching: (NSNotification *) note
{
	isFullyLaunched = YES;
	int argc = 0;
	char *argv[2];

	argv[argc++] = strdup([ [[NSBundle mainBundle] bundlePath] cString ]);

	if (currentFile)
	{
		argv[argc++] = strdup([currentFile cString ]);
		[[NSDocumentController sharedDocumentController] noteNewRecentDocumentURL:[NSURL fileURLWithPath:currentFile]];
	}

    // Set the main menu to contain the real app name instead of "SDL App"
    [self fixMenu:[NSApp mainMenu] withAppName:[[NSProcessInfo processInfo] processName]];

    // Hand off to main application code
    int status = SDL_main (argc, argv), i;

	for (i = 0 ; i < argc; i++)
		free(argv[i]);

    exit(status);
}

// called when user opens a file through the Finder
- (BOOL)application:(NSApplication *)theApplication
           openFile:(NSString *)filename
{
	if ( YES == isFullyLaunched )
	{
		// Send an event to the SDL code telling it to load the specified file.
		SDL_Event event = {0};
		event.user.type = SDL_USEREVENT;
		event.user.code = UE_OPENFILE;
		event.user.data1 = strdup([ filename cString ]);
		SDL_PushEvent(&event);
	}
	else // Can't launch until emulator is ready, keep hold of the filename though
	{
		currentFile = [NSString stringWithString:filename ];
	}

    return YES;
}


- (IBAction)fileOpen:(id)sender
{
    NSString *path = nil;
    NSOpenPanel *openPanel = [ NSOpenPanel openPanel ];
    NSString * dirPath = @"/home";
    if ( [ openPanel runModalForDirectory:dirPath
             file:@"SavedGame" types:nil ] ) 
	{
        path = [ [ openPanel filenames ] objectAtIndex:0 ];
    }
	else
	{
		return;
	}
	
	currentFile = [NSString stringWithString:path ];

	SDL_Event event = {0};
	event.user.type = SDL_USEREVENT;
	event.user.code = UE_OPENFILE;
	event.user.data1 = strdup([ path cString ]);
	SDL_PushEvent(&event);
	
	[[NSDocumentController sharedDocumentController] noteNewRecentDocumentURL:[NSURL fileURLWithPath:currentFile]];
}

- (IBAction)appPreferences:(id)sender
{
	SDL_Event event = {0};
	event.user.type = SDL_USEREVENT;
	event.user.code = UE_OPTIONS;
	SDL_PushEvent(&event);
/*
	if (options != nil)
		[options release];

	options = [Options alloc];
	[NSBundle loadNibNamed:@"Options" owner:options];
*/
}

- (IBAction)systemPause:(id)sender
{
	SDL_Event event = {0};
	event.user.type = SDL_USEREVENT;
	event.user.code = UE_PAUSE;
	SDL_PushEvent(&event);
}

- (IBAction)systemNMI:(id)sender
{
	SDL_Event event = {0};
	event.user.type = SDL_USEREVENT;
	event.user.code = UE_NMIBUTTON;
	SDL_PushEvent(&event);
}

- (IBAction)systemReset:(id)sender
{
	SDL_Event event = {0};
	event.user.type = SDL_USEREVENT;
	event.user.code = UE_RESETBUTTON;
	SDL_PushEvent(&event);
}

- (IBAction)systemDebugger:(id)sender
{
	SDL_Event event = {0};
	event.user.type = SDL_USEREVENT;
	event.user.code = UE_DEBUGGER;
	SDL_PushEvent(&event);
}

- (IBAction)systemMute:(id)sender
{
	SDL_Event event = {0};
	event.user.type = SDL_USEREVENT;
	event.user.code = UE_TOGGLEMUTE;
	SDL_PushEvent(&event);
}


- (IBAction)viewFullscreen:(id)sender
{
	SDL_Event event = {0};
	event.user.type = SDL_USEREVENT;
	event.user.code = UE_TOGGLEFULLSCREEN;
	SDL_PushEvent(&event);
}

- (IBAction)viewFrameSync:(id)sender
{
	SDL_Event event = {0};
	event.user.type = SDL_USEREVENT;
	event.user.code = UE_TOGGLESYNC;
	SDL_PushEvent(&event);
}

- (IBAction)viewGreyscale:(id)sender
{
	SDL_Event event = {0};
	event.user.type = SDL_USEREVENT;
	event.user.code = UE_TOGGLEGREYSCALE;
	SDL_PushEvent(&event);
}

- (IBAction)viewScanlines:(id)sender
{
	SDL_Event event = {0};
	event.user.type = SDL_USEREVENT;
	event.user.code = UE_TOGGLESCANLINES;
	SDL_PushEvent(&event);
}

- (IBAction)viewRatio54:(id)sender
{
	SDL_Event event = {0};
	event.user.type = SDL_USEREVENT;
	event.user.code = UE_TOGGLE54;
	SDL_PushEvent(&event);
}


// Help -> SimCoupe Help
- (IBAction)helpHelp:(id)sender
{
	[self openResourceFile:@"ReadMe.txt"];
}

// Help -> View ChangeLog
- (IBAction)helpChangeLog:(id)sender
{
	[self openResourceFile:@"ChangeLog.txt"];
}

// Help -> Homepage
- (IBAction)helpHomepage:(id)sender
{
	[[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:@"http://www.simcoupe.org/"]];
}


// Internal function used to launch a file from the app bundle's resources folder.
-(void) openResourceFile:( NSString *) fileName
{
	NSString *myBundle = [[NSBundle mainBundle] bundlePath];
	NSString *fullpath = [NSString stringWithFormat:@"%@/Contents/Resources/%@",myBundle, fileName];
	[[NSWorkspace sharedWorkspace] openFile:fullpath ];
}

@end


@implementation NSString (ReplaceSubString)

- (NSString *)stringByReplacingRange:(NSRange)aRange with:(NSString *)aString
{
    unsigned int bufferSize;
    unsigned int selfLen = [self length];
    unsigned int aStringLen = [aString length];
    unichar *buffer;
    NSRange localRange;
    NSString *result;

    bufferSize = selfLen + aStringLen - aRange.length;
    buffer = NSAllocateMemoryPages(bufferSize*sizeof(unichar));
    
    /* Get first part into buffer */
    localRange.location = 0;
    localRange.length = aRange.location;
    [self getCharacters:buffer range:localRange];
    
    /* Get middle part into buffer */
    localRange.location = 0;
    localRange.length = aStringLen;
    [aString getCharacters:(buffer+aRange.location) range:localRange];
     
    /* Get last part into buffer */
    localRange.location = aRange.location + aRange.length;
    localRange.length = selfLen - localRange.location;
    [self getCharacters:(buffer+aRange.location+aStringLen) range:localRange];
    
    /* Build output string */
    result = [NSString stringWithCharacters:buffer length:bufferSize];
    
    NSDeallocateMemoryPages(buffer, bufferSize);
    
    return result;
}

@end


#ifdef main
#  undef main
#endif

// Main entry point to executable - should *not* be SDL_main!
int main (int argc, char **argv)
{
	// enable automatic handling of Cmd+M etc. keys
	setenv ("SDL_ENABLEAPPEVENTS", "1", 1);

    [SDLApplication poseAsClass:[NSApplication class]];
    NSApplicationMain (argc, (const char **)argv);

    return 0;
}
