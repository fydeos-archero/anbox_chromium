// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_SESSION_ARCHERO_SESSION_H_
#define COMPONENTS_ARC_SESSION_ARCHERO_SESSION_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/observer_list.h"

#include "content/public/browser/browser_thread.h"
#include "content/browser/browser_main_loop.h"
#include "media/audio/audio_manager.h"
#include "media/audio/audio_io.h"

#include "archero_bridge.h"
// [AUD] Remove old audio arch ++
#ifdef OLD_AUDIO_ARCH
#include "archero_audio.h"
#endif
// [AUD] Remove old audio arch --

class Profile;

namespace storage{
class FileSystemURL;
}


// [AUD] Remove old audio arch ++
#ifdef OLD_AUDIO_ARCH
namespace media{
class AudioOutputStream;
}
#endif
// [AUD] Remove old audio arch --

namespace arc {

// [AUD] Remove old audio arch ++
#ifdef OLD_AUDIO_ARCH
class AudioServer;
#endif
// [AUD] Remove old audio arch --

class ArcHeroSession:
  public archero::ArcHeroBridge,
  public std::enable_shared_from_this<ArcHeroSession> {
public:
  ArcHeroSession():archero::ArcHeroBridge(CAPACITY_BRIDGE_DETECT_APP|CAPACITY_BRIDGE_DETECT_WINDOW){}

public:
  void Initialize(Profile* profile);
  bool LaunchApp(std::string &name, std::string &package, std::string &component);
  bool InstallApp(const std::string &file_path);
  void InstallApp2(const std::string &file_path);
  void Uninstall(const std::string &package, const std::string &component);
  void Close(int task_id);
  void OnArcHeroInstanceStarted(bool result){
    LOG(INFO) << "====== ArcHeroSession::OnArcHeroInstanceStarted " << result;
  }

private:
// [AUD] Remove old audio arch ++
#ifdef OLD_AUDIO_ARCH
  std::shared_ptr<AudioServer> audio_server_;
#endif
// [AUD] Remove old audio arch --
  base::ObserverList<Observer>::Unchecked observer_list_;

  mutable std::mutex mutex_;

  // WeakPtrFactory to use callbacks.
  base::WeakPtrFactory<ArcHeroSession> weak_factory_{this};

  Profile* profile_;

private:
  DISALLOW_COPY_AND_ASSIGN(ArcHeroSession);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_SESSION_ARCHERO_SESSION_H_
