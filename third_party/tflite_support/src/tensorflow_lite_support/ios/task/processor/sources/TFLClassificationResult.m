/* Copyright 2021 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/
#import "tensorflow_lite_support/ios/task/processor/sources/TFLClassificationResult.h"

@implementation TFLClassifications

- (instancetype)initWithHeadIndex:(NSInteger)headIndex
                         headName:(nullable NSString*)headName
                       categories:(NSArray<TFLCategory*>*)categories {
  self = [super init];
  if (self) {
    _headIndex = headIndex;
    _headName = headName;
    _categories = categories;
  }
  return self;
}

- (instancetype)initWithHeadIndex:(NSInteger)headIndex
                       categories:(NSArray<TFLCategory*>*)categories {
  return [self initWithHeadIndex:headIndex headName:nil categories:categories];
}

@end

@implementation TFLClassificationResult {
  NSArray<TFLClassifications*>* _classifications;
}

- (instancetype)initWithClassifications:
    (NSArray<TFLClassifications*>*)classifications {
  self = [super init];
  if (self) {
    _classifications = classifications;
  }
  return self;
}

@end
