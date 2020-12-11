// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_ANBOX_APP_WINDOW_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_ANBOX_APP_WINDOW_H_

#include "chrome/browser/ui/ash/launcher/arc_app_window.h"

// A ui::BaseWindow for a chromeos launcher to control ARC applications.
class AnboxAppWindow : public ArcAppWindow{                     
 public:
  AnboxAppWindow(int task_id,
               const arc::ArcAppShelfId& app_shelf_id,
               views::Widget* widget,
               ArcAppWindowDelegate* owner,
               Profile* profile);

  ~AnboxAppWindow() override;

public:
  void Close() override;

private:
  DISALLOW_COPY_AND_ASSIGN(AnboxAppWindow);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_ANBOX_APP_WINDOW_H_
