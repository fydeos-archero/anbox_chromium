// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_SESSION_ANBOX_SESSION_H_
#define COMPONENTS_ARC_SESSION_ANBOX_SESSION_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/observer_list.h"

#include "content/public/browser/browser_thread.h"
#include "content/browser/browser_main_loop.h"
#include "media/audio/audio_manager.h"
#include "media/audio/audio_io.h"

#include "archero_bridge.h"

class Profile;

namespace storage{
class FileSystemURL;
}

namespace media{
class AudioOutputStream;
}

namespace arc {

class AudioServer;

class AnboxSession:
  public archero::ArcHeroBridge,
  public std::enable_shared_from_this<AnboxSession> {

// public:
// friend class content::BrowserMainLoop;

public:
  AnboxSession():archero::ArcHeroBridge(CAPACITY_BRIDGE_DETECT_APP|CAPACITY_BRIDGE_DETECT_WINDOW){}

public:
  void Initialize(Profile* profile);
  bool LaunchApp(std::string &name, std::string &package, std::string &component);
  bool InstallApp(const std::string &file_path);
  void InstallApp2(const std::string &file_path);
  void Uninstall(const std::string &package, const std::string &component);
  void Close(int task_id);

private:
  // std::shared_ptr<anbox::Runtime> rt_;
  std::shared_ptr<AudioServer> audio_server_;
  // std::shared_ptr<anbox::bridge::AndroidApiStub> android_api_stub_;
  // std::shared_ptr<anbox::network::PublishedSocketConnector> publish_socket_;
  // std::shared_ptr<anbox::rpc::Channel> channel_;
  base::ObserverList<Observer>::Unchecked observer_list_;

  mutable std::mutex mutex_;
  // anbox::common::WaitHandle launch_wait_handle_;

  // WeakPtrFactory to use callbacks.
  base::WeakPtrFactory<AnboxSession> weak_factory_{this};

  Profile* profile_;

private:
  DISALLOW_COPY_AND_ASSIGN(AnboxSession);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_SESSION_ANBOX_SESSION_H_
