#pragma once

#include "constants.hpp"
#include "device_grabber.hpp"
#include "event_dispatcher_manager.hpp"
#include "grabber_server.hpp"
#include "logger.hpp"
#include "notification_center.hpp"
#include "session.hpp"
#include <sys/stat.h>

class connection_manager final {
public:
  connection_manager(const connection_manager&) = delete;

  connection_manager(device_grabber& device_grabber) : device_grabber_(device_grabber),
                                                       timer_(0),
                                                       last_uid_(0),
                                                       grabber_server_(device_grabber_) {
    prepare_socket_directory(0);
    grabber_server_.stop();

    timer_ = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_main_queue());
    if (!timer_) {
      logger::get_logger().error("failed to dispatch_source_create");
    } else {
      dispatch_source_set_timer(timer_, dispatch_time(DISPATCH_TIME_NOW, 1.0 * NSEC_PER_SEC), 1.0 * NSEC_PER_SEC, 0);
      dispatch_source_set_event_handler(timer_, ^{
        if (auto uid = session::get_current_console_user_id()) {
          if (last_uid_ != *uid) {
            last_uid_ = *uid;
            logger::get_logger().info("current_console_user_id: {0}", *uid);

            event_dispatcher_manager_ = nullptr;
            event_dispatcher_manager_ = std::make_unique<event_dispatcher_manager>();

            // set up grabber_server before prepare_socket_directory
            // in order to guarantee that the grabber_server is ready if console_user_server can make their receiver socket.
            grabber_server_.stop();
            grabber_server_.start();
            prepare_socket_directory(*uid);
            // unlink old socket.
            unlink(constants::get_console_user_socket_file_path());

            logger::get_logger().info("console_user_socket_directory_is_ready for {0}", *uid);
            notification_center::post_distributed_notification_to_all_sessions(constants::get_distributed_notification_console_user_socket_directory_is_ready());
          }
        }
      });

      dispatch_resume(timer_);
    }
  }

  void prepare_socket_directory(uid_t uid) {
    // make directories.
    mkdir(constants::get_socket_directory(), 0755);
    mkdir(constants::get_console_user_socket_directory(), 0700);

    // chmod,chown directories.
    chmod(constants::get_socket_directory(), 0755);
    chown(constants::get_socket_directory(), 0, 0);

    chmod(constants::get_console_user_socket_directory(), 0700);
    chown(constants::get_console_user_socket_directory(), uid, 0);
  }

private:
  device_grabber& device_grabber_;
  dispatch_source_t timer_;
  uid_t last_uid_;

  std::unique_ptr<event_dispatcher_manager> event_dispatcher_manager_;
  grabber_server grabber_server_;
};
