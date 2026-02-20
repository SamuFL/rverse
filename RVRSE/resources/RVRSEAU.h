
#include <TargetConditionals.h>
#if TARGET_OS_IOS == 1 || TARGET_OS_VISION == 1
#import <UIKit/UIKit.h>
#else
#import <Cocoa/Cocoa.h>
#endif

#define IPLUG_AUVIEWCONTROLLER IPlugAUViewController_vRVRSE
#define IPLUG_AUAUDIOUNIT IPlugAUAudioUnit_vRVRSE
#import <RVRSEAU/IPlugAUViewController.h>
#import <RVRSEAU/IPlugAUAudioUnit.h>

//! Project version number for RVRSEAU.
FOUNDATION_EXPORT double RVRSEAUVersionNumber;

//! Project version string for RVRSEAU.
FOUNDATION_EXPORT const unsigned char RVRSEAUVersionString[];

@class IPlugAUViewController_vRVRSE;
