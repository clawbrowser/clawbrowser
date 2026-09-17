#import <Foundation/Foundation.h>
#import <CoreText/CoreText.h>

// Read explicit font files only. No registration, installation, or fallback
// selection. This is a platform ingestion probe, not browser acceptance.
int main(int argc, char** argv) {
  @autoreleasepool {
    NSMutableArray* rows = [NSMutableArray array];
    for (int i = 1; i < argc; ++i) {
      NSString* path = [NSString stringWithUTF8String:argv[i]];
      CFArrayRef descriptors = CTFontManagerCreateFontDescriptorsFromURL(
          (__bridge CFURLRef)[NSURL fileURLWithPath:path]);
      NSMutableArray* faces = [NSMutableArray array];
      for (CFIndex index = 0; descriptors && index < CFArrayGetCount(descriptors); ++index) {
        CTFontDescriptorRef descriptor = (CTFontDescriptorRef)CFArrayGetValueAtIndex(descriptors, index);
        CTFontRef font = CTFontCreateWithFontDescriptor(descriptor, 24, nullptr);
        if (!font) continue;
        CFStringRef family = CTFontCopyFamilyName(font);
        CFStringRef postscript = CTFontCopyPostScriptName(font);
        CFCharacterSetRef charset = CTFontCopyCharacterSet(font);
        NSMutableDictionary* coverage = [NSMutableDictionary dictionary];
        NSDictionary* probes = @{@"latin": @0x0041, @"arabic": @0x0627,
          @"cjk": @0x4e2d, @"devanagari": @0x0915, @"emoji": @0x1f600};
        for (NSString* script in probes) {
          coverage[script] = @(charset && CFCharacterSetIsLongCharacterMember(
              charset, [probes[script] unsignedIntValue]));
        }
        [faces addObject:@{@"index": @(index), @"family": (__bridge NSString*)family,
          @"postscript": (__bridge NSString*)postscript, @"coverage": coverage}];
        if (charset) CFRelease(charset);
        CFRelease(family); CFRelease(postscript); CFRelease(font);
      }
      [rows addObject:@{@"file": [path lastPathComponent], @"faces": faces}];
      if (descriptors) CFRelease(descriptors);
    }
    NSData* data = [NSJSONSerialization dataWithJSONObject:rows options:NSJSONWritingPrettyPrinted error:nil];
    fwrite(data.bytes, 1, data.length, stdout);
    puts("");
  }
}
