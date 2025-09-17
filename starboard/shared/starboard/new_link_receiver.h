// new_link_receiver.h

#ifndef STANDALONE_LINK_RECEIVER_H_
#define STANDALONE_LINK_RECEIVER_H_

#include <memory>

// Callback function type to replace the Application::Link method.
// The provided string is valid only for the duration of the callback.
typedef void (*LinkCallback)(const char* link_data);

// Forward declaration for the implementation class (PImpl idiom).
class LinkReceiverImpl;

class LinkReceiver {
 public:
  // Constructor that listens on an ephemeral port (port 0).
  // The actual port can be retrieved if needed, but is typically written to a
  // temporary file for discovery.
  explicit LinkReceiver(LinkCallback link_callback);

  // Constructor that listens on a specific port.
  LinkReceiver(LinkCallback link_callback, int port);

  // Destructor safely shuts down the receiver thread.
  ~LinkReceiver();

  // Disable copy and move operations.
  LinkReceiver(const LinkReceiver&) = delete;
  LinkReceiver& operator=(const LinkReceiver&) = delete;
  LinkReceiver(LinkReceiver&&) = delete;
  LinkReceiver& operator=(LinkReceiver&&) = delete;

 private:
  std::unique_ptr<LinkReceiverImpl> impl_;
};

#endif  // STANDALONE_LINK_RECEIVER_H_
