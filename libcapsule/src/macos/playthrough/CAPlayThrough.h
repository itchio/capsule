/*
File: CAPlayThrough.h 
Abstract: CAPlayThough Classes. 
Version: 1.2.2 

Disclaimer: IMPORTANT:  This Apple software is supplied to you by Apple 
Inc. ("Apple") in consideration of your agreement to the following 
terms, and your use, installation, modification or redistribution of 
this Apple software constitutes acceptance of these terms.  If you do 
not agree with these terms, please do not use, install, modify or 
redistribute this Apple software. 

In consideration of your agreement to abide by the following terms, and 
subject to these terms, Apple grants you a personal, non-exclusive 
license, under Apple's copyrights in this original Apple software (the 
"Apple Software"), to use, reproduce, modify and redistribute the Apple 
Software, with or without modifications, in source and/or binary forms; 
provided that if you redistribute the Apple Software in its entirety and 
without modifications, you must retain this notice and the following 
text and disclaimers in all such redistributions of the Apple Software. 
Neither the name, trademarks, service marks or logos of Apple Inc. may 
be used to endorse or promote products derived from the Apple Software 
without specific prior written permission from Apple.  Except as 
expressly stated in this notice, no other rights or licenses, express or 
implied, are granted by Apple herein, including but not limited to any 
patent rights that may be infringed by your derivative works or by other 
works in which the Apple Software may be incorporated. 

The Apple Software is provided by Apple on an "AS IS" basis.  APPLE 
MAKES NO WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION 
THE IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS 
FOR A PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND 
OPERATION ALONE OR IN COMBINATION WITH YOUR PRODUCTS. 

IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL 
OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION, 
MODIFICATION AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED 
AND WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING NEGLIGENCE), 
STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE 
POSSIBILITY OF SUCH DAMAGE. 

Copyright (C) 2013 Apple Inc. All Rights Reserved. 

*/

#ifndef __CAPlayThrough_H__
#define __CAPlayThrough_H__

#define checkErr( err) \
  if(err) {\
    OSStatus error = static_cast<OSStatus>(err);\
    fprintf(stdout, "CAPlayThrough Error: %ld ->  %s:  %d\n",  (long)error,\
	__FILE__, \
	__LINE__\
	);\
    fflush(stdout);\
    return err; \
  }         

#include <CoreAudio/CoreAudio.h>
#include <AudioToolbox/AudioToolbox.h>
#include <AudioUnit/AudioUnit.h>
#include "CARingBuffer.h"
#include "AudioDevice.h"
#include "CAStreamBasicDescription.h"

class CAPlayThrough;

// This class will manage the lifecycle of the play through objects
class CAPlayThroughHost
{

  public:
    CAPlayThroughHost(AudioDeviceID input, AudioDeviceID output);
    ~CAPlayThroughHost();

    void    CreatePlayThrough(AudioDeviceID input, AudioDeviceID output);
    void    DeletePlayThrough();
    bool    PlayThroughExists();

    OSStatus  Start();
    OSStatus  Stop();
    Boolean    IsRunning();    

  private:
    CAPlayThrough* GetPlayThrough() { return mPlayThrough; }

    void AddDeviceListeners(AudioDeviceID input);
    void RemoveDeviceListeners(AudioDeviceID input);

    void ResetPlayThrough();

    // this sample now uses AudioObjectAddPropertyListenerBlock instead of AudioObjectAddPropertyListener
    /*static OSStatus StreamListener(AudioObjectID inObjectID,
      UInt32 inNumberAddresses,
      const AudioObjectPropertyAddress inAddresses[],
      void* inClientData);*/

    dispatch_queue_t StreamListenerQueue;
    AudioObjectPropertyListenerBlock StreamListenerBlock;

  private:
    CAPlayThrough *mPlayThrough;
};

#endif //__CAPlayThrough_H__

