/**
 * Push Notifications management for Argus Monitor
 * Handles service worker registration and push subscriptions
 */

class ArgusNotifications {
  constructor(config) {
    this.config = config;
    this.swRegistration = null;
    this.isSubscribed = false;
  }

  /**
   * Register service worker
   */
  async registerServiceWorker() {
    if (!('serviceWorker' in navigator)) {
      console.log('Service Worker not supported');
      return;
    }

    try {
      this.swRegistration = await navigator.serviceWorker.register('sw.js');
      console.log('Service Worker registered:', this.swRegistration);

      // Check if already subscribed
      const subscription = await this.swRegistration.pushManager.getSubscription();
      this.isSubscribed = !(subscription === null);
      this.updateNotifyButton();
    } catch (error) {
      console.error('Service Worker registration failed:', error);
    }
  }

  /**
   * Toggle push notification subscription
   */
  async togglePushSubscription() {
    const btn = document.getElementById('notification-btn');
    btn.disabled = true;

    try {
      if (this.isSubscribed) {
        await this.unsubscribe();
      } else {
        await this.subscribe();
      }
      this.updateNotifyButton();
    } catch (error) {
      console.error('Error toggling subscription:', error);
      alert('Failed to toggle notifications: ' + error.message);
    } finally {
      btn.disabled = false;
    }
  }

  /**
   * Subscribe to push notifications
   */
  async subscribe() {
    // Request notification permission
    const permission = await Notification.requestPermission();
    if (permission !== 'granted') {
      alert('Notification permission denied');
      return;
    }

    // Check if push is enabled on server
    const configUrl = this.config.buildUrl('/config.json');
    const configResponse = await fetch(configUrl);
    const config = await configResponse.json();

    if (!config.push_enabled) {
      alert('Push notifications not enabled on server');
      return;
    }

    // Get VAPID public key
    const vapidPublicKeyResponse = await fetch(this.config.buildUrl('/push/vapid_public_key'));
    const vapidPublicKey = await vapidPublicKeyResponse.text();

    // Subscribe to push
    const subscription = await this.swRegistration.pushManager.subscribe({
      userVisibleOnly: true,
      applicationServerKey: this.urlBase64ToUint8Array(vapidPublicKey)
    });

    // Send subscription to server
    const response = await fetch(this.config.buildUrl('/push/subscribe'), {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(subscription)
    });

    if (response.ok) {
      console.log('Subscribed to push notifications');
      this.isSubscribed = true;
    } else {
      throw new Error('Failed to subscribe on server');
    }
  }

  /**
   * Unsubscribe from push notifications
   */
  async unsubscribe() {
    const subscription = await this.swRegistration.pushManager.getSubscription();
    if (subscription) {
      await subscription.unsubscribe();

      // Notify server
      await fetch(this.config.buildUrl('/push/unsubscribe'), {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ endpoint: subscription.endpoint })
      });

      console.log('Unsubscribed from push notifications');
      this.isSubscribed = false;
    }

    // Unregister and re-register service worker for clean state
    await this.swRegistration.unregister();
    console.log('Service Worker unregistered');
    await this.registerServiceWorker();
  }

  /**
   * Update the notification button state
   */
  updateNotifyButton() {
    const btn = document.getElementById('notification-btn');
    if (this.isSubscribed) {
      btn.classList.add('subscribed');
      btn.textContent = 'UNSUB';
      btn.title = 'Unsubscribe from notifications';
    } else {
      btn.classList.remove('subscribed');
      btn.textContent = 'NOTIFY';
      btn.title = 'Subscribe to notifications';
    }
  }

  /**
   * Convert base64 string to Uint8Array for VAPID key
   */
  urlBase64ToUint8Array(base64String) {
    const padding = '='.repeat((4 - base64String.length % 4) % 4);
    const base64 = (base64String + padding).replace(/-/g, '+').replace(/_/g, '/');
    const rawData = window.atob(base64);
    const outputArray = new Uint8Array(rawData.length);
    for (let i = 0; i < rawData.length; ++i) {
      outputArray[i] = rawData.charCodeAt(i);
    }
    return outputArray;
  }
}
