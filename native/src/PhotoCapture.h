#pragma once

#include <jni.h>

namespace savr::photocapture {

void InitializeStorage(JNIEnv* env, jclass application);
bool QueueReadback(int sequence, unsigned int texture, int width, int height,
                   bool flipHorizontal, bool flipVertical);
bool TextureBusy();
void ReadbackAfterStereoFence(int sequence);
void ReportFailure(const char* reason);
bool FailureVisible();

} // namespace savr::photocapture
