// Give the linker all symbols it needs to build cobalt

#include "media/capture/video/linux/video_capture_device_factory_linux.h"

// // #include "media/capture/video/linux/video_capture_device_factory_v4l2.h"

namespace media {

VideoCaptureDeviceFactoryLinux::VideoCaptureDeviceFactoryLinux(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) {}

VideoCaptureDeviceFactoryLinux::~VideoCaptureDeviceFactoryLinux() = default;

VideoCaptureErrorOrDevice VideoCaptureDeviceFactoryLinux::CreateDevice(
    const VideoCaptureDeviceDescriptor& device_descriptor) {
  // Stub implementation: Log that it's not implemented and return an error.
  DCHECK(thread_checker_.CalledOnValidThread());
  NOTIMPLEMENTED();
  // Return an error indicating the device couldn't be created or the
  // functionality is not implemented.
  return VideoCaptureErrorOrDevice(
      VideoCaptureError::kVideoCaptureControllerInvalid);
}

void VideoCaptureDeviceFactoryLinux::GetDevicesInfo(
    GetDevicesInfoCallback callback) {
  // Stub implementation: Log that it's not implemented and return an empty
  // list.
  NOTIMPLEMENTED();
  DCHECK(thread_checker_.CalledOnValidThread());
}

}  // namespace media
