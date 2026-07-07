package com.google.android.libraries.tv.contextawareness;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.IBinder;
import android.os.RemoteException;
import android.util.Log;
import androidx.collection.ArraySet;
import com.google.android.apps.tv.tcas.awareness.IContextEventListener;
import com.google.android.apps.tv.tcas.awareness.ITvContextAwarenessService;
import java.util.ArrayList;
import java.util.List;
import java.util.Set;

/**
 * Client library for interacting with context awareness services. This class provides a unified
 * interface to register for various context events.
 */
public class ContextAwarenessClient {
  private static final String TAG = "ContextAwarenessClient";

  /** Event type for key presses. */
  public static final int EVENT_TYPE_KEY_PRESS = 1;

  /** Event subtype for any key press. */
  public static final int EVENT_SUBTYPE_KEY_PRESS_ANY = 101;

  // TV Context Awareness Service constants
  private static final String TV_CONTEXT_AWARENESS_SERVICE_PACKAGE =
      "com.google.android.apps.tv.tcas";
  private static final String TV_CONTEXT_AWARENESS_SERVICE_CLASS =
      "com.google.android.apps.tv.tcas.awareness.TvContextAwarenessService";
  private static final String TV_CONTEXT_AWARENESS_SERVICE_ACTION =
      "com.google.android.apps.tv.tcas.awareness.ITvContextAwarenessService";

  public interface ConnectionListener {
    void onConnected();
    void onDisconnected();
  }

  private final Context context;

  private ITvContextAwarenessService contextAwarenessService;

  private boolean isServiceBound = false;
  private boolean isBinding = false;
  private ConnectionListener connectionListener;
  private final Set<IContextEventListener> registeredListeners = new ArraySet<>();
  private final List<Runnable> pendingRegistrations = new ArrayList<>();

  public ContextAwarenessClient(Context context) {
    this.context = context.getApplicationContext();
  }

  public void setConnectionListener(ConnectionListener listener) {
    this.connectionListener = listener;
  }

  /**
   * Registers a listener for a specific event type.
   *
   * @param listener The listener to register.
   * @param eventType The type of event to listen for.
   * @param eventSubtype The subtype of event (if any).
   */
  public void registerContextEventListener(
      IContextEventListener listener, int eventType, int eventSubtype) {
    synchronized (this) {
      if (contextAwarenessService != null) {
        internalRegister(listener, eventType, eventSubtype);
      } else {
        Log.w(TAG, "Service not bound, queueing registration and attempting to bind.");
        pendingRegistrations.add(
            () -> {
              if (contextAwarenessService != null) {
                internalRegister(listener, eventType, eventSubtype);
              } else {
                Log.e(TAG, "Service became null before pending registration could complete.");
              }
            });
        if (!isBinding) {
          bindService();
        }
      }
    }
  }

  /**
   * Unregisters a listener for a specific event type.
   *
   * @param listener The listener to unregister.
   * @param eventType The type of event to unregister from.
   * @param eventSubtype The subtype of event (if any).
   */
  public void unregisterContextEventListener(
      IContextEventListener listener, int eventType, int eventSubtype) {
    synchronized (this) {
      if (contextAwarenessService != null) {
        try {
          contextAwarenessService.unregisterContextEventListener(listener, eventType, eventSubtype);
          registeredListeners.remove(listener);
          if (registeredListeners.isEmpty() && isServiceBound) {
            unbindService();
          }
        } catch (RemoteException e) {
          Log.e(TAG, "Failed to unregister context event listener", e);
        } catch (SecurityException e) {
          Log.e(TAG, "Security Exception unregistering context event listener", e);
        }
      } else {
        Log.w(TAG, "Service not bound.");
      }
    }
  }

  private void internalRegister(IContextEventListener listener, int eventType, int eventSubtype) {
    try {
      contextAwarenessService.registerContextEventListener(listener, eventType, eventSubtype);
      registeredListeners.add(listener);
    } catch (RemoteException e) {
      Log.e(TAG, "Failed to register context event listener", e);
    } catch (SecurityException e) {
      Log.e(TAG, "Security exception registering context event listener", e);
    }
  }

  private void bindService() {
    if (isServiceBound || isBinding) {
      throw new IllegalStateException("Service is already bound or binding.");
    }

    Log.i(TAG, "Charley: ContextAwarenessClient attempting bindService to package: " + TV_CONTEXT_AWARENESS_SERVICE_PACKAGE);
    Intent intent = new Intent(TV_CONTEXT_AWARENESS_SERVICE_ACTION);
    intent.setClassName(TV_CONTEXT_AWARENESS_SERVICE_PACKAGE, TV_CONTEXT_AWARENESS_SERVICE_CLASS);
    try {
      isBinding = true;
      boolean success = context.bindService(intent, serviceConnection, Context.BIND_AUTO_CREATE);
      if (!success) {
        Log.e(TAG, "Charley: Failed to bind to TvContextAwarenessService (bindService returned false)");
        isBinding = false;
      }
    } catch (SecurityException e) {
      isBinding = false;
      Log.e(TAG, "Charley: SecurityException binding to TvContextAwarenessService", e);
    }
  }

  private void unbindService() {
    if (!isServiceBound) {
      throw new IllegalStateException("Service is not bound");
    }
    context.unbindService(serviceConnection);
    isServiceBound = false;
    contextAwarenessService = null;
    if (connectionListener != null) {
      connectionListener.onDisconnected();
    }
  }

  private final ServiceConnection serviceConnection =
      new ServiceConnection() {
        @Override
        public void onServiceConnected(ComponentName name, IBinder service) {
          synchronized (ContextAwarenessClient.this) {
            Log.i(TAG, "Charley: TvContextAwarenessService connected!");
            contextAwarenessService = ITvContextAwarenessService.Stub.asInterface(service);
            isServiceBound = true;
            isBinding = false;
            for (Runnable registration : pendingRegistrations) {
              registration.run();
            }
            pendingRegistrations.clear();
            if (connectionListener != null) {
              connectionListener.onConnected();
            }
          }
        }

        @Override
        public void onServiceDisconnected(ComponentName name) {
          synchronized (ContextAwarenessClient.this) {
            Log.i(TAG, "Charley: TvContextAwarenessService disconnected!");
            contextAwarenessService = null;
            isServiceBound = false;
            isBinding = false;
            registeredListeners.clear();
            if (connectionListener != null) {
              connectionListener.onDisconnected();
            }
          }
        }
      };
}
