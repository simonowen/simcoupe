/*   SDLMain.m - main entry point for our Cocoa-ized SDL app
       Initial Version: Darrell Walisser <dwaliss1@purdue.edu>
       Non-NIB-Code & other changes: Max Horn <max@quendi.de>

    Feel free to customize this file to suit your needs
*/

#import <Cocoa/Cocoa.h>
#import "Options.h"

@interface SDLMain : NSObject
{
	NSString * currentFile;
	Options * options;
}

- (IBAction)appPreferences:(id)sender;
- (IBAction)fileOpen:(id)sender;
- (IBAction)viewFullscreen:(id)sender;
- (IBAction)viewFrameSync:(id)sender;
- (IBAction)viewGreyscale:(id)sender;
- (IBAction)viewScanlines:(id)sender;
- (IBAction)viewRatio54:(id)sender;
- (IBAction)systemNMI:(id)sender;
- (IBAction)systemReset:(id)sender;
- (IBAction)systemDebugger:(id)sender;
- (IBAction)systemMute:(id)sender;
- (IBAction)helpHelp:(id)sender;
- (IBAction)helpHomepage:(id)sender;

- (void)openResourceFile:( NSString *) fileName;

@end
