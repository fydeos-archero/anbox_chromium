// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/anbox_service_manager.h"

#include "base/logging.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/arc/session/arc_session.h"
#include "components/arc/session/arc_session_runner.h"

namespace arc {
namespace {

// Weak pointer.  This class is owned by arc::ArcServiceLauncher.
AnboxServiceManager* g_anbox_service_manager = nullptr;

}  // namespace

AnboxServiceManager::AnboxServiceManager()
    : arc_bridge_service_(std::make_unique<ArcBridgeService>()) {
  DCHECK(!g_anbox_service_manager);
  g_anbox_service_manager = this;
}

AnboxServiceManager::~AnboxServiceManager() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_EQ(g_anbox_service_manager, this);
  g_anbox_service_manager = nullptr;
}

// static
AnboxServiceManager* AnboxServiceManager::Get() {
  if (!g_anbox_service_manager)
    return nullptr;
  DCHECK_CALLED_ON_VALID_THREAD(g_anbox_service_manager->thread_checker_);
  return g_anbox_service_manager;
}

ArcBridgeService* AnboxServiceManager::arc_bridge_service() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return arc_bridge_service_.get();
}

}  // namespace arc
