// AUTOGENERATED FILE - DO NOT MODIFY!
// This file generated by Djinni from rocketspeed.djinni

#import "RSResultStatus.h"
#include "ResultStatus.hpp"
#import <Foundation/Foundation.h>

@interface RSResultStatus ()

- (id)initWithResultStatus:(RSResultStatus *)ResultStatus;
- (id)initWithStatus:(RSStatus *)status msgid:(RSMsgId *)msgid seqno:(RSSequenceNumber *)seqno;
- (id)initWithCppResultStatus:(const ::rocketglue::ResultStatus &)ResultStatus;
- (::rocketglue::ResultStatus)cppResultStatus;

@end