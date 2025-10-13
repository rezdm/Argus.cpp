// Service Worker for Argus Monitor PWA
// IMPORTANT: Increment this version number whenever you update any frontend files
const VERSION = '1.0.1';
const CACHE_NAME = `argus-monitor-v${VERSION}`;
const RUNTIME_CACHE = `argus-runtime-v${VERSION}`;

// Files to cache on installation
const PRECACHE_URLS = [
  './',
  './index.html',
  './style.css',
  './js/config.js',
  './js/ui.js',
  './js/notifications.js',
  './js/main.js',
  './manifest.json',
  './icons/icon-192x192.png',
  './icons/icon-512x512.png'
];

// Install event - cache static assets
self.addEventListener('install', (event) => {
  console.log('[SW] Installing service worker...');
  event.waitUntil(
    caches.open(CACHE_NAME)
      .then((cache) => {
        console.log('[SW] Precaching static assets');
        return cache.addAll(PRECACHE_URLS).catch((err) => {
          console.warn('[SW] Failed to precache some assets:', err);
          // Don't fail installation if some assets fail to cache
        });
      })
      .then(() => self.skipWaiting())
  );
});

// Activate event - clean up old caches
self.addEventListener('activate', (event) => {
  console.log('[SW] Activating service worker...');
  event.waitUntil(
    caches.keys().then((cacheNames) => {
      return Promise.all(
        cacheNames
          .filter((cacheName) => cacheName !== CACHE_NAME && cacheName !== RUNTIME_CACHE)
          .map((cacheName) => {
            console.log('[SW] Deleting old cache:', cacheName);
            return caches.delete(cacheName);
          })
      );
    }).then(() => self.clients.claim())
  );
});

// Fetch event - network first, then cache fallback
self.addEventListener('fetch', (event) => {
  // Skip cross-origin requests
  if (!event.request.url.startsWith(self.location.origin)) {
    return;
  }

  // API requests - network first with cache fallback
  if (event.request.url.includes('/status')) {
    event.respondWith(
      fetch(event.request)
        .then((response) => {
          // Clone the response before caching
          const responseClone = response.clone();
          caches.open(RUNTIME_CACHE).then((cache) => {
            cache.put(event.request, responseClone);
          });
          return response;
        })
        .catch(() => {
          // Network failed, try cache
          return caches.match(event.request).then((cachedResponse) => {
            return cachedResponse || new Response(
              JSON.stringify({ error: 'Offline - cached data unavailable' }),
              { headers: { 'Content-Type': 'application/json' } }
            );
          });
        })
    );
    return;
  }

  // Static assets - cache first, network fallback
  event.respondWith(
    caches.match(event.request).then((cachedResponse) => {
      if (cachedResponse) {
        return cachedResponse;
      }

      return fetch(event.request).then((response) => {
        // Don't cache non-successful responses
        if (!response || response.status !== 200 || response.type === 'error') {
          return response;
        }

        // Clone and cache the response
        const responseClone = response.clone();
        caches.open(RUNTIME_CACHE).then((cache) => {
          cache.put(event.request, responseClone);
        });

        return response;
      });
    })
  );
});

// Push notification event
self.addEventListener('push', (event) => {
  console.log('[SW] ========================================');
  console.log('[SW] Push event received!');
  console.log('[SW] Event type:', event.type);
  console.log('[SW] Has data:', !!event.data);
  console.log('[SW] ========================================');

  let notificationData = {
    title: 'Argus Monitor Alert',
    body: 'A service status has changed',
    icon: './icons/icon-192x192.png',
    badge: './icons/icon-72x72.png',
    tag: 'argus-notification',
    requireInteraction: true,
    data: {}
  };

  // Parse push data if available
  if (event.data) {
    try {
      console.log('[SW] Attempting to parse push data as JSON...');
      const data = event.data.json();
      console.log('[SW] Push data parsed successfully:', data);
      notificationData = {
        title: data.title || notificationData.title,
        body: data.body || data.message || notificationData.body,
        icon: data.icon || notificationData.icon,
        badge: data.badge || notificationData.badge,
        tag: data.tag || notificationData.tag,
        requireInteraction: data.requireInteraction !== false,
        data: data.data || data,
        actions: data.actions || []
      };
    } catch (e) {
      console.error('[SW] Failed to parse push data as JSON:', e);
      console.error('[SW] Error name:', e.name);
      console.error('[SW] Error message:', e.message);
      console.error('[SW] Error stack:', e.stack);
      try {
        const textData = event.data.text();
        console.log('[SW] Push data as text:', textData);
        notificationData.body = textData || notificationData.body;
      } catch (textError) {
        console.error('[SW] Failed to get text from push data:', textError);
      }
    }
  } else {
    console.log('[SW] No push data received, using defaults');
  }

  console.log('[SW] Showing notification:', notificationData.title);

  event.waitUntil(
    self.registration.showNotification(notificationData.title, {
      body: notificationData.body,
      icon: notificationData.icon,
      badge: notificationData.badge,
      tag: notificationData.tag,
      requireInteraction: notificationData.requireInteraction,
      data: notificationData.data,
      actions: notificationData.actions,
      vibrate: [200, 100, 200]
    }).then(() => {
      console.log('[SW] Notification shown successfully');
    }).catch((err) => {
      console.error('[SW] Failed to show notification:', err);
    })
  );
});

// Notification click event
self.addEventListener('notificationclick', (event) => {
  console.log('[SW] Notification clicked');
  event.notification.close();

  // Open the app or focus existing window
  event.waitUntil(
    clients.matchAll({ type: 'window', includeUncontrolled: true })
      .then((clientList) => {
        // Check if there's already a window open
        for (const client of clientList) {
          if (client.url.includes(self.location.origin) && 'focus' in client) {
            return client.focus();
          }
        }
        // Open new window if none exists
        if (clients.openWindow) {
          return clients.openWindow('/');
        }
      })
  );
});

// Background sync event (for future use)
self.addEventListener('sync', (event) => {
  console.log('[SW] Background sync:', event.tag);
  if (event.tag === 'sync-status') {
    event.waitUntil(
      fetch('/status')
        .then((response) => response.json())
        .then((data) => {
          console.log('[SW] Synced status data:', data);
        })
        .catch((err) => {
          console.error('[SW] Sync failed:', err);
        })
    );
  }
});

// Message event - handle messages from the app
self.addEventListener('message', (event) => {
  console.log('[SW] Message received:', event.data);

  if (event.data && event.data.type === 'SKIP_WAITING') {
    self.skipWaiting();
  }

  if (event.data && event.data.type === 'CACHE_URLS') {
    event.waitUntil(
      caches.open(RUNTIME_CACHE).then((cache) => {
        return cache.addAll(event.data.urls);
      })
    );
  }

  if (event.data && event.data.type === 'TEST_NOTIFICATION') {
    console.log('[SW] Showing test notification');
    self.registration.showNotification('Test Notification', {
      body: 'This is a test notification from the service worker',
      icon: './icons/icon-192x192.png',
      tag: 'test'
    });
  }
});
