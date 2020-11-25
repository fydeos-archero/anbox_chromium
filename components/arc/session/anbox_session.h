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

#include "anbox/common/wait_handle.h"

namespace anbox{
class Runtime;

namespace bridge{
class AndroidApiStub;

}

namespace application{
class Database;  
}

namespace network{
class PublishedSocketConnector;
}

namespace rpc{
class Channel;  
}

}

namespace arc {


class AnboxSession: public std::enable_shared_from_this<AnboxSession> {
public:
  struct AppInfo{
    std::string name;
    std::string package;
    std::string component;

    AppInfo(const std::string &n, const std::string &p, const std::string &c):
      name(n), package(p), component(c){}

    AppInfo(){}  
  };

  class Observer {
  public:    
    virtual void onAppAdded(void* pItem){}
    virtual void onAppRemoved(void* pItem){}

    virtual void OnTaskCreated(int task_id, const AppInfo &app_info){}
  };

public:
  AnboxSession();  
  ~AnboxSession();

public:
  void Initialize();
  bool LaunchApp(std::string &name, std::string &package, std::string &component);
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
    LOG(INFO) << "==== AnboxSession launcher callback";
    launch_wait_handle_.result_received();    
  }

  std::map<std::string, AppInfo>& GetLaunchedApps(){ return launched_apps_; }  

private:
  std::shared_ptr<anbox::Runtime> rt_;
  // std::shared_ptr<anbox::bridge::AndroidApiStub> android_api_stub_;  
  std::shared_ptr<anbox::network::PublishedSocketConnector> publish_socket_;
  std::shared_ptr<anbox::rpc::Channel> channel_;    
  base::ObserverList<Observer>::Unchecked observer_list_;

  mutable std::mutex mutex_;
  anbox::common::WaitHandle launch_wait_handle_;

  std::map<std::string, AppInfo> launched_apps_;  

  // WeakPtrFactory to use callbacks.
  base::WeakPtrFactory<AnboxSession> weak_factory_{this};

private:
  DISALLOW_COPY_AND_ASSIGN(AnboxSession);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_SESSION_ANBOX_SESSION_H_
