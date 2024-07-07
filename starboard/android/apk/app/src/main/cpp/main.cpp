
// Copyright 2024 The Cobalt Authors. All Rights Reserved.

#include <jni.h>

#include "AndroidOut.h"

#include <game-activity/GameActivity.cpp>     // NOLINT(build/include)
#include <game-text-input/gametextinput.cpp>  // NOLINT(build/include)

extern "C" {

#include <game-activity/native_app_glue/android_native_app_glue.c>  // NOLINT(build/include)

/*!
 * Handles commands sent to this Android application
 * @param pApp the app the commands are coming from
 * @param cmd the command to handle
 */
void handle_cmd(android_app* pApp, int32_t cmd) {
  switch (cmd) {
    case APP_CMD_INIT_WINDOW:
      // A new window is created, associate a renderer with it. You may replace
      // this with a "game" class if that suits your needs. Remember to change
      // all instances of userData if you change the class here as a
      // reinterpret_cast is dangerous this in the android_main function and the
      // APP_CMD_TERM_WINDOW handler case.
      // pApp->userData = new Renderer(pApp);
      break;
    case APP_CMD_TERM_WINDOW:
      // The window is being destroyed. Use this to clean up your userData to
      // avoid leaking resources.
      //
      // We have to check if userData is assigned just in case this comes in
      // really quickly
      if (pApp->userData) {
        //
        // auto* pRenderer = reinterpret_cast<Renderer*>(pApp->userData);
        pApp->userData = nullptr;
        // delete pRenderer;
      }
      break;
    default:
      break;
  }
}

/*!
 * This the main entry point for a native activity
 */
void android_main(struct android_app* pApp) {
  // Can be removed, useful to ensure your code is running
  aout << "Welcome to android_main" << std::endl;

  // Register an event handler for Android events
  pApp->onAppCmd = handle_cmd;

  // This sets up a typical game/event loop. It will run until the app is
  // destroyed.
  int events;
  android_poll_source* pSource;
  do {
    // Process all pending events before running game logic.
    if (ALooper_pollAll(0, nullptr, &events,
                        (void**)&pSource) >= 0  // NOLINT(readability/casting)
    ) {
      if (pSource) {
        pSource->process(pApp, pSource);
      }
    }

    // Check if any user data is associated. This is assigned in handle_cmd
    if (pApp->userData) {
      // We know that our user data is a Renderer, so reinterpret cast it. If
      // you change your user data remember to change it here
      // auto* pRenderer = reinterpret_cast<Renderer*>(pApp->userData);

      // Process game input
      // pRenderer->handleInput();

      // Render a frame
      // pRenderer->render();
    }
  } while (!pApp->destroyRequested);
}
}
