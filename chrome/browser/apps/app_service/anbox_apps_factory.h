

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_ANBOX_APPS_FACTORY_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_ANBOX_APPS_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace apps {

class AnboxApps;

class AnboxAppsFactory : public BrowserContextKeyedServiceFactory {
 public:
  static AnboxApps* GetForProfile(Profile* profile);

  static AnboxAppsFactory* GetInstance();

  static void ShutDownForTesting(content::BrowserContext* context);

 private:
  friend struct base::DefaultSingletonTraits<AnboxAppsFactory>;

  AnboxAppsFactory();
  ~AnboxAppsFactory() override;

  // BrowserContextKeyedServiceFactory overrides.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(AnboxAppsFactory);
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_ANBOX_APPS_FACTORY_H_
