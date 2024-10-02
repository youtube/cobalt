#include <iostream>
#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/task/single_thread_task_executor.h"


namespace downloader {
void PrintUsage(std::ostream* stream) {
  *stream << "Usage: downloader <url>" << std::endl;
}

bool Main(int argc, char** argv) {
  base::AtExitManager at_exit_manager;

  logging::LoggingSettings settings;
  settings.logging_dest =
      logging::LOG_TO_SYSTEM_DEBUG_LOG | logging::LOG_TO_STDERR;
  logging::InitLogging(settings);

  base::CommandLine::Init(argc, argv);
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch("help")) {
    PrintUsage(&std::cout);
    return true;
  }
  if (command_line.HasSwitch("url")) {
    std::string url = command_line.GetSwitchValueASCII("url");
    LOG(ERROR) << "URL URL :" << url;
  }

  // Just make the main task executor the network loop.
  base::SingleThreadTaskExecutor io_task_executor(base::MessagePumpType::IO);


  LOG(ERROR) << "Hey";
}
}  // namespace downloader

int main(int argc, char** argv) {
  return !downloader::Main(argc, argv);
}
