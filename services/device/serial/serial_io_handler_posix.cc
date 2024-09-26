// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/serial/serial_io_handler_posix.h"

#include <sys/ioctl.h>
#include <termios.h>

#include <algorithm>
#include <utility>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/posix/eintr_wrapper.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "components/device_event_log/device_event_log.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include <asm-generic/ioctls.h>
#include <linux/serial.h>

// The definition of struct termios2 is copied from asm-generic/termbits.h
// because including that header directly conflicts with termios.h.
extern "C" {
struct termios2 {
  tcflag_t c_iflag;  // input mode flags
  tcflag_t c_oflag;  // output mode flags
  tcflag_t c_cflag;  // control mode flags
  tcflag_t c_lflag;  // local mode flags
  cc_t c_line;       // line discipline
  cc_t c_cc[19];     // control characters
  speed_t c_ispeed;  // input speed
  speed_t c_ospeed;  // output speed
};
}

#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_MAC)
#include <IOKit/serial/ioss.h>
#endif

namespace {

// Convert an integral bit rate to a nominal one. Returns |true|
// if the conversion was successful and |false| otherwise.
bool BitrateToSpeedConstant(int bitrate, speed_t* speed) {
#define BITRATE_TO_SPEED_CASE(x) \
  case x:                        \
    *speed = B##x;               \
    return true;
  switch (bitrate) {
    BITRATE_TO_SPEED_CASE(0)
    BITRATE_TO_SPEED_CASE(50)
    BITRATE_TO_SPEED_CASE(75)
    BITRATE_TO_SPEED_CASE(110)
    BITRATE_TO_SPEED_CASE(134)
    BITRATE_TO_SPEED_CASE(150)
    BITRATE_TO_SPEED_CASE(200)
    BITRATE_TO_SPEED_CASE(300)
    BITRATE_TO_SPEED_CASE(600)
    BITRATE_TO_SPEED_CASE(1200)
    BITRATE_TO_SPEED_CASE(1800)
    BITRATE_TO_SPEED_CASE(2400)
    BITRATE_TO_SPEED_CASE(4800)
    BITRATE_TO_SPEED_CASE(9600)
    BITRATE_TO_SPEED_CASE(19200)
    BITRATE_TO_SPEED_CASE(38400)
#if !BUILDFLAG(IS_MAC)
    BITRATE_TO_SPEED_CASE(57600)
    BITRATE_TO_SPEED_CASE(115200)
    BITRATE_TO_SPEED_CASE(230400)
    BITRATE_TO_SPEED_CASE(460800)
    BITRATE_TO_SPEED_CASE(576000)
    BITRATE_TO_SPEED_CASE(921600)
#endif
    default:
      return false;
  }
#undef BITRATE_TO_SPEED_CASE
}

#if !BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS)
// Convert a known nominal speed into an integral bitrate. Returns |true|
// if the conversion was successful and |false| otherwise.
bool SpeedConstantToBitrate(speed_t speed, int* bitrate) {
#define SPEED_TO_BITRATE_CASE(x) \
  case B##x:                     \
    *bitrate = x;                \
    return true;
  switch (speed) {
    SPEED_TO_BITRATE_CASE(0)
    SPEED_TO_BITRATE_CASE(50)
    SPEED_TO_BITRATE_CASE(75)
    SPEED_TO_BITRATE_CASE(110)
    SPEED_TO_BITRATE_CASE(134)
    SPEED_TO_BITRATE_CASE(150)
    SPEED_TO_BITRATE_CASE(200)
    SPEED_TO_BITRATE_CASE(300)
    SPEED_TO_BITRATE_CASE(600)
    SPEED_TO_BITRATE_CASE(1200)
    SPEED_TO_BITRATE_CASE(1800)
    SPEED_TO_BITRATE_CASE(2400)
    SPEED_TO_BITRATE_CASE(4800)
    SPEED_TO_BITRATE_CASE(9600)
    SPEED_TO_BITRATE_CASE(19200)
    SPEED_TO_BITRATE_CASE(38400)
    default:
      return false;
  }
#undef SPEED_TO_BITRATE_CASE
}
#endif

}  // namespace

namespace device {

// static
scoped_refptr<SerialIoHandler> SerialIoHandler::Create(
    const base::FilePath& port,
    scoped_refptr<base::SingleThreadTaskRunner> ui_thread_task_runner) {
  return new SerialIoHandlerPosix(port, std::move(ui_thread_task_runner));
}

void SerialIoHandlerPosix::ReadImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsReadPending());

  // Try to read immediately. This is needed because on some platforms
  // (e.g., OSX) there may not be a notification from the message loop
  // when the fd is ready to read immediately after it is opened. There
  // is no danger of blocking because the fd is opened with async flag.
  AttemptRead();
}

void SerialIoHandlerPosix::WriteImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsWritePending());

  EnsureWatchingWrites();
}

void SerialIoHandlerPosix::CancelReadImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  StopWatchingFileRead();
  ReadCompleted(0, read_cancel_reason());
}

void SerialIoHandlerPosix::CancelWriteImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  StopWatchingFileWrite();
  WriteCompleted(0, write_cancel_reason());
}

bool SerialIoHandlerPosix::ConfigurePortImpl() {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  struct termios2 config;
  if (ioctl(file().GetPlatformFile(), TCGETS2, &config) < 0) {
#else
  struct termios config;
  if (tcgetattr(file().GetPlatformFile(), &config) != 0) {
#endif
    SERIAL_PLOG(DEBUG) << "Failed to get port configuration";
    return false;
  }

  // Set flags for 'raw' operation
  config.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHONL | ISIG);
  config.c_iflag &= ~(IGNBRK | BRKINT | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
  config.c_iflag |= PARMRK;
  config.c_oflag &= ~OPOST;

  // CLOCAL causes the system to disregard the DCD signal state.
  // CREAD enables reading from the port.
  config.c_cflag |= (CLOCAL | CREAD);

  DCHECK(options().bitrate);
  speed_t bitrate_opt = B0;
#if BUILDFLAG(IS_MAC)
  bool need_iossiospeed = false;
#endif
  if (BitrateToSpeedConstant(options().bitrate, &bitrate_opt)) {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    config.c_cflag &= ~CBAUD;
    config.c_cflag |= bitrate_opt;
#else
    cfsetispeed(&config, bitrate_opt);
    cfsetospeed(&config, bitrate_opt);
#endif
  } else {
    // Attempt to set a custom speed.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    config.c_cflag &= ~CBAUD;
    config.c_cflag |= CBAUDEX;
    config.c_ispeed = config.c_ospeed = options().bitrate;
#elif BUILDFLAG(IS_MAC)
    // cfsetispeed and cfsetospeed sometimes work for custom baud rates on OS
    // X but the IOSSIOSPEED ioctl is more reliable but has to be done after
    // the rest of the port parameters are set or else it will be overwritten.
    need_iossiospeed = true;
#else
    return false;
#endif
  }

  DCHECK(options().data_bits != mojom::SerialDataBits::NONE);
  config.c_cflag &= ~CSIZE;
  switch (options().data_bits) {
    case mojom::SerialDataBits::SEVEN:
      config.c_cflag |= CS7;
      break;
    case mojom::SerialDataBits::EIGHT:
    default:
      config.c_cflag |= CS8;
      break;
  }

  DCHECK(options().parity_bit != mojom::SerialParityBit::NONE);
  switch (options().parity_bit) {
    case mojom::SerialParityBit::EVEN:
      config.c_cflag |= PARENB;
      config.c_cflag &= ~PARODD;
      break;
    case mojom::SerialParityBit::ODD:
      config.c_cflag |= (PARODD | PARENB);
      break;
    case mojom::SerialParityBit::NO_PARITY:
    default:
      config.c_cflag &= ~(PARODD | PARENB);
      break;
  }

  error_detect_state_ = ErrorDetectState::NO_ERROR;
  num_chars_stashed_ = 0;

  if (config.c_cflag & PARENB) {
    config.c_iflag &= ~IGNPAR;
    config.c_iflag |= INPCK;
    parity_check_enabled_ = true;
  } else {
    config.c_iflag |= IGNPAR;
    config.c_iflag &= ~INPCK;
    parity_check_enabled_ = false;
  }

  DCHECK(options().stop_bits != mojom::SerialStopBits::NONE);
  switch (options().stop_bits) {
    case mojom::SerialStopBits::TWO:
      config.c_cflag |= CSTOPB;
      break;
    case mojom::SerialStopBits::ONE:
    default:
      config.c_cflag &= ~CSTOPB;
      break;
  }

  DCHECK(options().has_cts_flow_control);
  if (options().cts_flow_control) {
    config.c_cflag |= CRTSCTS;
  } else {
    config.c_cflag &= ~CRTSCTS;
  }

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  if (ioctl(file().GetPlatformFile(), TCSETS2, &config) < 0) {
#else
  if (tcsetattr(file().GetPlatformFile(), TCSANOW, &config) != 0) {
#endif
    SERIAL_PLOG(DEBUG) << "Failed to set port attributes";
    return false;
  }

#if BUILDFLAG(IS_MAC)
  if (need_iossiospeed) {
    speed_t bitrate = options().bitrate;
    if (ioctl(file().GetPlatformFile(), IOSSIOSPEED, &bitrate) == -1) {
      SERIAL_PLOG(DEBUG) << "Failed to set custom baud rate";
      return false;
    }
  }
#endif

  return true;
}

bool SerialIoHandlerPosix::PostOpen() {
  // The base::File::FLAG_WIN_EXCLUSIVE_READ and
  // base::File::FLAG_WIN_EXCLUSIVE_WRITE flags do nothing on POSIX-based
  // systems. Request exclusive access to the terminal device here.
  if (HANDLE_EINTR(ioctl(file().GetPlatformFile(), TIOCEXCL)) == -1) {
    SERIAL_PLOG(DEBUG) << "Failed to put terminal in exclusive mode";
    return false;
  }

#if BUILDFLAG(IS_CHROMEOS)
  // The Chrome OS permission broker does not open devices in async mode.
  return base::SetNonBlocking(file().GetPlatformFile());
#else
  return true;
#endif
}

void SerialIoHandlerPosix::PreClose() {
  StopWatchingFileRead();
  StopWatchingFileWrite();
}

SerialIoHandlerPosix::SerialIoHandlerPosix(
    const base::FilePath& port,
    scoped_refptr<base::SingleThreadTaskRunner> ui_thread_task_runner)
    : SerialIoHandler(port, std::move(ui_thread_task_runner)) {}

SerialIoHandlerPosix::~SerialIoHandlerPosix() = default;

void SerialIoHandlerPosix::AttemptRead() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (IsReadPending()) {
    int bytes_read = HANDLE_EINTR(read(file().GetPlatformFile(),
                                       pending_read_buffer().data(),
                                       pending_read_buffer().size()));
    if (bytes_read < 0) {
      if (errno == EAGAIN) {
        // The fd does not have data to read yet so continue waiting.
        EnsureWatchingReads();
      } else if (errno == ENXIO) {
        StopWatchingFileRead();
        ReadCompleted(0, mojom::SerialReceiveError::DEVICE_LOST);
      } else {
        SERIAL_PLOG(DEBUG) << "Read failed";
        ReadCompleted(0, mojom::SerialReceiveError::SYSTEM_ERROR);
      }
    } else if (bytes_read == 0) {
      StopWatchingFileRead();
      ReadCompleted(0, mojom::SerialReceiveError::DEVICE_LOST);
    } else {
      bool break_detected = false;
      bool parity_error_detected = false;
      size_t new_bytes_read =
          CheckReceiveError(pending_read_buffer(), bytes_read, break_detected,
                            parity_error_detected);

      if (break_detected) {
        ReadCompleted(new_bytes_read, mojom::SerialReceiveError::BREAK);
      } else if (parity_error_detected) {
        ReadCompleted(new_bytes_read, mojom::SerialReceiveError::PARITY_ERROR);
      } else {
        ReadCompleted(new_bytes_read, mojom::SerialReceiveError::NONE);
      }
    }
  } else {
    // Stop watching the fd if we get notifications with no pending
    // reads or writes to avoid starving the message loop.
    StopWatchingFileRead();
  }
}

void SerialIoHandlerPosix::OnFileCanWriteWithoutBlocking() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (IsWritePending()) {
    int bytes_written = HANDLE_EINTR(write(file().GetPlatformFile(),
                                           pending_write_buffer().data(),
                                           pending_write_buffer().size()));
    if (bytes_written < 0) {
      if (errno == EIO || errno == ENXIO) {
        StopWatchingFileWrite();
        WriteCompleted(0, mojom::SerialSendError::DISCONNECTED);
      } else {
        SERIAL_PLOG(DEBUG) << "Write failed";
        WriteCompleted(0, mojom::SerialSendError::SYSTEM_ERROR);
      }
    } else {
      WriteCompleted(bytes_written, mojom::SerialSendError::NONE);
    }
  } else {
    // Stop watching the fd if we get notifications with no pending
    // writes to avoid starving the message loop.
    StopWatchingFileWrite();
  }
}

void SerialIoHandlerPosix::EnsureWatchingReads() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(file().IsValid());
  if (!file_read_watcher_) {
    file_read_watcher_ = base::FileDescriptorWatcher::WatchReadable(
        file().GetPlatformFile(),
        base::BindRepeating(&SerialIoHandlerPosix::AttemptRead,
                            base::Unretained(this)));
  }
}

void SerialIoHandlerPosix::EnsureWatchingWrites() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(file().IsValid());
  if (!file_write_watcher_) {
    file_write_watcher_ = base::FileDescriptorWatcher::WatchWritable(
        file().GetPlatformFile(),
        base::BindRepeating(
            &SerialIoHandlerPosix::OnFileCanWriteWithoutBlocking,
            base::Unretained(this)));
  }
}

void SerialIoHandlerPosix::StopWatchingFileRead() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (file_read_watcher_) {
    // Check that file is valid before stopping the watch, to avoid getting a
    // hard to diagnose crash in MessagePumpLibEvent. https://crbug.com/996777
    CHECK(file().IsValid());
    file_read_watcher_.reset();
  }
}

void SerialIoHandlerPosix::StopWatchingFileWrite() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (file_write_watcher_) {
    // Check that file is valid before stopping the watch, to avoid getting a
    // hard to diagnose crash in MessagePumpLibEvent. https://crbug.com/996777
    CHECK(file().IsValid());
    file_write_watcher_.reset();
  }
}

void SerialIoHandlerPosix::Flush(mojom::SerialPortFlushMode mode) const {
  int queue_selector;
  switch (mode) {
    case mojom::SerialPortFlushMode::kReceiveAndTransmit:
      queue_selector = TCIOFLUSH;
      break;
    case mojom::SerialPortFlushMode::kReceive:
      queue_selector = TCIFLUSH;
      break;
    case mojom::SerialPortFlushMode::kTransmit:
      queue_selector = TCOFLUSH;
      break;
  }

  if (tcflush(file().GetPlatformFile(), queue_selector) != 0)
    SERIAL_PLOG(DEBUG) << "Failed to flush port";
}

void SerialIoHandlerPosix::Drain() {
  if (tcdrain(file().GetPlatformFile()) != 0)
    SERIAL_PLOG(DEBUG) << "Failed to drain port";
}

mojom::SerialPortControlSignalsPtr SerialIoHandlerPosix::GetControlSignals()
    const {
  int status;
  if (ioctl(file().GetPlatformFile(), TIOCMGET, &status) == -1) {
    SERIAL_PLOG(DEBUG) << "Failed to get port control signals";
    return mojom::SerialPortControlSignalsPtr();
  }

  auto signals = mojom::SerialPortControlSignals::New();
  signals->dcd = (status & TIOCM_CAR) != 0;
  signals->cts = (status & TIOCM_CTS) != 0;
  signals->dsr = (status & TIOCM_DSR) != 0;
  signals->ri = (status & TIOCM_RI) != 0;
  return signals;
}

bool SerialIoHandlerPosix::SetControlSignals(
    const mojom::SerialHostControlSignals& signals) {
  // Collect signals that need to be set or cleared on the port.
  int set = 0;
  int clear = 0;

  if (signals.has_dtr) {
    if (signals.dtr) {
      set |= TIOCM_DTR;
    } else {
      clear |= TIOCM_DTR;
    }
  }

  if (signals.has_rts) {
    if (signals.rts) {
      set |= TIOCM_RTS;
    } else {
      clear |= TIOCM_RTS;
    }
  }

  if (set && ioctl(file().GetPlatformFile(), TIOCMBIS, &set) != 0) {
    SERIAL_PLOG(DEBUG) << "Failed to set port control signals";
    return false;
  }

  if (clear && ioctl(file().GetPlatformFile(), TIOCMBIC, &clear) != 0) {
    SERIAL_PLOG(DEBUG) << "Failed to clear port control signals";
    return false;
  }

  if (signals.has_brk) {
    if (signals.brk) {
      if (ioctl(file().GetPlatformFile(), TIOCSBRK, 0) != 0) {
        SERIAL_PLOG(DEBUG) << "Failed to set break";
        return false;
      }
    } else {
      if (ioctl(file().GetPlatformFile(), TIOCCBRK, 0) != 0) {
        SERIAL_PLOG(DEBUG) << "Failed to clear break";
        return false;
      }
    }
  }

  return true;
}

mojom::SerialConnectionInfoPtr SerialIoHandlerPosix::GetPortInfo() const {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  struct termios2 config;
  if (ioctl(file().GetPlatformFile(), TCGETS2, &config) < 0) {
#else
  struct termios config;
  if (tcgetattr(file().GetPlatformFile(), &config) == -1) {
#endif
    SERIAL_PLOG(DEBUG) << "Failed to get port info";
    return mojom::SerialConnectionInfoPtr();
  }

  auto info = mojom::SerialConnectionInfo::New();
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // Linux forces c_ospeed to contain the correct value, which is nice.
  info->bitrate = config.c_ospeed;
#else
  speed_t ispeed = cfgetispeed(&config);
  speed_t ospeed = cfgetospeed(&config);
  if (ispeed == ospeed) {
    int bitrate = 0;
    if (SpeedConstantToBitrate(ispeed, &bitrate)) {
      info->bitrate = bitrate;
    } else if (ispeed > 0) {
      info->bitrate = static_cast<int>(ispeed);
    }
  }
#endif

  if ((config.c_cflag & CSIZE) == CS7) {
    info->data_bits = mojom::SerialDataBits::SEVEN;
  } else if ((config.c_cflag & CSIZE) == CS8) {
    info->data_bits = mojom::SerialDataBits::EIGHT;
  } else {
    info->data_bits = mojom::SerialDataBits::NONE;
  }
  if (config.c_cflag & PARENB) {
    info->parity_bit = (config.c_cflag & PARODD) ? mojom::SerialParityBit::ODD
                                                 : mojom::SerialParityBit::EVEN;
  } else {
    info->parity_bit = mojom::SerialParityBit::NO_PARITY;
  }
  info->stop_bits = (config.c_cflag & CSTOPB) ? mojom::SerialStopBits::TWO
                                              : mojom::SerialStopBits::ONE;
  info->cts_flow_control = (config.c_cflag & CRTSCTS) != 0;
  return info;
}

// break sequence:
// '\377'       -->        ErrorDetectState::MARK_377_SEEN
// '\0'         -->          ErrorDetectState::MARK_0_SEEN
// '\0'         -->                         break detected
//
// parity error sequence:
// '\377'       -->        ErrorDetectState::MARK_377_SEEN
// '\0'         -->          ErrorDetectState::MARK_0_SEEN
// character with parity error  -->  parity error detected
//
// break/parity error sequences are removed from the byte stream
// '\377' '\377' sequence is replaced with '\377'
size_t SerialIoHandlerPosix::CheckReceiveError(base::span<uint8_t> buffer,
                                               size_t bytes_read,
                                               bool& break_detected,
                                               bool& parity_error_detected) {
  size_t new_bytes_read = num_chars_stashed_;
  DCHECK_LE(new_bytes_read, 2u);

  for (size_t i = 0; i < bytes_read; ++i) {
    uint8_t ch = buffer[i];
    if (new_bytes_read == 0) {
      chars_stashed_[0] = ch;
    } else if (new_bytes_read == 1) {
      chars_stashed_[1] = ch;
    } else {
      buffer[new_bytes_read - 2] = ch;
    }
    ++new_bytes_read;
    switch (error_detect_state_) {
      case ErrorDetectState::NO_ERROR:
        if (ch == 0377) {
          error_detect_state_ = ErrorDetectState::MARK_377_SEEN;
        }
        break;
      case ErrorDetectState::MARK_377_SEEN:
        DCHECK_GE(new_bytes_read, 2u);
        if (ch == 0) {
          error_detect_state_ = ErrorDetectState::MARK_0_SEEN;
        } else {
          if (ch == 0377) {
            // receive two bytes '\377' '\377', since ISTRIP is not set and
            // PARMRK is set, a valid byte '\377' is passed to the program as
            // two bytes, '\377' '\377'. Replace these two bytes with one byte
            // of '\377', and set error_detect_state_ back to
            // ErrorDetectState::NO_ERROR.
            --new_bytes_read;
          }
          error_detect_state_ = ErrorDetectState::NO_ERROR;
        }
        break;
      case ErrorDetectState::MARK_0_SEEN:
        DCHECK_GE(new_bytes_read, 3u);
        if (ch == 0) {
          break_detected = true;
          new_bytes_read -= 3;
          error_detect_state_ = ErrorDetectState::NO_ERROR;
        } else {
          if (parity_check_enabled_) {
            parity_error_detected = true;
            new_bytes_read -= 3;
            error_detect_state_ = ErrorDetectState::NO_ERROR;
          } else if (ch == 0377) {
            error_detect_state_ = ErrorDetectState::MARK_377_SEEN;
          } else {
            error_detect_state_ = ErrorDetectState::NO_ERROR;
          }
        }
        break;
    }
  }
  // Now new_bytes_read bytes should be returned to the caller (including the
  // previously stashed characters that were stored at chars_stashed_[]) and are
  // now stored at: chars_stashed_[0], chars_stashed_[1], buffer[...].

  // Stash up to 2 characters that are potentially part of a break/parity error
  // sequence. The buffer may also not be large enough to store all the bytes.
  // tmp[] stores the characters that need to be stashed for this read.
  uint8_t tmp[2];
  num_chars_stashed_ = 0;
  if (error_detect_state_ == ErrorDetectState::MARK_0_SEEN ||
      new_bytes_read - buffer.size() == 2) {
    // need to stash the last two characters
    if (new_bytes_read == 2) {
      memcpy(tmp, chars_stashed_, new_bytes_read);
    } else {
      if (new_bytes_read == 3) {
        tmp[0] = chars_stashed_[1];
      } else {
        tmp[0] = buffer[new_bytes_read - 4];
      }
      tmp[1] = buffer[new_bytes_read - 3];
    }
    num_chars_stashed_ = 2;
  } else if (error_detect_state_ == ErrorDetectState::MARK_377_SEEN ||
             new_bytes_read - buffer.size() == 1) {
    // need to stash the last character
    if (new_bytes_read <= 2) {
      tmp[0] = chars_stashed_[new_bytes_read - 1];
    } else {
      tmp[0] = buffer[new_bytes_read - 3];
    }
    num_chars_stashed_ = 1;
  }

  new_bytes_read -= num_chars_stashed_;
  if (new_bytes_read > 2) {
    // right shift two bytes to store bytes from chars_stashed_[]
    memmove(&buffer[2], &buffer[0], new_bytes_read - 2);
  }
  memcpy(&buffer[0], chars_stashed_, std::min<size_t>(new_bytes_read, 2));
  memcpy(chars_stashed_, tmp, num_chars_stashed_);
  return new_bytes_read;
}

}  // namespace device
