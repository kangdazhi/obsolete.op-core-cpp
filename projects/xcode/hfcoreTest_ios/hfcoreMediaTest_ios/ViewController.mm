//
//  ViewController.m
//  hfmediatest_ios
//
//  Created by Vladimir Morosev on 9/12/12.
//
//

#import "ViewController.h"
#import "MediaEngineDelegateWrapper.h"
#include <openpeer/core/internal/core_MediaEngine.h>
#include <openpeer/core/test/TestMediaEngine.h>

@implementation ViewController

-(IBAction)test1
{
    openpeer::core::internal::MediaEnginePtr mediaEngineInternal =
        std::dynamic_pointer_cast<openpeer::core::internal::MediaEngine>(openpeer::core::internal::IMediaEngineForCallTransport::singleton());
    openpeer::core::test::TestMediaEnginePtr testMediaEngineInternal = std::dynamic_pointer_cast<openpeer::core::test::TestMediaEngine>(mediaEngineInternal);
    openpeer::core::IMediaEnginePtr mediaEngine = openpeer::core::IMediaEngine::singleton();
    
    mediaEngine->setCaptureRenderView((__bridge void*)_imgView1);
    mediaEngine->setChannelRenderView((__bridge void*)_imgView2);
  
    _surfacesSet = YES;
  
    bool saveToLibrary = true;
    NSDateFormatter *formatter;
    NSString *fileName;
    NSString *dateString;
    
    formatter = [[NSDateFormatter alloc] init];
    [formatter setDateFormat:@"dd-MM-yyyy-HH-mm"];
    
    dateString = [formatter stringFromDate:[NSDate date]];
    
    if (saveToLibrary)
        fileName = [NSString stringWithFormat:@"test-%@.mp4", dateString];
    else
        fileName = [NSString stringWithFormat:@"%@/Documents/test-%@.mp4", NSHomeDirectory(), dateString];
    
    const char* fileNameString = [fileName UTF8String];

    mediaEngine->startVideoCapture();
//    mediaEngine->startRecordVideoCapture(fileNameString, saveToLibrary);
}

-(IBAction)test2
{
    openpeer::core::internal::MediaEnginePtr mediaEngineInternal =
        std::dynamic_pointer_cast<openpeer::core::internal::MediaEngine>(openpeer::core::internal::IMediaEngineForCallTransport::singleton());
    openpeer::core::test::TestMediaEnginePtr testMediaEngineInternal = std::dynamic_pointer_cast<openpeer::core::test::TestMediaEngine>(mediaEngineInternal);
    openpeer::core::IMediaEnginePtr mediaEngine = openpeer::core::IMediaEngine::singleton();
    
    testMediaEngineInternal->setReceiverAddress("127.0.0.1");
    
    openpeer::core::internal::IMediaEngineForCallTransport::singleton()->startVoice();
    openpeer::core::internal::IMediaEngineForCallTransport::singleton()->startVideoChannel();
}

-(IBAction)test3
{
    openpeer::core::internal::MediaEnginePtr mediaEngineInternal =
        std::dynamic_pointer_cast<openpeer::core::internal::MediaEngine>(openpeer::core::internal::IMediaEngineForCallTransport::singleton());
    openpeer::core::test::TestMediaEnginePtr testMediaEngineInternal = std::dynamic_pointer_cast<openpeer::core::test::TestMediaEngine>(mediaEngineInternal);
    openpeer::core::IMediaEnginePtr mediaEngine = openpeer::core::IMediaEngine::singleton();
    
    openpeer::core::internal::IMediaEngineForCallTransport::singleton()->stopVoice();
    openpeer::core::internal::IMediaEngineForCallTransport::singleton()->stopVideoChannel();
}

-(IBAction)test4
{
    openpeer::core::internal::MediaEnginePtr mediaEngineInternal =
        std::dynamic_pointer_cast<openpeer::core::internal::MediaEngine>(openpeer::core::internal::IMediaEngineForCallTransport::singleton());
    openpeer::core::test::TestMediaEnginePtr testMediaEngineInternal = std::dynamic_pointer_cast<openpeer::core::test::TestMediaEngine>(mediaEngineInternal);
    openpeer::core::IMediaEnginePtr mediaEngine = openpeer::core::IMediaEngine::singleton();
    
//    mediaEngine->stopRecordVideoCapture();
    mediaEngine->stopVideoCapture();
}

-(IBAction)test5
{
    openpeer::core::IMediaEnginePtr mediaEngine = openpeer::core::IMediaEngine::singleton();
  
    if (mediaEngine->getCameraType() == openpeer::core::IMediaEngine::CameraType_Front)
        mediaEngine->setCameraType(openpeer::core::IMediaEngine::CameraType_Back);
    else if (mediaEngine->getCameraType() == openpeer::core::IMediaEngine::CameraType_Back)
        mediaEngine->setCameraType(openpeer::core::IMediaEngine::CameraType_Front);
}

-(IBAction)test6
{
    openpeer::core::IMediaEnginePtr mediaEngine = openpeer::core::IMediaEngine::singleton();
    /*
     if (mediaEngine->getLoudspeakerEnabled())
        mediaEngine->setLoudspeakerEnabled(false);
     else
        mediaEngine->setLoudspeakerEnabled(true);
     */
    if (_surfacesSet)
    {
        mediaEngine->setCaptureRenderView(NULL);
        mediaEngine->setChannelRenderView(NULL);
    }
    else
    {
        mediaEngine->setCaptureRenderView((__bridge void*)_imgView1);
        mediaEngine->setChannelRenderView((__bridge void*)_imgView2);
    }
    _surfacesSet = !_surfacesSet;
}

- (id)initWithNibName:(NSString *)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil
{
    self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil];
    if (self)
        self.title = @"Media Engine Test";
  
//    NSString* documents = [NSHomeDirectory() stringByAppendingString:@"/Documents/"];
//    const char* buffer = [documents UTF8String];
//    const char* receiverIPAddress = [receiverIPAddressTextField.text UTF8String];
//    const char* receiverIPAddress = "127.0.0.1";
  
    _surfacesSet = NO;
  
    mediaEngineDelegatePtr = MediaEngineDelegateWrapper::create();
  
    openpeer::core::test::TestMediaEngineFactoryPtr overrideFactory(new openpeer::core::test::TestMediaEngineFactory);
  
    openpeer::services::IFactory<openpeer::core::internal::IMediaEngineFactory>::override(overrideFactory);
  
    openpeer::core::internal::IMediaEngineForStack::setup(mediaEngineDelegatePtr);
  
//    IClient::setLogLevel(IClient::Log::Trace);
//    IClient::setLogLevel("openpeer_services", IClient::Log::Debug);      // recommend Debug
//    IClient::setLogLevel("openpeer_media", IClient::Log::Debug);         // recommend Debug
//    IClient::installTelnetLogger(59999, 60, true);
  
    openpeer::core::IMediaEnginePtr mediaEngine = openpeer::core::IMediaEngine::singleton();
  
    mediaEngine->setCameraType(openpeer::core::IMediaEngine::CameraType_Front);
    mediaEngine->setEcEnabled(true);
    mediaEngine->setAgcEnabled(true);
    mediaEngine->setNsEnabled(false);
    mediaEngine->setMuteEnabled(false);
    mediaEngine->setLoudspeakerEnabled(false);
    mediaEngine->setContinuousVideoCapture(true);
    mediaEngine->setDefaultVideoOrientation(IMediaEngine::VideoOrientation_Portrait);
    mediaEngine->setRecordVideoOrientation(IMediaEngine::VideoOrientation_LandscapeRight);
    mediaEngine->setFaceDetection(false);

    return self;
}

- (void)viewDidLoad
{
    [super viewDidLoad];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(orientationChanged) name:@"orientationChanged" object:nil];
    [[UIApplication sharedApplication] setIdleTimerDisabled: YES];
  
    [_imgView1 addObserver:self forKeyPath:@"image"
                 options:(NSKeyValueObservingOptionNew | NSKeyValueObservingOptionOld)
                 context:NULL];
    [_imgView2 addObserver:self forKeyPath:@"image"
                 options:(NSKeyValueObservingOptionNew | NSKeyValueObservingOptionOld)
                 context:NULL];

}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change
                       context:(void *)context
{
    if (object == _imgView1 && [keyPath isEqualToString:@"image"])
    {
        UIImage* image = [change objectForKey:NSKeyValueChangeNewKey];
        [_imgView1 setFrame:CGRectMake(_imgView1.frame.origin.x, _imgView1.frame.origin.y, image.size.width, image.size.height)];
    }
    else if (object == _imgView2 && [keyPath isEqualToString:@"image"])
    {
        UIImage* image = [change objectForKey:NSKeyValueChangeNewKey];
        [_imgView2 setFrame:CGRectMake(_imgView2.frame.origin.x, _imgView2.frame.origin.y, image.size.width, image.size.height)];
    }
}

- (void)viewDidUnload
{
    [super viewDidUnload];
    [[NSNotificationCenter defaultCenter] removeObserver:self name:@"orientationChanged" object:nil];
    [_imgView1 removeObserver:self forKeyPath:@"image"];
    [_imgView2 removeObserver:self forKeyPath:@"image"];
}

- (void)orientationChanged
{
    openpeer::core::IMediaEnginePtr mediaEngine = openpeer::core::IMediaEngine::singleton();
  
    mediaEngine->setVideoOrientation();
}

- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation
{
    return YES;
}

-(void)willRotateToInterfaceOrientation:(UIInterfaceOrientation)toInterfaceOrientation duration:(NSTimeInterval)duration
{
    [[NSNotificationCenter defaultCenter] postNotificationName:@"orientationChanged" object:nil];
}

@end
