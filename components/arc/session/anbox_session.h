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

#include "anbox/common/wait_handle.h"
#include "anbox/audio/sink.h"

class Profile;

namespace anbox{
class Runtime;

namespace bridge{
class AndroidApiStub;

}

namespace application{
class Database;  
}

namespace protobuf{
namespace bridge{
class Application;
}  
}

namespace network{
class PublishedSocketConnector;
}

namespace rpc{
class Channel;  
}

}

namespace storage{
class FileSystemURL; 
}

namespace media{
class AudioOutputStream;
}

namespace arc {

class AudioServer;

class AnboxSession:
  public std::enable_shared_from_this<AnboxSession> {
// public:
// friend class content::BrowserMainLoop;

public:  
  class Observer {
  public:    
    virtual void onAppAdded(anbox::protobuf::bridge::Application* pItem){}
    virtual void onAppRemoved(anbox::protobuf::bridge::Application* pItem){}

    virtual void OnTaskCreated(int task_id, const std::string &name, const std::string &package){}
    virtual void OnTaskRemoved(int task_id){}
  };

public:
  AnboxSession();  
  ~AnboxSession();

public:
  void Initialize(Profile* profile);
  bool LaunchApp(std::string &name, std::string &package, std::string &component);
  bool InstallApp(const std::string &file_path);
  void InstallApp2(const std::string &file_path);
  void Uninstall(const std::string &package, const std::string &component);
  void Close(int task_id);
  void OnAnboxInstanceStarted(bool result);

  // Adds or removes observers.
  void AddObserver(Observer* observer){
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    observer_list_.AddObserver(observer);
  }

  void RemoveObserver(Observer* observer){
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    observer_list_.RemoveObserver(observer);
  }

  base::ObserverList<Observer>::Unchecked& GetObserver(){
    return observer_list_;
  }

  void callback(){    
    launch_wait_handle_.result_received();    
  }    

private:
  std::shared_ptr<anbox::Runtime> rt_;
  std::shared_ptr<AudioServer> audio_server_;
  // std::shared_ptr<anbox::bridge::AndroidApiStub> android_api_stub_;  
  std::shared_ptr<anbox::network::PublishedSocketConnector> publish_socket_;
  std::shared_ptr<anbox::rpc::Channel> channel_;    
  base::ObserverList<Observer>::Unchecked observer_list_;

  mutable std::mutex mutex_;
  anbox::common::WaitHandle launch_wait_handle_;    

  // WeakPtrFactory to use callbacks.
  base::WeakPtrFactory<AnboxSession> weak_factory_{this};

  Profile* profile_;

private:
  DISALLOW_COPY_AND_ASSIGN(AnboxSession);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_SESSION_ANBOX_SESSION_H_
